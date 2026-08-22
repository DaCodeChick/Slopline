# AppWarrior UI/Graphics/Media Audit

*Scope: `AppWarrior/Source/Graphics`, `Images`, `User Interface`, `Views`, `Hardware`, `Misc`, `Libs/QTDataHandler`, `Resources`, `Error Msgs`, the `AppWarrior/Headers/C*.h` and `U*.h` headers, and the app-level consumers under `Apps/Client`, `Apps/Server`, `Apps/Tracker`, `Apps/FileFinder`, `Apps/Rez2Dat`. Read-only; based on actual file contents and call sites.*

---

## 1. Graphics

### What it is

The graphics layer is a hand-rolled, QuickDraw-*style* 2-D raster API with a portable core and two native backends. It is split by the historical `(M)`/`(W)` convention: **`(M)` = Macintosh** (Carbon QuickDraw/GWorld), **`(W)` = Windows** (GDI). Files with no suffix are the portable dispatch shim.

| File | LOC | Role |
|---|---|---|
| `GrafTypes.h/.cpp` | 463 / 659 | Value types: `SPoint`, `SRect`, `SColor`, `SPackedColor`, `SLine`, `SRoundRect`, `SMatrix3`, 150+ named color constants |
| `UGraphics.h` | 727 | Declares `UGraphics` static API + `TImageObj` object façade |
| `UGraphics.cpp` | 1342 | Portable shim: origin/clip translation, then forwards to `UGraphics_*` |
| `UGraphics(M).cpp` | 5254 | Mac Carbon backend |
| `UGraphics(W).cpp` | 3229 | Win32 GDI backend |
| `UPixmap.h/.cpp` | 87 / 1962 | Flattened multi-layer pixmap container ("AWPX") |
| `UIcon.h/.cpp/(M)/(W)` | 35 / 65 / 352 / 74 | Icon resource load/draw |
| `URegion.h/(M)/(W)` | 157 / 563 / 417 | Region (clipping/invalidation) algebra |

### Pixel formats and endianness

The canonical in-memory pixel struct is `SPixmap` (`UPixmap.h:20-29`): `width/height/depth/rowBytes/colorCount/data/colorTab`, with `depth` ∈ {1,2,4,8,16,24,32}. `SExtPixmap` (`UPixmap.h:32-55`) adds `transColor`, `pixPackType`, `pixCompType` (0=none, 1=RLE), `pixLaceType`, and `colorPackType` (0=rgb, 1=rgba).

The flattened on-disk format is documented in the `UPixmap.cpp` header comment (`UPixmap.cpp:30-64`): a `Uint32 format` magic **`'AWPX'`**, big-endian version 1, then one or more layer headers followed by a byte `data[]` area with explicit `colorDataOffset`/`pixelDataOffset`/`miscDataOffset`. All multi-byte fields are read through `FB()` ("from big-endian") and written through `TB()` ("to big-endian") — i.e. **the flattened format is fixed big-endian** and byte-swapped on Intel (`UPixmap.cpp:136,154,181,185,192,216`).

Color is modeled as **48-bit** `SColor {Uint16 red,green,blue}` (`GrafTypes.h:182-239`), with an explicit "packed" 32-bit conversion guarded by `#if CONVERT_INTS` that swaps between `0xRRGGBB` (big-endian/mac) and `0xBBGGRR` (little-endian/win) — see `GrafTypes.h:231-238`. The 5→8-bit expansion tables confirm the *color-table* 32-bit layout is **`0x00RRGGBB`** (R in bits 31–24, G in 23–16, B in 15–8, low byte spare), visible in `UPixmap.cpp:105-107` (`0xRR000000 / 0x00GG0000 / 0x0000BB00`). A known 16-bit endianness defect is documented in the header comment of `UPixmap.cpp:16-22` ("always produces ARRRRRGGGGGBBBBB … not on Intel").

### QuickDraw-style concepts

`UGraphics` mirrors a QuickDraw `GrafPort` closely: `NewCompatibleImage` (offscreen GWorld/bitmap), `SetOrigin/GetOrigin`, `SetClip/IntersectClip`, pen (`SetPenSize`), "ink" transfer modes (`mode_Copy/Or/Xor/Over/Atop/Add/Blend/Fade/…`, `UGraphics.h:27-47`), patterns (`SSimplePattern` 8-byte, `UGraphics.h:182-188`), `DrawPicture`/`TPicture`, `FontMetrics`, and `CopyPixels/CopyPixelsMasked/CopyPixelsTrans/StretchPixels`. Region semantics (`URegion`) expose QuickDraw-style `AddRect/AddOval/AddRoundRect`, difference/union/intersection/xor, and `IsPointWithin`.

### Consumers

- Every view's `Draw(TImage,…)` and the whole `CWindow::Draw` path.
- Image decoders render through `UGraphics::CopyPixels` (`CDecompressImage.cpp:59`).
- Icon rendering: `UIcon::Draw(Int32 inID, …)` used by `CIconView`/`CIconButtonView` and the banner code; `UIcon::Load` reads the `'ICON'`/`'cicn'` resources.
- Banner drawing in Client via `CMyBannerToolbarWin::SetBannerHdl` → `CPictureView`/`CAnimatedGifView`/`CImageView`.

### Verdict & replacement

**preserve-semantics-rewrite.** The *geometry types* (`SPoint`/`SRect`/`SColor`/`SMatrix3`/regions) and the *drawing surface abstraction* (`TImage`, clip, pen, ink, fonts, copy-pixels) are the correct abstraction to re-express over a modern raster stack. Both native backends (Mac QuickDraw/Carbon and Win32 GDI) are dead technology and must be **replaced-with-modern** (e.g. Skia, Cairo, or a platform 2-D canvas), keeping the `UGraphics`/`SPixmap`/`URegion` surface semantics. `UGraphics(M).cpp` (5254 LOC) and `UGraphics(W).cpp` (3229 LOC) are removable in their entirety once the surface is re-implemented. The `'AWPX'` flattened pixmap format must be decoded (read-only) to render legacy splash/banner assets (`Apps/Images/19Splash.HLPixmap`), but should not be a native modern format.

