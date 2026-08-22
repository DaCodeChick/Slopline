# Hotline Tracker & Utilities Audit

Scope: `Apps/Tracker`, `Apps/FileFinder`, `Apps/Rez2Dat`, `Apps/Images`, `Apps/ServerOLD/News`, `Apps/Server/Data`, `Apps/Client/Data`. All evidence is read-only from `/home/admin/Documents/GitHub/Slopline/legacy/`. Line citations use the decoded (MacRoman→UTF-8) source. The tracker sources are MacRoman; the protocol constants shared by client/server live in `Apps/Common Files/HotlineClientServerCommon.h`.

---

## 1. Tracker file inventory + old/new comparison

### 1.1 Files

| Path | Lines | Role |
|---|---|---|
| `Tracker/Old Tracker/Sources/TrackerServ.cpp` | 1716 | Old tracker: app + protocol (v1.2.0) |
| `Tracker/Old Tracker/Sources/TrackerServ.h` | 261 | Old tracker header (single `CMyApplication` = the tracker) |
| `Tracker/Old Tracker/Sources/TrackerServViews.cpp` | 237 | Options window views (login/ban lists) |
| `Tracker/Old Tracker/Sources/TrackerServWindows.cpp` | 421 | Options/stats windows |
| `Tracker/New Tracker/Sources/TrackerServ.cpp` | 2716 | New tracker: app + protocol (v1.3.0) |
| `Tracker/New Tracker/Sources/TrackerServ.h` | 465 | New tracker header (`CMyTracker` + `CMyApplication`) |
| `Tracker/New Tracker/Sources/TrackerServViews.cpp` | 488 | Toolbar/options/list views |
| `Tracker/New Tracker/Sources/TrackerServWindows.cpp` | 675 | Toolbar/options/log/stats/servers windows |
| `Tracker/New Tracker/Resources/LoginDatabaseFormat.txt` | — | Documents the binary login DB layout |
| `Tracker/New Tracker/hltracker.dat` | — | Runtime resource container (AWRZ) |
| `Tracker/New Tracker/Prefs` | — | Prefs DB (44 bytes, format below) |
| `Tracker/Old Tracker/Data/rand_seed`, `Tracker/Old Tracker/HLRand.out` | — | 256-byte RNG seed / RNG output |

### 1.2 Comparison

| Aspect | Old Tracker (v1.2.0) | New Tracker (v1.3.0) |
|---|---|---|
| Wire protocol | identical (see §2) | identical — **no protocol change** |
| Process model | `CMyApplication` *is* the tracker (one instance) | `CMyApplication` manages a `CPtrList<CMyTracker>`; many trackers per process |
| Config storage | plain text files in program folder: `Passwords`, `Banned`, `TrackerIP`, `ServersPerIP` | per-tracker folder (named by IP) with binary `Prefs`, `Login`, `Banned` (+ `.bak` backups) |
| Login entries | password only (`SMyLoginInfo` holds one pstring) | `SMyLogin`/`SMyLoginInfo` hold login **and** password, plus `nActive`, `psDateTime`, reserved — but `#define NEW_TRACKER 0` disables the login-name UI, so only the password is enforced |
| Ban entries | IP only | IP + active flag + date/time + 64-byte description |
| Logging | `#if USE_LOG_WIN 0` (compiled out) | always-on log window; options `logToFile/logServerConnect/logClientConnect/logTrackerStart/logTrackerStop` |
| Dead-server timeout | 660 s (11 min), `TrackerServ.cpp:879` | 1860 s (31 min), `TrackerServ.cpp` (`curTime - info->timeStamp > 1860`) |
| Internal server priority | `#define INTERNAL_SERVER_PRIORITY 0` (disabled) | `#define INTERNAL_SERVER_PRIORITY 0` (disabled) |
| Version string | `1.2.0.0` (rc) | `kTrackerServVers = "\pv1.3"`, rc `1.3.0.0` |

The "new" tracker is a UI/management rewrite, not a protocol upgrade. Both compile-time feature flags that would have added protocol features are off: `NEW_TRACKER = 0` (`TrackerServ.h:6`) and the client's `NEW_TRACKSERV` (see §2.5) is never `#define`d.

---

## 2. Tracker protocol behavior spec

The tracker does **not** speak the `myTran_*` transaction protocol (`HotlineClientServerCommon.h`). It uses its own tiny binary protocol over three sockets. All integers are big-endian (`TB()`/`FB()`).

