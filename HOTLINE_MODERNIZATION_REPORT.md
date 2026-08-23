# Hotline — Archaeological & Architectural Audit / Modernization Report

**Phase 1 deliverable (Archaeology).** This report records the complete repository-wide audit of the
legacy **Hotline** source tree (historical provenance: the repository formerly named GLoarbLine;
throughout this project the application and protocol are referred to as **Hotline**). It is written
**before any broad modernization work** and is intended to be reviewed to select the first
modernization implementation phase.

**Source of truth:** `legacy/` — a read-only checkout of `https://github.com/Schala/Gloarbline`
(773 files, ≈274k LOC: 211,882 C++, 38,154 headers, 20,761 C, 3,095 Rez resource source).

> **Status update:** implementation has begun. The controlling modernization charter is now
> **`AGENTS.md`** (it corrects this report's AppWarrior recommendation — AppWarrior is *preserved
> and modernized*, not dismantled; see §25 Addendum). Implementation progress is tracked in
> `docs/modernization-ledger.md`.

**Method:** six independent read-only sub-audits (reports kept in `audit/01…06-*.md`), plus
first-hand verification of every protocol-critical code path by the author of this report. Every
claim below is evidence-based with `file:line` citations against `legacy/`; anything not settled by
the source is marked **[UNKNOWN]**.

**Recommendation classifications used throughout:**

| Class | Meaning |
|---|---|
| **preserve-semantics-rewrite** | Behavior/bytes must survive; implement cleanly in modern C++23 |
| **replace-with-stdlib** | C++ standard library already provides the facility |
| **replace-with-modern-platform** | Use a contemporary OS/library facility |
| **remove-entirely** | The abstraction should disappear; nothing replaces it |
| **retain-temporarily** | Frozen reader/legacy-asset support during migration, deleted afterwards |

---

## 0. Executive summary

