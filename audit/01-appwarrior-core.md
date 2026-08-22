# AppWarrior Core Runtime Audit

**Tree audited:** `/home/admin/Documents/GitHub/Slopline/legacy/AppWarrior` (read-only). Consumers checked under `legacy/Apps` and `legacy/AppWarrior`.
**Method:** every claim below is from the actual `.h`/`.cpp` contents or a `grep -rn` call-site count. Line numbers are quoted as `file:line`. Usage counts are raw `grep -rn` line matches (they include declarations and repeated mentions, so treat them as *order-of-magnitude* evidence, not exact call counts). Most files are pure ASCII; only 6 files are ISO-8859 (notably `AppWarrior/Source/Crypt/HLSha1.cpp` and `AppWarrior/Headers/CGeneralListView.h`) — read those with `iconv -f MACINTOSH`.

---

## 0. Orientation

AppWarrior ("AW") is a 1996–2003 cross-platform C++ runtime that sat under the Hotline client, server, tracker and the NewsSynch utilities. Its center of gravity is **the Mac**: the API is a thin C++ veneer over the classic Mac Toolbox — Pascal strings (`\p...`, length byte first), the `Handle`/`HLock`/`HPurge` relocatable-memory model, `GetResource('EMsg', type)` string catalogs, `TMTask` timers, "seconds since 1904" epoch, `Str255`, four-char type/creator codes, and a resource-fork-alike file format. Windows support is a parallel `(W).cpp` implementation of the same headers using Win32 (`GlobalAlloc(GMEM_MOVEABLE)`, `SetTimer`, `SYSTEMTIME`/`FILETIME`). The portability boundary is the `#if MACINTOSH` / `#if WIN32` split inside same-named files (`UText(M).cpp` vs `UText(W).cpp`), plus `#if __INTEL__` vs `__POWERPC__` endian handling in `typedefs.h`.

Two critical cross-cutting facts:

1. **Endianness.** Hotline's wire format is **big-endian (network byte order)**. `typedefs.h:153-208` defines `swap_int()` and, on Intel, the `TB()`/`FB()` overloads (`TB` = "to big-endian", `FB` = "from big-endian") become byte-swaps; on PPC/68K they are identity. `CONVERT_INTS` is `1` on Intel, `0` on big-endian. Every protocol writer uses `TB()`, every reader uses `FB()`.
2. **There is no `UString` in this tree.** The string facility in the core runtime is **`UText`** (static functions over raw byte buffers + an encoding parameter). A *different, newer* string class (`CString`, `StString`, `UStringConverter`) exists only inside the vendored `AppWarrior/Source/Libs/QTDataHandler/AppWarrior/Utilities/` sub-library (a "BigRedH" component with its own `AW.h`/`AWTypes.h`). See §4.

---

## 1. Header inventory — `ANSI.h`, `typedefs.h`, `MoreTypes.h`, `GrafTypes.h`, `ImageTypes.h`

### 1.1 `ANSI.h` (135 LOC, ASCII)

A hand-rolled libc shim so AW does not depend on a vendor C runtime's header layout. It declares `malloc/calloc/realloc/free`, `memcpy/memmove/memset/memchr/memrchr/memcmp`, the full `str*` family **plus** non-standard `strrev`, `strtol/strtoul`, `ltostr/ultostr`, `sprintf/vsprintf`, `exit/atexit/abort`, `bsearch/qsort`, `extern int errno`. It also declares a full **Pascal-string** family (`pstrlen/pstrcpy/pstrcmp/...`), and `pstrlen`/`pstrtext` are defined `inline` (`ANSI.h:88-101`). The implementations live in `AppWarrior/Source/Misc/ANSI.cpp` (1855 LOC: `memcpy` at 163, `qsort` at 1678, `vsprintf` at 1516, `strrev` at 1002, `ltostr` at 1433, `pstrcpy` at 1023, etc.). A commented-out `__ctype_map` block (lines 102-136) was superseded by `UText`'s own ctype tables.

### 1.2 `typedefs.h` (286 LOC, ASCII)

- Fixed-width integer typedefs: `Int8/16/32/64`, `Uint8/16/32/64`, `Char8/Char16`, `Float32`, `fast_float`. **Usage in Apps is essentially total** (`Uint32` ~6,272 matches, `Uint8` ~3,677, `Uint16` ~1,244, `Int16` ~164, `Int32` ~104). These are the base currency of every struct on the wire.
- `nil` = `0`, `true`/`false` = `!0`/`0` (pre-`<cstdbool>`).
- `RANGE`, `HiWord`/`LoWord`, `FORCE_CAST`, `BPTR/CPTR/WPTR` pointer-cast macros.
- Custom global `operator new/delete` declarations (defined in `UMemory.cpp:1767-1780` to route through `UMemory`).
- `min`/`max`/`swap`/`abs`/`clamp`/`diff` templates; `RoundUp/Down2..64` macros.
- `swap_int()` + `TB()`/`FB()` endian helpers (§0.1).
- `scopekiller<T>`/`scopekill` (RAII `delete`) and `StValueChanger<T>` (RAII restore).
- `USES_FILE_EXTENSIONS` (1 on Win, 0 on Mac — encodes the Mac "no extensions, use type/creator codes" worldview).

### 1.3 `MoreTypes.h` (164 LOC, ASCII)

Message/event vocabulary shared by the app framework: `SKeyMsgData`, `SMouseMsgData`, `SKeyCmd` (all `Uint16/Uint32`); `TMessageProc` and `TComparePtrsProc` function-pointer types (used by `UTimer`, `CPtrList::Sort`, `CPtrTree::Sort`, `UApplication`). Enums: modifier-key bits (`modKey_Control=0x01 … modKey_Command=0x10`), priority levels (`priority_Higher=20 … priority_Lower=-20`), the message IDs (`msg_Error=1 … msg_Special=1000`), message priorities, and `perm_*` open/sharing permission bits. Also the `TIOReadProc/TIOWriteProc/TIOGetSizeProc/TIOSetSizeProc/TIOControlProc` callback typedefs that `URez` uses to abstract storage (§10). `TComparePtrsProc` is declared but **never called in Apps** (0 matches) — the apps sort with `CPtrTree::Sort`/`CPtrList::Sort` only inside AW itself.

### 1.4 `GrafTypes.h` (463 LOC, ASCII) + `Source/Graphics/GrafTypes.cpp`

Geometry/color value types: `SPoint` (Int32 x/y), `SShortPoint` (Int16), `SRect` (left/top/right/bottom), `SShortRect`, `SColor` (**48-bit** RGB, three `Uint16`s), `SPackedColor` (RGBA `Uint8`s), `SRoundRect`, `SLine`, `SFloatPoint`, `SMatrix3` (3×3 `fast_float`). `SColor::SetPacked/GetPacked` is deliberately endian-agnostic (branch on `CONVERT_INTS`, `GrafTypes.h:232-238`). A huge list of `color_*` constants and `gm_*` math constants (`gm_Pi`, `gm_DegToRad`, …). **Usage:** `SRect` ~2,049 and `SPoint` ~16 and `SColor` ~47 in Apps (client UI is rect-driven); `SShortRect`/`SShortPoint`/`SMatrix3`/`SFloatPoint` are essentially **unused** by apps (0 matches) — they exist for completeness.

### 1.5 `ImageTypes.h` (139 LOC, ASCII)

`#pragma options align=packed` on-disk image headers: GIF (`GIFHEADER`, `IMAGEBLOCK`, `CONTROLBLOCK`, `PLAINTEXT`, `APPLICATION`, `SImageInfo`) and BMP (`BITMAPFILEHEAD`, `BITMAPINFOHEAD`, `BITMAPRGB`). Also `errorType_Image=10` and `imgError_*` codes, `END_OF_FB=0xFFFFFFFF` / `END_OF_BYTES=0xFFFFFFFE` sentinels. Consumers are the `Source/Images/*` decompressors (`CDecompressGif/Bitmap/Pict` call `swap_int` on these fields, e.g. `CDecompressGif.cpp:160-161`). This header is the raw format knowledge for the image stack, not for the network protocol.

### Classification (headers)

| File | Verdict |
|---|---|
| `ANSI.h`/`ANSI.cpp` | **replace-with-stdlib** (keep the Pascal-string and `ltostr`/`strrev` helpers as thin shims during migration — see §S1) |
| `typedefs.h` | **replace-with-stdlib** (`<cstdint>`, `<algorithm>`, `<bit>` for byte-swap; keep `TB/FB` as explicit `htobe*` wrappers) |
| `MoreTypes.h` | **replace-with-modern-platform** (event/message vocab collapses into the UI/event framework; keep the `priority_*` + `msg_*` constants where the app loop still needs them) |
| `GrafTypes.h` | **replace-with-modern-platform** (`SPoint/SRect/SColor` → UI-framework geometry; only if no UI framework, retain a shim) |
| `ImageTypes.h` | **retain-temporarily** (GIF/BMP struct knowledge lives in the image decoders) |

---

## 2. Containers

All six are intrusive, manual-memory containers with **1-based** indexes in the public API, storing raw pointers. They are hand-rolled replacements for STL (which was not uniformly available on mid-90s CodeWarrior). None of them owns the pointed-to objects; ownership is always the caller's.

### 2.1 `CBoolArray` (`CBoolArray.h` 49, `CBoolArray.cpp` 294)

**What it is:** a bit-packed dynamic array of `bool` (one bit per item, "1024 items = 128 bytes" per the file header). Backed by a `THdl` (`mData`) so it can grow. API: `InsertItems/InsertItem/AddItem`, `RemoveItems/RemoveItem/Clear`, `MoveItems/MoveItem/SwapItems`, `SetItems/SetItem/GetItem`, `GetFirstSet/GetLastSet`, `SetDataHandle`. Internals: `ExpandData` allocates `(count/8)+1` bytes (`CBoolArray.cpp:30`), `ShiftItems` uses `UBitString::Copy` to bit-shift, every access locks the handle with `StHandleLocker`. **Unusual:** `MoveItems` and `SwapItems` only support a single item — multi-item raises `error_Unimplemented` / `DebugBreak` (`CBoolArray.cpp:249-251`, `278-282`).