### 2.1 Ports (both trackers, identical)

- **UDP 5499** — server registration transport (`TrackerServ.cpp` old `:270`, new `:148`).
- **TCP 5498** — client listing transport (old `:277`, new `:155`).
- **TCP 5497** — second client listener, labeled `mListenHttpTpt` (old `:283`, new `:161`). Despite the name, there is **no HTTP parsing**; both TCP ports run the identical `'HTRK'` binary handshake. The client selects 5497 only when `bTunnelThroughHttp` is set (`HotlineTasks.cpp:5929-5931`), so it is an "HTTP-port" fallback for firewalls, still binary on the wire.

### 2.2 Server→tracker registration (UDP 5499)

`ProcessRegister` reads each datagram (`old:509`, `new:592`) and dispatches on a leading `Uint16 type`:

```
Uint16 type;        // 1 = add/register, 2 = remove (ignored in both)
Uint16 port;        // server's Hotline port
Uint16 userCount;   // current connected users
Uint16 flags;       // bit0 (value 1) = "don't show in list (name only)"
Uint32 passID;      // numeric server identity, stable across address changes
pstring name;       // server name
pstring desc;       // server description
pstring password;   // tracker registration password
```

Evidence: the field layout is spelled out in the comment and `CUnflatten` reads (`old:533-548`, `new:622-637`). **The server's IP address is not a message field** — it is taken from the UDP source address of the datagram (`((SInternetAddress*)addr)`), which is why `port` is injected into `addr` before `RegisterServer`. Type `2` (unregister) is accepted on the wire but deliberately ignored ("not done and not really necessary" — `old:550`, `new:639`).

`RegisterServer` (`old:675`, `new:738`) rejects the registration if name/desc/password are empty, if the password is invalid, or if the source IP is banned. It then:
1. CRC32s the name (`UMemory::CRC(name+1, name[0])`) and keeps the list sorted by that CRC.
2. Decides **update vs. insert**: update if same name CRC + same exact name **and** (same address+port **or** same `passID`), else insert; a name collision with a *different* address returns without registering (`old:774`, `new` equivalent).
3. Maintains a running total `mUserCount` (sum of all `userCount`), and a per-IP counter list to enforce `mMaxServersPerIP` (`AddServerPerIP`/`RemoveServerPerIP`, `old:1532+`).

Password check differs by generation: old stores CRCs of passwords and compares `UMemory::CRC(inPass+1,inPass[0])` against a `Uint32` array (`old:1242-1244`); new stores plaintext login/password pstrings and does a byte compare (`TrackerServ.cpp:1905-1917`).

### 2.3 Client→tracker listing (TCP 5498/5497)

`ProcessClients` (`old:562`, `new:648`) accepts connections on both TCP ports. Once 6 bytes are available the client must send the handshake:

```
Uint32 'HTRK'   (0x4854524B)
Uint16 version  (== 1)
```

If it matches, the tracker echoes the same 6 bytes back, increments `mListingCounter`, and calls `SendServerList` (`old:632-636`, `new:722-727`). Any other value → `Disconnect()`.

`SendServerList` (`old:1064`, `new:1055`) streams the whole list as one or more "type 1" messages, chunked at an 8192-byte buffer:

```
Uint16 type = 1
Uint16 size          // bytes of payload following the 4-byte header
Uint16 totalCount    // total servers on this tracker
Uint16 count         // servers in *this* message
(repeat count times):
  Uint32 address     // server IP (network order)
  Uint16 port
  Uint16 userCount
  Uint16 flags
  pstring name
  pstring desc
```

Note `totalCount`/`count` are stored in a `Uint32` slot in the buffer but written as 16-bit (`old:1091-1093`, `new:1084-1086`). The client-side reader (`HotlineTasks.cpp` stage 4–5) reads `type`/`size` then treats the payload as `Uint16 total, Uint16 count, …`, so only a 16-bit server count is actually usable.

### 2.4 Name lookup (dead code)

`SendLookup` (`old:1136`, `new:1137`) is present in both — it emits `type 4` (found: address/port/userCount/flags/name/desc) or `type 5` (not found). **It is never called from either tracker** (verified by grep across all four source files; only the declaration and definition exist). No `type 2`/`type 3` request handling exists on the client path. Effectively the tracker only ever pushes the full list; there is no per-name query.