---

## 2. Image decoders

### What it is

`AppWarrior/Source/Images/*` is a family of progressive, buffer-fed raster decoders sharing a common base `CDecompressImage` (`CDecompressImage.h`). The base holds a source buffer, a `SPixmap`, a compat `TImage`, and does **row-progressive** update (`UpdateImage()`, `CDecompressImage.cpp:100-129`) so decoders can show an image as it streams (important for Hotline's progressive file/image preview).

- **`CDecompressJpeg`** (256 LOC) — a wrapper around the **Independent JPEG Group libjpeg 6b** (`Images/Jpeg/jversion.h:12` — `"6b  27-Mar-1998"`, "Copyright (C) 1998, Thomas G. Lane"). Full libjpeg source (75 files, ~23.8k LOC) is vendored. It supports progressive JPEG (`jpeg_has_multiple_scans`/`buffered_image`), 1/2/4/8 scale denominators, and only RGB or grayscale output (`CDecompressJpeg.cpp:107-108`). It feeds libjpeg through a custom memory source (`my_src_ptr`, 2048-byte chunks — `CDecompressJpeg.cpp:21`) and drives the row state machine manually (`ReadJpegImage`, `CDecompressJpeg.cpp:79-255`).
- **`CDecompressGif`** (816 LOC) — a hand-written GIF **89a** decoder (`GIFHEADER/IMAGEBLOCK/CONTROLBLOCK`, `ImageTypes.h:42-99`): LZW unpack, interlace passes, transparent color (`GetTransColor`/`GetTransMask`), extension blocks. It decodes a *single frame*; animation is driven by the view layer (`CAnimatedGifView`).
- **`CDecompressPict`** (2091 LOC) — an Apple **PICT v1/v2** opcode interpreter (`SMacPixMap`, opcode tables, `DoBitmap`/`DoPixmap`/`OpCode9A/9B`, bitplane unpack, colour tables, and **JPEG-in-PICT** via an embedded `CDecompressJpeg` — `CDecompressPict.h:74`, `CDecompressPict.cpp` `SearchJpeg/ReadJpeg`).
- **`CDecompressBitmap`** (201 LOC) — Windows **BMP** decoder (`BITMAPFILEHEAD`/`BITMAPINFOHEAD`, `ImageTypes.h:105-128`).
- **`CDecompressImage`** (165 LOC) — *not* a real factory; it is the abstract base + shared progressive-draw machinery. **The format-detection/factory dispatch lives in the app** (`Apps/Client/Source/HotlineWindows.cpp`), which sniffs the Hotline file type-code and instantiates the matching decoder.

### Which apps use which decoders

Only the **Client** decodes images. Grep across `Apps/` shows every `CDecompressJpeg/Gif/Pict/Bitmap` instantiation occurs in `Apps/Client/Source/HotlineWindows.cpp` (image preview window) and the `CImageView`/`CAnimatedGifView` definitions in `Apps/Client/Source/HotlineViews.cpp`. **Server** and **Tracker** never decode images — they only store/forward banner bytes (`HotlineServ.cpp:1285` "if is not an image we assume that is a QuickTime file"). `CDecompressImage*` is also passed to `CImageView` and `CAnimatedGifView`.

### Verdict & replacement

- **CDecompressJpeg** → **replace-with-modern**: delete vendored libjpeg 6b (ancient, unpatched) and decode via a modern image library (libjpeg-turbo, stb_image, or the platform image codec). Keep only the progressive-update contract if live preview is desired.
- **CDecompressGif** → **replace-with-modern** (animated GIF via the platform or a modern lib). The hand-written LZW has no value to preserve.
- **CDecompressPict** → **remove-entirely** for new code; **retain-temporarily** only if there are legacy Mac `.pict` assets/banner files in the wild that must still render. No modern stack should be built on PICT.
- **CDecompressBitmap** → **replace-with-modern** (trivial BMP decode).
- **CDecompressImage** base → **preserve-semantics-rewrite** as a thin `IImageDecoder` interface with a progressive-draw callback; the buffer/`SPixmap`/`TImage` plumbing maps onto the new raster surface.

---

## 3. UI framework

### Event / message model (`UMessageSys`)

`UMessageSys.h` defines a minimal priority message pump: `New/Dispose`, `Post/Replace/Flush/Peek`, `Execute`, and a `SetDefaultProc` fallback. Messages are `Uint32 inMsg` + optional `inData/inDataSize` + a `TMessageProc` callback + `inObject` (`UMessageSys.h:23-26`). `UApplication` wraps a process-global message system (`UApplication.h:14-30`): `Init/Process/Run/Quit`, `SendMessage/PostMessage/ReplaceMessage/FlushMessages/PeekMessage`. `CApplication` (`CApplication.h`) is the subclassable app base with `Run/UserQuit/Quit/KeyCommand/WindowHit` and `MessageHandler`/`WindowHitHandler` static trampolines. **Every event — window hits, timer fires, network completion — is funneled through this message system** to the app's `HandleMessage`.

### View hierarchy

`CView` (`CView.h:92`) is the base. It holds `mBounds/mID/mCommandID/mSizing/mState` plus bitfields for mouse/drag/focus/visible/enable/capture state. Its virtual surface is the classic widget contract: `Draw(TImage, updateRect, depth)`, `MouseDown/Up/Enter/Move/Leave`, `KeyDown/Up/Repeat`, drag-and-drop `DragEnter/Move/Leave/Drop`, `UpdateQuickTime`/`SendToQuickTime`, `Timer`, `ChangeState/TabFocusNext/TabFocusPrev`, and full-size queries (`GetFullSize/GetFullWidth/GetFullHeight`).

Containment is implemented via the **handler pattern**, not a base-class list: `CView` delegates install/remove/refresh/hit/visibility/capture/make-visible to a `CViewHandler*` (`CView.h:225-242`). `CViewContainer : CViewHandler` (`CViewContainer.h:8`) has `CSingleViewContainer` (one child; used by `CWindow` and `CScrollerView`) and `CMultiViewContainer` (`CPtrList<CView>`, used by `CContainerView`/`CTabbedView`). `CView::Refresh` invalidates through the handler (`CView.cpp:642-652`), and `CView::Hit` marshals a packed `SHitMsgData` to the handler (`CView.cpp:281-311`).

Full hierarchy (from `AppWarrior/Headers/*.h` class declarations):

```
CView
├─ CBoxView, CLabelView, CLabelUrlView, CLaunchUrlView, CTextView, CPasswordTextView,
│  CColorView, CSeparatorView, CButtonView, CCheckBoxView, CIconButtonView,
│  CSimpleIconButtonView, CIconView, CProgressView, CPictureView
├─ CLaunchUrlView
│  ├─ CImageView ── CAnimatedGifView
│  └─ CQuickTimeView
├─ CItemsView (abstract item model)
│  └─ CSelectableItemsView (selection + drag, CDragAndDroppable)
│     ├─ CListView ── CGeneralListView<T> (header template)
│     │   └─ CTabbedListView ── CGeneralTabbedListView<T>
│     └─ CTreeView<T> (header template)
│         └─ CTabbedTreeView<T>
├─ CView, CMultiViewContainer ── CContainerView ── CPaneView
├─ CView, CSingleViewContainer ── CScrollerView
└─ CView, CMultiViewContainer ── CTabbedView (+ CTabbedItemsView mixin)
CViewContainer ─ CSingleViewContainer ─ CWindow (CSingleViewContainer)
```

### Hit testing

Hit events flow **view → handler → window → app**: `CView::Hit(type, part, param, data)` packs `SHitMsgData{view,id,cmd,type,part,param,dataSize,data}` and calls `mHandler->HandleHit` (`CView.cpp:281-311`). `CWindow::HandleHit` (`CWindow.h:160`, `CWindow.cpp`) enqueues into `mHitQueue`; the app drains it in `CWindow::ProcessModal`/the message loop and dispatches to `CApplication::WindowHit`. Hit *types* are `hitType_Standard/Alternate/Change/Drop` (`CView.h:36-41`); standard command IDs (`cmd_OK/cmd_Cancel/…`) are predefined (`CView.h:44-67`). `CWindow` itself adds `hitType_WindowCloseBox = 20` (`CWindow.h:12`).

### Drawing model

Immediate-mode, update-rect, retained via invalidation. `CView::Draw(TImage inImage, const SRect& inUpdateRect, Uint32 inDepth)` is overridden per view and draws straight into the window's back-buffer `TImage`; `Refresh(rect)` computes the invalid rect and calls `mHandler->HandleRefresh`. `CWindow` owns the actual `UWindow` (`TWindow mWindow`, `CWindow.h:149`) and its `Draw(inUpdateRect)` paints background + children. Drawing is a **single-threaded, event-loop-driven** model (a `Refresh` merely marks; the pump repaints).

### Scrolling model

`CScrollerView` (`CScrollerView.h`) is a `CSingleViewContainer` that owns **one** content child and two scrollbars. It keeps `mHScrollVal/mVScrollVal` + `mHScrollMax/mVScrollMax`, computes `mContentRect/mDestRect/mHScrollRect/mVScrollRect` in `CalcRects/RecalcRects` (`CScrollerView.h:99-101`), and translates the child via the handler callbacks `HandleGetScreenDelta` (offset to apply when drawing the child), `HandleGetVisibleRect`, `HandleGetOrigin`, and `HandleMakeRectVisible` (`CScrollerView.h:114-126`). `ScrollToRect/ScrollToTop/Bottom`, `SetScroll`, and scrollbar drag thumbs are all in the 1483-LOC `CScrollerView.cpp`. List/tree views do *not* scroll themselves — they are wrapped in a `CScrollerView` (confirmed by app code: every list/tree is installed inside a `CScrollerView`).

### Consumer table (which apps instantiate which views)

From `grep -raoE "new C[A-Za-z]+View" Apps/` (counts per app source file), the **base framework views actually instantiated per app** are:

| Base view class | Client | Server | Tracker | NewsSynch (Server util) | FileFinder |
|---|---|---|---|---|---|
| `CLabelView` | ✓ (43+9+6+…) | ✓ (62) | ✓ (17/14) | ✓ (54) | |
| `CContainerView` | ✓ (43) | ✓ (21) | ✓ (7/6) | ✓ (11) | |
| `CButtonView` | ✓ (27) | ✓ (14) | ✓ (5/4) | ✓ (8) | |
| `CCheckBoxView` | ✓ (28) | ✓ (18) | ✓ (5) | ✓ (4) | |
| `CBoxView` | ✓ (22) | ✓ (10) | ✓ (2/1) | ✓ (14) | |
| `CIconButtonView` | ✓ (46) | ✓ (1) | ✓ (1) | ✓ (8) | |
| `CSimpleIconButtonView` | ✓ (4) | | | | |
| `CIconView` | ✓ (14) | ✓ (4) | | | |
| `CScrollerView` | ✓ (12) | ✓ (2) | ✓ (3/2) | ✓ (10) | |
| `CPaneView` | ✓ (4) | | | ✓ (7) | |
| `CTabbedView` | ✓ (1) | ✓ (1) | ✓ (1/1) | ✓ (1) | |
| `CColorView` | ✓ (2) | | | | |
| `CImageView` | ✓ (1) | | | | |
| `CAnimatedGifView` | ✓ (1) | | | | |
| `CQuickTimeView` (`CMyQuickTimeView`) | ✓ (3) | | | | |
| `CListView`/`CTreeView`/`CGeneralListView` | via `CMy*` subclasses | via `CMy*` | via `CMy*` | via `CMy*` | via `ComboListView` |
| `CPasswordTextView` | ✓ | | | | |
| `CLabelUrlView` / `CLaunchUrlView` | indirect (`CImageView`,`CQuickTimeView` base) | | | | |

Notes: the *heavy* consumers are the app-specific `CMy*` subclasses of `CListView`/`CTreeView`/`CGeneralListView` (e.g. `CMyFileListView`, `CMyFileTreeView`, `CMyNewsArticleTreeView`, `CMyUserListView`, `CMyServerTreeView`, `CMyAccountListView`, `CMyTrackerListView`, `CMyArticleTreeView`, `CMyTaskListView`, `CMyLoginListView`, `CMyPermBanListView`), all of which are thin item-provider overrides over the framework list/tree. `FileFinder` uses a single custom `ComboListView` (`Apps/FileFinder/Sources/FileFinder.cpp`).

This is the **UI requirements inventory** for a modern UI: label, box/frame, button, checkbox, icon-button, icon, color swatch, separator, container/pane, scroller (list/tree host), tabbed container, editable text, password field, progress bar, picture (static image), animated GIF, image (clickable/URL), QuickTime movie, list, and tree. Selection semantics are centralized in `CSelectableItemsView`/`CItemsView` (`itemBehav_*` flags, `CItemsView.h:65-80`).

### Verdict & replacement

**replace-with-modern.** The framework is a clean, consistent MVC-ish widget layer, but it is an immediate-mode, single-threaded, QuickDraw-era design with a bespoke message pump, manual invalidation, and no accessibility/layout/DPR/GPU concepts. The **semantic contract** (view tree, handler/container pattern, hit → `SHitMsgData` → app callback, item-based list/tree with selection behaviors) should be preserved as the mapping target for a modern retained UI (e.g. Qt/QtQuick, a web UI, or a custom Skia canvas widget layer). `UMessageSys`/`UApplication` message pump is superseded by any modern event loop.

---

## 4. QuickTime dependencies

### Every touchpoint

QuickTime is reached through **three** headers/systems:

1. **`UQuickTime.h`** (`AppWarrior/Headers/UQuickTime.h:1-6`) simply `#include`s `<QTML.h>`, `<Movies.h>`, `<Gestalt.h>` and `HL_Handler.h` — i.e. it is the "pull in the QuickTime SDK + Hotline data handler" header.
2. **`UOperatingSystem`** (`UOperatingSystem.h:10-20`) wraps availability: `InitQuickTime()`, `IsQuickTimeAvailable()`, `GetQuickTimeVersion()`, plus `CanHandleFlash()`.
3. **`CQuickTimeView`** (`CQuickTimeView.h/.cpp`, 897 LOC) is the concrete view. It holds `Movie mMovie; MovieController mController; ComponentInstance mInstance` and offers `SelectMovie`, `StreamMovie(Int8* inAddress)`, `SetMovie`, `StartMovie/StopMovie/CloseMovie/StopStreamMovie`, `SaveMovieAs`, `IsSupported(Uint32 inTypeCode)`, and `IsVideoTrack`. `IsSupported` (`CQuickTimeView.cpp:361-414`) accepts `'MooV'/'sooV'`, `'MPEG'/'Mpeg'/'mpeg'/'MPG '`; **audio formats are commented out** (`'MP3 '`, `'AIFF'/'AIFC'`, `'WAVE'/'.WAV'` — `CQuickTimeView.cpp:396-399`), so the retained code path is video movie playback only.

Call sites (all in **Client**):

- `Hotline.cpp:55` `UOperatingSystem::InitQuickTime()`; `Hotline.cpp:463-464` availability check at startup.
- `Hotline.cpp:2470-2490` — when opening/viewing a file, `CQuickTimeView::IsSupported(nTypeCode)` decides whether a file is shown as a movie; shows "QuickTime is not available" if missing.
- `Hotline.cpp:6207` and `Hotline.cpp:7175-7235` `GetResourceBanner(bool inQuickTimeBanner)` — the banner path **explicitly disables QuickTime banners** ("we do not allow quick time banners right now", `Hotline.cpp:7180-7182`) and always requests `'GIFf'`; QuickTime banner (`'MooV'` 128) is dead.
- `HotlineTasks.cpp:2849-3072` — `CMyViewFileTask::IsQuickTimeFile()` / `StartQuickTime()` for viewing a downloaded file as a movie.
- `HotlineWindows.cpp:3943-3984` — `CMyQuickTimeView` instantiation with `qtOption_ResizeWindow | qtOption_ShowController | qtOption_ShowSaveAs | qtOption_ResolveGrowBox`; `StreamMovie(csHotlineAddr)` streams a `hotline://…` URL; `StopStreamMovie`; `HL_HandlerIsReading`/`HL_HandlerCancelReading` (`HotlineWindows.cpp:4160-4179`) poll/cancel the streaming data handler.
- `HotlineViews.cpp:3303-3308` — `CMyQuickTimeView::LaunchURL()` launches the movie's URL via `CMyLaunchUrlTask`.
- `Hotline.cpp:7235-7239` — HTTP `User-Agent` string appends " with QuickTime %#s" when QT is present.

### Feature it powered

**Movie playback/streaming** (and, historically, the animated banner — now disabled). Specifically: viewing `.mov`/`.mpeg` files, streaming them from a `hotline://` URL through the QTDataHandler (below), and showing the movie controller (play/scrub/save-as). It did **not** power audio (that is `USound`), and it did **not** decode still images (that is the `CDecompress*` family). The HTTP UA string is the only non-playback QuickTime touch.

### Replacement options

Remove QuickTime entirely. Movie viewing is a legacy convenience feature (Hotline is a BBS/file/chat app, not a media player). Replacements, in order of effort: **(a) remove-entirely** (drop in-client movie playback); **(b) replace-with-modern** using a bundled media framework (FFmpeg/MPV) if movie playback must be retained; **(c) hand the file off to `UExternalApp`/OS "open with" for any media type** — the lowest-effort and most future-proof behavior. The QuickTime availability probe and the QuickTime UA-string fragment are deleted.

---

## 5. QTDataHandler sub-framework

### Inventory

`AppWarrior/Source/Libs/QTDataHandler/` (194 files, ~27.7k LOC total) is a **QuickTime Data Handler component** — a macOS/Windows QuickTime plug-in that teaches QuickTime to read media from Hotline's own URLs. Its registration is in `HL_Handler.cpp:38`: component `ComponentDescription desc = { 'dhlr', 'url ', 'htln', 0, 0 }` and in `QTResource.r:25` (`resource 'thng' (200, "Hotline Data Handler")`). The exported entry point is `HL_Handler` (`HL_Handler.cpp:10`), which dispatches the standard DataHandler selectors declared in `QTDispatcher.h` (`DataHandlerOpen/Close/CanDo/Version/SetDataRef/GetFileSize/OpenForRead/ScheduleData/FinishData/FlushData/…`). `CDataHandlerComponent` (`CDataHandlerComponent.h`) is a singleton managing `CDataHandlerConnection`s and `CDataProvider`s; `CAsyncReader` (`CAsyncReader.h`) is the async read path; `CDataProvider` maps a URL to a concrete data source.

The component embeds **a complete, self-contained mini-AppWarrior framework** under `QTDataHandler/AppWarrior/` (~24.5k LOC, in the `HL_BigRedH` namespace — note `HL_BigRedH CDataHandlerComponent` in `HL_Handler.cpp:82`): `Application`, `Debugging`, `Exceptions`, `FileSystem`, `Graphics` (its own `CGraphicsPort`, `CImage`, `CPen/CBrush/CFont/CRegion`), `Messages` (`CBroadcaster/CListener/CMessageWrangler`), `Networking` (`CNetTCPConnection`, `CNetUDPConnection`, `CNetTCPListener`), `Persistence` (`CFlattenable`, `CObjectFactory`, `CMemoryObjectCache`), `Resources` (`CResourceFile/CResourceList`), `Streams` (`CStreamBuffer`, `CFileStream`, `CAsyncStreamBuffer`), `Threads` (`CThread`, `CMutex`, `CSemaphore`, `CTimerThread`), and `Utilities` (`CString`, `CDateTime`, `UStringConverter`, `CRefCountBase`, `TProxy`). This is a **duplicate, independent reimplementation** of the same primitives that the main AppWarrior provides — it is *not* shared with the main framework.

### Consumers

The only external consumers are the Client's `CQuickTimeView` (via `HL_Handler.h` → `UQuickTime.h`) for `StreamMovie`, and the `HL_HandlerIsReading`/`HL_HandlerCancelReading` polling in `HotlineWindows.cpp:4160-4179`. Nothing outside QuickTime playback uses any QTDataHandler class.

### Verdict

**remove-entirely after QuickTime removal.** The entire 194-file tree exists to service QuickTime movie streaming. Once `CQuickTimeView` is gone, nothing remains that needs the `'dhlr'/'url '/'htln'` component, `HL_Handler`, `QTDispatcher`, `CDataHandlerComponent`, or `CAsyncReader`. The embedded `HL_BigRedH` mini-framework is dead code as well — its Networking/Streams/Threads/Persistence classes are not used by any app, and its Graphics/FileSystem/Utilities are parallel duplicates of the main AppWarrior (do **not** confuse them during modernization; only the main `AppWarrior/Source/` versions are in scope for preservation).

---

## 6. Hardware / platform services

| Module (files) | What it is | Consumers | Platform | Verdict |
|---|---|---|---|---|
| **`UKeyboard`** (`UKeyboard.h`, `.cpp`, `(M)`, `(W)`) | Key-state/modifier queries, `KeyToChar`, `FindKeyCommand`; full Mac-style keycode table (`UKeyboard.h:28-169`) | UI framework (`CWindow` dispatch, `CTextView`, `UEditText`), all apps via keyboard shortcuts | shared + M/W | **preserve-semantics-rewrite** (map onto modern key events) |
| **`UMouse`** (`UMouse.h`, `(M)`, `(W)`) | Cursor image, visibility, location, button state, double-click time | UI framework | shared + M/W | **preserve-semantics-rewrite** |
| **`USound`** (`USound.h`, `.cpp`, `(M)`, `(W)`) | `Beep`, `Play(THdl/TRez)`, low-level `TSoundOutput`, ADPCM compress/decompress, portable sound flatten | Client (`Hotline.cpp`, `HotlineNews.cpp`, `HotlineTasks.cpp` — chat/notification sounds), Server, Tracker | shared + M/W | **replace-with-modern** audio API (keep ADPCM codec only if legacy sound assets use it) |
| **`UDragAndDrop`** (`UDragAndDrop.h`, `(M)`, `(W)`) | Flavor-based drags, promised files, image/text flavors, `Track`, hilite | `CView` drag events, `CSelectableItemsView`/`CListView`/`CTreeView`, `CDragAndDroppable` | shared + M/W | **replace-with-modern** DnD (keep the flavor model semantics) |
| **`UClipboard`** (`UClipboard.h`, `(M)`, `(W)`) | Text/image/sound clipboard | `UEditText.cpp`, `CPasswordTextView.cpp` only (copy/paste); **no app directly calls it** | shared + M/W | **replace-with-modern** clipboard |
| **`UTooltip`** (`UTooltip.h`, `.cpp`) | Tooltip window state machine; `CView::MouseEnter` activates it (`CView.cpp:374-389`) | every view with `SetTooltipMsg` | shared | **replace-with-modern** |
| **`UExternalApp`** (`UExternalApp.h`, `(M)`, `(W)`) | Launch/activate/close external apps; file associations (`RegisterAssociation`/`LaunchAssociation`); `ReadSystemRegistry` | Client only (`Hotline.cpp:252-256` registers `\phbm`/`\phpf` associations; `Hotline.cpp:6261` launches updater) | shared + M/W | **replace-with-modern** (OS "open with") |
| **`UOleAutomation`** (`UOleAutomation.h`, `(W)`) | OLE Automation client + server, **`#if WIN32` only** | none in apps; only referenced by `URegularTransport(W).cpp` | Windows-only | **remove-entirely** (obsolete) |
| **`UService`** (`UService.h`, `(W)`) | Windows NT Service wrapper (install/uninstall/start/status/messages) | none in apps (Server does not use it); only header/lib + `UApplication(W)`/`UOperatingSystem(W)` internals | Windows-only | **remove-entirely** (obsolete) |
| **`UHttpTransact`** (`UHttpTransact.h`, shared `.cpp` 65.6k LOC + `(M)`/`(W)`) | HTTP 1.0/1.1 client transaction (`protocol_HTTP_1_0/1_1`, port 80) | Client (`Hotline.cpp`, `HotlineTasks.cpp`, `HotlineWindows.cpp`) for banner/agreement/update downloads; Server (`HotlineServ.cpp`) `LoadUrl`; NewsSynch | shared + M/W | **replace-with-modern** HTTP client |
| **`UTransport`** / **`URegularTransport`** (`UTransport.h/.cpp`, `URegularTransport(M/W)`) | Transport abstraction + `LaunchURL` (`UTransport.h:123`) | Client/Server/Tracker/NewsSynch | shared + M/W | **replace-with-modern** network layer; `LaunchURL` is browser-era |
| **`UNntpTransact`** (`UNntpTransact.h`, shared `.cpp` 96.3k LOC) | NNTP client (port 119, group/article commands) | NewsSynch utility (Server newsgroup sync) | shared | **replace-with-modern** or **remove-entirely** if NewsSynch is dropped |

---

## 7. Browser-era integration (obsolete)

The following are legacy Netscape/Mozilla-era *or* URL-launch affordances that must be treated as obsolete in a modern UI:

- **`CLaunchUrlView`** (`CLaunchUrlView.h/.cpp`, 140 LOC) — a `CView` that stores a URL + comment, changes the cursor to a hand, and `LaunchURL()` on mouse-up. Base of `CImageView`, `CAnimatedGifView`, `CQuickTimeView`.
- **`CLabelUrlView`** (`CLabelUrlView.h/.cpp`, 253 LOC) — a hyperlink-styled label (URL + hilite color, underline-on-hover, `LaunchURL()`).
- **`UTransport::LaunchURL`** (`UTransport.h:123-124`) and `URegularTransport::LaunchURL` (`URegularTransport.h:138`) — launch the default browser via the OS.
- **`CMyLaunchUrlTask`** (`HotlineTasks.cpp:2167-2250`, `Hotline.h:3442`) — a background task that wraps `UTransport::LaunchURL`; instantiated by `CMyImageView::LaunchURL`, `CMyAnimatedGifView::LaunchURL`, `CMyQuickTimeView::LaunchURL` (`HotlineViews.cpp:3255-3308`).
- **`UExternalApp`** association registration (`Hotline.cpp:252-256`) for `\phbm` ("Hotline Bookmark") and `\phpf` ("Hotline Partial File") — URL/file-association era integration.
- **Hardcoded vendor URLs** — `\phttp://www.lorbac.net` (`Hotline.cpp:1112-4271`, `HotlineServ.cpp:1021-1051`), `http://www.HotlineSW.com` (`HotlineServTrans.cpp`/`Error Msgs` strings), commented-out `hotlineisp.com` links (`Hotline.cpp:1146-1148`). These are dead-brand references to be removed.
- **`hotline://` URL scheme** — composed in `HotlineServ.cpp:1039-1048` (`"hotline://%#s@%#s:%lu"`) and consumed by `CQuickTimeView::StreamMovie` (`CQuickTimeView.cpp:93` compares `"hotline://"`).
- **HTTP user-agent branding** — `"Hotline/%s … with QuickTime %#s …"` (`Hotline.cpp:7231-7239`).

**Verdict: remove-entirely** as *browser*-integration, but **preserve-semantics-rewrite** the single useful behavior — "open an external URL/file with the OS default handler" (a modern equivalent of `LaunchURL`/`UExternalApp::LaunchAssociation`). There is no embedded Netscape/Mozilla/IE plugin *source* in the tree (no NPAPI/ActiveX plugin project); the "browser" integration is entirely *URL-launching and file-association* integration.

---

## 8. Resources and assets

### `AppWarrior/Resources/`

Only `Resource.h` (104 bytes) — the (otherwise empty) framework resource ID header. The framework's real runtime resources are the string/icon resources compiled into `Apps/Images/*.dat` and the per-app `.r`/`.rc` files.

### `AppWarrior/Error Msgs/`

Eight paired files, e.g. `UError(1)` + `UError(1).dat`, `UImage(10)`, `URez(6)`, `UMemory(3)`, `UFileSys(4)`, `UTransport(8)`, `UDragAndDrop(19)`, `USerialPort(20)`. The plain file is the **Rez source listing** (a numbered string list; e.g. `UError(1)` line 1 = "An unknown error has occured."); the `.dat` is its **compiled** binary (starts with magic `IVA1` then length/offset/string records). These are the localized framework error/status strings, compiled by `Rez2Dat` and loaded at runtime through `URez` (the `EMSG` resource type). `UError(1)` is referenced as `errorType_* = 1` and the framework's `UError`/`Fail` reporting maps `errorType/errorCode` to these string tables.

### `Apps/Images/` (binary asset inventory, via `file`)

| Asset | Format | Purpose |
|---|---|---|
| `19Banner.gif`, `185Banner.gif`, `19BannerISP.gif`, `19BannerISP.old.gif`, `GLbanner.gif` | GIF89a 468×60 | server banner images |
| `19Banner.mov`, `185Banner.mov`, `19BannerISP.mov`, `19BannerISP.old.mov` | Apple QuickTime movie | legacy animated banners (now unused) |
| `19Banner.png`, `19Splash.png` | PNG (RGBA / RGB) 468×60, 218×335 | PNG banner/splash |
| `19SplashISP.gif` | GIF89a 218×335 | ISP splash |
| `19Splash.HLPixmap` | `data` (AWPX flattened pixmap) | splash pixmap |
| `19Hotline.ico`, `19hotlineserv.ico`, `19aHotline.ico`, … | Windows icon | app icons |
| `19hotlineserv-a.iconsuite.icns` | Mac OS X icns (`ICN#`) | Mac app icon |
| `19Hotline.iconsuite`, `19Hotlineserv.iconsuite` | empty (0 bytes) | placeholder icon suites |
| `messageboard-icon.gif` (16×16), `securiphone-icon.gif` (15×15), `xprings-icon.gif` (16×16), `HL-ISP2.gif` (16×16) | GIF89a | toolbar/small icons |
| `hlc19.dat` (860k), `hlci19.dat` (877k), `hls19.dat` (13k) | `data` (Rez resource fork images) | compiled client/ISP/server resource files (icons, EMSG strings, GIF banner) |

### `.r` Rez files

`AppWarrior/Source/User Interface/MsgBox.r` (`'MSGB'` template for message boxes — icon/picture/sound/text-style/pstring title/message/buttons), `AppWarrior/Source/Libs/QTDataHandler/QTResource.r` (`'thng'`/`'strn'`/`'stri'`/`'dlle'` for the data handler), and `Apps/FileFinder/Resources/{SimpleAlert,AppIcon,Version}.r`. Windows uses `Apps/Client/Resources/Win/hotline.rc` (MENU `File/Edit/GLoarbLine`, `STRINGTABLE "GLoarbLine"`, icon/cursor resources) plus `Icons/*.ico` and `Cursors/*.cur`.

### `Rez2Dat` purpose

`Apps/Rez2Dat/Rez2Dat.cpp` is a **Mac-only build tool** that converts a Mac resource fork (`cicn`/`EMSG`/`GIFf` resources) into the flat **`.dat`** resource images that the cross-platform `URez` resource manager reads. Its transfer table (`Rez2Dat.cpp:16-…`) maps, e.g. `'EMSG'`→`'EMSG'`, `'GIFf' 128`→`'GIFf' 128` ("the banner non movie version"), and **`'cicn'`→`'ICON'`** for icons 128–158. It produced `hlc19.dat`/`hls19.dat`/`hlci19.dat`. It is **build-time tooling, not runtime code** — retain only as reference for re-extracting legacy assets.

### `URez` consumers in UI

`URez` (`URez.h`) is the custom cross-platform resource manager (type+id+handle, `LoadItem/ReleaseItem/GetItemListing`, a **search chain**). In the UI it backs icon resources (`UIcon::Load(Int32 inID)`), the banner (`GetResourceBanner` `URez::SearchChain('GIFf', 128)` — `Hotline.cpp:7205-7212`), and the EMSG string tables. The `'ICON'`/`'cicn'` icon pipeline and the `'AWPX'` pixmap both flow through it.

**Verdict: retain-temporarily** (read-only) to extract legacy icons/banners/strings during migration; the resource manager itself is **replace-with-modern** (plain files / a modern asset bundle) and `Rez2Dat` is **remove-entirely** after asset extraction.

---

## 9. Application shell

- **`UApplication`** (`UApplication.h`; `UApplication(M).cpp` 25.1k LOC, `UApplication(W).cpp` 10.4k LOC) — the native app bootstrap: `Init/Process/Run/Quit/Abort/Error`, the global message system, `SetCanOpenDocuments`/`GetDocumentToOpen` (Apple Events), `GetAppRef`. Platform-specific; the Mac side is much larger (Apple Event/menu handling).
- **`CApplication`** (`CApplication.h`, `CApplication.cpp` 174 LOC) — the cross-platform app base: `Run/UserQuit/Quit`, `KeyCommand`, `WindowHit` (`WindowHitHandler` trampoline), `HandleMessage`, key-command installation via `UWindow::InstallKeyCommands`. `extern CApplication *gApplication` is the app singleton. Every app subclasses it: `CMyApplication` (Client, `Hotline.cpp`), `HotlineServ`, `TrackerServ`, `NewsSynch`, `FileFinder`.
- **`UWindow`** (`UWindow.h`, `UWindow(M).cpp` 4553 LOC, `UWindow(W).cpp` 3430 LOC) — the native window wrapper: `New(… layer/options/style/parent)`, title, visibility, bounds/limits, **layers** (`windowLayer_Popup/Modal/Float/Standard`, `UWindow.h:26-29`), collapse/zoom, back color, `BringToFront/SendToBack`, mouse capture, refresh, and window-level key/mouse/drag/QuickTime dispatch. `CWindow` (`CWindow.h/.cpp`, 1690 LOC) layers the view container, hit queue, modal processing (`ProcessModal`), and window **snapping** (`CSnapWindows gSnapWindows`, `CWindow.h:197-248`) on top. **Menu handling** is platform-native (Mac Apple Menu + Windows `.rc` `MDIMenu`), surfaced to apps through `CApplication::KeyCommand`/`InstallKeyCommands` and `UKeyboard::FindKeyCommand` — there is no cross-platform menu abstraction in AppWarrior.
- **`UProgramCleanup`** (`UProgramCleanup.h`, `.cpp` 2443 LOC) — installs process-termination callbacks (`InstallSystem`/`InstallAppl`) so destructors/cleanup run on quit/crash. App-level safety net.
- **`UUserInterface`** (`UUserInterface.h`, `.cpp` 281 + `(M)` 634 + `(W)` 485 LOC) — UI subsystem `Init` (shared), plus per-platform control factory/theme setup.
- **`Error Msgs/`** — framework string resources (see §8), loaded by `UError`/`Fail` for localized error dialogs.

**Verdict:** the `CApplication`/`CWindow`/`UWindow` *shape* (app base + window list + hit dispatch + key commands) is a **preserve-semantics-rewrite** target for the new shell; the native `UApplication(M/W)` and `UWindow(M/W)` bodies are **replace-with-modern**.

---

## 10. View class inventory

| View class | File (LOC) | Instantiated-by (app) | Purpose | Modern-UI mapping |
|---|---|---|---|---|
| `CView` | `Views/CView.cpp` (752) | base | widget base: bounds, state, draw, mouse/key/drag/timer, hit | abstract widget base |
| `CWindow` | `User Interface/CWindow.cpp` (1690) + `UWindow(M/W)` | all apps | top-level window + view container + hit queue + snapping | window/widget host |
| `CContainerView` | `Views/CContainerView.cpp` (1201) | Client, Server, Tracker, NewsSynch | multi-child container with layout | container/panel |
| `CPaneView` | `Views/CPaneView.cpp` (592) | Client, NewsSynch | titled pane (collapsible group) | group box / panel |
| `CScrollerView` | `Views/CScrollerView.cpp` (1483) | all apps | single-content scroller + scrollbars | scroll area |
| `CBoxView` | `Views/CBoxView.cpp` (127) | Client, Server, Tracker, NewsSynch | plain frame/box | frame/box |
| `CLabelView` | `Views/CLabelView.cpp` (105) | all apps | static text label | label |
| `CLabelUrlView` | `Views/CLabelUrlView.cpp` (253) | (none direct; base semantics) | hyperlink label → LaunchURL | link label |
| `CLaunchUrlView` | `Views/CLaunchUrlView.cpp` (140) | via CImageView/CQuickTimeView | clickable URL view | link/click handler |
| `CButtonView` | `Views/CButtonView.cpp` (489) | Client, Server, Tracker, NewsSynch | push button | button |
| `CCheckBoxView` | `Views/CCheckBoxView.cpp` (309) | Client, Server, Tracker, NewsSynch | checkbox | checkbox |
| `CIconButtonView` | `Views/CIconButtonView.cpp` (314) | Client (many), Server, Tracker, NewsSynch | icon button | icon button |
| `CSimpleIconButtonView` | `Views/CSimpleIconButtonView.cpp` (196) | Client | simple icon button | icon button |
| `CIconView` | `Views/CIconView.cpp` (59) | Client, Server | static icon | icon |
| `CColorView` | `Views/CColorView.cpp` (186) | Client | color swatch | color swatch |
| `CSeparatorView` | `Views/CSeparatorView.cpp` (43) | Client | separator line | separator |
| `CTextView` | `Views/CTextView.cpp` (417) | Client, Server (via app views) | editable/selectable text | text edit |
| `CPasswordTextView` | `Views/CPasswordTextView.cpp` (401) | Client | password field (uses UClipboard) | password field |
| `CProgressView` | `Views/CProgressView.cpp` (70) | Client | progress bar | progress bar |
| `CPictureView` | `Views/CPictureView.cpp` (113) | Client | static picture/pixmap | image view |
| `CImageView` | `Views/CImageView.cpp` (120) | Client | decoded image (clickable URL) | image + link |
| `CAnimatedGifView` | `Views/CAnimatedGifView.cpp` (390) | Client | animated GIF (frame timer) | animated image |
| `CQuickTimeView` | `Views/CQuickTimeView.cpp` (897) | Client (as `CMyQuickTimeView`) | QuickTime movie playback | removed / media player |
| `CItemsView` | `Views/CItemsView.cpp` (903) | base | abstract item model + item draw/mouse | item view base |
| `CSelectableItemsView` | (in CItemsView) | base | selection + drag behaviors | selection model |
| `CListView` | `Views/CListView.cpp` (301) | via `CMy*` in all apps | single-column list | list |
| `CGeneralListView<T>` | `Headers/CGeneralListView.h` (499, header-only) | via `CMy*` | templated list (columns, sort) | table/list |
| `CTreeView<T>` | `Headers/CTreeView.h` (1682, header-only) | via `CMy*` (files, news, users) | templated tree | tree |
| `CTabbedView` | `Views/CTabbedView.cpp` (1367) | Client, Server, Tracker, NewsSynch | tab container | tabs |
| `CTabbedItemsView` | `Views/CTabbedItemsView.cpp` (534) | (mixin) | tab strip over items | tab header |
| `CTabbedListView` | `Views/CTabbedListView.cpp` (166) | (mixin) | tabbed list | tabbed list |
| `CTabbedTreeView<T>` | `Headers/CTabbedTreeView.h` (221) | (mixin) | tabbed tree | tabbed tree |
| `CGeneralTabbedListView<T>` | `Headers/CGeneralTabbedListView.h` (314) | (mixin) | templated tabbed list | tabbed table |
| `CWizard` | `User Interface/CWizard.cpp` (271) | (none in apps) | wizard with Back/Next/Finish | wizard |
| `MsgBox` | `User Interface/MsgBox.cpp` (543) + `.r` | all apps | message box (`MakeMsgBox`, `DisplayMessage`) | message dialog |

---

## 11. Risks and unknowns

1. **`CWizard` has no app consumer** (`grep new CWizard` → none; only the file exists). It is dead framework code — do not build the modern wizard on it.
2. **`CListView`/`CTreeView`/`CGeneralListView` are almost always subclassed** by app `CMy*` classes. The modern mapping must reproduce the *item-provider/selection/drag* contract, not just the base widget, or ~30 app view files must be rewritten.
3. **`CTreeView<T>` and `CGeneralListView<T>` are header-only templates** (1682 and 499 LOC in headers) — any LOC accounting that only counts `.cpp` will understate the UI code size.
4. **QuickTime banner is already dead** (`Hotline.cpp:7180` hard-disables it), but the `'MooV'` assets and the QTDataHandler still ship. The QuickTime surface is smaller than the directory size suggests.
5. **QTDataHandler embeds a second AppWarrior** (`HL_BigRedH`, ~24.5k LOC). It must not be confused with, or merged into, the main `AppWarrior/Source` framework during modernization.
6. **Endianness is a real hazard**: `'AWPX'` pixmaps and the resource `.dat` files are big-endian; `SColor` 32-bit packing flips under `CONVERT_INTS`; 16-bit color is documented as endianness-inconsistent (`UPixmap.cpp:16-22`). Any asset extractor must byte-swap explicitly.
7. **`USound` ADPCM** may be the only codec for legacy in-band sound assets; if old servers still push ADPCM audio, the codec must be re-implemented or the feature dropped.
8. **`UClipboard`/`UOleAutomation`/`UService` are effectively unused by the apps** (UClipboard only via `UEditText`/`CPasswordTextView`; OLE/Service have zero app consumers). They are safe to drop/rewrite without behavioral risk.
9. **Encoding**: several files are MacRoman/ISO-8859-1 (pascal strings `\p…`, `pstring`). Modernization must convert these literal string assets (`\phttp://www.lorbac.net`, error strings) to UTF-8 and strip the dead vendor URLs.
10. **`Apps/ServerOLD/` and `Old Tracker/` are near-duplicates** of `Server`/`New Tracker` (the `new C...View` grep shows identical counts) — audit them once, not twice.
11. **`FileFinder` uses its own `ComboListView`** rather than the framework list, and `NewsSynch` defines extra `CButtonPopupView`/`CMenuListView`/`CPopupView` views — a reminder that not all UI is in `AppWarrior/Views`.