**Consumers:** only `Apps/Client/Source/Hotline.h` (1 match; a member/typedef, i.e. effectively unused by app logic). `CBoolArray` is itself a consumer of `UBitString`.

**Still relevant?** No live app use found. **Replacement:** `std::vector<bool>` or (better) `std::vector<char>`/`std::bitset` with an index shim. **Classification:** **remove-entirely** (or preserve as a tiny `std::vector<bool>` adapter if the client header is ever revived).

### 2.2 `CLinkedList` / `CLink` (`CLinkedList.h` 49, `.cpp` 167)

**What it is:** intrusive singly-linked list of `CLink` nodes (each node embeds `mNext`). `AddFirst/AddLast`, `RemoveFirst/RemoveLast`, `GetFirst/GetLast/GetIndexedLink/GetCount`, `IsInList/RemoveLink`, `StealList/SetHead`. `AddLast`/`RemoveLast`/`GetLast` are O(n) (no tail pointer). Not owning; caller-managed nodes.

**Consumers:** server `HotlineServ.h` (3 matches in each of `Server` and `ServerOLD`), client `Hotline.h` (1), `HotlineWindows.cpp` (1). ~49 `CLink` matches in Apps — used for small intrusive queues (e.g. pending transfer/connection lists).

**Still relevant?** Marginal. **Replacement:** `std::list`/`std::deque`, or `std::forward_list` where intrusive linking is genuinely wanted. **Classification:** **replace-with-stdlib**.

### 2.3 `CPtrList` / `CVoidPtrList` (`CPtrList.h` 85, `.cpp` 451)

**What it is:** the workhorse growable pointer array. `CVoidPtrList` stores `void** mData` with `mCount/mMaxCount`, grows by `RoundUp8` and shrinks when `>32` slots free. **1-based** `GetItem/SetItem/InsertItem` (note `SetItem`/`GetItem` are 1-based, `CPtrList.h:32-33`). API: `AddItem/InsertItem/RemoveItem(ptr|index)/Preallocate/Truncate/Clear`, `MoveForward/MoveBackward/MoveToFront/MoveToBack` (the latter two are mis-commented — `MoveToFront` actually *appends* and `MoveToBack` actually *prepends*, `CPtrList.cpp:248-266`), `GetNext/GetPrev` iteration with a cursor index, `Sort` (heapsort, `CPtrList.cpp:297-367`), `SortedSearch` (binary search returning 1-based insertion index), `IsInList` (linear `SearchLongArray`). `CPtrList<T>` is the strongly-typed template wrapper. **Pointer ownership:** none; `Clear`/`~` only free the array, never the elements.

**Consumers:** by far the most-used container — **~307 matches in Apps** across the client (`Hotline.h` 35, `HotlineNews.cpp` 6, `HotlineViews.cpp` 6, `HotlineMisc.cpp` 6), the server (`HotlineServ.h` 21, `HotlineServWindows.cpp` 8), the tracker, and every NewsSynch file (`NewsSynch.h` 34, `SynchViews.cpp` 12). Iteration pattern is almost always `GetItemCount()` + `GetItem(i)` or the `GetNext(ptr, idx)` cursor; insertion is `AddItem`/`InsertItem`; removal `RemoveItem(ptr)`.

**Still relevant?** Core. **Replacement:** `std::vector<T*>` (drop the 1-based convention and re-map call sites); keep `Sort`/`SortedSearch` via `std::sort` + `std::lower_bound` with the same `TComparePtrsProc` signature, or migrate to `std::unique_ptr<T>` where ownership is known. **Classification:** **replace-with-stdlib**.

### 2.4 `CPtrTree<T>` (`CPtrTree.h` 638, header-only template)

**What it is:** a flat, level-encoded tree (not a linked tree). Items are a contiguous `STreeItem[]` where each `STreeItem` = `{Uint16 nItemLevel /*1-based depth*/, Uint32 nChildCount, void* pItem}` (`CPtrTree.h:8-12`). Children immediately follow their parent in the array; hierarchy is reconstructed from `nItemLevel` deltas. **1-based** indexes everywhere (`AddItem` returns `nIndex+1`, `GetItem` rejects `!inIndex`). API: `AddItem(parentIdx, ptr)`, `InsertItem(parentIdx, siblingPos, ptr)`, `RemoveItem(idx|ptr, removeChildTree)` (with `removeChildTree=false` meaning "promote first child"), `RemoveChildTree`, `SetItem/GetItem/GetItemIndex`, `SetParentItem/GetParentItem/GetParentIndex`, `GetItemLevel/GetTreeCount/GetRootCount/GetChildTreeCount`, `GetNext/GetPrev` (optional `inSameParent` sibling iteration), `Sort(parentIdx, compareProc)` (a **bubble-sort over same-level siblings** — `CPtrTree.h:548-616` — swapping whole subtrees as contiguous runs), `IsInList` (linear scan). `GetTreeItem` is a full linear scan (`CPtrTree.h:618-637`). **Unusual:** sorting is per-parent only and O(n²); "insert at sibling position" is relative to the parent's *direct* children; level is a `Uint16` so depth is capped at 65535.

**Consumers:** NewsSynch (`NewsSynch.h` 2, in both Server and ServerOLD), client `Hotline.h` 1. ~5 matches total — used for hierarchical news/tracker listings.

**Still relevant?** Yes, for the tree view models. **Replacement:** a `std::vector<T*>` + parallel `depth`/`childCount` arrays, or a node-based tree (`std::map`/custom `Node{vector<unique_ptr<Node>>}`). If the level-encoded flat layout is itself persisted or sent on the wire, preserve the layout semantics. **Classification:** **replace-with-stdlib** (verify whether the flat encoding is serialized before discarding).

### 2.5 `UIDVarArray` (`UIDVarArray.h` 85, `.cpp` 623)

**What it is:** an associative array of `{Uint32 ID → variable-size byte blob}`. Keys are **unique, non-zero, kept sorted ascending** (binary search + a `lastSearchID/lastSearchIndex` cache, `UIDVarArray.cpp:555-619`). Storage: an offset table (`SIVAOffsetTabEntry {id, offset}` with one sentinel entry) plus a `THdl` data blob; data is stored in ID order and insert/remove does `UMemory::Insert/Remove` byte shuffles. API: `New/Dispose`, `AddItem(id, data, size, options /*kReplaceIfExists*/)`, `RemoveItem`, `SetItem/GetItem/GetItemSize`, `ReadItem/WriteItem` (offset within a blob), `GetItemPtr/ReleaseItemPtr` (locked pointer pair; `StIDVarArrayPtr` RAII), and `FlattenToHandle/Unflatten/GetItem` (static, direct-from-buffer lookup). **Duplicate handling:** `AddItem` fails with `error_ItemAlreadyExists` unless `kReplaceIfExists`. **Ownership:** owns the byte blobs (via `dataHdl`) and offset table; not the caller.

**Flattened format (protocol/storage-relevant, quoted from `UIDVarArray.cpp:12-26`):**
```
Uint32 format;      // always 'IVA1'
Uint32 rsvd;        // 0
Uint32 textEncoding;// 0
Uint32 itemCount;
struct { Uint32 id; Uint32 offset; } offsetTab[itemCount+1]; // sorted by id; sentinel gives last size
Uint8  data[];
```
All multi-byte values are `TB`/`FB` (big-endian) on write/read (`UIDVarArray.cpp:338-341`, `441-457`). `Unflatten` validates: format tag, `itemCount & 0xFF000000` overflow guard, strictly increasing IDs, in-range offsets (`UIDVarArray.cpp:398-457`).

**Consumers:** **zero direct Apps call sites.** Its real consumers are inside AW: `UError(M).cpp`/`UError(W).cpp` use `UIDVarArray::GetItem` to look up error-message text from the `'EMSG'` resources / `.dat` catalogs (§9). It is therefore a *loading format* for the error catalogs, not a protocol field container.

**Still relevant?** Only as the `.dat` error-catalog reader. **Replacement:** read the catalogs with a simple hand-written `IVA1` parser (or keep this class verbatim as a frozen reader) and, for any new ID→blob needs, `std::map<uint32_t,std::vector<uint8_t>>`. **Classification:** **retain-temporarily** (frozen IVA1 reader only).

### 2.6 `UBitString` (`UBitString.h` 24, `.cpp` 141; alias `UBits`)

**What it is:** pure static bit-twiddling over caller-supplied buffers. Bit **0 is the LSB of byte 0** (`#define GET_BIT(p,i) … >>(i&7)`, `UBitString.cpp:22`), i.e. little-bit order — the opposite convention from `UMemory::GetBit/SetBit` which use MSB-first (`UMemory.h:135-147`). API: `Get/Set/Clear/Copy` (overlap-safe), `GetFirstSet/GetLastSet/GetNextSet`. **Note the two coexisting, opposite bit orders** (`UBitString` = LSB-first, `UMemory::GetBit` = MSB-first) — a latent trap.