### 2.5 Keepalive and the "CrazyServer" quirk

- **Keepalive:** none. `myTran_KeepConnectionAlive = 500` belongs to the client↔server protocol, not the tracker. Tracker liveness is purely the registration timeout (§3.3). There is no heartbeat in either direction.
- **CrazyServer:** `myTran_CrazyServer = 127` (`HotlineClientServerCommon.h:35`) is a **client→server** transaction, implemented by `CMyCrazyServerTask` in the client (`Client/Source/HotlineTasks.cpp:5537`, description "Set Crazy Server ;-)"). It is a server (not tracker) feature and is absent from both trackers. No "CrazyServer" logic exists anywhere in `Tracker/`.
- **Incomplete v2 handshake (client side):** the client's `CMyGetTrackServListTask` has a `#if NEW_TRACKSERV` block that would send `"HTRK\0\2"` plus a 32-byte login + 32-byte password and expect version `2` in the ack (`HotlineTasks.cpp:5940-5960, 6020-6064`). `NEW_TRACKSERV` is never defined anywhere in the tree, so the client uses v1 (`HTRK\0\1`, no credentials) — and **neither tracker ever checks for a login/password or returns version 2**. The "tracker login/password on connect" feature is a half-implemented dead path.

---

## 3. Tracker architecture narrative

**Threading: none.** Both trackers are single-threaded, event-driven applications on the AppWarrior `CApplication`/`UTransport` framework. `main()` initializes `UTransport`, constructs the app, `StartUp()`, `Run()` (`old:19-39`, `new:30-52`). Socket activity arrives as messages (`msg_DataArrived`, `msg_ConnectionRequest`, `msg_ConnectionEstablished`, …) dispatched through `HandleMessage`/`MessageHandler`; the UDP registration socket is routed to `ProcessRegister` and the TCP listeners to `ProcessClients` (`old:338-356`, `new:253-268`). There is no `std::thread`/thread pool and no per-connection worker.

**Connection handling.** `ProcessClients` first drains `Accept()` on 5498 then 5497, wrapping each accepted transport in an `SMyClient{tpt, isEstablished}` added to `mClientList`; on a connect it resets the error counter (`old:579-612`, `new:659-691`). It then iterates the client list, removing closed transports, and for each not-yet-established client waits for ≥6 bytes to validate the `HTRK`/v1 handshake before streaming the list. Each client is served inline; a slow client blocks only its own non-blocking receive check.

**Server list storage.** Registered servers are held as an array of `SMyServerInfo*` pointers in a relocatable handle `THdl mServerList` (grown 1024 pointers at a time). `SMyServerInfo` (`TrackerServ.h:19-29`) is a packed, variable-length record:

```c
struct SMyServerInfo {
  Uint32 nameCRC, timeStamp, address, passID;
  Uint16 port, userCount, flags;
  Uint8  data[];        // pstring name, pstring desc, (+ pstring pass in new)
};
```

The array is kept **sorted by `nameCRC`** for binary search (`FindServerByName`, `old:926+`); insertions and CRC-changing updates memmove the pointer array to preserve order. The new tracker additionally appends the registration password to `data[]` (`NewServerInfo`, `new:1521-1541`) though it is never transmitted. Address lookups are a linear scan (`FindServerByAddress`).

**Per-IP quota.** A separate `CPtrList<SMyServerPerIP>` (`{Uint32 address; Uint32 count;}`) counts registered servers per source IP to enforce `mMaxServersPerIP` (0 = unlimited), rebuilt whenever that setting changes (`BuildServerPerIPList`, `old:1532+`).

**User-count bookkeeping.** `mUserCount` is a single aggregate; on update the delta between old and new `userCount` is added/subtracted (`old:806-812`), on removal the whole entry's count is subtracted.

**Dead-server cleanup.** A repeating timer (`mRemoveOldTimer->Start(300000, kRepeatingTimer)` — every 5 minutes, `old:318`, `new` equivalent in `CMyApplication`) calls `RemoveOldServers`, which walks the list and deletes any entry whose `timeStamp` is older than 660 s (old) / 1860 s (new), decrementing `mUserCount` and the per-IP count (`old:850-921`, `new:464-521`). There is no explicit unregister path (`type 2` ignored), so the timeout is the only reaper.

