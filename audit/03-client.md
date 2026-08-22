# Hotline Client Audit

*Scope: `/home/admin/Documents/GitHub/Slopline/legacy/Apps/Client/` (read-only). The client is the GLoarbLine / "Hotline Connect" GUI application, historically branded **GLoarbLine** (see `Resources/Win/hotline.rc` VERSIONINFO: "GLoarbLine Connect Client", 1.9.7.2). It is a C++ application built on the in-tree **AppWarrior** framework (`legacy/AppWarrior/`) and shares protocol constants with the server via `legacy/Apps/Common Files/HotlineClientServerCommon.h`. All source is MacRoman/ISO-8859-1 encoded with Pascal-string (`"\p..."`) literals; it was read with `iconv -f MACINTOSH` and `grep -a`.*

---

## 1. File inventory

### Client source (`Source/`, 44,207 LOC total)

| File | LOC | Role |
|---|---|---|
| `Source/Hotline.cpp` | 9,714 | Entry point (`main()`) and `CMyApplication` (subclass of `CApplication`): startup, key handling, connection orchestration, chat/broadcast, options/prefs, bookmarks, secret console, incoming-transaction dispatch (`ProcessTran_*`), task manager. |
| `Source/Hotline.h` | 4,545 | Master header: view/command IDs, window & task class declarations, `CMyApplication` declaration, pref structures, feature flags. |
| `Source/HotlineWindows.cpp` | 7,562 | All `CMy*Window` subclasses and their view layouts (chat, files, news, users, tracker, options, login, banner, dialogs). |
| `Source/HotlineTasks.cpp` | 6,629 | The entire task model: `CMyTask` base, `CMyTransactTask`, connection/login/agreement/banner, all file-transfer tasks (download/upload/folder/resume), news tasks, admin tasks, tracker task, keepalive. |
| `Source/HotlineNews.cpp` | 3,807 | News reader: category list/tree/explorer windows, article tree, article text window, post/reply/delete flows. |
| `Source/HotlineViews.cpp` | 3,311 | Custom AppWarrior view subclasses (status list/tree views, icon picker, image/animated-GIF/QuickTime views, access checkbox). |
| `Source/HotlineAdmInSpector.cpp` | 2,042 | **Duplicate of `HotlineTracker.cpp`** (only 8 lines differ). See §7. |
| `Source/HotlineTracker.cpp` | 2,042 | Tracker server-list UI (`CMyServerWindow`, `CMyServerTreeView`, `CMyTrackServWindow`), tracker add/edit, server list parsing/filtering. |
| `Source/Admin/HotlineAdminViews.cpp` | 1,807 | Admin account editor views: user tree, access-privilege checkbox grid, "AdmInSpector" online-user tree. |
| `Source/HotlineNewsReadHistory.cpp` | 981 | News read-history tracking (`CMyNewsReadHistory`, `CNZReadList`). |
| `Source/HotlineMisc.cpp` | 704 | `CMyCacheList` (list cache), `CHttpIDList` (HTTP cookie persistence), `CMyPathData`, `CMySearchText`, `EventCallBack`, substitution-table scramble helpers (`InitSubsTable`/`BuildSubsTable`/`SubsData`/`ReverseSubsTable` — dead chat-scram obfuscation) and `GenerateRandomPassword` (new-account password generator). |
| `Source/Admin/HotlineAdminWindows.cpp` | 591 | Admin windows: `CMyAdminWin` (account editor), `CMyAdminEditUserWin`, `CMyAdmInSpectorWin`. |
| `Source/Admin/HotlineAdmin.h` | 331 | Admin module header (admin window/view classes, access structs). |
| `Source/CChatLog.cpp` | 141 | XML chat logger (`CChatLog`). |

### Resources / data

| Path | Role |
|---|---|
| `Data/hlc19.dat` (972 KB) | Hotline rez-file (icons, cursors, pictures, sounds); loaded on Windows via `URez::AddProgramFileToSearchChain("\pData//hlc19.dat")` (`Hotline.cpp:7610`, `LoadRezFile`). |
| `Resources/Win/hotline.rc` | Windows resources: MDI menu, string table ("GLoarbLine"), VERSIONINFO (1.9.7.2), app/doc/partial-file icons. Includes a **missing `Resource.h`** (not present in tree). |
| `Resources/Win/Icons/*.ico` (30), `Cursors/*.cur` (3) | Windows icons/cursors. |
| *(none)* | No Mac `.rsrc`/`.r` files — Mac resources are inside `hlc19.dat`. |

### Shared dependencies (outside `Apps/Client/`, listed for context)

`Apps/Common Files/`: `HotlineClientServerCommon.h` (332) — transaction/field/privilege IDs; `HotlineArchiveDecoder.{cpp,h}` (405/91) — folder-archive reconstruction used by folder downloads; `HotlineFolderDownload.{cpp,h}` (277/49) — recursive local mirror of a downloaded folder tree; `HotlineArchiveStruct.h` (133).

---

## 2. Architecture narrative

### 2.1 Application object and entry point

`main()` (`Hotline.cpp:44`) initializes `UOperatingSystem::Init()` and `UOperatingSystem::InitQuickTime()` (`:54-55`), then `UTransport::Init()` and a TCP-availability check (`:59-62`). On Macintosh it installs a URL handler and sets the menu bar (`:67-69`). It then constructs the singleton `gApp = new CMyApplication`, calls `StartUp()` and `Run()` (`:73-75`). All serious errors are funneled through `SError`/`UError::Alert` (`:80-83`).

`CMyApplication` subclasses `CApplication` (`Hotline.h:4045`). Its constructor (`Hotline.cpp:88`) installs a global key-command table (F1 options, F2-F7 window toggles, F9 private message, F12 secret, etc. `:92-135`), initializes `USound` (`:134`) and disables sound until prefs load (`:139`). `StartUp()` (`:239`) registers protocol/file associations: `"\photline"` (Hotline URL protocol), `"\phbm"` (bookmark), `"\phpf"` (partial file) (`:248-256`), and — unless Shift is held — reads preferences, the custom distribution file, and the rez file, then creates fonts, the toolbar, and the main windows (`:266-478`).

### 2.2 Window and view model

The client is a window-per-feature MDI-like design. `CMyApplication` holds the main windows as members (`Hotline.h:4199-4215`): `mChatWin` (public chat), `mTasksWin`, `mUsersWin`, `mAdminWin`, `mAdmInSpectorWin`, `mTracker`, `mToolbarWin`, plus `CPtrList` collections for file windows (`mFileWinList`), view-file windows, server-info windows, news-category windows, article-tree windows, article-text windows, and user-info windows. File/news windows are created on demand and added to these lists (`GetFolderWinByPath`/`SetFolderWinContent` in `Hotline.h:4440+`). Windows are AppWarrior `CWindow` subclasses that build their view hierarchy in their constructors (`HotlineWindows.cpp`). Note: `CWindow` has **no numeric window ID** (`CWindow(bounds, layer, options, style, parent)` — `AppWarrior/Headers/CWindow.h:22`); windows are identified by title/style, while numeric identity lives on *views* via `CView::SetID(viewID_*)` (enum `Hotline.h:47-227`).

### 2.3 Task / "thread" model

There are **no OS threads for protocol work**. The client uses a cooperative task scheduler:

- `CMyTask` (`HotlineTasks.cpp:15`) registers itself via `gApp->AddTask()` and each subclass implements `Process()` as an explicit **state machine** (a `switch (mStage)` with `goto goFromStart`). `Finish()` (`:49`) marks the task finished; `CMyApplication::KillFinishedTasks()` (`Hotline.cpp:8461`) later `delete`s it.
- `ProcessTasks()` (`Hotline.cpp:8476`) iterates all non-finished tasks and calls `Process()`. It is driven by `UApplication::PostMessage(msg_DataArrived)` (via `ScheduleTaskProcess()`, `:8681`) and by the AppWarrior run loop — i.e. the client pumps tasks on a message/timer cadence, not preemptively.
- Queuing: `QueueUnder()` (`HotlineTasks.cpp:31`, default `false`; overridden by transfer tasks) and `IsFirstQueuedTask()` (`Hotline.cpp:8440`) implement the transfer queue. A queued download sits in `mStage == 0` until it is first among queueing tasks, then advances (`HotlineTasks.cpp:2556-2563`). `bQueueTransfers` (pref) plus Shift-modifier controls whether a transfer enters the queue (`Hotline.cpp:2489`).
- `CMyTransactTask` (`HotlineTasks.cpp:60`) wraps a request/response transaction: `NewSendSession(inType)` (`:74`) delegates to `gApp->GetTransactor()->NewSendSession()`, and `Process()` polls `mTranSession->IsReceiveComplete()` then `ReceiveData(mFieldData)`.

### 2.4 Connection lifecycle

1. **Connect** — `DoConnect()` (`Hotline.cpp:2067`) parses `host:port` (`:2145-2155`), stashes login/password (`:2092-2099`), sets `mUseCrypt`, optionally prompts to disconnect first, then spawns `new CMyConnectTask` (`:2177-2182`). `CMyConnectTask` itself calls `gApp->MakeTransactor()` and `ScheduleTaskProcess()` (`HotlineTasks.cpp:103-107`).
2. **Transactor** — `MakeTransactor()` (`Hotline.cpp:8715`) creates `UTransact::New(transport_TCPIP, !bTunnelThroughHttp)` and sets the protocol version `SetVersion(0x484F544C, 2)` (i.e. magic `"HOTL"`, version 2) (`:8729-8730`). A single `mTpt` transactor multiplexes all transactions and server push messages.
3. **TCP connect** — `StartTransactorConnect()` (`:8763`) dials directly or through a SOCKS firewall (`StartConnectThruFirewall`, `:8765-8769`); `CMyConnectTask::Process()` polls `GetConnectStatus()` until `kConnectionEstablished` (`HotlineTasks.cpp:124-176`), then sets `mIsConnected` and constructs `CMyLoginTask` (`:175`).
4. **Login** — see §3 (the two-stage HOPE/crypt vs. normal handshake).
5. **Agreement/banner** — on success the server pushes `myTran_ShowAgreement`; `ProcessTran_ShowAgreement` (`Hotline.cpp:9332`) reads the agreement text and banner type, and for `URL ` banners starts `CMyGetBannerTask` (HTTP). The client then sends `myTran_Agreed` via `CMyAgreedTask` (`HotlineTasks.cpp:1717-1780`), which on completion sets `mIsAgreed`, calls `ShowConnected()`, starts `CMyGetOnlineUsersTask` and `ProcessStartupPath()` (`:1769-1778`).
6. **Session state** — `mIsConnected`, `mIsAgreed`, `mServerVers` (bitfield `Hotline.h:4183-4197`); `IsConnected()`/`GetServerVersion()` (`Hotline.cpp:1557-1566`); privilege checks `HasGeneralPriv`/`HasFolderPriv`/`HasBundlePriv` (`:1568-1581`) against `mMyAccess`, refreshed whenever the server pushes `myTran_UserAccess` (`ProcessTran_UserAccess`, `Hotline.cpp:9608`).
7. **Disconnect** — `DoDisconnect()` (`Hotline.cpp:2185`) deletes `gCrypt` (`:2189-2192`), then `KillTasksAndDisconnect()` (`:8516`), which kills transfers and disposes the transactor; `DisposeTransactor()` (`:8745`) resets `mServerVers/mIsConnected/mIsAgreed`. Abrupt loss is detected in `ProcessAll()` (`:8686`): if `mTpt` is no longer connected it processes any final pushed messages, calls `KillTasksAndDisconnect()`, and posts the "connection closed" alert (`:8690-8698`). An `AutoReconnect()` helper exists but is commented out (`:8692`, `:9071`).

### 2.5 Data path

A separate **transfer transport** is used for file data (not the transactor): `StartTransferTransportConnect()` (`Hotline.cpp:8774`) opens a second TCP connection to `basePort+1` (or `+3` through the HTTP tunnel). After the download transaction reply, `CMyDownloadFileTask` sends an `HTXF` header (`{0x48545846, refNum, 0, 0}`, `HotlineTasks.cpp:2661-2667`) and then streams the file through `mFile->ProcessUnflatten()` (`:2697`). This mirrors the Hotline two-channel design (control channel + data channel).

---

## 3. Protocol interactions table

The client **sends** transactions two ways: directly via `mTpt->SendTransaction(...)` (fire-and-forget) and via `NewSendSession(...)` (request/response inside a task). Incoming server *pushes* are dispatched in `CMyApplication::ProcessIncomingData()` (`Hotline.cpp:8614-8684`). Transaction/field IDs are defined in `Apps/Common Files/HotlineClientServerCommon.h`.