The legacy tree is the **Hotline 1.9-era client, server and tracker** (2003 Hotsprings GPL release,
as imported and lightly cleaned by the repository author in 2016/2023), built on **AppWarrior**, a
1996–2003 cross-platform C++ runtime whose center of gravity is the classic Macintosh (Pascal
strings, relocatable `Handle`s, Carbon, QuickDraw-style graphics, MacRoman text, "seconds since
1904"), with a parallel Windows implementation of the same headers, and a cooperative, message-driven,
**thread-free** event architecture shared by all applications.

Key facts that shape the whole modernization:

1. **The wire protocol is the canonical Hotline transaction protocol.** Connection handshake
   `'TRTP'`/`'HOTL'`/version 1/sub-version 2 (or 3 for transfers), 20-byte big-endian transaction
   header (`flag, isReply, type, id, error, totalSize, dataSize`), and field-parameter lists
   (`count:u16` + `{id:u16, size:u16, data}`). This byte layout matches the publicly documented
   Hotline protocol ([hlwiki Protocol page](https://hlwiki.com/index.php?title=Protocol)) and is what
   interoperable Hotline implementations (hxd etc.) speak. *(Note: sub-audit `06-protocol.md`
   conjectured this framing was non-standard based on an external recollection; that recollection
   was incorrect — the canonical protocol documentation confirms this tree byte-for-byte. The
   framing is standard Hotline.)*
2. **Application-layer protocol constants** — transactions 100–500, field IDs 100–337, 55 privilege
   bits in a 64-bit mask — live in `Apps/Common Files/HotlineClientServerCommon.h` and must be
   preserved verbatim (including two historical field-ID collisions).
3. **The server implements only the legacy login path.** The "HOPE" encrypted login (session key +
   HMAC-SHA1/MD5 + Blowfish-OFB64 + `HLCrypt` key schedule) is fully implemented on the **client**
   but dead on the server; the server compares a bitwise-NOT-scrambled login and a plaintext
   password. All crypto primitives exist in-tree and are protocol-relevant for compatibility with
   older clients/servers.
4. **No threads anywhere.** Client, server and tracker are cooperative message-pump applications
   (`UApplication`/`UMessageSys`) with explicit state-machine "tasks" instead of threads; the server
   is a single-threaded poll loop over a connection list.
5. **QuickTime and its entire support framework** (a 194-file QuickTime DataHandler component
   embedding a second, ~24.5k-LOC duplicate mini-AppWarrior) power only client-side movie
   playback/streaming and can be removed wholesale.
6. **The tree already documents its own legacy:** `ServerOLD` (vs `Server`), `Old Tracker` (vs
   `New Tracker`), `*.original.cpp` files, `.dat` resource containers built from Mac resource forks
   by the now-unbuildable `Rez2Dat` tool, and a `FileFinder` that is an unrelated XSP-framework demo
   with a missing external dependency.
7. **Security baseline is poor but bounded:** 2 MB transaction caps and bounds-checked field parsing
   coexist with stack overflows, unbounded allocations from network-supplied sizes, plaintext
   password storage, and advisory-only client-side privilege checks. A catalog of 29 client-side and
   10 protocol-layer hazards is included below (§17) and must drive the rewrite.

The recommended end state is a clean C++23 codebase organized as `hotline::protocol` /
`hotline::net` / `hotline::core` / `hotline::client` / `hotline::server` / `hotline::tracker`, with
a UI that is explicitly not a port of AppWarrior, and with every wire byte proven by golden tests
captured from this code.

---

## 1. Repository / component map

```
legacy/
├── AppWarrior/                  ~111k LOC — cross-platform app framework (Mac-centric, Win32 port)
│   ├── Headers/                 (103 headers) — ANSI.h, typedefs.h, containers, U* modules, C* views
│   ├── Source/                  platform-split by suffix: (M)ac 29 files / (W)indows 31 files
│   │   ├── Crypt/               HLBlowfish, HLMD5, HLSha1, HLRand, HLCrypt  (protocol crypto)
│   │   ├── Data/                containers, UFieldData (protocol codec), UText, UMemory, URez
│   │   ├── Files/               UFileSys  (MoreFiles-idiom filesystem layer)
│   │   ├── Graphics/            UGraphics/UPixmap/UIcon/URegion (QuickDraw-style, AWPX pixmaps)
│   │   ├── Hardware/            UTransport/UTransact (protocol transport), UHttpTransact, UNntpTransact
│   │   ├── Images/              vendored IJG libjpeg 6b (1998), GIF89a, PICT, BMP decoders
│   │   ├── Libs/QTDataHandler/  194 files — QuickTime 'dhlr'/'url '/'htln' component + embedded
│   │   │                        "BigRedH" mini-framework (~24.5k LOC duplicate of AppWarrior)
│   │   ├── Libs/UUID/           vendored DCE UUID implementation
│   │   ├── Misc/                ANSI.cpp, UDateTime, UMath, UError, UGUID, UApplication, UTimer
│   │   ├── User Interface/      CWindow, CWizard, MsgBox
│   │   └── Views/               CView + widget/tree/list hierarchy (header-only templates)
│   ├── Error Msgs/              UError(N) string catalogs + compiled 'IVA1' .dat files
│   ├── Libraries/               umbrella prefix-header shims (build-config matrix)
│   └── Resources/               empty Resource.h
│
└── Apps/
    ├── Common Files/            HotlineClientServerCommon.h (protocol constants),
    │                            HotlineArchiveDecoder, HotlineFolderDownload, 'harc' archive structs
    ├── Client/                  44.2k LOC — Hotline Connect client (CMyApplication god-object)
    │   ├── Source/              Hotline.cpp (9.7k), HotlineWindows.cpp (7.6k), HotlineTasks.cpp (6.6k),
    │   │                        HotlineNews.cpp (3.8k), HotlineViews.cpp (3.3k), HotlineTracker.cpp,
    │   │                        HotlineAdmInSpector.cpp (duplicate of HotlineTracker.cpp), Admin/
    │   └── Data/hlc19.dat       972 KB AWRZ resource container; Resources/Win/hotline.rc + icons
    ├── Server/                  30k LOC — current server (HotlineServ*, HotlineServTrans.cpp 5.3k,
    │                            HotlineServNewsDatabase.cpp, HotlineServIDTranslate.cpp)
    ├── ServerOLD/               historical server + NewsSynch utility + seeded Users/{admin,guest,test}
    ├── Tracker/Old Tracker/     v1.2.0 tracker (single instance, text config)
    ├── Tracker/New Tracker/     v1.3.0 tracker (multi-tracker management rewrite; same wire protocol)
    ├── FileFinder/              752 LOC XSP-framework file-find dialog demo (missing dependency)
    ├── Rez2Dat/                 1,187 LOC Mac Carbon resource-fork→AWRZ .dat build tool (unbuildable)
    └── Images/                  banners/splash/icons (GIF/PNG/MOV/AWPX) + compiled .dat containers
```

Line-count details, per-file roles, and platform splits are in `audit/01…06-*.md`.

---

## 2. Application targets discovered

| Target | LOC (src) | What it is | Modern fate |
|---|---|---|---|
| **Hotline Client** ("Hotline Connect" 1.9.7.2) | 44,207 | Chat/file/news/tracker/user/admin GUI client | Rebuild: `hotline::client` core + new UI |
| **Hotline Server** (1.9.x) | ~30k | The BBS server: chat, files, news, users, agreement/banner, tracker registration, HTTP port listener | Rebuild: `hotline::server` core (+ optional headless/config tool) |
| **Hotline ServerOLD** | ~27k | Historical predecessor of Server (kept in-tree) | Reference only; do not compile |
| **Tracker (Old 1.2.0 / New 1.3.0)** | 2.9k / 4.3k | UDP/TCP server-directory tracker; tiny bespoke protocol (not `myTran_*`) | Rebuild: `hotline::tracker` (~a few hundred lines of C++23) |
| **NewsSynch** (Server + ServerOLD utilities) | ~10k total | NNTP↔news-DB synchronization tool | Analyze; likely drop or replace with a script |
| **FileFinder** | 752 | XSP-framework local file dialog **demo** — not a Hotline component; missing XSP dependency | Remove from build graph; archive as historical sample |
| **Rez2Dat** | 1,187 | Mac Carbon build tool converting resource forks → AWRZ `.dat` | Remove; assets already shipped as `.dat`/loose files |

The three live network targets (client, server, tracker) share one protocol layer and one set of
AppWarrior runtime services.

---

## 3. AppWarrior subsystem inventory

AppWarrior is a 1996–2003 cross-platform C++ runtime: a thin C++ veneer over the classic Mac
Toolbox with a parallel Win32 implementation (`(M)`/`(W)` file suffix convention; 113 files contain
platform `#if`s; 104 `WIN32` and 82 `MACINTOSH` conditional sites across the tree). The desired
outcome is **Hotline without AppWarrior**, not "AppWarrior C++23". Inventory with verdicts:

### 3.1 Runtime core (`audit/01-appwarrior-core.md` — full per-file table)

| Subsystem | Components | Consumers | Verdict |
|---|---|---|---|
| Headers | `ANSI.h` (hand-rolled libc: malloc/str*/pstr*/qsort/sprintf…), `typedefs.h` (`Uint32` family, `TB()`/`FB()` endian helpers, global `operator new`), `MoreTypes.h` (message/event vocabulary, `TMessageProc`), `GrafTypes.h` (`SPoint/SRect/SColor(48-bit)`), `ImageTypes.h` (GIF/BMP on-disk headers) | total (e.g. `Uint32` ≈6,272 matches) | replace-with-stdlib / modern platform; see §4 of core report for the per-declaration `ANSI.h` migration plan |
| Containers | `CBoolArray` (bit-packed, dead), `CLinkedList` (intrusive singly-linked), `CPtrList` (workhorse pointer array, ~307 app uses), `CPtrTree` (flat level-encoded pointer tree), `UIDVarArray` (ID→blob map, IVA1 catalog reader), `UBitString` (LSB-first bit ops, dead) | §7 below | per-container, §7 |
| Memory | `UMemory` three-tier model (`TPtr` fixed / `THdl` relocatable handle / pool allocator); Knuth boundary-tag pool (`UMemory(alloc).cpp`); Mac `NewHandle/HLock/HPurge` + Win `GlobalAlloc(GMEM_MOVEABLE)` backends; `StHandleLocker/StPtr/StHdl` RAII | everything | remove-entirely — no relocatable-handle recreation (§6) |
| Text | `UText` (~861 app uses: ctype/case/compare/format over MacRoman byte buffers), `CFlatten`/`CUnflatten` (big-endian binary stream: P/W/L-strings, DateTimeStamp), `UMime` (ext/type↔MIME tables) | everything | UText → stdlib + explicit encoding layer; CFlatten → preserve-semantics-rewrite (wire/persisted layouts); UMime → modern |
| Time/math/err/GUID | `UTimer` (message-based ms timers), `UDateTime`/`SDateTimeStamp` (**wire timestamp** `{u16 year, u16 msecs, u32 seconds}`; Mac 1904-vs-Windows within-year divergence documented), `UMath` (LCG PRNG, 64-bit `SHuge` helpers, trig), `UError`/`SError` (thrown errors + EMSG catalogs), `UGUID` (vendored DCE UUID, Mac-heavy) | UDateTime ≈213, UError ≈38 app uses | <chrono>/<random>/std UUID; keep `SDateTimeStamp` wire codec; keep OS-error→app-error mapping tables |
| Crypto (`Source/Crypt/`) | `HLBlowfish` (OFB-64, zero IV), `HLMD5`/`HLSha1` (HMAC RFC 2104), `HLCrypt` (two-key HMAC schedule + `Perm*Key`), `HLRand` (HMAC-SHA1 DRBG w/ persisted 256-B seed) | client login; transport crypto hook | **preserve-semantics-rewrite** (highest priority) |
| Serialization | `UFieldData` — **the** protocol field codec (≈410 app matches); `UDigest` (Base64/UU/MD5); `UMemory::CRC` (CCITT-32) & `AdlerSum`/`Checksum`/`PackIntegers` | everywhere in transactions | preserve-semantics-rewrite (codec), stdlib (digest/checksum kept as utilities) |
| Message/event | `UMessageSys` (priority queue), `UApplication` (global run loop), `CApplication` (app base), `UTimer`, `UProgramCleanup` | all apps (cooperative architecture) | replace-with-modern-platform (event loop of the new UI core) |
| Filesystem | `UFileSys` (MoreFiles idiom: type/creator codes, aliases, resource forks, `FlattenRef 'FSrf'`, **resumable flatten/unflatten** used by transfers, temp-file classes) | ~131 app uses | std::filesystem + preserved wire codecs (FILP/RFLT/listing) |
| Resources | `URez` — **not** classic resource-fork format; reads a custom flat **`'AWRZ'`** container (big-endian map) with `'%_nm'` name tables; search chains | EMSG catalogs, icons, banners, splash | retain-temporarily (frozen AWRZ reader) → bespoke asset readers |
| Debug | `UDebug`, `DebugBreak` macros | framework internals | remove-entirely |

### 3.2 UI / graphics / media (`audit/02-appwarrior-ui-graphics.md`)

- **Graphics:** QuickDraw-style `UGraphics` (pen/ink/clip/origin, `CopyPixels*`) over Carbon
  QuickDraw (5,254 LOC) / Win32 GDI (3,229 LOC); `SPixmap`/`SExtPixmap` with a fixed big-endian
  flattened **`'AWPX'`** format; 48-bit `SColor`; `URegion` algebra. Verdict:
  preserve-semantics-rewrite of the surface/geometry contract over a modern raster stack (Skia/Cairo
  or the chosen UI toolkit); both native backends are dead.
- **Image decoders:** vendored **IJG libjpeg 6b (1998)**, hand-written GIF89a (single frame +
  view-driven animation), Apple PICT v1/v2 interpreter (with JPEG-in-PICT), BMP. Only the **Client**
  decodes images; server/tracker forward bytes. Verdict: modern image lib; drop PICT (or keep as an
  isolated reader for legacy assets).
- **UI framework:** `CView` + handler-pattern containers (`CSingle/CMultiViewContainer`), `CWindow`
  (no numeric ID; numeric identity on views via `viewID_*`), immediate-mode drawing with update
  rects, hit-testing marshalled as `SHitMsgData` → `CApplication::WindowHit`, `CScrollerView`
  wrapping one content child, `CTabbedView`, header-only `CTreeView<T>` (1,682 LOC) and
  `CGeneralListView<T>` (499 LOC), `MsgBox`, dead `CWizard`. A full per-app consumer table is in
  `audit/02` §3/§10. Verdict: replace-with-modern — the semantic contract (view tree, item-provider
  list/tree, selection, drag flavors) is the mapping target for a modern retained UI (Qt/QtQuick, a
  web UI, or a Skia canvas layer).
- **Application shell:** `UApplication(M)` (25.1k LOC incl. Apple Events) / `(W)` (10.4k LOC),
  `UWindow(M/W)`, `CApplication`, `UProgramCleanup`, `UUserInterface`. Verdict: modern event
  loop/toolkit; the `CApplication` shape (app base, window list, hit dispatch, key commands) is a
  useful blueprint, not a port.
- **Hardware/services:** `USound` (beep/ADPCM assets), `UKeyboard`/`UMouse`, `UDragAndDrop`
  (flavors incl. Hotline-specific `'HLFN'/'HLFP'/'HLUI'/'TYCO'/'ISDR'`), `UClipboard` (zero direct
  app use), `UTooltip`, `UExternalApp` (launch/associations), `UOleAutomation` (**zero app
  consumers** — remove), `UService` (NT service, **zero app consumers** — remove), `UHttpTransact`
  (HTTP 1.0/1.1 client used for banner/agreement/update downloads and server `LoadUrl`),
  `UNntpTransact` (NNTP, used by NewsSynch), `UTransport::LaunchURL`. Verdict: modern equivalents;
  OLE/Service disappear.

### 3.3 QuickTime DataHandler sub-framework

`AppWarrior/Source/Libs/QTDataHandler/` (194 files, ~27.7k LOC) is a QuickTime **Data Handler
component** (`ComponentDescription {'dhlr','url ','htln'}`) teaching QuickTime to read media over
Hotline's `hotline://` URLs. It embeds a complete second AppWarrior ("BigRedH", ~24.5k LOC: its own
Networking/Threads/Streams/Persistence/Graphics/`CString`). Its only consumers are the client's
`CQuickTimeView` streaming path. **Verdict: remove-entirely after QuickTime removal** (§11); do not
confuse `HL_BigRedH` classes with the main AppWarrior.

### 3.4 Error catalogs & resources

`AppWarrior/Error Msgs/U<Module>(N)` + `.dat`: numbered string catalogs compiled into **`'IVA1'`**
flattened `UIDVarArray` blobs, loaded through `URez` (`'EMSG'` type) on Windows and classic
`GetResource` on Mac. Verdict: re-author as UTF-8 string resources; frozen IVA1 reader only for
legacy `.dat` extraction.

---

## 4. Hotline protocol implementation locations

| Layer | Location | Notes |
|---|---|---|
| Protocol constants (transactions 100–500, fields 100–337, 55 privileges, user-data struct) | `Apps/Common Files/HotlineClientServerCommon.h` | single source of truth; preserve verbatim incl. collisions `myField_Visible=112`/`myField_UserFlags=112` and `myField_number=114`/`myField_ChatID=114`, `myField_Visible(enum)=117`/`myField_IconId=117` |
| Transaction framing + establish handshake | `AppWarrior/Source/Hardware/UTransact.cpp` (`STranHdr`, `'TRTP'` handshake, `_gTransactMaxReceiveSize` = 2 MB framework default — **the server overrides it to 512 KB** at `HotlineServ.cpp:187` "prevent attacks"; default session timeout 200 s) | canonical Hotline framing (hlwiki-confirmed) |
| Transport (TCP/UDP socket layer) | `AppWarrior/Source/Hardware/UTransport.cpp` + `URegularTransport(M/W).cpp` (Winsock `WSAAsyncSelect` / Mac OpenTransport) | message-driven: `msg_DataArrived/msg_NewConnection/msg_ConnectionClosed` |
| Field codec | `AppWarrior/Source/Data/UFieldData.cpp` | count + {id,size,data}, big-endian, no padding |
| Binary stream codec | `AppWarrior/Headers/CFlatten.h` (P/W/L-strings, rects, timestamps) | used by tracker/file/DB formats |
| Encrypted transaction mode | `UTransact.cpp` `_TNSendTran`/`_TNProcessIncomingData` + `Source/Crypt/*` (`HLCrypt`, `HLBlowfish`, `HLMD5`, `HLSha1`) | header-encryption + `flag` key permutation (2/7/13 re-roll quirk) |
| Client protocol behavior | `Apps/Client/Source/HotlineTasks.cpp` (send paths, 100+ task classes), `Hotline.cpp` `ProcessIncomingData`/`ProcessTran_*` (push handlers) | full transaction catalog in `audit/06` §3 |
| Server protocol behavior | `Apps/Server/Source/HotlineServTrans.cpp` (54 `ProcessTran_*` handlers), `HotlineServ.cpp` (dispatch `4890-5110`, accept/establish loop, transfer sessions) | full dispatch table in `audit/06` §3 |
| File-transfer sub-protocol | `HotlineTasks.cpp` (client), `HotlineServ.cpp` `5137-5560` (server): `'HTXF'` 16-byte header on `basePort+1/+3`; FILP flat-file stream; RFLT resume data (`UFileSys(W).cpp:2216-2232`); folder-download item stream + `dlFldrAction_*` commands | §16 |
| Archive container | `Apps/Common Files/HotlineArchiveStruct.h` + `HotlineArchiveDecoder.cpp` — `'harc'` + per-file `'zlib'`/`'raw '` compressed FILP payloads | used for client self-update resource |
| Tracker protocol | `Apps/Tracker/{Old,New} Tracker/Sources/TrackerServ.cpp` — UDP 5499 registration, TCP 5498/5497 `'HTRK'` v1 handshake + chunked "type 1" server lists | bespoke binary protocol, **not** `myTran_*` |
| News database format | `Apps/Server/Source/HotlineServNewsDatabase.cpp` + `.h` (article-list wire layout) | §16 |
| Auth | `Apps/Client/Source/HotlineTasks.cpp:1459-1650` (login task), `Apps/Server/Source/HotlineServTrans.cpp:1523-1924` (`ProcessTran_Login`) | §21/§17 |

**Protocol-relevant constants that must be preserved verbatim** (full numeric tables in
`audit/06-protocol.md`, "Protocol constants that must be preserved verbatim"): transaction IDs
100–500, field IDs 100–337, privilege IDs 0–54, `'TRTP'/'HOTL'/'HTXF'/'HTRK'/'FILP'/'RFLT'/'harc'/'AWRZ'/'IVA1'/'AWPX'/'HLNZ'/'HTLS'` magic values, client version integer **197**, server
`myField_Vers` 197, news flavor codes (`plain_text=1, jpeg=10, gif=11`).

---

## 5. Networking architecture

```
        client                            server                          tracker
    mTpt UTransact('HOTL',v2)        4 listeners: main/transfer/           UDP 5499 (registration)
        │  + transfer tpt 'HTXF'     HTTP main/HTTP transfer on           TCP 5498 + 5497 ('HTRK')
        ▼                             basePort/+1/+2/+3 (default 5500-5503)
  UTransport (message-driven socket layer; Winsock WSAAsyncSelect on Win, OpenTransport on Mac)
        ▼
  UTransact: STranHdr framing + session tasks + establish ('TRTP') + optional HLCrypt
        ▼
  UFieldData: field parameter lists   (server dispatch: ProcessTran_* per transaction ID)
        ▼
  application handlers                (chat/users/files/news/agreement/banner/tracker reg)
```

- **Boundaries that exist today:** transport → framing → codec → application handlers are already
  distinct modules (`UTransport` / `UTransact` / `UFieldData` / `ProcessTran_*`), but they are
  entangled through a global message pump, a global `gCrypt` pointer (`UTransact.cpp:7`), global
  `gApp` singletons, and `THdl`-based buffers.
- **Control/data split:** one control connection (transactions) + one transfer connection
  (`basePort+1`, or `+3` through the "HTTP tunnel" listener) identified by the 16-byte `'HTXF'`
  header with `refNum` routing (`type` 0=file, 1=folder, 2=banner; odd `refNum` = download, even =
  upload). The server binds four listeners at `basePort`, `+1`, `+2`, `+3` (defaults 5500–5503;
  `HotlineServ.cpp:243-269`).
- **HTTP facade:** the `+2`/`+3` "HTTP" listeners accept the *same* binary protocol (firewall-
  friendly ports, no HTTP parsing); `UHttpTransact` is separately a real HTTP 1.0/1.1 *client* used
  for agreement/banner/update downloads. The tracker's TCP 5497 "HTTP" listener likewise speaks the
  binary `'HTRK'` protocol.
- **Message vocabulary:** `msg_DataArrived=100`, `msg_NewConnection`, `msg_ConnectionClosed=103` …
  drive both apps; heavy receive work happens inside the message handler (`_TNProcessIncomingData`).
- **Session/state:** `STransact` (estab timer, per-task receive buffers, version tags),
  `STransactSession` (timeout timer, send/recv task pair), server-side `SMyConnect` list with
  per-client state (login/agree stages, lastPath, access mask, vers).

**Modern target** (illustrative, derived from actual responsibilities):

```
socket/transport (asio or equivalent; RAII, deterministic shutdown)
   → connection (establish handshake, TLS later, keepalive, state machine)
   → framing (Hotline transaction header codec)
   → Hotline codec (field list encode/decode, bounded)
   → session (login/agreement/user state, access mask)
   → application services (chat, files, news, users, tracker)
   → UI (client) / server frontend (config, log)
```

---

## 6. Ownership / memory-management patterns

- **Three pointer kinds** (`UMemory.cpp:2-46`): raw `void*`, `TPtr` (fixed block), and `THdl` — a
  **relocatable handle** replicating the classic Mac Memory Manager: `NewHandle/Clone`, `Lock`/
  `Unlock` (with a 4-byte lock-count prefix on Mac), `HPurge`-style discardability, `SetSize/
  Reallocate`, and `StHandleLocker`/`StPtr`/`StHdl` RAII wrappers. Windows emulates it with
  `GlobalAlloc(GMEM_MOVEABLE)` + a 4-byte size prefix.
- **Custom pool allocator:** `UMemory(alloc).cpp` — a Knuth boundary-tag pool used in release
  builds (`USE_POOL_ALLOC`), plus global `operator new/delete` routed through `UMemory`.
- **Container ownership:** none of the containers own their elements (`CPtrList/CPtrTree/
  CLinkedList` store raw pointers; caller manages lifetime — a recurring leak/UB source).
- **Network buffers:** `THdl` handles (`rcvBuffer`, `dataHdl`) with `Lock`+offset arithmetic
  everywhere in `UTransact`/`UFieldData`.

**Modernization:**
- `TPtr` → `new`/`std::make_unique`/`std::vector`; `THdl` → **owning byte buffers**
  (`std::vector<std::byte>`/`std::unique_ptr<std::byte[]>`), rewriting `Lock()+offset` to direct
  indexing. **Do not recreate relocatable handles** — the abstraction exists only because Mac
  memory moved; modern memory does not. `StHandleLocker` is not a mutex and must simply disappear.
- Pool allocator and global `operator new/delete` → delete.
- Pointer containers → value/`unique_ptr` semantics per use (§7).
- `gCrypt`/`gApp` singletons → explicit session/application ownership (constructor-injected).

---

## 7. Custom container inventory and usage

All six containers are intrusive, 1-based, non-owning, manual-memory data structures
(`audit/01` §2). Evidence-based verdicts:

| Container | Actual semantics (from source) | App usage | Replacement |
|---|---|---|---|
| `CBoolArray` | bit-packed growable bool array over a `THdl`; single-item Move/Swap only (`CBoolArray.cpp:249-282`) | ~0 (client header only) | **remove-entirely** (`std::vector<bool>`/`std::bitset` if ever needed) |
| `CLinkedList`/`CLink` | intrusive singly-linked list; `AddLast/RemoveLast/GetLast` are O(n); no ownership | ~49 matches: server connection lists (`mConnectList`), client queues | `std::list`/`std::deque` (or `std::vector` + swap-remove) per access pattern |
| `CPtrList`/`CVoidPtrList` | the workhorse growable pointer array; 1-based indexes; `Sort` (heapsort) / `SortedSearch`; cursor iteration `GetNext`; **no element ownership** | **~307 matches** across all apps | `std::vector<T>` or `std::vector<std::unique_ptr<T>>`; `std::sort`+`std::lower_bound` for the sorted-search pattern; drop the 1-based convention |
| `CPtrTree<T>` | **flat, level-encoded tree**: `STreeItem{u16 level, u32 childCount, void*}` contiguous array; children follow parent; per-parent bubble-sort; linear `GetTreeItem` | ~5 matches (news/tracker tree models) | `std::vector` + parallel depth/child-count (if the flat layout is persisted) **or** a node tree `std::vector<std::unique_ptr<Node>>`; *not* `std::map` — it is not an associative container |
| `UIDVarArray` | ID→blob associative array, sorted unique non-zero IDs, byte-shuffling storage, `'IVA1'` flattened format | 0 direct app uses; internal: error-catalog reader (`UError(M/W)`) | **retain-temporarily** as a frozen IVA1 reader for `.dat` extraction; `std::map<uint32_t, std::vector<std::byte>>` for any new need |
| `UBitString` | static LSB-first bit ops (opposite bit order to `UMemory::GetBit` MSB-first!) | 0 (only CBoolArray uses it) | **remove-entirely** |

Two latent traps to document in the new codebase: the 1-based indexing convention and the two
opposite bit orders.

---

## 8. String / text representation inventory

- **No `UString` in the core.** The core string facility is `UText` — static functions over raw
  byte buffers + an encoding tag; the only string *classes* (`CString`/`StString`/`UStringConverter`)
  live in the vendored QTDataHandler sub-library.
- **Pascal strings everywhere:** `"\p..."` literals, `pstr*` functions from `ANSI.h`, length-prefixed
  buffers (`mUserName[32]` etc.); the client/source files are MacRoman/ISO-8859 encoded.
- **Wire text encoding is MacRoman/raw bytes** (script code always 0; no UTF-8 handling anywhere;
  `UText(W)` carries an identity MacRoman↔Windows-1252 map).
- `UText` functions: ctype/case maps, encoding-aware compares, `Format` (printf-style),
  `IntegerToText/SizeToText/SecondsToText`, `ReplaceNonPrinting`, login-message templating.
- `CFlatten` P/W/L-strings: 1/2/4-byte length prefixes, big-endian.
- `UMime` extension↔MIME and type-code↔MIME tables (used by the server's HTTP facade).

**Modernization:** `std::string` (UTF-8 internally) / `std::string_view` / `std::format`; an
explicit **MacRoman↔UTF-8 codec** for (a) reading legacy on-disk data (prefs, bookmarks, news DBs,
user files) and (b) legacy-protocol text fields (or keep raw-byte strings in the legacy codec layer
and convert at the boundary — decision point in Phase 4). Preserve the exact P/W/L-string wire
layouts in the codec. Drop the `UText(W)` translation table and dead `pstr*` bulk.

---

## 9. Filesystem / resource architecture

- **Filesystem:** `UFileSys` is a MoreFiles-idiom layer: opaque `TFSRefObj`, forged
  `kRootFolderHL/kProgramFolder/...` sentinels, `SFSListItem` (type/creator/size/flags/dates/name),
  `SFlattenRef{'FSrf',vers 1,'MACH'|'WIND',data}` persisted references, resumable
  `StartFlatten/ResumeFlatten/ProcessFlatten` + `StartUnflatten/...` (the file-transfer wire codec),
  aliases, resource-fork open, comments, "move to trash", type/creator emulation on Windows
  (3,863 LOC).
- **Resources:** classic Mac resource *API shape* (`URez`: Load/Release/useCount/search chain) over a
  custom flat **AWRZ** container (not the resource-fork format); runtime assets ship as
  `hls19.dat`/`hlc19.dat`(972 KB)/`hlci19.dat`/`hltracker.dat`; error strings as IVA1 `'EMSG'`
  catalogs; icons as `'ICON'` (converted from `cicn`), banner as `'GIFf'` 128 (QuickTime `'MooV'` 128
  present but client-side disabled), splash as `'PIXM'`/`'AWPX'`.
- **Server filesystem semantics:** paths as Pascal strings in `myField_FilePath`; drop boxes
  detected **by name substring** ("drop box"), not metadata (`HotlineServTrans.cpp:1008-1021`);
  type/creator codes exposed in listings; comments in file info; the news DB is files under a root
  folder with `.hnz` (`'HLNZ'`) databases.

**Modernization:** `std::filesystem` for all path/IO; keep only (a) the FILP/RFLT transfer codecs,
(b) `SFSListItem` listing layout, (c) AWRZ/IVA1/AWPX readers for legacy asset extraction, (d) the
drop-box *behavior* (renamed to explicit folder flags in the new server, with the name-substring
rule preserved as a compatibility option if needed). Delete type/creator/alias/resource-fork/trash/
comment machinery; re-author assets as plain files (GIF/PNG icons already exist under
`Apps/Images/`).

---

## 10. Threading and synchronization architecture

**There are no threads in the applications.** The whole product family is cooperative and
single-threaded:

- `UApplication`/`UMessageSys`: one priority message queue; `Execute` dispatches highest-priority
  first; timers (`UTimer`) post `msg_Timer` messages; sockets post `msg_DataArrived` etc.
- Client: `CMyApplication::ProcessTasks()` iterates `CMyTask` subclasses — explicit `switch(mStage)`
  state machines (with `goto` restarts) for every network operation (connect/login/transfer/news/
  tracker/admin); transfer queueing is cooperative (`QueueUnder()`, `IsFirstQueuedTask()`).
- Server: `for(;;)` poll loop — drain `Accept()`, walk the `mConnectList` (`CLinkedList`), process
  one transaction per client per pass, drain the message system (`HotlineServ.cpp:4615+`).
- Tracker: same message/poll model; timer-driven dead-server reaper.
- The only threading primitives in the tree are inside the QTDataHandler mini-framework
  (`CThread/CMutex/CSemaphore`) — removed with QuickTime.

Implications: (a) there is **no existing concurrency to preserve** — a modern server may use
`std::jthread`/asio I/O threads freely as long as per-connection semantics (ordering, the
single-pass dispatch) are reproduced; (b) the legacy task state machines are the authoritative
behavioral specification for every client operation and must be captured as tests before being
re-expressed as coroutines/async code; (c) timers/keepalive (client 180 s `KeepConnectionAlive`,
server connection timeouts) move to `<chrono>`/event-loop timers.

---

## 11. QuickTime dependencies

| Touchpoint | Location | Feature |
|---|---|---|
| `UOperatingSystem::InitQuickTime/IsQuickTimeAvailable/GetQuickTimeVersion` | `Misc/UOperatingSystem(M/W)` | capability probe at client startup |
| `UQuickTime.h` | `#include <QTML.h>, <Movies.h>, <Gestalt.h>` | SDK pull-in |
| `CQuickTimeView` (897 LOC) | `Views/CQuickTimeView.cpp` | movie playback UI: `SelectMovie/StreamMovie/SetMovie/Start/Stop/SaveMovieAs`, `IsSupported('MooV','sooV','MPEG'…)`; audio formats commented out |
| `CMyQuickTimeView` | `Client/Source/HotlineWindows.cpp:3943-3984`, `HotlineViews.cpp:3303-3308` | view downloaded/streamed movies; `StreamMovie(csHotlineAddr)` |
| `CMyViewFileTask::IsQuickTimeFile/StartQuickTime` | `HotlineTasks.cpp:2849-3072` | view file as movie |
| Banner QuickTime (`'MooV'` 128) | `Hotline.cpp:7175-7235` | **already hard-disabled** ("we do not allow quick time banners right now"); `GIFf` only |
| QTDataHandler component | `AppWarrior/Source/Libs/QTDataHandler/` (194 files) | QuickTime DataHandler for `hotline://` media streaming; `HL_HandlerIsReading/HL_HandlerCancelReading` polling |
| HTTP UA string | `Hotline.cpp:7231-7239` | "with QuickTime %#s" branding |

**Verdict:** remove QuickTime entirely. The only feature lost is in-client movie playback/streaming
— a legacy convenience in a BBS client. Replacement options (in preference order): (a) **remove the
feature** and document the discard; (b) hand media files to the OS default handler
("open with" — lowest effort, most future-proof); (c) a bundled modern player (FFmpeg/MPV) only if
playback is genuinely required. With QuickTime gone, the entire QTDataHandler tree, `UQuickTime.h`,
the QT availability probe, and the UA-string fragment are dead and removed. Banner behavior is
unaffected (already GIF-only).

---

## 12. Netscape / Mozilla-era dependencies

No NPAPI/ActiveX plugin source exists in the tree. The browser-era code is **URL-launching and
file-association integration**, all obsolete as browser integration:

- `CLaunchUrlView` (140 LOC) / `CLabelUrlView` (253 LOC) — hyperlink views calling `LaunchURL()`.
- `UTransport::LaunchURL` / `URegularTransport::LaunchURL` — launch default browser.
- `CMyLaunchUrlTask` (`HotlineTasks.cpp:2167-2250`) — wraps URL launch as a task.
- `UExternalApp` — file associations (`\phbm` bookmark, `\phpf` partial file) and app launching.
- Hardcoded dead vendor URLs: `www.lorbac.net`, `www.HotlineSW.com`, commented `hotlineisp.com`.
- `hotline://` URL scheme composition (`HotlineServ.cpp:1039-1048`).
- `UHttpTransact` (65.6k LOC with platform halves) — real HTTP client; **not** browser code; it is
  still needed (agreement/banner/update downloads) but should be replaced by a modern HTTP client.
- `UNntpTransact` (96.3k LOC) — NNTP client for NewsSynch; survives only if NewsSynch does.

**Verdict:** remove URL-view classes and vendor URLs; keep a single "open URL/file with OS default
handler" utility; replace HTTP/NNTP clients with modern libraries (or drop NNTP with NewsSynch).

---

## 13. Classic Mac / Carbon dependencies

- **Language/runtime:** Pascal strings, `nil`/`min_Int32`-style typedefs, `pragma options
  align=packed`, MacRoman text, four-char type/creator codes, "seconds since 1904" epoch
  (`UDateTime(M).cpp:198`), `Handle`/`HLock`/`HPurge` memory model (§6), `TMTask` timers,
  `GetResource('EMSG')` catalogs, `DebugStr`/MacsBug.
- **Toolbox:** QuickDraw/GWorld (`UGraphics(M)`, 5,254 LOC), OpenTransport (`URegularTransport(M)`),
  Carbon event/menu/window management (`UApplication(M)` 25.1k LOC, `UWindow(M)` 4,553 LOC, Apple
  Events, `UExternalApp(M)`), `UFileSys(M)` (3,844 LOC — volumes, aliases, resource forks,
  `OpenResourceFork`), `URez` (resource-manager-shaped), `UGUID(M)` (2,451 LOC vendored DCE UUID
  with Mac node synthesis), `USound(M)`.
- **Build heritage:** `.r` Rez files (`MsgBox.r`, `carb.r`), CodeWarrior-era prefix headers
  (`AWHeaders(W*)` shims, generated targets absent), Rez2Dat (Carbon-only, unbuildable), no Xcode
  projects in tree.
- **MoreFiles / Universal Interfaces:** already deleted from the repo (see §15); surviving code
  (`UFileSys` idioms) references their concepts without the headers.

**Verdict:** all classic Mac code paths are removed. What survives is the *semantics* that are
protocol-visible (big-endian layouts, Pascal-string wire formats, MacRoman payload bytes, file
type/creator codes on the wire) — preserved inside the explicit codec layer, not in platform code.

---

## 14. Windows-specific dependencies

- **Win32 backend:** `GlobalAlloc(GMEM_MOVEABLE)` handles, `SetTimer`, `SYSTEMTIME`/`FILETIME`,
  GDI (`UGraphics(W)` 3,229 LOC), `UApplication(W)` (10.4k), `UWindow(W)` (3,430),
  `UFileSys(W)` (3,863 — `SHGetFileInfo`, type/creator emulation), `UError(W)` Win32-error tables,
  `UClipboard(W)`, `UOleAutomation(W)` (COM/OLE — **zero app consumers**), `UService(W)` (NT
  service — **zero app consumers**).
- **Networking:** Winsock 1.1 with `WSAAsyncSelect` + hidden `HWND` message windows
  (`URegularTransport(W).cpp:129-132`) — an event-driven socket layer that only works inside a
  Win32 message loop.
- **App-level:** `.rc` files (menus, string tables, icons/cursors; `hotline.rc` references a missing
  `Resource.h`), MDI-mode remnants (commented out), `#if WIN32` inline branches (40 in
  `HotlineWindows.cpp`), `USES_FILE_EXTENSIONS`, `\r\n` vs `\r` line-ending branches.

**Verdict:** Win32-specific infrastructure is removed with AppWarrior; the only Windows-only
services worth preserving are (optionally) a Windows installer/service wrapper for the new server —
new code, not a port. `UOleAutomation` and `UService` have zero app consumers and vanish.

---

## 15. Missing / deleted obsolete dependencies

| Dependency | Status | Consequence |
|---|---|---|
| **Universal Interfaces** (Pascal/C Mac SDK headers) | deleted by repo author (commit `b0cf398`) | surviving `.h`s no longer reference them; `legacy` does not compile as a Mac build today |
| **MoreFiles** | deleted | `UFileSys` retains MoreFiles-style concepts (`SFSListItem`, flatten refs) but no external dependency |
| **JavaClasses.jar** | deleted (commit `8d2fdca`) | an old Java bundle once shipped in-tree; nothing references it |
| **XSP framework** (OpenSprings) | never in this tree | `FileFinder` cannot build (`#include "XSP_Core.h"` etc.) |
| **Mac Carbon AppWarrior library variant** (`AWHeaders(M-Carbon).h` target) | never in this tree | `Rez2Dat` cannot build |
| **Project files** (CodeWarrior `.mcp`, MSVC `.dsp/.vcproj`) | never in this tree | exact historical compile flags/defines (`DEBUG`, `NEW_TRACKSERV`, `USE_POOL_ALLOC`) are unverifiable |
| **`Resource.h`** (Windows) | missing | `hotline.rc` references it; any Windows rebuild must regenerate it |

**Directive:** do not resurrect any of these. The surviving code must be migrated off their
concepts, exactly as the project brief states.

---

## 16. Serialization and endian assumptions

**One global rule:** every multi-byte integer on the wire is **big-endian**; there is no
`SwapLong`/`SwapShort` — the helpers are `swap_int()` + `TB()`/`FB()` (identity on big-endian
hosts, byte-swap on Intel, `CONVERT_INTS` in `typedefs.h`; ≈2,084 call sites). The modern codec
must centralize this as explicit `std::endian`-aware `encode_u16/u32` over
`std::span<std::byte>`.

Byte-precise formats verified in this audit (`audit/06-protocol.md` is the full reference):

1. **Establish (client→server, 12 B BE):** `'TRTP' 'HOTL' u16 version=1 u16 subVersion` (2 = normal,
   3 = transfer). **Server→client (8 B):** `'TRTP' u32 error`. `'NICK'` is also accepted in place of
   `'TRTP'` by `ReceiveEstablish` (quirk to preserve).
2. **Transaction header (20 B BE, packed):** `u8 flag; u8 isReply; u16 type; u32 id; u32 error; u32
   totalSize; u32 dataSize` + data. Constraints: `id != 0` for requests, `totalSize/dataSize` in
   (0, 2 MB], multi-part reassembly by `(isReply,id)`, reply echoes request `id`.
3. **Field list:** `u16 count; count × { u16 id; u16 size; u8 data[size]; }` — **no padding**
   (`ALIGN_FIELDS=0`), field size ≤ 65535, IDs not required sorted; integers 2-byte if they fit
   `Int16` else 4-byte (`AddInteger`), 1/2/4-byte decode.
4. **Nested layouts:** `SMyFileInfo` (type/creator/size/rsvd u32, nameScript/nameSize u16, name
   bytes — no length byte), `SMyUserInfo` (u16 id, i16 iconID, u16 flags, u16 nameSize, name),
   `SMyUserAccess` (8 B = 2×u32 privilege bitmask, bit index = `myAcc_*`; **raw native-order copy —
   endianness hazard to normalize**, `HotlineServTrans.cpp:1719`), `SMyUserDataFile` (734 B, exact
   struct in `HotlineClientServerCommon.h:311-330`), news category lists (v15 format with `SGUID` +
   `addSN/delSN`), news article list (`u32 groupID, u32 count, pstrings, per-article `u32 id`,
   `SDateTimeStamp` date, `u32 parentID`, `u32 flags`, `u16 flavorCount`, title/poster pstrings,
   flavor pstrings + u16 sizes), tracker registration and `'HTRK'` server-list messages.
5. **Timestamp:** `SDateTimeStamp {u16 year, u16 msecs, u32 seconds}` — "midnight Jan 1 of `year` +
   `seconds` + `msecs`". **Cross-platform divergence:** Mac `GetDateTimeStamp` emits
   `year=1904, seconds=since-1904` (`UDateTime(M).cpp:100-108`), Windows emits `year=YYYY,
   seconds=within-year`. The portable/Windows convention is the one used by protocol code; pin it
   down with a capture or documented decision before porting.
6. **File transfer:** `'HTXF' {u32 protocol; u32 refNum; u32 dataSize; u16 type; u16 rsvd}` (server
   parse), `'FILP'` flat-file stream (`SFlatFileHeader 'FILP' v1 forkCount` + `'INFO'` fork +
   `'DATA'` fork; Mac used 3 forks/'AMAC', Win 2 forks/'MWIN'; `xferOption 2` = raw data fork),
   `'RFLT'` resume data (`u32 format='RFLT', u16 version=1, rsvd[34], u16 count,
   {u32 fork,u32 dataSize,u32 rsvdA,u32 rsvdB}[]`), folder-download item stream (`u16 size, u16
   type(bit0 folder), u16 pathCount, {u16 script, u8 len, name}[]`) driven by
   `dlFldrAction_{SendFile=1,ResumeFile=2,NextFile=3}`.
7. **Archive:** `'harc'` v1 with `u16 fileCount/fileAutoLaunch/rsvd3size` and per-file
   `{u32 type 'file'/'fldr'/'link'/'text', u16 pathSize, path items, u16 rsvdSize, u32
   compressionType 'zlib'/'raw ', u32 decompressedSize, u32 compressedSize, data}` — payloads are
   zlib-compressed FILP files.
8. **Resource container:** `'AWRZ'` v1 (map + data regions, `%_nm` name tables, BE map integers);
   error catalogs `'IVA1'` (BE id/offset table + blob data); pixmap `'AWPX'` (BE layer headers with
   explicit data offsets); news DB `'HLNZ'`; tracker prefs/login/banned binary files.
9. **Checksums:** `UMemory::CRC` = CCITT-32 (poly 0x04C11DB7, init -1) — used by tracker server-list
   ordering and the client secret-command table; `AdlerSum` = Adler-32; `Checksum` = byte sum.
   Preserve as small utilities.
10. **Community-ID codec** (`HotlineServIDTranslate`): 32-bit int ⇄ 6-char base-39 string, alphabet
    `" ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789._"`, little-endian digit order — used for ad-system
    serial numbers / news GUIDs. (It is **not** a transaction-ID translation table; no 1.2→1.5
    transaction mapping exists in this tree — legacy compatibility is field-level only.)

**Modernization rule for this whole area:** no `reinterpret_cast` overlay of C structs onto network
buffers; explicit `hotline::protocol::Codec` with `decode_packet(std::span<const std::byte>) ->
std::expected<Packet, DecodeError>` and bounded readers; every field width/endianness handled
explicitly; malformed input must never read past the span.

---

## 17. Security-sensitive legacy patterns

The full catalogs: client — 29 items in `audit/03-client.md` §6; protocol layer — 10 items in
`audit/06-protocol.md` §10; plus tracker/server items in `audit/05` and `audit/04`. Highlights that
must drive the rewrite:

**Memory safety (fix = bounded codec + validated sizes):**
- **Deliberate backdoor:** any account whose login begins `FLOOD` makes the server send a private
  message **500 times** to its recipient (`HotlineServTrans.cpp:325-341`) — a debug "flooder bot"
  left in production server code; must be removed.
- 1-byte stack over-read + overflow in server path caching (`HotlineServTrans.cpp:985-990`).
- Unbounded copy into `mServerBannerURL[256]` (heap overflow, `Hotline.cpp:9409`).
- `.hpf` suffix `pstrcat` overflow of 256-byte stack buffers (`HotlineTasks.cpp:2448, 3632`).
- `CChatLog::AppendLog` off-by-one write/read (`CChatLog.cpp:85-93`).
- Unbounded allocations from network-supplied sizes (banner `TransferSize`, server file sizes,
  tracker 16-bit sizes; `HotlineWindows.cpp:3859,7044`).
- Archive decoder trusts `fileCount/compressedSize/decompressedSize` (explicit TODO at
  `HotlineArchiveDecoder.cpp:254`).
- `ResumeFlatten` iterates `forkInfo[count]` without validating `count` against the buffer.
- Server-trusted transfer size vs unbounded client disk writes (`HotlineTasks.cpp:2691-2706`).
- Operator-precedence NULL-deref (`HotlineWindows.cpp:4364`), missing-return UB
  (`HotlineAdminViews.cpp:1528`), unaligned typed stores on attacker offsets
  (`HotlineViews.cpp:764`).

**Credential/authentication issues:**
- Plaintext password storage in `.hbm` bookmarks/`SaveConnect`/prefs; reversible bitwise-NOT
  "scrambling" of login/password on the wire and in `UserData` files.
- The HOPE encrypted login exists client-side only; the shipped server does plaintext-equivalent
  comparison. → Preserve legacy algorithm for compatibility, isolate + document, add TLS as a
  modern extension.
- `HLRand` seeded from a persisted file / uninitialized memory on first run.

**Authorization/behavior issues:**
- Client-side privilege checks are advisory; several remote-admin actions (`DoSecret` commands)
  fire with no `HasGeneralPriv` gate; server-side checks are the real enforcement to verify.
- News folder-delete privilege bypass via old/new type-code mismatch; duplicate field IDs (112/114);
  `HasGeneralPriv/HasFolderPriv/HasBundlePriv` identical (single mask read).
- Drop boxes identified by folder-name substring — spoofable server-side metadata, but a
  protocol-observable behavior to preserve or replace deliberately.
- Banner HTML parsed from untrusted server content and URL-launched (scheme allowlist exists but is
  fragile); chat-log XML injection; tracker response parsing into packed structs.
- Format-string `SendErrorMsg` overload drops its body (silent error loss).

**Already-good patterns to keep:** transaction size caps enforced by connection kill
(`UTransact.cpp:1019-1026`; 2 MB framework default, server overrides to 512 KB); `_FDBuildTables`
bounds-checks every field; `UFileSys::ValidateFileName` /`ValidateFilePath` path-component
sanitization (keep `..`-rejection; re-implement in the new FS layer); chat/post size clamps;
server-side flood counters (120 tx/min, 20 priv-msg/min, 10 KB chat/min) and connection autoban.

---

## 18. Likely dead / historical code

Full inventories: client — 23 items (`audit/03` §7); framework — `audit/01`/`audit/02`; tracker —
`audit/05`. Notable:

- **`Apps/ServerOLD/`** and **`Apps/Tracker/Old Tracker/`** — historical trees; the "new" versions
  are byte-identical on the wire but differ in management/UI. Audit once, exclude from the build,
  keep as reference.
- **`HotlineAdmInSpector.cpp`** ≡ `HotlineTracker.cpp` + 8 lines — a failed-rename duplicate
  (2,042 dead LOC); the real admin inspector lives in `Admin/`.
- **QTDataHandler** (194 files) and its `HL_BigRedH` mini-framework — dead after QuickTime removal.
- **`UZlibCompress` (compressor), `CBoolArray`, `UBitString`, `UClipboard`, `UOleAutomation`,
  `UService`, `CWizard`, `UProgramCleanup`, `UDebug`** — zero or internal-only consumers.
- **Client dead paths:** `#if 0` easter eggs (pork mode, scram chat, Livestock), `NEW_TRACKSERV` /
  `NEW_TRACKER` protocol flags (never defined), old pre-1.5 news (`#if !NEW_NEWS` compiled out),
  broken `AutoReconnect`, nine remote-action tasks whose `Finish()` is mis-pasted into
  `GetShortDesc()` (never complete), `CMyCrazyServerTask` never sends, `viewID_BannerNext` unused,
  dead vendor URLs/stubs (Securiphone/Xsprings), chat-scram substitution tables.
- **`*.original.cpp`** files (`UTransact.original.cpp`, `UDigest.original.cpp`, `CWindow.original.cpp`…)
  — pre-cleanup snapshots kept in-tree.
- **`FileFinder`** — unrelated XSP demo; **`Rez2Dat`** — unbuildable Carbon tool; both archival.
- **Tracker `SendLookup` (type 4/5)** — never called in either tracker.
- **Server dead code:** `ProcessTran_SendMessage` (131) replies with the joke string `"Hahahaha!"`;
  the `FLOOD`-login 500× message backdoor (`HotlineServTrans.cpp:325-341`); stray
  `DebugBreak("visible")`; commented `NEW_NEWS`/`ProcessTran_Flood` blocks; legacy agreement-bypass
  branch for pre-1.5 clients.

Per the brief: exclude from the modern build, document status, retain the historical source as
reference — do not silently delete without the comparison work documented here.

---

## 19. Recommended modern replacement per major subsystem

| Legacy | Replacement | Class |
|---|---|---|
| `ANSI.h`/`ANSI.cpp`, `typedefs.h` | `<cstdint>/<cstring>/<cstdlib>/<algorithm>/<bit>`, `nullptr`, `std::format`; keep `TB/FB` as codec helpers | replace-with-stdlib |
| `UMemory` (handles, pool) | values, `std::vector<std::byte>`, `std::unique_ptr`; standard allocator | remove-entirely |
| `CPtrList/CLinkedList/CPtrTree/CBoolArray/UBitString` | §7 mapping | replace-with-stdlib / remove |
| `UIDVarArray` | frozen `IVA1` reader; `std::map` if a new ID→blob map is needed | retain-temporarily |
| `UText`/`UMime` | `std::string`/`string_view` + MacRoman↔UTF-8 codec + `<cctype>`/ICU-adjacent utilities | replace-with-stdlib |
| `CFlatten` | explicit big-endian `BinaryWriter/BinaryReader` over `std::span<std::byte>` | preserve-semantics-rewrite |
| `UFieldData` | `hotline::protocol::FieldList` codec (exact wire layout) | preserve-semantics-rewrite |
| `UTransact/UTransport/URegularTransport` | `hotline::net` (asio-style transport + Hotline framing + session) | preserve-semantics-rewrite (wire), replace-with-modern (sockets) |
| `UMessageSys/UApplication/CApplication/UTimer` | modern event loop (UI toolkit loop / asio) + `<chrono>` timers | replace-with-modern-platform |
| `UError/SError` | `std::expected`/`std::error_code` + structured `DecodeError`; keep OS-error mapping tables | replace-with-modern |
| `UDateTime/SDateTimeStamp/UGUID` | `<chrono>` + 8-byte BE timestamp codec; platform UUID | preserve-semantics-rewrite (codecs) / replace-with-modern |
| `UMath` | `<random>/<cmath>/<bit>`; keep CCITT-32 CRC utility | replace-with-stdlib |
| `HLBlowfish/HLMD5/HLSha1/HLCrypt/HLRand` | reimplement over a maintained crypto library (OpenSSL/Botan) **byte-for-byte** with captured reference vectors | preserve-semantics-rewrite |
| `UFileSys` | `std::filesystem` + transfer codecs (FILP/RFLT/listing) | replace-with-modern + preserve codecs |
| `URez`/`.dat`/`Rez2Dat` | plain asset files; frozen AWRZ/IVA1/AWPX readers for extraction | retain-temporarily → remove |
| `UGraphics/UPixmap/UIcon/URegion` | chosen UI toolkit's canvas / Skia; AWPX reader for legacy assets | replace-with-modern |
| Image decoders (IJG 6b/GIF/PICT/BMP) | modern image library (libjpeg-turbo/stb/…) | replace-with-modern (PICT: remove) |
| `CView`/`CWindow`/views hierarchy | Qt/QtQuick (or web UI) modeled on the documented semantic contract | replace-with-modern |
| QuickTime + QTDataHandler | nothing (or OS "open with" for media) | remove-entirely |
| `UHttpTransact`/`UNntpTransact` | modern HTTP client (libcurl/asio); NNTP only with NewsSynch | replace-with-modern |
| Browser/URL integration, vendor URLs | one "open with OS default" utility | remove-entirely |
| `USound`/`UKeyboard`/`UMouse`/`UDragAndDrop`/`UExternalApp` | toolkit/platform equivalents (keep ADPCM only for legacy assets) | replace-with-modern |
| `UOleAutomation`/`UService` | nothing | remove-entirely |
| Server user DB / prefs / bookmarks | JSON/Toml/SQLite with keychain-backed secrets | replace-with-modern |
| NewsSynch | revisit after server rewrite (likely script over the news DB) | remove or replace |

---

## 20. Abstractions that should simply disappear

No modern replacement, no shim, no "ModernX" wrapper — callers migrate:

1. **Relocatable `THdl`/`StHandleLocker`/`StPtr`/`StHdl` and the pool allocator** — the compactible
   Mac heap no longer exists; replace with owning containers. (`StHandleLocker` is not a mutex and
   must not become one.)
2. **`ANSI.h`/`ANSI.cpp`** — a hand-written libc for 1990s compilers.
3. **`CBoolArray`, `UBitString`, `UZlibCompress` compressor, `UProgramCleanup`, `UDebug`,
   `UClipboard`, `UOleAutomation`, `UService`, `CWizard`** — zero or internal-only consumers.
4. **QuickTime everywhere** (`UQuickTime.h`, `CQuickTimeView`, `UOperatingSystem` QT probes, the
   entire QTDataHandler component including `HL_BigRedH`).
5. **Browser/URL-launch views** (`CLaunchUrlView`/`CLabelUrlView`) and dead vendor URL strings.
6. **Type/creator codes, aliases, resource forks, "move to trash", comments-in-FS-metadata** — the
   MoreFiles worldview, in `UFileSys` and everywhere they surface in listings/UI. (Wire-format
   occurrences of type/creator bytes remain, encoded explicitly by the codec.)
7. **Classic resource architecture** (`URez` manager, `'EMSG'` catalogs, `.dat` containers) once
   assets are re-authored — only frozen readers remain during migration.
8. **AppWarrior's global `operator new/delete`, `nil`/`FORCE_CAST`/`BPTR` macros, `SHuge` 64-bit
   emulation, `swap_int` host-endian conditionals** — replaced by `<cstdint>`, `std::bit_cast`,
   `std::byteswap`/explicit codecs.
9. **The 1-based container convention, `pstr*` buffers, `\p` literals** — replaced by modern
   strings/zero-based ranges (wire P-strings remain *inside* the codec only).
10. **`FileFinder` and `Rez2Dat`** — non-Hotline/unbuildable tooling; archived, not modernized.
11. **The `CMyTask` cooperative scheduler** (client) and **message-pump architecture** — superseded
    by async/coroutine code in the new client; the *state machines* are preserved as behavior
    specs/tests, not as infrastructure.
12. **Global singletons** (`gApp`, `gCrypt`, `gApplication`) — replaced by explicit ownership.

---

## 21. Protocol behaviors that require tests before modification

(Ordered by risk. Golden vectors derivable from source are in `audit/06` §11.1.)

1. **Framing round-trip:** 20-byte BE header, multi-part reassembly, reply-id matching, size caps
   (framework 2 MB / server 512 KB), `'TRTP'/'HOTL'/v2` establish + `'NICK'` acceptance quirk,
   `'HTXF'` transfer routing.
2. **Field codec:** count/id/size/data layout, no padding, variable-width integers (2/4-byte,
   1-byte decode), 65535 field cap, duplicate-ID behavior, `headerSize` offsets.
3. **Login paths:** normal path — bitwise-NOT scramble round-trip, login lower-casing,
   password byte-compare, error replies; HOPE path — session-key validation, algorithm-name
   negotiation, `HMAC(login|password, sessionKey)`, `HLCrypt::Init` two-key schedule, `Perm*Key(n)`.
4. **Encrypted transactions:** header encryption, `flag` distribution quirk (2/7/13 re-roll), 2-byte
   old-key prefix + remainder under permuted key — capture a reference trace from this exact code.
5. **Blowfish OFB-64 zero-IV + HMAC-MD5/SHA-1 vectors** (Eric Young tables + RFC 2104 vectors) and
   `HLRand` determinism given a seed file.
6. **File transfer:** FILP stream order (INFO/DATA forks), RFLT resume data, `xferOption` semantics,
   folder-download item headers + `dlFldrAction_*` protocol, `.hpf` partial-file naming,
   `WaitingCount` queue updates, transfer-session version (subVersion 3) routing.
7. **News:** v15 category list layout (`SGUID` + SNs), legacy `NewsCatListData` type-code format,
   article-list layout (dates as year+within-year seconds — pin the convention), threading
   (parent/firstChild/prev/next), flavor codes, community-ID (base-39) round-trip.
8. **Tracker:** UDP registration layout + IP-from-source rule + type-2-ignored quirk, CRC-sorted
   server list, `'HTRK'` v1 handshake, chunked "type 1" messages (8192-byte boundary), 16-bit
   counts, name-collision update-vs-insert rule, per-IP quota, reaper timeouts.
9. **User account records:** 734-byte `SMyUserDataFile` layout, `SMyUserAccess` bit order (and its
   endianness hazard), scrambled-login/password storage convention.
10. **Server behaviors:** dispatch coverage for all 54 handlers, drop-box name rule, path caching
    fix (1-byte overflow), `SendErrorMsg` overload semantics, agreement/banner sequence, keepalive
    no-op, version negotiation (`myField_Vers` 197, client `vers >= 150` branches).
11. **Archive codec:** `'harc'` + zlib FILP payloads, raw fallback, bounds validation.
12. **Registry of known wire bytes:** the golden byte examples in `audit/06` §11.1
    (`54 52 54 50 …` establish, keepalive header, single-field body, `myField_Vers=197`).

Testing strategy: capture reference vectors **from this code** (a small harness compiled against
the legacy `Crypt/`+`UFieldData` sources, or hand-computed from the quoted layouts) before the
rewrite; then the modern codec must reproduce them byte-for-byte; add malformed-input fuzz cases;
run ASan/UBSan; optionally verify live interop against an hxd-compatible client/server.

---

## 22. Proposed modern directory / module architecture

(Illustrative, derived from actual responsibilities; final shape to be confirmed in the first
implementation phase.)

```
hotline/
├── protocol/                  # wire knowledge, pure, no I/O
│   ├── constants.{h,cpp}      # transaction/field/privilege IDs (verbatim tables + static_asserts)
│   ├── endian.h               # encode/decode u16/u32 BE helpers
│   ├── field_list.{h,cpp}     # UFieldData replacement (bounded encode/decode)
│   ├── framing.{h,cpp}        # STranHdr replacement: header codec, establish, reassembly
│   ├── payloads.{h,cpp}       # SMyFileInfo/SMyUserInfo/SMyUserAccess/userData/date/guid layouts
│   ├── transfer.{h,cpp}       # FILP/RFLT/folder-item/HTXF codecs
│   ├── archive.{h,cpp}        # 'harc' decoder (bounded, validated)
│   ├── news.{h,cpp}           # news list/article layouts
│   ├── tracker.{h,cpp}        # tracker registration/list messages
│   └── auth.{h,cpp}           # legacy login algorithms (scramble, HMAC key schedule) — isolated + documented
├── crypto/                    # thin wrapper over maintained crypto lib reproducing legacy vectors
├── net/
│   ├── transport.{h,cpp}      # RAII sockets (asio), connect/listen/accept, TLS extension
│   ├── connection.{h,cpp}     # establish + framing + keepalive + deterministic shutdown
│   └── session.{h,cpp}        # login/agreement/state machine, access mask
├── core/                      # shared domain
│   ├── users.{h,cpp}          # account model, privilege bitset, user DB (SQLite/JSON)
│   ├── chat.{h,cpp}           # chat/IM/invite logic
│   ├── files.{h,cpp}          # virtual file tree, drop-box flags, path safety
│   ├── news.{h,cpp}           # news DB (categories/articles/threads) + HLNZ reader/writer
│   └── log.{h,cpp}            # structured logging (std::format), no secrets
├── client/                    # client application core (UI-independent, testable)
│   ├── connection/  transfers/  chat/  files/  news/  users/  tracker/  bookmarks/
│   └── prefs/ (migrates legacy prefs/bookmarks)
├── server/
│   ├── server.{h,cpp}         # listeners (main/transfer/HTTP), connection table, dispatch
│   ├── handlers/              # one unit per transaction group (chat, files, users, news, admin)
│   ├── news_database/  user_database/  ban_list/  agreement_banner/
│   └── tracker_registrar.{h,cpp}
├── tracker/
│   └── tracker.{h,cpp}        # UDP/TCP tracker (~few hundred lines)
├── platform/                  # narrow, only what std doesn't cover
│   ├── launch_url / sounds / uuid / keychain / media_open
├── ui/                        # modern UI (Qt/web/etc.) — separate target
├── assets/                    # extracted banners/icons/strings (UTF-8)
└── tests/                     # golden vectors, codec round-trips, malformed inputs, behavior tests
```

Boundaries: `protocol` depends on nothing; `crypto` on the crypto lib; `net` on `protocol`; `core`
on `protocol`; client/server/tracker on `net`+`core`; `ui` on `client` (never the reverse).

---

## 23. Proposed incremental migration sequence

(Adjust when dependency analysis suggests better ordering; the brief's 12 phases mapped to this
tree.)

1. **Phase 1 — Archaeology (done):** this report + `audit/01…06-*.md`; freeze `legacy/` as
   reference; record the modernization ledger (`docs/modernization-ledger.md`).
2. **Phase 2 — Foundation:** CMake + Ninja, C++23, warnings-as-info, ASan/UBSan presets; empty
   `hotline::protocol` target with test harness; CI.
3. **Phase 3 — Protocol characterization:** golden vectors + behavioral tests from §21; reference
   harness against the legacy `Crypt/`/`UFieldData`/`UTransact` code (compiled read-only as a test
   oracle).
4. **Phase 4 — Core types & codecs:** `constants`, `endian`, `field_list`, `framing`, `payloads`,
   `auth` (legacy algorithms), `crypto` wrapper — with `static_assert`s on sizes and tests green.
5. **Phase 5 — Transfer & archive codecs:** FILP/RFLT/HTXF/folder items/harc + tracker messages.
6. **Phase 6 — Networking:** RAII transport + connection + session over asio; establish handshake,
   keepalive, TLS extension; malformed-input hardening.
7. **Phase 7 — Server core:** headless server (`hotline::server`) with dispatch table per §4, user
   DB, news DB, ban list, tracker registration, agreement/banner; behavioral parity tests vs the
   legacy handlers; path-safety hardening.
8. **Phase 8 — Client core:** `hotline::client` services (connection/transfers/chat/news/users/
   tracker/bookmarks) as async state machines replacing `CMyTask`s; prefs migration.
9. **Phase 9 — Tracker:** `hotline::tracker` per the wire spec; optional fixes (32-bit counts,
   unregister) behind compatibility decisions.
10. **Phase 10 — UI:** modern client UI consuming `hotline::client`; assets re-authored from
    `Apps/Images/`; legacy AWRZ/AWPX readers retired.
11. **Phase 11 — Cleanup:** remove compatibility readers, `ServerOLD`/`Old Tracker`/`FileFinder`/
    `Rez2Dat` from the build graph (kept in `legacy/`), delete dead code documented in §18.
12. **Phase 12 — Verification:** sanitizers, golden tests, optional live interop with hxd-style
    peers, security review of §17 items, architecture review.

Each phase is a reviewable, coherent change set (per the brief's §58), with the ledger answering:
what was replaced, why, what provides it now, how behavior was verified.

---

## 24. Major risks and unresolved questions

1. **Wire-text encoding decision.** Payload text is MacRoman/raw bytes; legacy peers will send
   non-ASCII as MacRoman. Decide: raw-byte legacy codec + UTF-8 at the UI boundary (recommended),
   with a documented MacRoman↔UTF-8 mapping for legacy on-disk data. Getting this wrong corrupts
   names for old clients.
2. **`SDateTimeStamp` convention.** Mac 1904-vs-Windows within-year divergence (§16.5). The
   Windows/portable convention is used by protocol code; confirm against a live peer capture and
   pin it in the codec.
3. **HOPE login.** Client-only in this build; server does the legacy path. Decide whether the new
   server implements HOPE for old `mUseCrypt` clients (recommended: yes, using the preserved
   algorithms) and capture reference vectors.
4. **`SMyUserAccess` endianness** (raw native copy, `HotlineServTrans.cpp:1719`). Normalize in the
   new codec and verify against real clients; document the historical hazard. *(Refined during
   Phase 3 implementation: the legacy SetBit/ClearBit are byte-based, so the wire bytes for a
   given privilege set are host-independent — the hazard only affects numeric interpretation of
   the two u32 values. The modern `AccessMask` models the mask as a u64 read big-endian and
   sidesteps it; see the modernization ledger, Phase 3.)*
5. **Duplicate field IDs** (112/112, 114/114): preserve the collisions verbatim (existing
   peers depend on them); document prominently; do not "fix" silently. *(Verified during
   Phase 1 implementation: the previously listed "117/117" is NOT a field-ID collision —
   transaction 117 (NotifyChatChangeUser) and field 117 (IconId) live in different ID
   namespaces.)*
6. **Build provenance:** no historical project files; exact defines/flags unknown (`DEBUG`,
   `NEW_TRACKSERV`, `USE_POOL_ALLOC`). Modern build defines its own configuration.
7. **NewsSynch/NNTP:** whether to rebuild newsgroup sync depends on product scope; legacy `HLNZ`
   format must at least be *readable* for migration.
8. **Tracker evolution:** whether to keep the legacy `'HTRK'` wire format for old clients
   (recommended: keep, plus optional modern HTTPS+JSON API), and whether to fix known defects
   (16-bit counts, ignored unregister, no keepalive).
9. **`ServerOLD` vs `Server` behavioral deltas** — the full comparison is in `audit/04-server.md`
   (1.9.6 → 1.9.7: rebrand to the derivative name, added chat-log CSV logging, wrong-user-name
   kick, enlarged pref buffers, `myField_Vers` 196→197; no protocol semantics dropped). The newer
   `Server` is authoritative unless a quirk is externally observable. Note the historical
   version strings ("Hotline Server 1.9.6" vs "GLoarbLine Server 1.9.7") are legacy branding — the
   modernized project remains **Hotline**.
10. **Licensing:** the tree carries "(c)2003 Hotsprings Inc., GPL" headers; the modern project's
    license must remain compatible (the workspace LICENSE is the modern project's own).
11. **Live-interop verification:** no packet captures exist yet; recommend capturing against hxd or
    a running legacy server before finalizing codecs for subtle areas (tracker 16-bit counts,
    timestamp semantics).
12. **News DB (`HLNZ`) internals** and tracker binary prefs/logins are only partially reverse
    engineered from the writer code; treat migration readers as best-effort until proven.
13. **UI framework choice** is intentionally deferred to Phase 10; the audit only fixes the
    requirements inventory (view/widget contract, item-provider lists/trees, drag flavors, icon
    set) so the choice is architectural, not emotional.
14. **Framework trust boundary unverified:** server path traversal/alias resolution is delegated to
    `UFileSys`/`StFileSysRef` (no explicit `..` rejection at the server layer); the framework's
    path primitives must be audited or replaced with `std::filesystem` + explicit sandboxing before
    the new server ships. The per-account root seam (`Users/<login>/Files`) already exists and is
    the natural chroot point.

---

## Appendix A — Sub-audit reports

| Report | Scope | Key sections |
|---|---|---|
| `audit/01-appwarrior-core.md` | AppWarrior runtime core | per-declaration ANSI.h migration plan, UFieldData/CFlatten/IVA1 layouts, Mac-Handle inventory, crypto inventory, full file/verdict table |
| `audit/02-appwarrior-ui-graphics.md` | Graphics/UI/media/hardware | view-class table, QuickTime touchpoints, QTDataHandler inventory, browser-era list, asset inventory |
| `audit/03-client.md` | Hotline client | file inventory, architecture, transaction table, feature map, 29 security items, 23 dead-code items |
| `audit/04-server.md` | Server + ServerOLD + NewsSynch | file inventory, Server-vs-ServerOLD delta, architecture (4 listeners, poll loop, flood protection), 54-handler dispatch table, user DB, news DB, path security, NewsSynch verdict |
| `audit/05-tracker-utils.md` | Trackers, FileFinder, Rez2Dat, assets | tracker wire spec, old/new comparison, binary asset inventory, HLNZ/user-DB samples |
| `audit/06-protocol.md` | wire protocol deep dive | byte-precise framing/fields/auth/transfer/archive/news/tracker, endianness audit, golden vectors, verbatim constant tables |

*(Note: `audit/06` contains one factual correction relative to external protocol lore — this tree's
framing is the standard Hotline transaction protocol, confirmed against the canonical protocol
documentation — applied in this report.)*

## Appendix B — Primary protocol constant tables (verbatim, abbreviated)

Full tables (all transaction IDs, field IDs, privileges) live in
`Apps/Common Files/HotlineClientServerCommon.h` and are reproduced in `audit/06-protocol.md`.
Selected values cited throughout this report:

- **Establish:** client → `'TRTP' 'HOTL' 00 01 00 02`; server → `'TRTP' 00 00 00 00`.
- **Transactions:** Login=107, Agreed=121, ChatSend=105, ChatMsg=106, GetFileNameList=200,
  DownloadFile=202, UploadFile=203, DownloadFldr=210, GetUserNameList=300, GetUserList=348,
  GetNewsCatNameList=370, GetNewsArtData=400, PostNewsArt=410, KeepConnectionAlive=500, …
- **Fields:** Data=101, UserName=102, UserID=103, UserIconID=104, UserLogin=105, UserPassword=106,
  RefNum=107, TransferSize=108, UserAccess=110, Vers=160, FileNameWithInfo=200, FileResumeData=203,
  FileXferOptions=204, FileComment=210, NewsCatListData=320, NewsCatListData15=323,
  NewsArtListData=321, NewsArtID=326, NewsArtData=333, SessionKey=3587, MacAlg=3588,
  S_CipherAlg=3777, C_CipherAlg=3778, …
- **Privileges:** 55 bits (0–54) in `SMyUserAccess` — DeleteFile, UploadFile, DownloadFile, …,
  ViewDropBoxes, MakeAlias, Broadcast, NewsPostArt, … AdmInSpector=53, PostBefore=54.

*End of Phase 1 report. Per the assignment, implementation stops here pending review of this
document and selection of the first modernization phase.*

---

## 25. Addendum — architectural direction update (supersedes conflicting recommendations)

**The controlling charter is now `AGENTS.md`** (adopted after this report was written). Where this
report conflicts with AGENTS.md, AGENTS.md wins. In particular:

1. **AppWarrior is preserved and modernized** — not dismantled. This supersedes the
   "dismantle AppWarrior" thrust of §19/§20 recommendations and any reading of §22 that drops the
   framework. Modern AppWarrior is a lightweight native cross-platform C++23 framework; the Hotline
   **client, server and tracker remain AppWarrior-based applications**. Its meaningful architecture
   (application model, event model, views/windows/controls, timers, graphics, transport) survives;
   its obsolete internals (classic-Mac handles, ANSI.h, QuickTime, Netscape-era code, hand-rolled
   STL duplicates) do not.
2. **Native UI only.** The UI layer is native/lightweight desktop technology — Cocoa/AppKit via
   Objective-C++ at the macOS boundary, Wayland/X11 backends on Linux, native Win32/modern Windows
   facilities on Windows. Electron, CEF-as-UI, browser-hosted UI and embedded-web UIs are all
   excluded. This replaces §22's "ui: modern UI (Qt/web/etc.)" option.
3. **Modular AppWarrior.** AppWarrior splits into separable components (core / platform / network /
   UI / graphics). A headless Hotline server uses AppWarrior core without pulling in any GUI stack.
   §22's `hotline::` module layering remains valid as the *application* architecture on top of the
   framework.
4. **Phase sequence re-mapped.** §23's 12 phases are amended so that AppWarrior core modernization
   (types, containers, event/application model, platform backend boundaries) is a first-class
   early track alongside the protocol work, instead of the framework being scheduled for removal.

**Audit corrections verified during implementation** (per AGENTS.md: trusted source behavior wins,
documentation updated):

- **§24 risk #5** — the listed "117/117" duplicate is *not* a field-ID collision: transaction 117
  (`NotifyChatChangeUser`) and field 117 (`IconId`) live in different ID namespaces. The real,
  verbatim-preserved field-ID collisions are **112/112** (`UserFlags`/`Visible`) and **114/114**
  (`ChatId`/`Number`). Verified against
  `legacy/Apps/Common Files/HotlineClientServerCommon.h:106,117,121`.
- **Field IDs 3771/3772** — the historical header assigns `myField_S_CipherAlg = 0x0EC1` and
  `myField_C_CipherAlg = 0x0EC2`, which are **3777 and 3778**; the adjacent `// 3771` / `// 3772`
  comments are arithmetic typos. The compiled hex literals (used identically by the tree's client
  and server) are authoritative; Appendix B and `audit/06` have been corrected.

**Implementation status:** Phases 1–3 of the implementation are complete: build foundation +
`hotline::protocol` wire codec (Phase 1); `appwarrior::testing` (Phase 1b); `appwarrior::core`
with `endian`/`bits`/`align`/`ivar` and the typedefs.h + container verdicts (Phase 2);
`hotline::crypto` digests/HMAC/key-schedule plus the payload codecs and login scramble (Phase 3) —
81/81 tests across gcc/clang/ASan-UBSan. See `docs/modernization-ledger.md`.