**Failure resilience.** An error counter (`mErrorCount`) aborts to `ResetServer()` after >100 transport errors; `mResetCount` aborts the app after >10 resets (`old:429-472`).

**New-tracker management layer.** In v1.3, `CMyApplication::ReadTrackers()` scans the program folder for visible subfolders and instantiates one `CMyTracker` per folder (`new:2417+`); each folder holds `Prefs` (flattened: version, window visibility, 3 short rects, 28 reserved bytes, `maxServersPerIP`, `opts`, 4 IP bytes — see `new:1584-1703` and the 44-byte `Prefs` hexdump `0001 0001 …`), `Login` (`'logn'/'HTLS'`, version + count + `SMyLoginInfo` records, format documented in `Resources/LoginDatabaseFormat.txt`), and `Banned` (`'bann'/'HTLS'`). A toolbar lists/edits/starts/stops each tracker and shows aggregate "global stats."

---

## 4. FileFinder spec and modernization verdict

**What it actually is.** `Apps/FileFinder/Sources/FileFinder.cpp` (752 lines) is a **local filesystem file-browse/search dialog demo**, not a Hotline network utility. Its header comment and `ReadMeFileFinder.html` both describe the "Open Modules – version 0.1 – Demo Release" requirements: show current folder, jump to parent (+ "up"), back/forward history, favorite folders, list folder contents, type a filename/path/pattern, deep search with full paths, and open a file via the OS.

**Implementation.** Built on the external **XSP** framework (`#include "XSP_Core.h" / XSP_File.h / XSP_GUI.h / XSP_Codecs.h / CreateSkin.h`), using `ref_obj`, `refc<>`, `FileName`, `DialogWindow`, `ComboListView`, `ListView`, `ButtonView`, `SkinBuilder`, `EventLoop`. Core classes:
- `FileDialogStorage` — persistent state: `selectedFile`, `parentFolder`, `favoriteFolders`, `filters`.
- `FileFindDialog` — the dialog window; `RepopulateContent()` lists the folder, with folders first then files (`:555-598`); a non-empty filter triggers a **recursive** search via `FileFind(folderName, /*recursive=*/true, FileFilterPredicate(fp), flist)` (`:568-584`). `DialogEventSelection` dives into folders or launches files through the OS; `DialogEventBack/Forward/UpFolder/Filter` manage history and navigation.
- `MySimpleApp` / `MainEntryPoint()` — builds a skin, opens a 520×348 "Find files" window, runs the XSP event loop (`:607-752`). A `#if DEBUG` block runs framework unit tests (`String`, `RegExp`, `LocalTime`, `FileName`, `WinMgr`) before launch.

**Protocol transactions:** **none.** Grep for `tran`, `socket`, `transport`, `TCP`, `UDP`, `login`, `connect`, `server`, `port`, `protocol` finds only an unrelated `ReportError` call (`:671`). It sends nothing to any server; it enumerates the local disk.

**Build matrix.** `Makefiles/Makefile.gcc`, `Makefiles/Makefile.Darwin` (X11), `mingw_mak/gcc-mingw32.mak` (MinGW), `MSVC_Proj/FileFinder_MSVC.dsp`. The include paths point outside the checkout (`../../../Core/Includes`, `../../../GUI/Includes`, `../../../File/Includes`, `../../../Codecs/Includes`, `../../../../Libs/regexpp2/include`), i.e. the OpenSprings/XSP tree, which is **not present** in `legacy/`.

**Verdict.** Not a Hotline component at all — it is an OpenSprings framework sample that leaked into the tree. No protocol relevance, no network code, and it cannot be built from this checkout because the XSP dependency is missing. Modernization value is nil (it is a UI demo); discard or keep only as a historical XSP usage example.

---

## 5. Rez2Dat spec and verdict

**Purpose.** `Apps/Rez2Dat/Rez2Dat.cpp` (1187 lines, Mac Carbon — includes `AWHeaders(M-Carbon).h` and `<Resources.h>`) migrates resources between three stores: the **Mac resource fork** (`MacResourceFile`), the runtime **`HL_DatFile`** container, and loose disk files. Its companion `carb.r` is **empty (0 bytes)**.

**Core mapping.** A static `sToTransfer[]` table (kept "alphabetically sorted on field 3") enumerates ~693 transfers between a source `(rty, rid)` and a destination `(dty, did)`:

| Source type | Count | Dest type | Meaning |
|---|---|---|---|
| `cicn` | 647 | `ICON` | color icons, IDs 128–217, 1000–1003, 1250–1251, 14068–14072, 1968, … |
| `EMSG` | 7 | `EMSG` | error-message strings (IDs 1,3,4,6,8,10,19) |
| `GIFf` | 1 | `GIFf` | banner (non-movie), ID 128 |
| `MooV` | 1 | `MooV` | banner movie, ID 128 |
| `MSGB` | 28 | `MSGB` | message-box resources |
| `PICT` | 1 | `PIXM` | splash picture → pixmap |
| `snd ` | 10 | `SOUN` | sounds |
| `ppat` | 1 | `ppat` | pattern |
| `harc` | 1 | `harc` | Hotline archive resource |

`Rez2Dat(folder, srcDat, srcRez, dstDat)` opens the old `.dat`, the Mac resource file, and a new `.dat`, then `Transfer`s each table entry (`:1068-1094`). `Transfer` copies (converting `cicn→ICON`, `PICT→PIXM`, `snd→SOUN`) into the `HL_DatFile`.

**Output/input format.** `HL_DatFile` is the AppWarrior "Rez" container — the `.dat` files begin with the 4-byte magic **`AWRZ`** + version `0001` (see `hls19.dat`/`hlc19.dat`/`hltracker.dat` hexdumps). `ExportToFile` writes items out as `"<id>_<resAttr>.<TYPE>"` files; `ImportFromFiles` pulls loose files (`*.HLpixmap`, `.gif`, `.mov`) back in. As committed, `StartUp()` runs only `ImportFromFiles("\pImages","\phlci19.dat")` and `ImportFromFiles("\pImages","\phlc19.dat")`, injecting ISP icons (`msgBoard`→ICON 429, `Securiphone`→432, `HL-ISP2`→433, `Xsprings`→434); the real `Rez2Dat(…)` and `ExportFromDat(…)` calls are commented out (`:1156-1166`).

**Consumers of the output.** The `.dat` files are the shipped runtime resources, loaded via `URez::AddProgramFileToSearchChain`:
- Server: `Data//hls19.dat` — `Server/Source/HotlineServ.cpp:72`, `ServerOLD/Source/HotlineServ.cpp:70`.
- Client: `Data//hlc19.dat` — `Client/Source/Hotline.cpp:7616`.
- Tracker: `hltracker.dat` — `Tracker/New Tracker/Sources/TrackerServ.cpp:11`.

**Verdict.** Obsolete developer tooling. It only compiles against the Mac Carbon AppWarrior variant (`AWHeaders(M-Carbon).h`, whose library header is **not** in this checkout — only the Windows `AWHeaders(W*)` variants are present under `AppWarrior/Libraries`), so it cannot be built here. Its purpose is fulfilled by the already-committed `.dat` binaries; no runtime depends on Rez2Dat. Modern relevance is archival only (it documents how `.dat` containers were assembled from Mac resources).

---

## 6. Binary asset inventory (`Apps/Images/`)

All `.dat` files are `AWRZ` containers; all `.gif/.png/.mov` are banner/splash/icon media; `.iconsuite` files are empty placeholders.