| Txn (ID) | C/S | Purpose | Evidence (file:line) |
|---|---|---|---|
| `myTran_Login` (107) | sends | 2-stage login (HOPE crypt: empty login/pw + `myField_MacAlg`,`myField_C_CipherAlg`; reply carries `SessionKey`/`MacAlg`/`S_CipherAlg`; then HMAC'd login/pw + chosen cipher; normal path: bitwise-NOT-scrambled login/pw + `myField_Vers=197`) | `HotlineTasks.cpp:1459-1650` |
| `myTran_Agreed` (121) | sends | accept agreement; payload = `GetClientUserInfo()` (`UserName`,`UserIconID`,`Options`,`AutomaticResponse`) | `HotlineTasks.cpp:1717-1744`, `Hotline.cpp:6801` |
| `myTran_ShowAgreement` (109) | handles | server agreement text + banner type (`ServerBannerType`/`ServerBanner`/`ServerBannerUrl`/`NoServerAgreement`) | `Hotline.cpp:9332` |
| `myTran_ServerBanner` (122) | handles | rotated banner (`URL` → HTTP fetch, else inline image → `CMyDownloadBannerTask`) | `Hotline.cpp:9422` |
| `myTran_ChatSend` (105) | sends | chat line; `myField_Data` (≤2048 B), optional `myField_ChatOptions=1` (action), optional scram marker `0xCA` | `Hotline.cpp:2196-2277` |
| `myTran_ChatMsg` (106) | handles | chat line or private-chat line (`myField_ChatID`, `myField_Data`) | `Hotline.cpp:9139` |
| `myTran_SendInstantMsg` (108) | sends | private instant message (`CMySendPrivMsgTask`) | `HotlineTasks.cpp:1309` |
| `myTran_ServerMsg` (104) | handles | inbound instant message | `Hotline.cpp:9107` |
| `myTran_NewMsg` (102) | handles | news "message board" post (pre-1.5 news) | `Hotline.cpp:9090` |
| `myTran_GetMsgs` (101) | sends | fetch old news message board (`CMyGetOldNewsTask`) | `HotlineTasks.cpp:183` |
| `myTran_PostMsg` (103) | sends | post old news (`CMyOldPostNewsTask`) | `HotlineTasks.cpp:1153` |
| `myTran_InviteNewChat` (112) | sends | create/invite to private chat | `HotlineTasks.cpp:6504` |
| `myTran_InviteToChat` (113) | handles | incoming chat invite | `Hotline.cpp:9470` |
| `myTran_RejectChatInvite` (114) | sends | decline invite | `Hotline.cpp:9483`, `:6123` |
| `myTran_JoinChat` (115) | sends | accept invite | `HotlineTasks.cpp:6580` |
| `myTran_LeaveChat` (116) | sends | leave private chat | `Hotline.cpp:6112` |
| `myTran_NotifyChatChangeUser` (117) | handles | chat member join/rename | `Hotline.cpp:9514` |
| `myTran_NotifyChatDeleteUser` (118) | handles | chat member leave | `Hotline.cpp:9566` |
| `myTran_NotifyChatSubject` (119) | handles | chat subject change | `Hotline.cpp:9596` |
| `myTran_SetChatSubject` (120) | sends | set chat subject | `Hotline.cpp:6187` |
| `myTran_SetClientUserInfo` (304) | sends | set own name/icon/options (post-login, icon change, refuse flags) | `Hotline.cpp:1875`, `:6327`, `:6335`, `HotlineTasks.cpp:1686` |
| `myTran_DisconnectUser` (110) | sends | admin: disconnect a user | (via `CMyDisconnectUserTask`) |
| `myTran_DisconnectMsg` (111) | handles | "you were disconnected by admin" | `Hotline.cpp:9130` |
| `myTran_IconChange` (123) | sends | force another user's icon | `HotlineTasks.cpp:5292` |
| `myTran_NickChange` (124) | sends | force another user's nick | `HotlineTasks.cpp:5714` |
| `myTran_FakeRed` (125) | sends | fake "red" (busy) flag on a user | `HotlineTasks.cpp:5399` |
| `myTran_Away` (126) | sends | set away state | `HotlineTasks.cpp:5660` |
| `myTran_CrazyServer` (127) | sends | server-side easter egg | `HotlineTasks.cpp:5552` |
| `myTran_BlockDownload` (128) | sends | block a user's downloads | `HotlineTasks.cpp:5346` |
| `myTran_Visible` (129) | sends | toggle visibility | `HotlineTasks.cpp:5502` |
| `myTran_AdminSpector` (130) | sends/handles | request admin info on a user / push admin info | `HotlineTasks.cpp:5605`, `Hotline.cpp:9658` |
| `myTran_StandardMessage` (131) | sends | send canned server message | `HotlineTasks.cpp:5451` |
| `myTran_GetFileNameList` (200) | sends | list folder contents | `HotlineTasks.cpp:277` |
| `myTran_DownloadFile` (202) | sends | request file download (`FileName`,`FilePath`,`FileResumeData`) | `HotlineTasks.cpp:2590`, `:3015` |
| `myTran_UploadFile` (203) | sends | upload file | `HotlineTasks.cpp` (upload task) |
| `myTran_DeleteFile` (204) | sends | delete file/folder | `CMyDeleteFileTask` |
| `myTran_NewFolder` (205) | sends | create folder | `CMyNewFolderTask` |
| `myTran_GetFileInfo` (206) | sends | file/folder info+comment | `CMyGetFileInfoTask` |
| `myTran_SetFileInfo` (207) | sends | rename/set comment/type | `CMySetFileInfoTask` |
| `myTran_MoveFile` (208) | sends | move file | `CMyMoveFileTask` |
| `myTran_MakeFileAlias` (209) | sends | make alias | `CMyMakeFileAliasTask` |
| `myTran_DownloadFldr` (210) | sends | folder (archive) download | `HotlineTasks.cpp:3399` |
| `myTran_DownloadInfo` (211) | handles | download queue position update (`RefNum`,`WaitingCount`) | `Hotline.cpp:9665` |
| `myTran_DownloadBanner` (212) | sends | fetch inline banner image | `HotlineTasks.cpp:3797` |
| `myTran_UploadFldr` (213) | sends | folder upload | `CMyUploadFldrTask` |
| `myTran_KillDownload` (214) | sends | cancel a queued server download | `CMyKillDownloadTask` |
| `myTran_GetUserNameList` (300) | sends | online users list | `HotlineTasks.cpp:1420` |
| `myTran_NotifyChangeUser` (301) | handles | user joined/changed | `Hotline.cpp:9245` |
| `myTran_NotifyDeleteUser` (302) | handles | user left | `Hotline.cpp:9208` |
| `myTran_GetClientInfoText` (303) | sends | fetch another client's info | `CMyGetClientInfoTask` |
| `myTran_GetUserList` (348) | sends | admin: full account list | `CMyGetUserListTask` |
| `myTran_SetUserList` (349) | sends | admin: save account list | `CMySetUserListTask` |
| `myTran_NewUser` (350) | sends | admin: create account | `CMyNewUserTask` |
| `myTran_DeleteUser` (351) | sends | admin: delete account | `CMyDeleteUserTask` |
| `myTran_GetUser` (352) | sends | admin: fetch one account | `CMyOpenUserTask` |
| `myTran_SetUser` (353) | sends | admin: modify account | `CMySetUserTask` |
| `myTran_UserAccess` (354) | handles | own access mask update | `Hotline.cpp:9608` |
| `myTran_UserBroadcast` (355) | sends/handles | broadcast message to all users | `HotlineTasks.cpp:1354`, `Hotline.cpp:9652` |
| `myTran_GetNewsCatNameList` (370) | sends | news category list | `HotlineTasks.cpp:526` |
| `myTran_GetNewsArtNameList` (371) | sends | article list in a category | `HotlineTasks.cpp:942` |
| `myTran_DelNewsItem` (380) | sends | delete category/folder | `HotlineTasks.cpp:884` |
| `myTran_NewNewsFldr` (381) | sends | new news folder | `HotlineTasks.cpp:762` |
| `myTran_NewNewsCat` (382) | sends | new news category | `HotlineTasks.cpp:814` |
| `myTran_GetNewsArtData` (400) | sends | fetch article body | `HotlineTasks.cpp:1032` |
| `myTran_PostNewsArt` (410) | sends | post article/reply | `HotlineTasks.cpp:1253` |
| `myTran_DelNewsArt` (411) | sends | delete article | `HotlineTasks.cpp:1199` |
| `myTran_KeepConnectionAlive` (500) | sends | keepalive every 180 s (`kKeepConnectionAliveTime`, `Hotline.h:…`, `Hotline.cpp:8796`) | `HotlineTasks.cpp:1393` |

**Login handshake details.** In the crypt path (`mUseCrypt`), stage 1 sends `myField_MacAlg = {0,2,9,"HMAC-SHA1",8,"HMAC-MD5"}` (offering SHA-1 then MD5) and `myField_C_CipherAlg = {0,1,8,"BLOWFISH"}` (`HotlineTasks.cpp:1475-1479`). The server replies with `myField_SessionKey`, its `myField_MacAlg`, and `myField_S_CipherAlg`; the client validates the session key length (≥32), matches `HLSha1`/`HLMD5` by name, matches `HLBlowfish`, runs `SelfTest()`, HMACs login and password with the session key, and resends (`:1556-1640`). It then builds `gCrypt = new HLCrypt` initialized with Blowfish + the hash + session key (`:1643-1644`). `gCrypt` is torn down on disconnect (`Hotline.cpp:2189-2192`).

**Wire framing.** Fields are serialized by `UFieldData` (AppWarrior `UFieldData.h`): a header of field-count/sizes followed by a flat data region; `AddField`/`AddInteger`/`AddPString`/`GetPString`/`GetInteger` are the client's only accessors. Transactions are framed by `UTransact`/`URegularTransport` with the `HOTL` v2 version negotiated in `MakeTransactor()`.

**Tracker protocol** is *not* a Hotline transaction — see §4.6.

---

## 4. Feature map

### 4.1 File browser (lists / icons / folders / drop boxes / comments / permissions)

- Implemented as `CMyFileListWin` (`CMyItemsListWin`+`CMyFileWin`), `CMyFileTreeWin`, `CMyFileExplWin` (`Hotline.h:2006-2128`), built over `CMyFileListView` (`CMyListStatusView`) and `CMyFileTreeView` (`CMyTreeStatusView<SMyFileItem>`) (`Hotline.h:998-1105`). Three window styles (`optWindow_List/Tree/Expl`, pref `nFilesWindow`).
- Lists come from `myTran_GetFileNameList` (`CMyGetFileListTask`, `HotlineTasks.cpp:277`); results are cached in `CMyCacheList` (30-entry LRU, `HotlineMisc.cpp:86`). `SetFolderWinContent()` routes a list to the matching open window (`Hotline.h:4443`).
- Icons: `HotlineFileTypeToIcon()` maps type/creator to rez icon IDs; the user-icon picker `CMyOptIconTab`/`CMyIconPickView` uses a 91×7 icon grid (`Hotline.h:1406`, `HotlineViews.cpp:123-133` icon id table).
- Permissions are enforced locally per-window via `SetAccess()` gated on `HasFolderPriv(myAcc_*)` (e.g. `myAcc_ViewDropBoxes`, `myAcc_DownloadFolder`, `myAcc_UploadFile`, `myAcc_DeleteFile`, `myAcc_MakeAlias`, `myAcc_SetFileComment` — privileges in `HotlineClientServerCommon.h`). Comments set via `CMyFileInfoWin` → `CMySetFileInfoTask` (`myTran_SetFileInfo`).
- **Modern replacement:** a `hotline::client::FileBrowser` service exposing directory listing/cache/permission model, with the tree/list/explorer rendered by a modern widget layer.

### 4.2 Transfers (download / upload / resume / folder download)

- **Download** `CMyDownloadFileTask` (`HotlineTasks.cpp:2550`): request → parse `TransferSize`/`FileSize`/`RefNum`/`WaitingCount` → open data channel (`basePort+1`) → send `HTXF` header → stream into `mFile->ProcessUnflatten()`. Filename validated by `UFileSys::ValidateFileName` (`Hotline.cpp:2432`). Partial downloads are written with a `.hpf` extension and the extension stripped on completion (`:2719-2725`); the partial-file association `\phpf` is registered at startup (`:255-256`).
- **Resume** `myField_FileResumeData` (`HotlineTasks.cpp:2574-2582`, `mFile->ResumeUnflatten()` `:2443`). Server-side queue supported via `myTran_DownloadInfo` and `myField_WaitingCount` (`Hotline.cpp:9665`).
- **Upload** `CMyUploadFileTask`/`CMyUploadFldrTask`: `StartFlatten()` then `ProcessFlatten()` streams the local file up (`HotlineTasks.cpp:4055-4258`); resume uses the server-returned `resumeData` (`:4816`).
- **Folder download** `CMyDownloadFldrTask` (`:3367`) streams an archive decoded by `CMyArchiveDecoder` (`HotlineArchiveDecoder.cpp`); `HotlineFolderDownload.cpp` (`CMyDLItem`/`CMyDLFldr`) recursively mirrors the folder tree locally and even carries an auto-launch file ref (`HotlineArchiveDecoder.h:52`).
- **View file** `CMyViewFileTask` (`HotlineTasks.cpp:2785`) reuses download; text/images render in `CMyViewFileWin`, QuickTime media streams via `CMyQuickTimeView` (`:2945-2970`, `HotlineWindows.cpp:3943-3956`).
- **Modern replacement:** a `hotline::client::TransferEngine` (queue, resume, archive streaming) with `httpx`/`boost::asio`-style IO, decoupled from QuickTime (use platform media players).

### 4.3 News reader

- `CMyNewsCategoryListWin/TreeWin/ExplWin` over `CMyNewsCategoryListView`/`TreeView` (`HotlineNews.cpp:801-1905`). Category list: `myTran_GetNewsCatNameList` (`CMyGetNewsCatListTask`, `HotlineTasks.cpp:526`); supports old (`myField_NewsCatListData`) vs 1.5+ (`myField_NewsCatListData15`) formats. New folder/category/delete via `myTran_NewNewsFldr`/`NewNewsCat`/`DelNewsItem`.
- Article threads: `CMyNewsArticleTreeView` (`HotlineNews.cpp:29`) renders parent/child article tree; fields `NewsArtParentArt`/`1stChildArt`/`PrevArt`/`NextArt` thread navigation. Article body: `myTran_GetNewsArtData` with `myField_NewsArtDataFlav` (`hlNewsFlav_plain_text=1`, `jpeg=10`, `gif=11`).
- Post/reply/delete: `myTran_PostNewsArt` (`CMyPostNewsTextArticle`, `HotlineTasks.cpp:1253`), `myTran_DelNewsArt` (`CMyNewsDeleteArticTask`, `:1199`). Old (pre-1.5) message board still present behind `#if !NEW_NEWS` (e.g. `CMyGetOldNewsTask`, `CMyOldPostNewsTask`).
- **Read history** `CMyNewsReadHistory`/`CNZReadList` (`HotlineNewsReadHistory.cpp`, `Hotline.h:616-707`), persisted under `Data/News History` (`GetNewsHistRef`, `Hotline.cpp:7628`); unread articles are flagged in the tree. Enabled by `USE_NEWS_HISTORY=1` (`Hotline.h:10`).
- **Modern replacement:** a `hotline::client::NewsService` (categories/articles/threads/read-state) with JSON persistence; rendering in a webview or native text widget.

### 4.4 Users list / private chat / chat rooms / instant messages

- Users list `CMyUserListWin` over `CMyUserListView` (`Hotline.h:1134`, `2397`); populated by `myTran_GetUserNameList` (`SetOnlineUsers`, `Hotline.cpp:6795`); live updates via `myTran_NotifyChangeUser`/`NotifyDeleteUser` (`:9245`/`:9208`).
- Public chat `CMyPublicChatWin` (`Hotline.h:1778`); `DoChatSend` (`Hotline.cpp:2196`) enforces `myAcc_SendChat`, 2048-byte cap, chat scram (`ChatScram`, `:6845` — **dead in this build**: `mScramChat` is only set `true` inside a `#if 0` block, `Hotline.cpp:4869`) and pork-mode easter egg (`:2217`).
- Private chat `CMyPrivateChatWin` (`Hotline.h:1802`, also `CLink`/`CDragAndDropHandler`); invite flow `InviteNewChat`/`InviteToChat`/`JoinChat`/`RejectChatInvite`/`LeaveChat`/`SetChatSubject` (§3). Auto-refuse/auto-response options send `myField_Options` bits in `GetClientUserInfo` (`:6801`).
- Instant messages `CMyPrivMsgWin`/`CMySendPrivMsgWin` (`Hotline.h:1517`, `1602`); sent via `myTran_SendInstantMsg` (`HotlineTasks.cpp:1309`), received via `myTran_ServerMsg` (`Hotline.cpp:9107`).
- **Modern replacement:** a `hotline::client::ChatService` (rooms, presence, invites, messages) driving native or web chat UI.

### 4.5 Admin inspector & account editor

- **AdmInSpector** (online-user inspection): `CMyAdmInSpectorWin` + `CMyAdmInSpectorTreeView` (`Admin/HotlineAdminWindows.cpp:465`, `Admin/HotlineAdminViews.cpp:1453`) lists online users and exposes `myTran_AdminSpector` (`CMyAdminSpectorTask`, `HotlineTasks.cpp:5605`) plus the extended admin actions (`FakeRed`, `Away`, `Visible`, `BlockDownload`, `IconChange`, `NickChange`, `StandardMessage`, `DisconnectUser`) — the same actions reachable from the hidden secret console (§4.8).
- **Full account editor** `CMyAdminWin` + `CMyAdminUserTreeView` + `CMyAdminUserAccessView`/`CMyAdminAccessCheckBoxView` (`Admin/HotlineAdminWindows.cpp:18`, `Admin/HotlineAdminViews.cpp:8-1350`): lists all accounts (`myTran_GetUserList`), edits the 64-bit `SMyUserAccess` mask (55 defined privilege bits; 51 exposed as tri-state checkboxes) via a checkbox grid, and saves/reverts via `myTran_SetUserList`, with per-user `NewUser`/`DeleteUser`/`GetUser`/`SetUser` (`HotlineAdminViews.cpp:804` builds the save list). This is effectively a full *server* account editor embedded in the client.
- **Modern replacement:** keep privilege-bit semantics in `hotline::client::Admin`, but gate the account editor behind explicit server-admin auth; render with a modern form/table UI.

### 4.6 Tracker client

- `CMyTracker` (`HotlineTracker.cpp:686`) manages a list of tracker servers (`SMyTrackerInfo`), shown in `CMyServerWindow`/`CMyServerTreeView` (`HotlineTracker.cpp:…`, `Hotline.h:2916-2985`). Trackers are added/edited/removed via `CMyTrackServWindow` (`HotlineTracker.cpp:465+`); server entries are filterable (word search, `FilterServerList`, `:1369`).
- **Protocol:** raw TCP, port **5498** (direct) or **5497** (HTTP-tunneled) (`HotlineTasks.cpp:5926-5928`). Client sends a header `HTRK\0\1` (magic + version 1; the `NEW_TRACKSERV=0` variant with login/password is **dead code** — see §7), waits for the `HTRK` ack, then reads `{type:u16, len:u16, data}` messages; type 1 carries `{count:u16, entries…}` which `AddListFromData` parses into `SMyServerInfo` (`HotlineTasks.cpp:6020-6090`, `HotlineTracker.cpp:995`). Default trackers hardcoded: `saddle.dyndns.org`, `dmp.fimble.com`, `pcempirez.dynip.com`, `lovetrain.nu`, `Hotline.kicks-ass.net`, `supertracker.kicks-ass.org` (`Hotline.cpp:8127-8199`), used only when the custom-distribution file is corrupt/missing.
- **Modern replacement:** replace the bespoke TCP tracker protocol with HTTPS+JSON; a `hotline::client::TrackerService` with pluggable tracker sources.

### 4.7 Bookmarks / address book

`DoSaveServerBookmark` (`Hotline.cpp:2385`) writes a `.hbm` file (type `'HTbm'`) into `Bookmarks/`; `SaveServerFile` (`:7559`) serializes `SMyServerConfig` (format `'HTsc'`, address+login+**plaintext password**+useCrypt). `ReadServerFile`/`OpenDocument` handle double-click association. No in-app address book beyond the file-based bookmarks and the "Save Connect" autoconnect record (`mSaveConnect`, `SaveConnect`, `:9036`).

### 4.8 Secret console / easter eggs

`DoSecret()` (`Hotline.cpp:4543`, bound to Ctrl+F12 `viewID_Secret`) opens `CMySecretWin` and parses 3-letter command codes that trigger the extended admin tasks: `ICO` (icon change), `NIK` (nick change), `MSG` (standard message), `VIS` (visible), `BLK` (block download), `CRA` (crazy server), `ADM` (admin spector), `AWAY` (`:4557-4700`). A further CRC-keyed block dispatches hidden chat commands: `"icon"`, `"anthedge"`, `"fuelharp"` (leech rating), `"stats"`, and disabled ones (`"hydrowart"` scram-chat, `"brokerguard"` baconizer/pork mode, `"hoozyurdaddy"` secret sound) inside `#if 0` (`:4740-4893`).

---

## 5. Platform-specificity inventory

| Concern | MACINTOSH | WIN32 |
|---|---|---|
| Conditional-compilation sites (approx) | `#if MACINTOSH` appears in `Hotline.cpp` (27), `HotlineWindows.cpp` (24), `HotlineViews.cpp` (5), `HotlineTasks.cpp` (4), `HotlineNews.cpp` (4), `Hotline.h` (4), `CChatLog.cpp` (3) | `WIN32`/`_WIN32` in `HotlineWindows.cpp` (40), `Hotline.cpp` (23), `HotlineTasks.cpp` (9), `HotlineAdmInSpector.cpp` (8), `HotlineTracker.cpp` (8), `HotlineNews.cpp` (6), others smaller |
| Resources | `hlc19.dat` rez file (Mac + Windows share it); `gMacMenuBarID=128`; `_InstallURLHandler()`; `_AddResFileToSearchChain()` | `hotline.rc`, `_SetWinIcon()`, `_ActivateNextWindow()`, `_IsTopVisibleModalWindow()`, `_gUseMDIWindow`/`_gMDIWindowMenu` (MDI mode, commented out in `main()` `Hotline.cpp:45-51`) |
| Fonts | `kMyDefaultFixedFontSize=9` (small) | `=12` |
| Text/chat | `"\r"` line endings; `UText::CompareInsensitive` paths | `"\r\n"`; `.hpf`/`.hbm` suffix append (`Hotline.cpp:2394-2396`, `:2718`) |
| Network | HTTP tunnel uses different port math both platforms share | same |
| Listing | `LIST_BACKGROUND_OPTION = scrollerOption_NoBkgnd` | `0` (`Hotline.h:31-34`) |

The client is predominantly **portable AppWarrior code**; Windows-specific code is concentrated in `HotlineWindows.cpp` (window creation/icon plumbing) and the `.rc`. There are **no separate Windows-only source files** in `Apps/Client` — all `#if WIN32` is inline.

---

## 6. Security-sensitive patterns

1. **Plaintext password storage.** `SaveServerFile` writes `SMyServerConfig.passwordData` verbatim to `.hbm` bookmarks (`Hotline.cpp:7576-7584`), and `SaveConnect` stores `psPassword` verbatim (`:9051`). Pref `SMyPrefs` also holds `unlockCode` in clear (`:7813`). *Fix:* encrypt at rest (OS keychain/DPAPI) or store only a token.
2. **Weak login obfuscation (non-crypt path).** Normal login "scrambles" credentials with a per-byte bitwise NOT (`*p = ~(*p)`, `HotlineTasks.cpp:1494-1507`) — trivial to reverse on the wire. The stronger HMAC/Blowfish path exists but is optional (`mUseCrypt`). *Fix:* always use the crypt path; replace with TLS.
3. **No transport encryption by default.** `UTransact::New(transport_TCPIP, …)` is raw TCP (`Hotline.cpp:8728`); `HLCrypt` (Blowfish/HMAC) is only session-encryption, not authenticated channel encryption. *Fix:* wrap the wire in TLS.
4. **Download path traversal surface.** Server-supplied file names are sanitized by `UFileSys::ValidateFileName` which replaces `\\ / : * ? " < > |` and control chars (`AppWarrior/Source/Files/UFileSys(W).cpp:924-975`), and `.lnk` is stripped (`Hotline.cpp:2436`). Folder-download sub-paths are validated per component by `UFS::ValidateFilePath` (`HotlineTasks.cpp:3473`, `UFileSys.cpp:118-140`), which rejects `..`. However the *original* unsanitized server name is retained in `mFileFldrName` and later concatenated into a URL in `DoViewDloadFile` (`Hotline.cpp:9698-9729`) — a URL-injection surface (not a file write). *Fix:* canonicalize and verify every path component under the download root, and URL-encode user/servable text before `LaunchURL`.
4b. **Server-trusted transfer size vs unbounded disk writes.** `CMyDownloadFileTask`/`CMyDownloadFldrTask` stream `mTpt->ReceiveBuffer()` straight into `mFile->ProcessUnflatten()` (`HotlineTasks.cpp:2691-2706`, `:3592-3625`), finishing only when `mDownloadedSize >= mTotalSize` where `mTotalSize` is the server's `myField_TransferSize`. A malicious server can declare a small size while sending far more — the client keeps writing to disk (oversized-file/disk-exhaustion). *Fix:* cap written bytes to the declared size and abort on overflow.
4c. **Server-controlled allocation sizes.** Login crypto allocates `new UInt8[GetFieldSize(...)]` for session key/MAC/cipher (`HotlineTasks.cpp:1570,1587,1613`; only the session key is length-checked), and `CMyDownloadBannerTask` allocates `mBanner = UMemory::New(mBannerSize)` from `myField_TransferSize` (`:3886`). *Fix:* bound all sizes before allocation.
5. **Fixed-buffer Pascal strings.** `mUserName[32]`, `mUserLogin[33]`, `mUserPassword[33]`, `mServerName[32]` (`Hotline.h:4155-4159`) are copied with `UMemory::Copy(dst, src, src[0]+1)` in many places; `ReadPrefs` copies `info.userName[0]+1` bytes from a 33-byte field into `mUserName[32]` (`Hotline.cpp:7899`) — a potential 1-byte over-read/write if the length byte is 32. No bounds are checked on `mUserName` writes (`Hotline.cpp:1702, 5456, 8036`). *Fix:* length-checked Pascal-string helpers and fixed-capacity checked copy.
6. **Network data into fixed buffers.** Chat lines (`ProcessTran_ChatMsg`) read `myField_Data` size then allocate `StPtr buf(s)` and `GetField` (`Hotline.cpp:9150-9160`) — size is honored; but other handlers use fixed 256-byte `Uint8 str[256]` with `GetPString` (which is capped by `UFieldData`). `CMyGetTrackServListTask` trusts a 16-bit `mMsgDataSize` from the tracker and `Reallocate`s to it (`HotlineTasks.cpp:6050-6053`) — a malformed tracker could force a large allocation; the header length field is otherwise unbounded. *Fix:* cap message sizes against sane maxima.
7. **Banner URL handling.** `CMyGetBannerTask::CheckURL` (`HotlineTasks.cpp:2096`) restricts banner click-through to `http://`, `mailto:`, `hotline:` and rebuilds relative URLs against the banner host — a deliberate (legacy) mitigation, but `CMyGetBannerTask::SearchImageTag` (`:2033`) scans banner HTML for image URLs and `CMyLaunchUrlTask` later launches the chosen URL. Banner content itself is parsed as HTML from an untrusted server. *Fix:* sanitize HTML, allowlist schemes, and sandbox banner rendering.
8. **Chat-log XML injection.** `CChatLog::AppendLog` manually escapes only `<`, `>`, `&` (not quotes) into a 1024-byte buffer with the comment "we hope for no overflow" (`CChatLog.cpp:74-120`). Truncation is bounded, but unescaped attributes and the `from='%#s'` user field are injection risks into the local `ChatLog.xml`. *Fix:* use a proper XML serializer and escape all text/attribute data.
9. **Tracker response parsing into fixed structs.** `SMyTrackServInfo` (packed addr/port/userCount[8]/name/desc) and `AddListFromData` parse untrusted tracker bytes; the 2-byte count is honored but individual entry lengths derive from the stream (`HotlineTracker.cpp:995+`). *Fix:* bounds-checked binary reader.
10. **`memcmp` on server-selected algorithm names** (`serverHash.data+2` vs `HLSha1::GetName()`, `HotlineTasks.cpp:1585-1597`) — safe here because lengths come from `GetFieldSize`, but the pattern of trusting a server string to select crypto is fragile. *Fix:* compare against a whitelist with fixed lengths.
11. **CRC-based "secret" backdoor.** The client accepts admin-ish actions from a hidden UI (`DoSecret`) and from chat commands matched by `UMemory::CRC(code, -37)` (`Hotline.cpp:4543-4893`). Privilege checks exist (`HasGeneralPriv`), but the mechanism is a latent abuse surface.
12. **Missing privilege checks on remote-admin actions.** `DoSecret()` fires `CMyIconChangeTask`/`CMyNickChangeTask`/`CMyStandardMessageTask`/`CMyVisibleTask`/`CMyBlockDownloadTask`/`CMyCrazyServerTask`/`CMyAdminSpectorTask`/`CMyAwayTask` with **no `HasGeneralPriv` gate** (`Hotline.cpp:4563-4728`); `DoAdmInSpector`'s `myAcc_OpenUser` check is commented out, leaving only a server-version gate (`Hotline.cpp:5663, 5681-5689`); the `DoSendFakeRed`/`DoSendAway`/`DoSendVisible`/`DoBlockDownload`/`DoChangeIcon` wrappers (`Hotline.cpp:3979-4001`) rely solely on button enablement, not re-checking. *Fix:* enforce privileges server-side (client checks are advisory) and add client-side checks.
13. **Unbounded copy into `mServerBannerURL[256]`.** In the agreement path the banner URL handle is copied with no cap: `mServerBannerURL[0] = UMemory::Copy(mServerBannerURL+1, pServerBannerUrl, nUrlSize)` (`Hotline.cpp:9409`) — a heap overflow on `CMyApplication`; the sibling `ProcessTran_ServerBanner` path is correctly bounded (`Hotline.cpp:9451-9454`). *Fix:* clamp to `sizeof(mServerBannerURL)-1`.
14. **`.hpf` append overflows a 256-byte stack buffer.** `pstrcat(mValidatedName, "\p.hpf")` (`HotlineTasks.cpp:2448`) and `pstrcat(str, "\p.hpf")` (`:3632`) append 4 bytes to names already capped at 255 by `ValidateFileName` — up to a 4-byte overflow of `Uint8[256]` locals. *Fix:* reserve room for the suffix before `ValidateFileName`/append.
15. **Duplicate field IDs.** `myField_Visible = 112` collides with `myField_UserFlags = 112`, and `myField_number = 114` collides with `myField_ChatID = 114` (`HotlineClientServerCommon.h:106/108/117/121`). `CMyVisibleTask` therefore sends its flag under the `UserFlags` number. *Fix:* renumber in a protocol revision; at minimum document the aliasing.
16. **Login/account "scrambling" is reversible.** Beyond the login NOT-scramble, the admin account editor scrambles stored login/password with the same bitwise NOT (`HotlineAdminViews.cpp:171-173,532-535`, `HotlineTasks.cpp:4973-4984,5101-5114`); `SMyUserDataFile` carries `loginData[32]`/`passwordData[32]` this way. *Fix:* treat these as plaintext; encrypt with real keys.
17. **News category parser reads unvalidated lengths.** The 1.5+ category-list parser copies `nameSize` (and other counts) from the server's `data[300]` buffer without validating against the buffer length, risking an uninitialized-stack read (`HotlineNews.cpp:877-892, 1286-1316`). *Fix:* validate all sizes before `UMemory::Copy`.
18. **News folder-delete privilege bypass.** Old-format type codes (1=folder/10=category) vs new (2=bundle/3=category) mismatch in `DoNewsFldrTrash` (`Hotline.cpp:3246-3264`) and `CMyDeleteNewsFldrItemTask` (`HotlineTasks.cpp:857-868`) can let a category be deleted under the folder-delete permission path. *Fix:* normalize type codes in one place.
19. **Tracker parsing: unaligned/type-punned reads and unbounded `pstrcpy`.** `AddListFromData` reads `addr = *((Uint32*)p)++` (unaligned, endian-dependent) and copies Pascal strings without slack (`HotlineTracker.cpp:1016-1034`), and `ANSI.cpp`'s `pstrcpy` is unbounded (`AppWarrior/Source/Misc/ANSI.cpp:1023-1035`). *Fix:* bounds-checked, byte-wise decode.
20. **Broken `AutoReconnect`** (`Hotline.cpp:9071-9089`): a comma expression discards the format string and always reconnects to `lorbac.net`. Disabled (call commented at `:8692`), but a trap if re-enabled. *Fix:* delete or rewrite.
21. **`HasGeneralPriv`/`HasFolderPriv`/`HasBundlePriv` are identical** (`Hotline.cpp:1567-1581`) — all three read the single `mMyAccess`; per-category privileges are not actually distinguished. *Fix:* map to the correct privilege dimensions or consolidate.
22. **`StartUp()` ordering bug.** `mTracker->ReadPrefs(...)` is called (`Hotline.cpp:299`) before `mTracker` is constructed (`:305`) — reading through an uninitialized member. *Fix:* construct `mTracker` first.
23. **Operator-precedence NULL-deref in `CMyServerInfoWin::IsThisServer`.** `if (!mTempFile.IsValidFile() || !inServerName && !inServerName[0] || !inData)` (`HotlineWindows.cpp:4364`, sibling at `:4309`) — `&&` binds tighter, so a **NULL** `inServerName` dereferences NULL at `!inServerName[0]`, and an **empty** non-NULL name is *not* rejected. *Fix:* `!inServerName || !inServerName[0]`.
24. **Unaligned 16-bit store on attacker-influenced offset.** `*(Uint16 *)(buf+sa+1) = sb;` in `CMyFileListView::Drop`/`CMyFileTreeView::Drop` (`HotlineViews.cpp:764-766, 1468-1470`) where `sa` derives from the drag payload (≤63) — potential misaligned write (bounded by `buf[2048]`). *Fix:* byte-wise store.
25. **Unbounded extension append.** `strlen(pExtension)` is copied into `Uint8 psFileName[256]` after the server name (`HotlineWindows.cpp:4352`) — server name + extension must together fit 256 with no clamp. *Fix:* size-capped copy.
26. **Dormant uninitialized return in `HotlineFileTypeToIcon`.** Local `Int16 id;` is unassigned if `gApp->mIconSet ∉ {1,2}` (`HotlineViews.cpp:1871+`); currently prefs force 1 or 2 (`Hotline.cpp:387-393`) so it is latent. *Fix:* initialize and add a `default`.
27. **Off-by-one stack write + over-read in `CChatLog::AppendLog`.** `Uint8 usrNameBuf[64]`, `u = usrNameBuf+1`, and the loop `for(; p<pe && *p != ':'; ++p) { *u++ = *p; }` (`CChatLog.cpp:85-93`) can write 64 bytes into indices 1..**64** — one past the buffer — when the first 64 message bytes contain no `:`; then `msg[min(msgZ,64)]` is a 1-byte over-read when `msgZ == 64`. *Fix:* bound the loop to `sizeof(usrNameBuf)-1`.
28. **Unbounded allocations from network-supplied sizes.** `mBufferSize = inTotalSize; mBuffer = UMemory::NewClear(mBufferSize);` (`HotlineWindows.cpp:3859` — server file size); `UMemory::NewHandle(inData->GetFieldSize(myField_Data))` (`HotlineWindows.cpp:7044`); and `UMemory::New(nDataSize)` from a 4-byte length in the persisted old-format `CHttpIDList` file (`HotlineMisc.cpp:398`). None cap the size → OOM/DoS. Contrast with the correctly clamped sites (`HotlineViews.cpp:483-485,1013-1015,2472-2475`; `HotlineMisc.cpp:451-456`). *Fix:* cap every allocation against a sane maximum.
29. **`CMyBannerWin::SetBanner` returns success when disabled.** The whole build is wrapped in `if (!gApp->mNoBanner) { … return true; }` (`HotlineWindows.cpp:5108`) but the function ends with an unconditional `return true;` (`:5358`) — with banners off it still reports success without building anything. *Fix:* return `false` in the disabled case.

---

## 7. Dead / historical code inventory

1. **`HotlineAdmInSpector.cpp` is a stale duplicate of `HotlineTracker.cpp`** — `diff` shows only 8 differing lines (copyright header and one class qualifier `CMyAdmITreeView::CMyServerTreeView`, which is itself inconsistent with the declared class `CMyServerTreeView`). The real admin-inspector window lives in `Admin/HotlineAdminWindows.cpp`. The file appears to be a failed rename/refactor artifact (2,042 dead LOC).
2. **`#if 0` blocks:** `Hotline.cpp:4765, 4776, 4849, 4876, 4893` (scram-chat, baconizer, secret sound, send-buffer tuning); `HotlineNewsReadHistory.cpp:51`. Additional disabled features: MDI mode (`main()` `:45-51`), `AutoReconnect()` call (`:8692`), commented QuickTime type gate (`:2460-2478`), commented `hotlineisp.com` URLs (`:1146-1152`).
3. **Old (pre-1.5) news** behind `#if !NEW_NEWS`: `CMyGetOldNewsTask`, `CMyOldPostNewsTask`, `CMyOldNewsWin` (compiled out because `NEW_NEWS=1`, `Hotline.h:6`).
4. **New tracker protocol** `NEW_TRACKSERV` is never defined → the `HTRK\0\2` + login/password header path (`HotlineTasks.cpp:5940-5957`, `:6020`, `:6061`) is dead; the client uses the version-1 protocol.
5. **Commented-out old prefs migration** (`GetPrefsRef`, `Hotline.cpp:7650-7671`), commented rez-file alternatives (`LoadRezFile`, `:7611-7614`), commented `hltracker.com` trackers (`:8193-8199`).
6. **`CMyAdmITreeView`** — a class name referenced only in the duplicated file (undeclared in `Hotline.h`), indicating the duplicate was never compiled after its rename.
7. **Disabled help system** `USE_HELP=0` (`Hotline.h:15`) leaves the `viewID_Help*` command IDs (`Hotline.h:96-105`) inert.
8. **`ISPSecuriphone`/`Xsprings`/`UnusedAD`** view IDs and `DoLaunchSecuriphone` (`Hotline.h:117-126`, `:4424`) — dead third-party menu stubs; `viewID_ISP`/`viewID_Xsprings` handlers are fully commented out (`Hotline.cpp:1142-1153`).
9. **Nine remote-action tasks have a mis-pasted completion block.** `CMyIconChangeTask`, `CMyBlockDownloadTask`, `CMyFakeRedTask`, `CMyStandardMessageTask`, `CMyVisibleTask`, `CMyCrazyServerTask`, `CMyAdminSpectorTask`, `CMyAwayTask`, `CMyNickChangeTask` all have `ShowProgress(2,2); Finish();` inside `GetShortDesc()` (which is declared to return `Uint32` but returns nothing) while `Process()` never calls `Finish()` — these tasks never complete normally in the task window (`HotlineTasks.cpp:5296-5723`).
10. **`CMyCrazyServerTask` never sends.** Its constructor creates the session but never calls `SendData` (`HotlineTasks.cpp:5552-5554`) — `myTran_CrazyServer` (127) is unreachable.
11. **`CMyAdmInSpectorTreeView::GetItemCount()` has no `return`** (`Admin/HotlineAdminViews.cpp:1528-1531`) — undefined behavior; its `CompareLogins` casts `SMyUserListItem*` to `SMyAdminUserInfo*` (`:1656-1675`) and is dead because the `Sort` call is commented out (`:1629`).
12. **News category drag-and-drop is wired but non-functional** — `CMyNewsCategoryWin` inherits `CDragAndDropHandler` (`Hotline.h:2160`) but never overrides `HandleDrop` (base returns false).
13. **`CMyNewsArticTextWin::DoDelete` is broken** — `dynamic_cast<CMyItemsWin*>(this)` returns null → the delete task is never spawned and a handle leaks (`HotlineNews.cpp:3543-3563`).
14. **News read-history multi-client corruption** is a documented TODO (`HotlineNewsReadHistory.cpp:5-14`).
15. **Stale comments in the admin access table** — the `accessItems[]` numeric comments mislabel privilege bits (e.g. `myAcc_SpeakBefore` marked `//41` but is 45, `myAcc_PostBefore` `//41` but is 54, `myAcc_Visible` `//43` but is 48; `HotlineAdminViews.cpp:1203-1271`).
16. **`SMyTrackServInfo`** (`Hotline.h:500-505`) is documentation-only and inconsistent with the real tracker parser (`AddListFromData`, `HotlineTracker.cpp:995+`).
17. **`CMyApplication` destructor empty** (`Hotline.cpp:488-490`); `DoFlood()` empty (`:4536-4539`); `ShowAdminSpector()` stub (`:7274-7277`); `ExpandDefaultTracker()` empty (`HotlineTracker.cpp:1826-1829`).
18. **`CMyAboutWin` declared but never implemented** (`Hotline.h:2443`) — the About/splash box is actually built by `MakeClickPicWin` (`HotlineViews.cpp:3067`, used at `Hotline.cpp:3938`).
19. **`#if BETAVERS` "Livestock" easter egg** — `Can Slaughter Sheep`/`Tip Cows`/`Juggle Chickens` privileges commented out in `CMyEditUserWin` (`HotlineWindows.cpp:6250-6257`); the `SPACE_DOT_COM` "Msg Board" rebrand path is another dormant compile-time toggle (`HotlineWindows.cpp:1773, 2006`).
20. **No clipboard usage anywhere** in the client (grep-verified for `UClipboard`/`GetClipboard`/`SetClipboard`/scrap) — all inter-window data movement is Hotline-specific drag-and-drop flavors `'HLFN'`/`'HLFP'`/`'HLUI'`/`'TYCO'`/`'ISDR'`.
21. **Chat-scram substitution cipher is fully dead.** `InitSubsTable`/`BuildSubsTable`/`SubsData`/`ReverseSubsTable` (`HotlineMisc.cpp:566-703`) are only reachable through `mScramChat`, which is initialized `false` (`Hotline.cpp:163`) and set `true` only inside the `#if 0` `"hydrowart"` block (`Hotline.cpp:4851-4869`); `ChatScram`/`ChatUnscram` (`:6845-6905`) therefore never execute. `GenerateRandomPassword` (`HotlineMisc.cpp:645`) is unrelated — a new-account password generator called from the New-User dialogs (`Hotline.cpp:5430, 5745`).
22. **`viewID_BannerNext = 1043`** is defined but never referenced; `BANNER_AUTO_REFRESH_MS = 0` (`Hotline.h:17`) compiles out banner auto-cycling.
23. **Leftover debug statements** remain (commented `DebugBreak`/`MsgBox`/`gloglo` in `HotlineViews.cpp:474,478,2462,2481,2487,2771,2828`; `HotlineWindows.cpp:570-571`).

---

## 8. Modernization recommendations

**Split core vs UI.** The protocol logic is already fairly well separated by the task classes, but it is entangled with `CMyApplication` (a single god-object holding transport, session state, windows, and prefs). The clean target:

- **`hotline::client` (pure core, no AppWarrior):**
  - `Connection` — transactor/transport, version negotiation, TLS, keepalive, disconnect detection (extract from `CMyConnectTask`, `MakeTransactor`, `ProcessAll`).
  - `Auth` — the login/agreement handshake and HLCrypt/UDigest logic (`CMyLoginTask`, `CMyAgreedTask`), with modern crypto.
  - `Transactions` — a `UFieldData`-compatible field serializer and transaction framing (port `UFieldData`/`CFlatten` semantics to `std::vector<uint8_t>`/`span`).
  - `TransferEngine` — download/upload/resume/folder-archive with a pluggable filesystem interface (replace `UFS`/`TFSRefObj` with `std::filesystem`/abstract FS).
  - `FileBrowser`, `NewsService`, `ChatService`, `TrackerService`, `Admin`, `Bookmarks` — one service per feature, each an async request/response API over the Connection.
  - `TaskRunner` — replace the cooperative `Process()` state machines with `boost::asio`/coroutines (`co_await`) or an explicit async state machine; keep the queue semantics.
- **Modern UI layer (separate):**
  - Replace AppWarrior `CWindow`/`CView`/`CTreeView`/`CGeneralListView` with a modern toolkit (Qt, wxWidgets, or a web frontend over a local API). The view subclasses in `HotlineViews.cpp`/`HotlineWindows.cpp` (custom drawing, tabbed lists/trees, icon picker, QuickTime/AnimatedGIF/banner views) are the largest porting surface and should not be carried forward.
  - Move chat logging, news read-history, prefs, and bookmarks to JSON/SQLite persistence with OS keychain for secrets.
- **Protocol compatibility:** preserve the numeric transaction/field/privilege IDs and the `HOTL` v2 + `HTXF`/`HTRK` wire behavior (or gate a v3) so the new client can talk to legacy servers; encapsulate the legacy encoding behind the core's codec.
- **Delete outright:** `HotlineAdmInSpector.cpp` (duplicate), the `#if 0` easter eggs, the old-news and `NEW_TRACKSERV` dead paths, and the CRC secret console (or move behind an explicit, authenticated admin API).

---

## 9. Risks / unknowns

1. **No build system in tree** — only sources and an `.rc` that `#include "Resource.h"` (missing). Project files (CodeWarrior `.mcp`/MSVC `.dsp/.vcproj`) are absent, so exact compile flags, `NEW_TRACKSERV`/`CONVERT_INTS`/`DEBUG` definitions, and the Mac resource build cannot be verified from this tree.
2. **AppWarrior internals** (transactor framing, `UFieldData` wire layout, `UFS`/`URez` behavior) are out of this audit's scope; the exact byte layout of `HOTL` transactions and `hlc19.dat` is inferred, not byte-verified.
3. **`hlc19.dat` contents** were not enumerated; the icon/picture/sound resource IDs (e.g. 410, 4134, `HotlineViews.cpp:123`) are referenced but their mapping is opaque.
4. **`HotlineAdmInSpector.cpp` vs `HotlineTracker.cpp`** — which one is actually compiled is unknowable without project files; both are present and near-identical.
5. **Server-side contract** for resume/queue/folder-archive fields is inferred from the client's reading of `myField_FileResumeData`/`DownloadInfo`/`HTXF`; the precise archive format lives in the server (`Apps/Server`), not this scope.
6. **`mUseCrypt` default** and when the client opts into the crypt path (vs. the weak NOT-scramble) depends on `DoConnect(..., inUseCrypt)` call sites and saved bookmarks; the default for ad-hoc connects appears to be the weak path unless `useCrypt` is set.
7. **Macintosh code paths** were reviewed but not compiled/run; Carbon-specific behavior (`gMacMenuBarID`, URL handler, rez chain) is unverified.
8. **Potential 1-byte overflow** in `ReadPrefs` copying `info.userName` into `mUserName[32]` and similar unchecked Pascal-string copies warrant a focused follow-up before any reuse.
9. **Protocol field-ID collisions** (`myField_Visible`/`myField_UserFlags` = 112; `myField_number`/`myField_ChatID` = 114) mean wire compatibility for the extended admin/chat features is ambiguous without the server's own constants; a renumber would break legacy peers, so a v3 negotiation is needed.
10. **The nine broken remote-action tasks** (§7.9) suggest those admin paths were never exercised end-to-end in this build; their true runtime behavior (and whether the server ever receives their transactions) is unverified.
11. **`CMyAdmInSpectorTreeView::GetItemCount()` (no `return`)** and other undefined-behavior sites mean the code only "works" by virtue of never being called on these paths — fragile under any compiler/optimizer change.

---

*Audit based entirely on the source in `legacy/Apps/Client/` (plus shared `Apps/Common Files/` and `AppWarrior/` headers for cross-references). No files under `legacy/` were modified.*