**Consumers:** zero in Apps; `CBoolArray` is the only consumer (`CBoolArray.cpp:12`). **Replacement:** `std::bitset`/`std::vector<bool>`. **Classification:** **remove-entirely** (fold into `CBoolArray`'s removal).

---

## 3. Memory — `UMemory`, handles, `StHandleLocker`

Files: `UMemory.h` (274), `UMemory.cpp` (1784), `UMemory(alloc).cpp` (1113), `UMemory(M).cpp` (1033), `UMemory(W).cpp` (1052), `UMemory(priv).h` (52). Alias `UMem`.

### 3.1 Three pointer kinds (documented in `UMemory.cpp:2-46`)

- `void*` — arbitrary.
- **`TPtr`** (`TPtrObj*`) — a fixed, **non-resizable** allocation from `UMemory::New/NewClear/New(data,size)`. You may not even read its size (no size query). `Reallocate(TPtr)` may move it.
- **`THdl`** (`THdlObj*`) — a **relocatable handle** (`UMemory::NewHandle/NewHandleClear/Clone`). Data is only reachable via `Lock()`→`Unlock()`; must never be cast directly.

### 3.2 The allocator split

- **`UMemory(alloc).cpp`** — a Knuth *TAoCP Vol.1 §2.5* boundary-tag pool allocator (`_MEPool*`). Free blocks in a circular doubly-linked "rover" list; each block has a header/trailer tag where the 2 low bits carry `this_in_use`/`prev_in_use` and size is a multiple of 4/8 (`alignment` 8 on PPC/Intel, `UMemory(alloc).cpp:184-192`). Heaps are `min_heap_size=32768` grown from `_MESysAlloc`. **This is the real allocator in release builds** (`USE_POOL_ALLOC=1` when not `DEBUG`).
- **`UMemory(M).cpp`** — `TPtr` = pool alloc or `NewPtr`; `THdl` = **real Mac `Handle`** via `NewHandle/TempNewHandle/NewHandleClear`, plus `HLock/HUnlock/HPurge/HNoPurge/SetHandleSize/ReallocateHandle/DisposeHandle/HandToHand` and `Munger` for insert/remove (`UMemory(M).cpp:756`). Because the Mac `Handle` has no lock counter, AW reserves a **4-byte lock-count prefix** in every handle (`NewHandle` does `inSize += 4`, writes `*(Uint32*)*h = 0`, and `Lock()` returns `*h + 4`, `UMemory(M).cpp:224-241, 556-562`). `GetSize()` returns `GetHandleSize()-4`.
- **`UMemory(W).cpp`** — `TPtr` = pool or `GlobalAlloc(GMEM_FIXED)`; `THdl` = `GlobalAlloc(GMEM_MOVEABLE)` with a **4-byte size prefix** stored at the front (because Win95's `GlobalSize` rounds up to 8; `UMemory(W).cpp:197`). `Lock()` returns `GlobalLock()+4`.

`UMemory.cpp` (the shared part) is **not** the allocator: it is the data-ops layer — `Copy/Fill/Clear/Equal/Compare/Search/SearchByte/SearchLongArray`, `Checksum` (simple byte sum), **`CRC` = CCITT-32** (table at `UMemory.cpp:474-539`, `crc = (crc<<8) ^ table[((crc>>24)^byte)]`), **`AdlerSum`** (Adler-32, `UMemory.cpp:563-609`), `Match` (glob `* ? [] \`), `Token`, `HexDump`, `PackIntegers/UnpackIntegers` (a 2-bit-per-integer size-code varint), and `Flatten/FlattenToHandle/GetFlattenSize` (concatenate `SDataPtr[]` with optional 4-byte alignment). It also defines global `operator new/delete` (`UMemory.cpp:1767-1780`).

### 3.3 The classic-Handle abstraction inventory

| API | Semantics | Modern replacement |
|---|---|---|
| `UMemory::NewHandle/NewHandleClear/Clone` | alloc relocatable block (+4-byte lock/size prefix) | `std::unique_ptr<uint8_t[]>` / `std::vector<uint8_t>` |
| `UMemory::Dispose(THdl)` | free (asserts not locked, not a Mac resource handle) | `delete` / scope exit |
| `UMemory::Lock` / `Unlock` | pin + return `data+4`; nested refcount on Mac (`UMemory(M).cpp:556-604`) | no-op on `std::vector` (already stable) |
| `UMemory::SetSize/GetSize/Grow` | resize handle (contents preserved on Mac; prefix updated on Win) | `vector::resize` |
| `UMemory::Reallocate(THdl)` | resize **discarding** contents; re-materialize discarded handle | `vector::resize` |
| `UMemory::SetDiscardable/ClearDiscardable` | `HPurge`/`HNoPurge` or `GMEM_DISCARDABLE`; emulate Mac "purgeable" memory | **delete entirely** |
| `UMemory::Set/Clear/Append/Insert/Remove/Replace/Read/Write/ReadLong/WriteLong/ReadWord/WriteWord` | byte-buffer ops on a handle | `std::vector<uint8_t>` + `<algorithm>` |
| `StHandleLocker` (`UMemory.h:177-185`) | RAII Lock/Unlock | **remove** (unneeded) |
| `StPtr`/`StHdl` (`UMemory.h:187-223`) | RAII TPtr/THdl | `std::unique_ptr` / `std::vector` |
| `TPtrObj::operator delete` / `THdlObj::operator delete` | route `delete` to `UMemory::Dispose` | normal `delete` |
| `_HdlToWinHdl` (`UMemory(W).cpp:1023`) | strip the 4-byte prefix to hand a THdl to Win32 | **remove** |

**Why relocatable handles must NOT be recreated.** The `THdl` model exists solely to paper over the classic Mac Memory Manager (compactible heap, `HLock` pinning, purgeable blocks). On any modern platform memory does not move under you, so the entire Lock/Unlock/prefix dance is pure overhead and a source of bugs (there are explicit `DebugBreak` guards for "dispose locked handle", "unlock too many times", "cannot resize locked handle", and a 4-byte overhead on *every* allocation). Recreating it would reproduce the one thing a modern runtime should delete. The correct translation is: **THdl → owning byte buffer (`std::vector<uint8_t>` or `std::unique_ptr<uint8_t[]>`), with a mechanical rewrite of `Lock()`+offset arithmetic into direct indexing.** The only subtlety to preserve is the **on-wire byte layout** of whatever those buffers hold (handled by `TB/FB`, not by the handle itself).

### 3.4 Classification
- `UMemory.cpp` data-ops: **replace-with-stdlib** (keep `CRC`/`AdlerSum`/`Checksum`/`PackIntegers` as small utilities — `Checksum` and `CRC` are protocol/checksum-relevant, see §S2).
- `UMemory(alloc).cpp` pool: **remove-entirely** (the OS malloc is now faster and safer).
- `UMemory(M/W).cpp` handle layer: **remove-entirely** (replace THdl/TPtr with std containers; do not reimplement relocatable handles).
- `StHandleLocker`/`StPtr`/`StHdl`: **remove-entirely**.

---

## 4. Text / strings — `UText`, `CFlatten`, `UMime`

### 4.1 `UText` (`UText.h` 97, `UText.cpp` 1289, `UText(M).cpp` 91, `UText(W).cpp` 53)

**What it is:** a static string utility class over **byte buffers + an encoding tag** (`inEncoding` param, `0` = default). Comparison: `Equal`, `Compare`, `CompareInsensitive`, `SearchInsensitive`, `ReplaceTextLoginMessage` (a Hotline-specific login-message templating helper). Conversions: `IntegerToText/SizeToText/SecondsToText/TextToInteger/TextToUInteger`, `Format/FormatArg` (printf-style). Case/mapping: `MakeLowercase/MakeUppercase/ReplaceNonPrinting`, `GetVisibleLength/GetCaretTime/FindWord`, and a full ctype set (`IsAlphaNumeric…IsHex`, `ToUpper/ToLower`) backed by three static tables `__ctype_map/__lower_map/__upper_map` (declared `UText.h:89-91`). The ctype bit flags (`__control_char=0x01 … __upper_case=0x80`) are the same layout as the commented-out block in `ANSI.h`. `UText(M).cpp` maps Mac-specific bits (`VisibleLength` via `TextUtils`, caret ticks→ms, `FindWordBreaks`); `UText(W).cpp` carries a MacRoman↔Windows-1252 translation table `_UTCharMap_AWToPC` (256 bytes, currently identity for the high half, `UText(W).cpp:8-43`) and `GetCaretBlinkTime`.

**Consumers:** the most-used utility — **~861 matches in Apps** (client/server/tracker/NewsSynch all format and compare text through it).

**Still relevant?** Very. **Replacement:** most of the ctype/case/compare surface maps to `<cctype>`/`std::tolower`/`<algorithm>`/`<string_view>`; `Format` maps to `std::format`/`snprintf`; the encoding-aware case-insensitive compare and the MacRoman handling map to an explicit UTF-8 + iconv layer (the tree is pre-Unicode: 8-bit MacRoman is the *wire* text encoding — see §S2 risk). **Classification:** **replace-with-stdlib** (with a small encoding shim retained during migration).

### 4.2 "UString" — does not exist in the core

`grep -rn UString` finds it only in `AppWarrior/Source/Libs/QTDataHandler/...` (`CString_M/W.cpp`, `StString.h`, `UStringConverter.h/.cpp`, `CException.cpp`, `CDataHandlerConnection.cpp`, `CDataProvider.cpp`). This is the **vendored "QTDataHandler"/BigRedH component**, a second-generation string/persistence library with its own headers (`AW.h`, `AWTypes.h`, `CEndianOrder.h`, `CRefCountBase.h`, `UAWError.h`). It is a **separate codebase** that happens to be checked into the same tree and is out of scope for the core runtime; note it as a boundary.

### 4.3 `CFlatten` / `CUnflatten` (`CFlatten.h` 270, header-only)

**What it is:** a cursor-based serializer/deserializer over a caller buffer (used for **structured binary**, not just strings). `CFlatten` writes: `WriteByte/Word/Long` (Word/Long via `TB` = big-endian, `CFlatten.h:18-19`), `WritePString` (1-byte length prefix), `WriteWString` (2-byte BE length), `WriteLString` (4-byte BE length), `WriteShortColor/ShortPoint/ShortRect/DateTimeStamp` (fixed BE layouts), and `Align2/4/8`. `CUnflatten` mirrors with `FB` and bounds-checked `ReadPString/ReadWString` (`EnoughData`/`EnufData`). Streaming `operator<<`/`>>` for integer types. `WriteDateTimeStamp` writes `{Uint16 year, Uint16 msecs, Uint32 seconds}` all BE (`CFlatten.h:167-173`) — identical to `SDateTimeStamp::Flatten` on Win.

**Consumers:** `CFlatten` ~11 / `CUnflatten` ~21 in Apps (NewsSynch, `HotlineServ.cpp`, `TrackerServ.cpp`, `HotlineTasks.cpp`, `HotlineTracker.cpp`); heavily used inside the QTDataHandler persistence layer (its `CFlattenable`/`CGenericObject`/`CMemoryObjectCache`).

**Still relevant?** Yes, as a *stream* primitive. **Replacement:** a modern big-endian `BinaryWriter/BinaryReader` over `std::span<uint8_t>` (this is essentially what it is). The `PString/WString/LString` length-prefix semantics are protocol-relevant and must be preserved bit-for-bit. **Classification:** **preserve-semantics-rewrite**.

### 4.4 `UMime` (`UMime.h` 19, `.cpp` 329)

**What it is:** static extension↔MIME and type-code↔MIME maps. `ConvertExtension_Mime`, `ConvertMime_Extension`, `ConvertNameExtension_Mime`, `ConvertTypeCode_Mime(Uint32 typeCode)` (four-char code → MIME), `ConvertMime_TypeCode`. Hardcoded tables (e.g. `.txt→text/plain`, `.gif→image/gif`, `.jpg→image/jpeg`, `.pct→image/pict`; and the Mac `TEXT/ttxt`, `GIFf/…` type-code mappings).

**Consumers:** ~12 in Apps (client `Hotline.cpp`/`HotlineTasks.cpp`/`HotlineWindows.cpp`, NewsSynch) — used when the server filesystem maps Mac type/creator codes to MIME for HTTP.

**Still relevant?** Marginal (modern stacks use real MIME sniffing). **Replacement:** a `std::map<string,string>` or a dedicated mime library. **Classification:** **replace-with-modern-platform** (or retain the type-code↔MIME table if the legacy server filesystem must keep serving old clients).

---

## 5. Time / math / error / GUID

### 5.1 `UTimer` (`UTimer.h` 39, `UTimer(M).cpp` 245, `UTimer(W).cpp` 158)

**What it is:** one-shot/repeating timers that fire a `TMessageProc` message (`msg_Timer`) through `UApplication`. `Init/New/StartNew/Dispose/Start/WasStarted/Stop/Simulate`. **Units are milliseconds on both platforms.** Mac: wraps the Time Manager `TMTask`/`InsXTime`/`PrimeTime` (and patches `ExitToShell` on non-Carbon to clean up timers, `UTimer(M).cpp:234-243`). Windows: wraps `SetTimer(NULL, 1, ms, proc)` with a global singly-linked timer list.

**Consumers:** ~60 in Apps (client/server/tracker window and task code). **Still relevant?** Yes, as an event-loop abstraction. **Replacement:** the host run-loop timer / `std::chrono` + a message queue. **Classification:** **replace-with-modern-platform**.

### 5.2 `UDateTime` / `SDateTimeStamp` / `SCalendarDate` (`UDateTime.h` 121, `UDateTime.cpp` 725, `UDateTime(M).cpp` 226, `UDateTime(W).cpp` 397)

**What it is:** calendar + timestamp math. `SCalendarDate` is a packed `{Int16 year,month,day,hour,minute,second,weekDay,val}`. `SDateTimeStamp` is the **wire timestamp**:
```c
struct SDateTimeStamp { Uint16 year; Uint16 msecs; Uint32 seconds; };  // UDateTime.h:80-84
```
Semantics: **"midnight Jan 1 of `year` + `seconds` seconds + `msecs` milliseconds"**. The portable `UDateTime.cpp` conversion code (`SCalendarDate::operator SDateTimeStamp`, `UDateTime.cpp:358-375`, and the reverse at `510-570`) treats `seconds` as *seconds since the start of `year`*. `Flatten` writes `year`/`msecs`/`seconds` big-endian (`UDateTime(W).cpp:136-150`).

**Magic numbers/units (documented in `UDateTime.cpp:3-16`):** `1 second = 60 ticks = 1000 ms = 1e6 µs`; `milliseconds = (ticks * 50) / 3`. The Mac epoch **"seconds since 1904"** is explicit at `UDateTime(M).cpp:198` ("mac seconds start at midnight 1/1/1904") and `GetSeconds()` returns `GetDateTime()` (Mac seconds since 1904). `UText(M).cpp:45` does `(GetCaretTime()*50)/3` for ticks→ms.

**⚠ Cross-platform inconsistency (protocol risk).** On **Mac**, `UDateTime::GetDateTimeStamp` sets `year=1904, msecs=0, seconds=mac-seconds-since-1904` (`UDateTime(M).cpp:100-108`). On **Windows**, `GetDateTimeStamp` sets `year=currentYear, msecs=milliseconds, seconds=seconds-since-Jan-1-of-year` (`UDateTime(W).cpp:223-231, 381-394`). The flattened layout is the same 8 bytes, but the **`seconds` field means two different things per platform** (absolute-since-1904 vs. within-year). The apps overwhelmingly use the Windows/portable convention (`seconds` within `year`) — the Mac `GetDateTimeStamp` 1904 behavior looks like a latent bug that only shows on Mac builds. **Flag and decide which convention is canonical before any port.**

**Consumers:** ~213 `UDateTime` + ~79 `SDateTimeStamp` + ~59 `SCalendarDate` matches in Apps. **Replacement:** `<chrono>` + a 8-byte BE (year,ms,sec) codec with the chosen convention pinned. **Classification:** **replace-with-stdlib** (with the wire codec preserved as `preserve-semantics-rewrite`).

### 5.3 `UMath` (`UMath.h` 86, `UMath.cpp` 149, `(M)/(W)` 66/72)

**What it is:** (a) a **deterministic** PRNG — `SetRandomSeed/CalcRandomSeed/GetRandom` uses an LCG `seed = seed*1103515245 + 12345` (`UMath.cpp:26`) and `FlipCoin` uses a different bit-twiddling LCG (`UMath.cpp:34-56`); (b) 64-bit helpers `Add64/Sub64/Mul64U/Div64U` over `SHuge {hi,lo}` (endian-dependent field order, `UMath.h:6-14`) — these exist because the compiler lacked 64-bit ints; (c) trig wrappers declared but only `(M)/(W)` implement a couple (`UMath(M).cpp`/`UMath(W).cpp`). `RoundToLong/RoundToULong`, `AddWillOverflow/SubWillUnderflow/MulWillOverflow` inline helpers.

**Consumers:** ~13 in Apps. **Still relevant?** The 64-bit helpers and overflow checks are now redundant; the LCG is used for *non-crypto* randomness only. **Replacement:** `<cstdint>`/`uint64_t`, `<random>` (do **not** use `UMath::GetRandom` for anything security-sensitive). **Classification:** **replace-with-stdlib**.

### 5.4 `UError` / `SError` (`UError.h` 108, `UError.cpp` 196, `(M)/(W)` 370/190)

**What it is:** the exception/error-reporting system. `SError {Int16 type, id; const Int8* file; Uint32 line; Int32 special; Uint16 fatal:1, silent:1}` is **thrown** (not returned) by `__Fail` (`UError.cpp:113-126`); the `Fail/FatalFail/SilentFail/Require/CheckError/ASSERT` macros (`UError.h:87-101`) call `__Fail` with `__FILE__/__LINE__` in debug. Error domains: `errorType_Misc=1`, `errorType_Requirement=2`, `errorType_Program=100`, plus per-module `errorType_Memory=3`, `errorType_FileSys=4`, `errorType_Rez=6`, `errorType_Transport=8`, `errorType_Image=10`. `GetMessage` loads human text from the `'EMSG'` catalogs (§9); `GetDetailMessage` formats `ID type/id [special] @ file #line`. `UError(M/W).cpp` each carry a large, sorted table mapping native OS errors (Mac `-33 … -5039`; Win32 `2 … 11002`) to `(type,id)` via binary search — this is the OS-error→AW-error boundary.

**Consumers:** ~38 `UError`/`SError` matches in Apps; `Require`/`Fail` are used everywhere in AW itself.

**Still relevant?** The *error taxonomy and the OS-error mapping tables* are useful; the exception style is fine. **Replacement:** `std::system_error`/`std::error_code` with a `HotlineErrorCategory`, or a plain `enum class` + `std::runtime_error`. Keep the OS-error mapping tables. **Classification:** **replace-with-modern-platform**.

### 5.5 `UGUID` / `SGUID` + `Source/Libs/UUID/UUIDLib.h` (`UGUID.h` 47, `UGUID.cpp` 82, `UGUID(M).cpp` 2451, `UGUID(W).cpp` 90; `UUIDLib.h` 82)

**What it is:** RFC-4122/DCE UUID. `SGUID {Uint32 time_low; Uint16 time_mid; Uint16 time_hi_and_version; Uint8 clock_seq_hi_and_reserved; Uint8 clock_seq_low; Uint8 node[6]}` (MSB-first field order). `UGUID::Generate/IsEqual/ToText/FromText/Flatten/Unflatten`. Text format is the standard `xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx` (`UGUID.cpp:2-56`). The **portable** `UGUID.cpp` has only To/From text (16-byte `Flatten` is field-order native). The heavy lifting is `UGUID(M).cpp` (2451 LOC): it vendors the classic **PandaWave/DCE `uuid` implementation** (`UUIDLib.h`) with a Mac-only random-node generator that MD5s `GetDateTime`+`Microseconds`+OS values to synthesize a node ID and persists it in prefs (`UGUID(M).cpp:292-373`). `UGUID(W).cpp` (90 LOC) presumably delegates to COM `CoCreateGuid`.

**Consumers:** ~34 in Apps — all server/NewsSynch (`CNewsDatabase*.cpp`, `HotlineServNewsDatabase.cpp`, `HotlineServ.cpp`) for persistent unique IDs.

**Still relevant?** Yes (IDs must stay 16-byte, text format stable). **Replacement:** platform UUID API (`uuid_generate`, `UuidCreate`, `std::uuid`); keep `SGUID` as a `std::array<uint8_t,16>` + the text codec. **Classification:** **replace-with-modern-platform**.

---

## 6. Crypto — `Source/Crypt/*` (PROTOCOL-RELEVANT)

Files: `HLCrypt.h/.cpp` (60/76), `HLBlowfish.h/.cpp` (43/236) + `HLBlowfishData.h` (314), `HLMD5.h/.cpp` (23/44), `HLSha1.h/.cpp` (46/484), `HLRand.h/.cpp` (41/137).

`HLCrypt.h` defines the base vocabulary: `DataBuffer {Uint8* data; Uint32 len}`, abstract `HLCipher` (`Init/SetEncodeKey/SetDecodeKey/Encode/Decode/SelfTest`) and `HLHash` (`GetMacLen`/`HMAC_XXX(output, key, text)`).

### 6.1 `HLCrypt` — session-key derivation

`HLCrypt::Init(cipher, hash, sessionKey, password, isClient)` (`HLCrypt.cpp:17-54`) takes ownership of `cipher` and `hash` and derives **two** keys via HMAC:
```
temp1 = HMAC(password, sessionKey);   temp1 = HMAC(password, temp1);
temp2 = HMAC(password, temp1);
client: encode=temp2, decode=temp1 ;  server: encode=temp1, decode=temp2
```
`PermEncodeKey(n)`/`PermDecodeKey(n)` re-HMAC the key with the session key `n` times (`HLCrypt.cpp:57-74`) — this is the Hotline **key-permutation / re-keying** mechanism. `mMacLen` = hash output length (16 for MD5, 20 for SHA-1), so the cipher keys are hash-length bytes.

### 6.2 `HLBlowfish` — the transport cipher

Standard Blowfish (16-round Feistel; P-array and four S-boxes seeded from the hex digits of π in `HLBlowfishData.h`; key schedule XORs the key bytes cyclically, `HLBlowfish.cpp:137-167`). **Mode = OFB-64** with an **all-zero 8-byte IV** reset in `Init()` (`HLBlowfish.cpp:4-16`, `OFB64` at `170-196`). `Encode`/`Decode` are identical XOR streams (`OFB64` is self-inverse), reading the IV via `TB()` to keep byte order stable across platforms. `SelfTest` runs the standard 64-bit and variable-key Blowfish vectors.

**Key size:** variable, 1..56 bytes (cyclic wrap); the *derived* key is `mMacLen` bytes.

### 6.3 `HLMD5` / `HLSha1` — HMAC

`HLMD5::HMAC_XXX` is textbook RFC-2104 HMAC-MD5 (ipad `0x36`, opad `0x5c`, 64-byte blocks, key hashed if >64 bytes; `HLMD5.cpp:4-45`). `HLSha1` is the standard SHA-1 (`sha_ctx` digest/count/block/index, `HLSha1.h:16-22`) with an HMAC wrapper; `GetMacLen()` returns 20. Note `HLMD5` names itself `"\pHMAC-MD5"` and `HLSha1` `"\pHMAC-SHA1"` (Pascal strings).

### 6.4 `HLRand` — CSPRNG

A Yarrow-lite / HMAC-DRBG: `sRandPool[20]` is iterated as `temp = HMAC(key=(uint8*)&sRandCounter, text=sRandPool)`; `sRandCounter++` per block (`HLRand.cpp:108-131`). Seeded from a persisted `rand_seed` file (256 bytes) in the program's `Data` folder (`ReadSeed`/`WriteSeed`, `HLRand.cpp:42-106`); `Churn(data)` mixes caller entropy in. `Init()`/`Cleanup()` are called from `CApplication` ctor/dtor (`CApplication.cpp:25,54`).

### Consumers

`HLBlowfish`/`HLMD5`/`HLSha1`/`HLCrypt` are consumed by `Apps/Client/Source/HotlineTasks.cpp` and `Hotline.cpp` (client login/crypto negotiation) and referenced from both server `HotlineServ.h` files; `HLRand` is used inside AW (`CWindow.cpp` 14, `UTransact.cpp` 3) for nonces/challenge randomness.

### Classification

**preserve-semantics-rewrite** (highest priority). These exact algorithms — Blowfish-OFB64 with zero IV, HMAC-MD5/SHA-1, the `HLCrypt` two-key schedule and `Perm*Key` — are **the Hotline wire crypto**. Replace the implementations with OpenSSL/Botan/`std`-adjacent primitives but keep byte-for-byte behavior, and add test vectors captured from this code before deleting it. `HLRand` can be swapped for a system CSPRNG (`std::random_device`/`getrandom`/`RtlGenRandom`) **as long as** no persisted `rand_seed` file or cross-version output compatibility is required.

---

## 7. Misc

### 7.1 `UDigest` (`UDigest.h` 66, `UDigest.cpp` 661, `UDigest.original.cpp` 706)

**What it is:** Base64 + uuencode + MD5 codecs returning heap-allocated buffers (`outDataSize` set). `Base64_Encode/Decode`, `UU_Encode/Decode`, `MD5_Encode` (16-byte digest). Also defines `_MD5`/`_MD5Digest` classes (RFC-1321). `UDigest.original.cpp` is the pre-cleanup copy (the `_MD5` class was later hoisted into the shared `UDigest.h`; the current `.cpp` differs from `.original.cpp` mainly by that hoist, `diff` shows lines 283-327 removed from the `.cpp`). **Consumers:** no Apps call sites; AW-internal: `UNntpTransact.cpp` (Base64/UU for NNTP article bodies, 4 sites) and `UGUID(M).cpp` (MD5 for UUID node synthesis). **Classification:** **replace-with-stdlib** (Base64/UU/MD5 available in any crypto/encoding lib) — preserve nothing beyond output format.

### 7.2 `UFieldData` (`UFieldData.h` 147, `.cpp` 1124) — **THE protocol field serializer**

See the dedicated §S2. **Consumers:** `UTransact.cpp`/`UTransact.original.cpp` in AW, and ~149 `StFieldData` + ~410 `TFieldData` matches in Apps (`HotlineServTrans.cpp`, `Hotline.cpp`, `HotlineNews.cpp`, `HotlineTasks.cpp`, `HotlineAdmin*.cpp`, `HotlineViews.cpp`, `HotlineWindows.cpp`). This is the highest-traffic serialization class in the whole tree. **Classification:** **preserve-semantics-rewrite**.

### 7.3 `UMessageSys` (`UMessageSys.h` 51, `.cpp` 441)

**What it is:** a **priority message queue** (the app event loop's backbone). `Post/Replace/Flush/Peek/Execute`, `SetDefaultProc/GetDefaultProc`. Messages carry `(msg, data, dataSize, priority, proc, context, object)`; `Execute` dispatches highest-priority first (priority scan at `UMessageSys.cpp:330-372`). `UApplication` is a thin global wrapper over one `TMessageSys` instance (§7.8). **Consumers:** zero Apps; used only by `UApplication(M/W).cpp` internally. **Classification:** **replace-with-modern-platform** (fold into the event loop / a `std::priority_queue`).

### 7.4 `UZlibCompress` / `UZlibDecompress` (`UZlibCompress.h` 76, `.cpp` 294)

**What it is:** zlib wrappers (`zCompressLevel_None=0 … Best=9, Default=-1`), `New/Dispose/Compress/Process` and `Decompress/Process`. **Consumers:** `UZlibCompress` (the compressor) has **zero callers** anywhere (dead code); `UZlibDecompress` is used only by `Apps/Common Files/HotlineArchiveDecoder.cpp` (1 site). **Classification:** **replace-with-modern-platform** (link zlib directly); the compressor half is **remove-entirely**.

### 7.5 `UError` global error state

There is **no global "last error" state** in `UError` — errors are carried as thrown `SError` objects; the only "global" is the `'EMSG'` catalog lookup and the per-platform `gErrorTable` mapping. (The *transport* layer has its own error handling, out of scope.)

### 7.6 `UProgramCleanup` (`UProgramCleanup.h` 19, `.cpp` 129)

**What it is:** an `atexit`-like registry (two lists: system and app) stored as `THdl` arrays of function pointers; `InstallSystem/InstallAppl` append, `CleanupAppl` runs app procs in install order under `try/catch` (`UProgramCleanup.cpp:87-106`). Consumers: AW-internal (transport, sound, timer, window, file system install cleanup hooks). **Classification:** **replace-with-stdlib** (`std::atexit`/scope guards/`std::vector<std::function<void()>>`).

### 7.7 `UDebug` (`UDebug.h` 36, `UDebug(M).cpp` 124, `UDebug(W).cpp` 153)

**What it is:** debugger helpers (`Break/BreakAssembly/BreakSource/LogToDebugFile`) and the `DebugBreak/DebugLog/DebugLogFile` macros (no-ops in release, `UDebug.h:28-36`). Mac uses `DebugStr`/MacsBug; Win uses `DebugBreak`/`OutputDebugString`. **Classification:** **remove-entirely** (use platform logging/asserts).

### 7.8 `UOperatingSystem`, `CApplication`, `UApplication`, `UClipboard`

- **`UOperatingSystem`** (`UOperatingSystem.h` 25, `(M)/(W)` 278/239): OS capability probe — QuickTime availability/version, Flash support, `GetSystemVersion`. ~17 Apps matches. **replace-with-modern-platform.**
- **`CApplication`** (`CApplication.h` 49, `.cpp` 174): the app object (message handler, `Run/Quit/UserQuit`, `WindowHit`, timers, `gApplication` singleton). ~18 Apps matches (each app subclasses it). **replace-with-modern-platform.**
- **`UApplication`** (`UApplication.h` 43, `(M)/(W)` 1037/399): the global message loop — `Init/Process/ProcessAndSleep/Run/Quit/Abort`, `SendMessage/PostMessage/ReplaceMessage/FlushMessages/PeekMessage`, `SetMessageHandler`, `ShowWantsAttention`. ~113 Apps matches. This is the client/server run-loop abstraction. **replace-with-modern-platform.**
- **`UClipboard`** (`UClipboard.h` 22, `(M)/(W)` 264/237): clipboard get/set for text, image, sound. Zero Apps call sites (used by `UEditText`, `UDragAndDrop`, `CPasswordTextView`). **replace-with-modern-platform** (or remove if the UI framework provides it).

---

## 8. Filesystem — `UFileSys` (`UFileSys.h` 528, `UFileSys.cpp` 1105, `(M)` 3844, `(W)` 3863)

**What it is:** a complete cross-platform filesystem abstraction in the **MoreFiles / Mac Finder** idiom. `TFSRefObj*` is an opaque filesystem reference; sentinel start-folder constants `kRootFolderHL / kProgramFolder / kTempFolder / kPrefsFolder / kDesktopFolder` are forged `(TFSRefObj*)max_Uint32-N` values (`UFileSys.h:62-66`). `SFSListItem` carries `{typeCode, creatorCode, size, flags, rsvd, createdDate, modifiedDate, name[]}` (`UFileSys.h:72-78`). `SFlattenRef {sig 'FSrf', vers 1, OS 'MACH'|'WIND', dataSize, data[]}` is the flattened-reference format (`UFileSys.h:80-89`).

API surface (very large): New/NewTemp/Clone/Dispose/SetRef; CreateFile(+type/creator)/CreateFolder/CreateAlias/Exists; DeleteFile/Folder/FolderContents/MoveToTrash; SetName/GetName/GetPath/GetFolder/EnsureUniqueName/ValidateFileName; Move/MoveAndRename/SwapData; Open/Close/Read/Write/GetSize/SetSize/Flush/ReadToHdl/Copy; type/creator/comment/date-stamp/hidden/equals; `UserSelectFile/Folder/SaveFile` (native dialogs); `GetListing/GetListNext` (directory enumeration); path helpers (`GetPath/AppendToPath/MakePathData/GetApplicationPath/GetTempPath`); `ConvertPathDataToUrl/ConvertUrlToPathData`; alias resolution; `FlattenRef/UnflattenRef`; and **resumable flatten/unflatten** of whole files (`StartFlatten/ResumeFlatten/ProcessFlatten` … `StartUnflatten/ResumeUnflatten/ProcessUnflatten`) used to stream files over the wire. `Mac`-only `OpenResourceFork` (`UFileSys.h:113-117`). Stack helpers `StFileSysRef`, `StFileOpener`; temp-file classes `CTempFile`, `CEncodedTempFile`, `CPurgedTempFile`, `CGeneralFile`.

**MoreFiles-era concepts encoded here:** volume/parent references, **type/creator codes**, file **aliases**, resource forks, `perm_*` sharing bits, disk free space as `Uint64`, comment fields, "move to trash". The Windows side emulates type/creator via a sidecar/registry mapping (the `(W).cpp` is 3,863 LOC of `SHGetFileInfo`/`GetFileAttributes`/`CreateFile` plumbing).

**Consumers:** ~126 `UFileSys`/`UFS` + ~131 `TFSRefObj` matches in Apps — every app does all I/O through this. **Still relevant?** The *abstraction* is still the I/O seam, but the type/creator/alias/resource-fork machinery is obsolete. **Replacement:** `std::filesystem` (or the platform-native API) for paths/IO; keep the `SFSListItem`-style listing codec and the resumable flatten/unflatten wire protocol as `preserve-semantics-rewrite`; delete type/creator/alias/resource-fork/trash/comment concepts. **Classification:** **replace-with-modern-platform** (with protocol-relevant listing/flatten codecs preserved).

---

## 9. `AppWarrior/Error Msgs/` and `AppWarrior/Libraries/`

### 9.1 `Error Msgs/`

Eight pairs of files named `U<Module>(N)` and `U<Module>(N).dat` where **N = the `errorType_*` code** (`UError(1)`=Misc, `UMemory(3)`, `UFileSys(4)`, `URez(6)`, `UTransport(8)`, `UImage(10)`, `UDragAndDrop(19)`, `USerialPort(20)`).

- The **`.dat`** files are **flattened `UIDVarArray` ('IVA1') blobs** mapping error *ID* → message text. Hexdump of `UMemory(3).dat` confirms: `49 56 41 31` = `"IVA1"`, itemCount=3, IDs `0x64/0x65/0x66` (100/101/102), followed by the three message strings. These are the **binary error catalogs**.
- The plain **`(N)`** files are the **human-readable source** (tab-indented `ID \t message`). `UError(1)` lists exactly the 13 `error_*` strings also inlined in `UError.cpp:38-60`.
- **How they're loaded:** Mac — classic `GetResource('EMSG', type)` (`UError(M).cpp:68`) then `UIDVarArray::GetItem`. Windows — `URez::SearchChain('EMSG', type)` → `StRezLoader` → `UIDVarArray::GetItem` (`UError(W).cpp:23-33`). So the `.dat` files are compiled into `'EMSG'` resources on Mac and into AWRZ resource files on Windows.

### 9.2 `Libraries/`

- **`AppWarrior.lib.h`** (124 LOC) is the **umbrella / precompiled-header** include list for the whole runtime — every header, grouped (types, misc, data, files, graphics, images, UI, crypt, hardware, views). The `.original.h` copy differs only by the **later addition of the `Crypt/` headers** (diff shows `HLCrypt.h…HLRand.h` appended after line 68).
- **`AWHeaders(W).h`**, `AWHeaders(WD).h`, `AWHeaders(WH).h`, `AWHeaders(WHD).h`, `AWHeaders(W_ISP).h`, `AWHeaders(WD_ISP).h` are per-build-configuration **prefix-header shims**. Each is 1 line: `#include "AWHeaders(W)"` etc. The actual target file `AWHeaders(W)` is **not in the tree** (it was a CodeWarrior/MSVC-generated prefix header). Naming: `W`=Windows, `D`=Debug, `H`=?, `ISP`=? (likely an Internet Service Provider / release-vs-ISP build variant — not decodable from this snapshot). These tell us the **link/compile configuration matrix** (Win, Win+Debug, Win+ISP, …) but the referenced headers are generated at build time.

---

## 10. `URez` (`URez.h` 222, `URez.cpp` 1883)

**What it is:** a custom **Resource Manager** over pluggable I/O callbacks (`TIOReadProc/TIOWriteProc/…`, `URez.h:57`). API mirrors classic Mac `Resource Manager` semantics: `New/NewFromFile/Dispose`, `AddItem/RemoveItem/RemoveAllItems/ItemExists`, `LoadItem/ReleaseItem/ChangedItem/SetItemHandle` (with `useCount` refcounting, `URez.cpp:99`), `ReadItem/GetItemSize`, `Get/SetItemName/Attributes/ID/Info`, `GetTypeListing/GetItemListing/GetItemListNext`, a **search chain** (`AddSearchChainStart/End`, `SearchChain`, `AddProgramFileToSearchChain`), `Save`, `GetLowestUnusedID`, `Reload`. Stack helpers `StRezLoader` (RAII Load/Release) and `StRezReleaser`.

**Format knowledge it encodes (quoted from `URez.cpp:18-68`):** it does **not** read the classic Mac resource-fork format; it reads a **custom flat "AWRZ" file**:
```
header: Uint32 format='AWRZ'; Uint16 version=1; Uint8 rsvd[18]; Uint32 mapSize; Uint32 dataSize
data:   Uint8 resData[dataSize]
map:    Uint32 checkOffset; Uint32 typeCount;
        per type: Uint32 type; Uint32 itemCount;
                  per item: Uint32 id; Uint32 attrib; Uint32 dataSize; Uint32 dataOffset
```
Resource **names** are stored in `'%_nm'` resources (one per type, ID = the type code) as `{Uint32 count; {Uint32 id; Uint16 size; Uint8 data[size]} item[]}` on 4-byte boundaries (`URez.cpp:49-59`). All map integers are `FB`-read (big-endian). `_RZReadRezFile` (map parse at `URez.cpp:1819-1858`) validates offsets against the data region and fails `error_Corrupt` on out-of-range entries.

**Consumers:** ~14 `URez` matches in Apps; AW-internal consumers are `UError(W).cpp` (the `'EMSG'` catalogs), `UApplication(W).cpp`, and the UI resource loading. It is the mechanism by which Windows builds get "resources" without a Mac resource fork.

**Still relevant?** Only as the AWRZ reader for legacy resource files (error catalogs, UI resources). **Replacement:** delete the Resource Manager; parse the specific legacy files (IVA1 catalogs, any AWRZ UI blobs) with bespoke readers. **Classification:** **retain-temporarily** (frozen AWRZ/IVA1 reader) or **remove-entirely** if the app's resources are re-authored natively.

---

## Summary table

| File(s) | LOC (total) | Platform split | Verdict |
|---|---|---|---|
| `Headers/ANSI.h` + `Misc/ANSI.cpp` | 135 + 1855 | portable | replace-with-stdlib |
| `Headers/typedefs.h` | 286 | portable (`__INTEL__`/`__POWERPC__`) | replace-with-stdlib |
| `Headers/MoreTypes.h` | 164 | portable | replace-with-modern-platform |
| `Headers/GrafTypes.h` + `Graphics/GrafTypes.cpp` | 463 | portable | replace-with-modern-platform |
| `Headers/ImageTypes.h` | 139 | portable | retain-temporarily |
| `Headers/CBoolArray.h` + `Data/CBoolArray.cpp` | 49 + 294 | portable | remove-entirely |
| `Headers/CLinkedList.h` + `Data/CLinkedList.cpp` | 49 + 167 | portable | replace-with-stdlib |
| `Headers/CPtrList.h` + `Data/CPtrList.cpp` | 85 + 451 | portable | replace-with-stdlib |
| `Headers/CPtrTree.h` (hdr-only) | 638 | portable | replace-with-stdlib |
| `Headers/UIDVarArray.h` + `Data/UIDVarArray.cpp` | 85 + 623 | portable | retain-temporarily (IVA1 reader) |
| `Headers/UBitString.h` + `Data/UBitString.cpp` | 24 + 141 | portable | remove-entirely |
| `Headers/UMemory.h` + `Data/UMemory.cpp` | 274 + 1784 | portable | replace-with-stdlib |
| `Data/UMemory(alloc).cpp` + `Data/UMemory(priv).h` | 1113 + 52 | portable | remove-entirely |
| `Data/UMemory(M).cpp` / `Data/UMemory(W).cpp` | 1033 / 1052 | M / W | remove-entirely (no handle re-creation) |
| `Headers/UText.h` + `Data/UText.cpp` + `(M)/(W)` | 97 + 1289 + 91 + 53 | portable + M/W | replace-with-stdlib |
| `Headers/CFlatten.h` (hdr-only) | 270 | portable | preserve-semantics-rewrite |
| `Headers/UMime.h` + `Data/UMime.cpp` | 19 + 329 | portable | replace-with-modern-platform |
| `Headers/UTimer.h` + `Misc/UTimer(M/W).cpp` | 39 + 245 + 158 | M / W | replace-with-modern-platform |
| `Headers/UDateTime.h` + `Misc/UDateTime.cpp` + `(M)/(W)` | 121 + 725 + 226 + 397 | portable + M/W | replace-with-stdlib (wire codec preserved) |
| `Headers/UMath.h` + `Misc/UMath.cpp` + `(M)/(W)` | 86 + 149 + 66 + 72 | portable + M/W | replace-with-stdlib |
| `Headers/UError.h` + `Misc/UError.cpp` + `(M)/(W)` | 108 + 196 + 370 + 190 | portable + M/W | replace-with-modern-platform |
| `Headers/UGUID.h` + `Misc/UGUID.cpp` + `(M)/(W)` + `Libs/UUID/UUIDLib.h` | 47 + 82 + 2451 + 90 + 82 | M-heavy | replace-with-modern-platform |
| `Source/Crypt/*` (Blowfish/MD5/SHA1/Rand/Crypt) | 236+314+44+484+137+76 (+hdrs) | portable | preserve-semantics-rewrite |
| `Headers/UDigest.h` + `Data/UDigest.cpp` (+`.original.cpp`) | 66 + 661 + 706 | portable | replace-with-stdlib |
| `Headers/UFieldData.h` + `Data/UFieldData.cpp` | 147 + 1124 | portable | preserve-semantics-rewrite |
| `Headers/UMessageSys.h` + `Data/UMessageSys.cpp` | 51 + 441 | portable | replace-with-modern-platform |
| `Headers/UZlibCompress.h` + `Data/UZLibCompress.cpp` | 76 + 294 | portable | replace-with-modern-platform (compressor: remove) |
| `Headers/UProgramCleanup.h` + `Misc/UProgramCleanup.cpp` | 19 + 129 | portable | replace-with-stdlib |
| `Headers/UDebug.h` + `Misc/UDebug(M/W).cpp` | 36 + 124 + 153 | M / W | remove-entirely |
| `Headers/UOperatingSystem.h` + `Misc/UOperatingSystem(M/W).cpp` | 25 + 278 + 239 | M / W | replace-with-modern-platform |
| `Headers/CApplication.h` + `Misc/CApplication.cpp` | 49 + 174 | portable | replace-with-modern-platform |
| `Headers/UApplication.h` + `Misc/UApplication(M/W).cpp` | 43 + 1037 + 399 | M / W | replace-with-modern-platform |
| `Headers/UClipboard.h` + `Misc/UClipboard(M/W).cpp` | 22 + 264 + 237 | M / W | replace-with-modern-platform |
| `Headers/UFileSys.h` + `Files/UFileSys.cpp` + `(M)/(W)` | 528 + 1105 + 3844 + 3863 | portable + M/W | replace-with-modern-platform |
| `Headers/URez.h` + `Data/URez.cpp` | 222 + 1883 | portable | retain-temporarily (AWRZ reader) |
| `Error Msgs/*` | — | data files | retain-temporarily (IVA1 catalogs) |
| `Libraries/*` | 2611 + shims | build config | remove-entirely (build system) |

---

## S1. `ANSI.h` migration plan

Every declaration and its replacement:

| Declaration | Modern replacement |
|---|---|
| `NULL` → `0L` | `nullptr` (C++), `<cstddef>` |
| `typedef unsigned int/long size_t` | `<cstddef>` |
| `OFFSET_OF` | `offsetof` (`<cstddef>`) |
| `malloc/calloc/realloc/free` | `new`/`delete`, `std::allocator`, `std::vector` |
| `memcpy/memmove/memset/memchr/memcmp` | `<cstring>` |
| `memrchr` | `<cstring>` (GNU) / hand-roll or drop |
| `strlen/strcpy/strncpy/strcat/strncat/strcmp/strncmp/strchr/strrchr/strpbrk/strspn/strcspn/strtok/strstr` | `<cstring>` |
| `strrev` | `std::reverse` |
| `strtol/strtoul` | `std::strtol/strtoul` or `std::from_chars` |
| `ltostr/ultostr` | `std::to_chars`/`snprintf` (or keep as a 10-line shim) |
| `sprintf/vsprintf` | `snprintf`/`std::format` |
| `exit/atexit/abort` | `<cstdlib>` |
| `bsearch/qsort`, `_compare_func` | `std::binary_search`/`std::sort` |
| `extern int errno` | `<cerrno>`/`<system_error>` |
| `pstr*` family (Pascal strings) | keep as a **thin** `pstring` utility during migration (wire format uses length-prefixed strings in places) or drop once all `\p`-literals are gone |
| `pstrlen/pstrtext` inline | same as above |
| `__ctype_map/__lower_map/__upper_map` (commented out) | `<cctype>` (already superseded by `UText`) |

**Pascal-string note:** the tree is full of `"\p..."` literals and `pstr*` calls; these must be normalized to length+data buffers or `std::string` during the port, but any **wire** PString (length-prefixed in the protocol) must keep the exact 1-byte-length encoding.

---

## S2. Protocol-relevant serialization inventory

The Hotline protocol's transaction payloads are **big-endian "field data"**; two serialization primitives are relevant, plus the digest/checksum helpers.

### S2.1 `UFieldData` — the transaction field container (byte-level)

Wire layout (`UFieldData.cpp:2-11` and `_FDBuildTables` at 739-831):

```
+0  Uint16 count            (big-endian)
+2  repeated `count` times:
      Uint16 id              (big-endian)
      Uint16 size            (big-endian)  — byte length of `data`
      Uint8  data[size]      (raw bytes, NO padding: ALIGN_FIELDS == 0, UFieldData.cpp:39)
```

- **Endianness:** every `id`/`size`/`count` is `TB()`-written and `FB()`-read (`UFieldData.cpp:456-458, 490-498`; `_FDBuildTables` reads `FB(*(Uint16*)p)` at 780, 800, 804).
- **Field IDs are `Uint16`, not sorted** in the buffer; lookup is by a **sorted** `SFDEntry{id,index}` lookup table (heapsort + binary search, `UFieldData.cpp:833-980`). `GetFieldInfo/ByIndex` expose `(id, size, dataOffset)`.
- **Header size** (`inHeaderSize`): callers can reserve a leading region (e.g. the transaction's own header) that `UFieldData` skips — `New(h, headerSize)`, `SetDataHandle(h, headerSize)`. `offsetTable` entries are relative to `headerSize`.
- **Helpers:** `AddInteger` stores 2 or 4 bytes BE depending on magnitude (`UFieldData.cpp:516-528`); `GetInteger` accepts 1/2/4-byte encodings; `AddPString` strips the Pascal length byte and stores only the text (`AddPString` at 530-533); `GetPString/GetCString` restore it.
- **Robustness:** `_FDBuildTables` bounds-checks every field header/data against the handle size and stops at a truncated tail (sets `tableSize` = fields actually parsed, `UFieldData.cpp:799-815`), which is how it tolerates partially-transferred data (comment at 107).

This is the **highest-value class to preserve bit-for-bit.** A modern `preserve-semantics-rewrite` should keep the exact layout (count/id/size/data, BE, no padding) and the ID-lookup API, backed by `std::vector<uint8_t>`.

### S2.2 `CFlatten` / `CUnflatten` — structured stream

Fixed layouts (all big-endian, `CFlatten.h`):
- `WriteByte`: 1 byte.
- `WriteWord/Long`: `TB`-swapped `Uint16`/`Uint32`.
- `WritePString`: `Uint8 len` + `len` bytes (`CFlatten.h:21, 119-125`).
- `WriteWString`: `Uint16 len` (BE) + bytes (`CFlatten.h:127-133`).
- `WriteLString`: `Uint32 len` (BE) + bytes (`CFlatten.h:135-140`).
- `WriteShortColor`: 4 bytes `R G B 0` (8-bit channels, `CFlatten.h:142-149`).
- `WriteShortPoint`: 2×`Int16` BE (4 bytes); `WriteShortRect`: 4×`Int16` BE (8 bytes).
- `WriteDateTimeStamp`: `Uint16 year, Uint16 msecs, Uint32 seconds` BE (8 bytes) — identical to `SDateTimeStamp::Flatten`.
- `Align2/4/8` zero-pad to boundary.

`CUnflatten` reads all of the above with `FB` and bounds checks (`ReadPString` returns validity, `ReadWString` returns pointer+size). This is a generic stream, used by the QTDataHandler persistence layer and a handful of app files; its string/time/rect encodings must be preserved wherever they appear in a persisted or transmitted format.

### S2.3 `UDigest` / `UMemory` checksums

- `UDigest::MD5_Encode` → 16-byte MD5 (raw bytes). `Base64_*`/`UU_*` → text encodings used for NNTP bodies.
- `UMemory::Checksum` = simple 32-bit byte sum (`inInit`-seeded).
- `UMemory::CRC` = CCITT-32 (poly `0x04C11DB7`, MSB-first, `crc = (crc<<8) ^ tab[(crc>>24)^byte]`, init `-1` default) — the same table/algorithm as the classic `CRCTable` used by Hotline file transfer checksums. **Preserve exactly.**
- `UMemory::AdlerSum` = Adler-32 (mod 65521).
- `UMemory::PackIntegers/UnpackIntegers` = a 2-bit-per-item size-code varint (sizes 0/1/2/4 bytes), used for compact integer lists; preserve if it appears in any stored format.

### S2.4 `UIDVarArray` flattened format

`'IVA1'` + reserved + textEncoding + itemCount + sorted `{id, offset}[]` + data (all BE) — the error-catalog storage format (§9.1). Preserve as a reader.

### S2.5 `SDateTimeStamp` wire format

`{Uint16 year, Uint16 msecs, Uint32 seconds}` BE, semantics "year + within-year seconds + ms" — with the **Mac 1904 divergence documented in §5.2 that must be resolved**.

### S2.6 Endianness helpers

There is **no `SwapLong`/`SwapShort`** anywhere in the tree (`grep` = 0). The endian helpers are:
- `swap_int(Uint32/Uint16)` — `typedefs.h:153-171`.
- `TB()`/`FB()` (to/from big-endian) — `typedefs.h:173-208`; identity on big-endian, `swap_int` on Intel.
- `CDecompress*` image codecs call `swap_int` directly on GIF/BMP/PICT fields (`CDecompressGif.cpp:160-161, 239-242, 427-430`; `CDecompressPict.cpp:1953-1987`; `CDecompressBitmap.cpp:70-80`); the server `HotlineServ.cpp:3947-4021` uses `swap_int` on file-transfer integers.

---

## S3. Mac Handle inventory

| API / symbol | Location | Semantics | Semantic replacement |
|---|---|---|---|
| `TPtr` / `THdl` typedefs | `UMemory.h:28-29` | opaque fixed/relocatable blocks | `unique_ptr`/`vector` |
| `UMemory::New/NewClear/New(data)` | `UMemory(M/W).cpp` | fixed alloc (pool) | `new`/`make_unique`/`vector` |
| `UMemory::NewHandle/NewHandleClear/Clone` | `UMemory(M/W).cpp` | relocatable alloc (+4 prefix) | `vector<uint8_t>` |
| `UMemory::Dispose(TPtr/THdl)` | `UMemory(M/W).cpp` | free (guards locked/resource handle) | `delete`/scope |
| `UMemory::Lock/Unlock` | `UMemory(M).cpp:548-605` / `(W):513-549` | pin + refcount; returns `*h+4` | direct indexing |
| `UMemory::SetSize/GetSize/Grow/Reallocate/ReallocateClear` | `UMemory(M/W).cpp` | resize handle | `vector::resize` |
| `UMemory::SetDiscardable/ClearDiscardable` | `UMemory(M).cpp:613-642` / `(W):557-578` | `HPurge`/`HNoPurge`; `GMEM_DISCARDABLE` | **delete** |
| `StHandleLocker` (AW) | `UMemory.h:177-185` | RAII Lock/Unlock | **remove** |
| `StHandleLocker` (QTDataHandler) | `Libs/QTDataHandler/StHandleLocker.h` | RAII `HGetState`/`HLock`/`HUnlock` on raw `Handle` (separate BigRedH code) | **remove** |
| `StPtr` / `StHdl` | `UMemory.h:187-223` | RAII TPtr/THdl | `unique_ptr`/`vector` |
| `operator new/delete` (global) | `UMemory.cpp:1767-1780` | route to UMemory | remove |
| `_HdlToWinHdl` | `UMemory(W).cpp:1023` | strip prefix for Win32 | remove |
| Mac `Handle` usage in `URez::LoadItem` | `URez.cpp:380-391` | `readProc` into a `NewHandle` | `vector` |
| Mac `Handle` usage in `UError(M).cpp:68` | `GetResource('EMSG')` + `HLock` | resource catalog | bespoke file reader |
| `UFileSys::OpenResourceFork` | `UFileSys.h:113-117` | Mac resource fork open | remove |

**Recreate-nothing rule:** the 4-byte lock/size prefix, the `HLock`/`HPurge`/`HNoPurge` state machine, `GMEM_DISCARDABLE`, and the "cannot dispose locked handle" debug guards all exist to emulate a compactible Mac heap that no longer exists. A port must convert `THdl` to owning byte buffers and rewrite `Lock()+offset` arithmetic to indexing; it must not reintroduce a relocatable-handle layer.

---

## S4. Crypto inventory

| Component | Algorithm | Key size | Mode / notes | Determinism |
|---|---|---|---|---|
| `HLBlowfish` | Blowfish (16 rounds) | variable 1..56 B (derived: hash-len) | **OFB-64, zero IV** (reset per `Init`); self-inverse | deterministic given key+IV |
| `HLMD5` | HMAC-MD5 (RFC 2104) | — | `GetMacLen`=16 | deterministic |
| `HLSha1` | HMAC-SHA-1 | — | `GetMacLen`=20 | deterministic |
| `HLCrypt` | key schedule over `HLHash` | — | 2 keys: `t1=HMAC(pw,sk)`, `t1=HMAC(pw,t1)`, `t2=HMAC(pw,t1)`; client enc=t2/dec=t1, server enc=t1/dec=t2; `PermEncode/DecodeKey(n)` re-HMACs `key=HMAC(key,sessionKey)` n× | deterministic |
| `HLRand` | HMAC-SHA1 DRBG over `sRandPool` | 256-B seed file | `temp=HMAC(counter, pool)`; counter++; `Churn()` mixes entropy | non-deterministic (seeded) |

**Usage evidence:** `HLBlowfish/HLMD5/HLSha1` are instantiated in `Apps/Client/Source/HotlineTasks.cpp` (client crypto negotiation); `HLCrypt` referenced from client (`Hotline.h`, `Hotline.cpp`, `HotlineTasks.cpp`) and both servers (`HotlineServ.h`); `HLRand` used internally (nonce/challenge randomness in `CWindow.cpp`, `UTransact.cpp`).

**Preservation directive:** these are the Hotline wire-crypto primitives. Before replacing, **capture reference vectors** (Blowfish OFB-64 with zero IV, the HMAC key schedule, `PermEncodeKey(0/1/n)`) from this exact code, then re-implement with a maintained crypto library (OpenSSL/Botan) and validate byte-for-byte. `HLRand` may be replaced by the OS CSPRNG if no persisted-seed or output-compatibility contract exists.

---

## Risks / unanswered questions

1. **`SDateTimeStamp` cross-platform semantics differ** — Mac `GetDateTimeStamp` emits `year=1904, seconds=since-1904`; Windows/portable code emits `year=YYYY, seconds=within-year`. The 8-byte wire layout is identical. **Must determine the canonical wire meaning before porting** (evidence suggests the Windows/portable "within-year" convention is the real protocol one; the Mac 1904 path may be a latent bug). (`UDateTime(M).cpp:100-108` vs `UDateTime(W).cpp:223-231`/`381-394`.)
2. **Two opposite bit orders coexist** — `UBitString` is LSB-first (`UBitString.cpp:22`), `UMemory::GetBit/SetBit` are MSB-first (`UMemory.h:135-147`). Any bit-level wire format must be checked against the *specific* helper used.
3. **`UFieldData` max field size** = 65535 (`max_Uint16` guard, `UFieldData.cpp:436`); the Hotline protocol's per-field cap should be confirmed against external docs.
4. **`CPtrTree` level is `Uint16`** (depth ≤ 65535) and sort is O(n²) per parent; **`CBoolArray` cannot Move/Swap >1 item** (`CBoolArray.cpp:249-251`). If revived, these limits must be respected or reworked.
5. **`UZlibCompress` (compressor) is dead code** (0 callers); only `UZlibDecompress` is used (by `HotlineArchiveDecoder.cpp`). Confirm there is no server-side "archive compression" path that was simply not found in this snapshot.
6. **`AWHeaders(W)`/`AWHeaders(W_ISP)` etc. target files are absent** from the tree — the "H" and "ISP" build variants cannot be fully decoded from the snapshot (likely generated prefix headers / release-vs-ISP build configs).
7. **`UString` does not exist in the core** — the only string *classes* are in the vendored `QTDataHandler` sub-library; any migration plan must decide whether that sub-library is in scope at all.
8. **`CFlatten` vs `UFieldData` overlap** — the task description attributed "string flattening for protocol field serialization" to `CFlatten`, but the actual transaction field serializer is `UFieldData`; `CFlatten` is a generic stream used mostly by the QTDataHandler persistence layer. Verify which one any given wire path uses.
9. **MacRoman is the wire text encoding** — the client/server exchange 8-bit (MacRoman) text; the only translation table present (`UText(W).cpp`) is currently identity for the high half. A modern UTF-8 port must define the exact MacRoman↔UTF-8 mapping for **both** on-disk legacy data and live protocol text, or old clients will see corrupted non-ASCII.
10. **`HLRand` persisted seed file** (`rand_seed`, 256 bytes in the program `Data` folder) is a compatibility surface: replacing the CSPRNG breaks reproducibility of any derived material but not the protocol (which uses `HLCrypt` keys, not `HLRand` output) — confirm before removing.
11. **Unverified claim about `URez`:** it was described as a "classic Mac resource fork reader," but it actually reads/writes the **custom 'AWRZ' flat format** (`URez.cpp:18-68`) through I/O callbacks. It is Mac-*Resource-Manager-API-shaped*, not Mac-resource-*fork-format*. Any downstream plan expecting classic `rsrc` fork parsing must be corrected.

---

*End of audit. All paths are under `legacy/`; the report is the sole output and no legacy file was modified.*