| Path | Format | Size | Purpose |
|---|---|---|---|
| `19Banner.gif` | GIF89a 468×60 | 10,874 | Hotline v1.9 default server banner (static) |
| `19Banner.png` | PNG 468×60 | 14,166 | Banner render/repack source |
| `19Banner.mov` | QuickTime (unoptimized) | 14,795 | Animated banner (movie) variant |
| `19BannerISP.gif` / `.mov` / `.old.gif` / `.old.mov` | GIF 468×60 / QuickTime | 21,983 / 19,997 / 22,277 / 21,298 | ISP-branded banner + older revisions |
| `185Banner.gif` / `.mov` | GIF 468×60 / QuickTime (fast-start) | 5,310 / 68,555 | Banner set (v1.85 era?) |
| `GLbanner.gif` | GIF89a 468×60 | 8,257 | "GL" banner (GLoarbLine?) |
| `19Splash.gif` (ISP) | GIF 218×335 | 44,625 | Splash screen (ISP) |
| `19Splash.png` | PNG 218×335 | 52,543 | Splash render source |
| `19Splash.HLPixmap` | `AWPX` pixmap 218×335 | 292,184 | Splash as AppWarrior pixmap (PIXM 130) |
| `19Hotline.ico` | Windows ICO, 2 icons | 1,078 | Client app icon |
| `19hotlineserv.ico` | Windows ICO, 2 icons | 1,078 | Server app icon |
| `19hotlineserv-a.iconsuite.icns` | Mac `ICN#` icon | 42,801 | Server Mac icon |
| `19Hotline.iconsuite`, `19Hotlineserv.iconsuite` | empty (0 B) | 0 | Placeholder suite dirs |
| `HL-ISP2.gif` | GIF 16×16 | 586 | ISP icon (→ ICON 433) |
| `messageboard-icon.gif` | GIF 16×16 | 588 | Message-board icon (→ ICON 429) |
| `securiphone-icon.gif` | GIF 15×15 | 910 | Securiphone icon (→ ICON 432) |
| `xprings-icon.gif` | GIF 16×16 | 165 | Xsprings icon (→ ICON 434) |
| `hls19.dat` | `AWRZ` v1 container | 13,000 | Server runtime resources |
| `hlc19.dat` | `AWRZ` v1 container | 860,584 | Client runtime resources (Images copy) |
| `hlci19.dat` | `AWRZ` v1 container | 876,904 | Client "ISP" variant resources |

**Protocol-relevant vs cosmetic.** The **banner** is protocol-relevant: the server transmits it to clients via `myTran_ServerBanner`/`myField_ServerBanner` + `myField_ServerBannerType` (`HotlineClientServerCommon.h:29,123,152`), and the stored formats are **GIF (468×60)** or **QuickTime movie** — matching the `GIFf`/`MooV` resources in the `.dat`. Everything else (splash, icons, sounds, EMSG strings) is cosmetic UI/application resource.

---

## 7. Data file inventory (News/, Data/, Users/)

### 7.1 News (`ServerOLD/News/`)

| Path | Format | Size | Purpose |
|---|---|---|---|
| `xc.hnz` | `HLNZ` (v2) news DB | 5,446 | Default news category "xc" (a Hotline news archive) |
| `news free/news.lists.filters.hnz` | `HLNZ` news DB | 1,034 | Default Usenet-style group `news.lists.filters` |
| `news free/microsoft.public.z.newsmon.hnz` | `HLNZ` news DB | 1,034 | Default Usenet-style group `microsoft.public.z.newsmon` |

The `.hnz` magic is **`HLNZ`** and the format is produced/consumed by the server's NewsSynch utility (`ServerOLD/Utilities/NewsSynch/Sources/CNewsDatabase_v2.cpp` / `_v3.cpp`, which `CreateFile('HLNZ','HTLS')` and verify `head->type == TB('HLNZ')`). The two "news free" files are near-identical stubs (same size; differ only at byte 9 — the embedded per-group hash), i.e. **default news content** rather than real message archives. `xc.hnz` embeds its own filename (`"xc.hnz"` at offset 0x58) and is the single default news category.

### 7.2 Server/Client Data

| Path | Format | Size | Purpose |
|---|---|---|---|
| `Server/Data/hls19.dat` | `AWRZ` v1 | 13,000 | Server runtime resources (identical to `Images/hls19.dat`) |
| `ServerOLD/Data/hls19.dat` | `AWRZ` v1 | 13,000 | Same server resource container |
| `ServerOLD/Data/rand_seed` | 256 B binary | 256 | Random-number entropy seed |
| `Client/Data/hlc19.dat` | `AWRZ` v1 | **972,130** | Client runtime resources — **differs** from `Images/hlc19.dat` (860,584 B), i.e. a later/independent build |

`hls19.dat` header shows 5 resource-type slots (`0000 0005` at 0x20) with the `ER/TR/RZ/ME/FS` table and human-readable error strings (`"alertcaution 1002+"`, `"stop 303++"`, `"op…"`). `hlc19.dat` carries a source-file manifest (`"UDragAndDrop().dat"`, `"UError().dat"`, `"UFileSys().dat"`, `"UMemory().dat"`, `"UR…"` …) listing the merged module `.dat`s — confirming it is an assembled resource container, not a hand-authored file.

### 7.3 Default user databases (`ServerOLD/Users/`)

| Path | Size | Format |
|---|---|---|
| `Users/admin/UserData` | 734 | `SMyUserDataFile` record |
| `Users/guest/UserData` | 734 | `SMyUserDataFile` record |
| `Users/test/UserData` | 734 | `SMyUserDataFile` record |

Each is exactly 734 bytes — matching the packed `SMyUserDataFile` struct in `HotlineClientServerCommon.h:311-330` (`2+2+8+2+512+2+64+2+64+2+32+2+32 = 734`). The `admin` record begins `0001 0000 fff3 cfff ffff fe00 …` = version 1, icon 0, access words `0xfff3cfff`/`0xfffffe00` (a near-full privilege bitmap). The three files differ only at the name/alias/login/password/access offsets — i.e. **default seeded accounts** (admin/guest/test). The newer `Server/` tree has no `Users/` directory (accounts are created at runtime).

---

## 8. Modernization recommendations

1. **Reimplement the tracker as a spec, not a port.** The wire protocol is tiny and fully reverse-engineered here (§2): a UDP `type/port/userCount/flags/passID/name/desc/password` registration on 5499 and a `HTRK`/v1 + chunked "type 1" list push on 5498/5497. A small Go/Rust/Node service can replace both trackers in a few hundred lines; no C++/AppWarrior dependency is needed.
2. **Fix the protocol defects rather than replicate them.** Drop the dead `SendLookup` (type 4/5) or actually wire a name query; make the 16-bit `totalCount`/`count` fields 32-bit for >65535 servers; define a real unregister (`type 2`) instead of relying on the 11/31-minute timeout; add an explicit keepalive if liveness matters.
3. **Do not resurrect `NEW_TRACKSERV` (v2 credentials) as-is.** It is half-implemented in the client and unsupported by either tracker. If tracker authentication is desired, design it cleanly (e.g. TLS + token) rather than the 32-byte padded login/password handshake stub.
4. **Treat the `.dat` (AWRZ) containers as opaque legacy assets.** They are the shipped resources; a modern build should ship the underlying GIF/PNG/ICO/pixmap files directly (most already exist under `Apps/Images/`) and drop the AWRZ packing. `Rez2Dat` and `carb.r` are archival only.
5. **FileFinder is out of scope for any Hotline rebuild.** It is an XSP demo with no network code and a missing external dependency; archive it under "historical samples" and remove it from any build graph.
6. **Migrate configuration off the legacy file formats.** The new tracker's binary `Prefs`/`Login`/`Banned` (and the old tracker's plain-text `Passwords`/`Banned`/`TrackerIP`/`ServersPerIP`) should become a simple config file + SQLite or in-memory tables; `LoginDatabaseFormat.txt` is sufficient documentation to migrate the login DB.
7. **Preserve the assets inventory (§6) as the canonical banner/icon corpus.** The 468×60 GIF and QuickTime banner formats are the protocol-visible server banner; keep them available for a compatibility mode.

---

## 9. Risks / unknowns

- **`legacy/` checkout is partial.** The XSP framework (FileFinder) and the Mac Carbon AppWarrior library variant (Rez2Dat) are absent, so both utilities cannot be built/verified here. FileFinder and Rez2Dat behavior is inferred from source only.
- **`SendLookup` is dead code** in both trackers, yet the client reader (`HotlineTasks.cpp`) only understands list pushes; the type 4/5 lookup was never completed end-to-end. Its intended requester is unknown.
- **Byte-order/sub-format subtleties.** The 16-bit `totalCount`/`count` inside a `Uint32` slot, and the `HTRK`/version field, were read from source; I did not capture live packets. A protocol capture against a running tracker would confirm endianness/padding.
- **`Client/Data/hlc19.dat` (972,130 B) ≠ `Images/hlc19.dat` (860,584 B).** The runtime client ships a different resource build than the `Images/` working copy; which one is canonical is undetermined.
- **"CrazyServer" scope.** `myTran_CrazyServer` is confirmed client/server only and absent from the tracker; its exact server-side effect was not analyzed (outside this tracker/utilities scope).
- **Tracker "HTTP" listener (5497).** Confirmed binary (no HTTP parsing) in both trackers; the intended HTTP facade is presumed (client HTTP-tunnel fallback) but the client's tunnel framing was not traced in this audit.
- **`harc` resource** (Hotline archive) is referenced by Rez2Dat but its consumer path was not traced; it may be a legacy folder-archive codec no longer used.
