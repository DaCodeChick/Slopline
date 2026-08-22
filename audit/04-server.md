# Hotline Server Audit

Scope: `legacy/Apps/Server` (current, self-identifies as **GLoarbLine Server 1.9.7**), `legacy/Apps/ServerOLD` (**Hotline Server 1.9.6**), and the shared `Utilities/NewsSynch` tool. All files are MacRoman/ISO-8859; citations are to `Apps/Server/Source/` unless noted. Framework = `legacy/AppWarrior` (Hotline's own C++ class library: `CApplication`, `UTransport`, `UTransact`, `TFSRefObj`, etc.). Protocol IDs/fields/privileges are defined in `Apps/Common Files/HotlineClientServerCommon.h`.

---

## 1. File inventory

| Area | File | LOC | Role |
|---|---|---|---|
| Server | HotlineServ.cpp | 6,649 | Application: startup, 4 listeners, accept/establish loop, session/transfer state machines, user DB, message board, prefs, logs |
| Server | HotlineServTrans.cpp | 5,238 | All `ProcessTran_*` transaction handlers (the protocol behavior spec) |
| Server | HotlineServWindows.cpp | 2,190 | Windows/Mac admin UI: toolbar, options, accounts, bans, stats |
| Server | HotlineServNewsDatabase.cpp | 1,520 | `UMyNewsDatabase` — HLNZ binary news DB engine |
| Server | HotlineServ.h | 1,033 | Structs (`SMyClient`, `SMyOptions`, download/upload task structs), class decls |
| Server | HotlineServViews.cpp | 434 | UI list/check views |
| Server | HotlineServIDTranslate.cpp | 272 | Community-ID and Base-36 encode/decode |
| Server | HotlineServNewsDatabase.h | 232 | HLNZ on-disk structs |
| Server | HotlineServMisc.cpp | 18 | `WinIsNewsCat` (.hnz detection) |
| Server | HotlineServIDTranslate.h | 40 | decls |
| ServerOLD | HotlineServ.cpp | 6,589 | −60 vs Server (no chat log / wrong-user-name) |
| ServerOLD | HotlineServTrans.cpp | 5,194 | −44 (no lastPath/lastNewsPath, no chat logging) |
| ServerOLD | HotlineServWindows.cpp | 2,165 | −25 |
| ServerOLD | HotlineServ.h | 1,025 | −8 |
| NewsSynch | NewsSynch.cpp / .h | 2,303 / 2,856 | App shell / header |
| NewsSynch | SynchTasks.cpp | 2,853 | Sync engine (NNTP ↔ HLNZ) |
| NewsSynch | SynchViews.cpp / SynchWindows.cpp | 2,395 / 2,591 | UI |
| NewsSynch | CNewsDatabase.cpp | 413 | v1 HLNZ reader |
| NewsSynch | CNewsDatabase_v2.cpp | 2,099 | v2 HLNZ reader |
| NewsSynch | CNewsDatabase_v3.cpp | 2,044 | v3 HLNZ reader |
| NewsSynch | CNewsDatabase.h | 156 | decls |
| NewsSynch | Views/* | ~2,300 | popup/list views |

Identical between Server and ServerOLD (byte-for-byte): `HotlineServIDTranslate.{cpp,h}`, `HotlineServMisc.cpp`, `HotlineServNewsDatabase.{cpp,h}`, `HotlineServViews.cpp`. The two `Utilities/NewsSynch` trees differ only in `Data/rand_seed`.

Sample data (ServerOLD only): `Data/hls19.dat` (resource fork), `Data/rand_seed`, `News/` (one `.hnz` category `xc.hnz` + 2 empty 1,034-byte `.hnz`), `Users/{admin,guest,test}/UserData`, `Prefs`, `Banned`, `HLServer.mk`, `185Banner.gif`. Only stray artifact: `ServerOLD/Source/._HotlineServWindows.cp` (AppleDouble header).

---

## 2. Server vs ServerOLD — key differences

- Version bump `kServerVersion` 1.9.6 → `"\p1.9.7"` (`HotlineServ.cpp:7`); rebrand "Hotline Server" → "GLoarbLine Server" (`:160,:163,:295`).
- **Chat logging added**: `kMyChatLogFileName "\pChatLog.txt"` (`:32`), `CMyApplication::LogChats()` (`:2982`) writes tab- or `;`-separated (CSV) lines to `Chat Log.txt`/`Chat Log.csv`; new option `myOpt_LogChats = 0x10000` (`HotlineServ.h:68`), "Log Chats to file" checkbox (`HotlineServWindows.cpp:155`).
- **Wrong-user-name feature added**: `kDefaultWrongUserName` (`:13`), new `mWrongUserName[256]` pref (persisted `:3695,:3796`), default applied `:3918`, checked in `CheckName()` when `myOpt_kickunnamedusers` (`:3203`).
- Pref buffers enlarged 128→256 bytes for `mIncorrectlogin/mMessageBeforedl/mPostBeforedl/mAcceptPrivate/mLoginMessageText/mServerDesc` (`HotlineServ.h:760-768`).
- User-info text (both `GetClientInfoText` formats) now appends `lastPath`/`lastNewsPath` (`HotlineServTrans.cpp:4290,4293`); these per-client buffers are filled in `GetFileNameList` (`:991`) and `GetNewsArtNameList` (`:1390`).
- Options bit constants renumbered 4→5 hex digits (adds `myOpt_LogChats`).
- Server replies `myField_Vers = 197` (vs 196) at login (`HotlineServTrans.cpp:1684`).
- `DebugBreak("visible")` left in Server `ProcessTran_Visible` (`:716`); ServerOLD has it removed.

**Net**: Server = ServerOLD + chat-logging + wrong-user-name kick + wider UI buffers + version/name rebrand. Nothing was dropped from the core protocol.

---

## 3. Architecture

**Startup** (`HotlineServ.cpp:54 main`, `:150 StartUp`): `UOperatingSystem::Init`, `UTransport::Init`, require TCP; Win32 adds `Data/hls19.dat` to resource chain (`:72`). `StartUp` creates `Users/`, `Files/`, `News/` folders under `kProgramFolder` (`:171-181`), loads `MessageBoard.txt` (`:183`), sets `_gTransactMaxReceiveSize = 524288` (`:187`), reads prefs/perm-ban list/agreement/banner, decodes serial number (`:194`).

**Listeners — four sockets** (`:241-269`), all `SetMessageHandler(MessageHandler, this)`:
1. `mListenTpt` Hotline transaction protocol on `nBasePortNum` (default 5500).
2. `mTransferListenTpt` raw file-transfer on `base+1`.
3. `mListenHttpTpt` HTTP on `base+2`.
4. `mTransferListenHttpTpt` HTTP transfer on `base+3`.

Tracker: `mTrackerTpt` UDP timer every 300 s if `myOpt_UseTracker` (`:304-311`).

**Event loop / threading — single-threaded cooperative polling, no per-connection threads.** `CApplication::MessageHandler` (AppWarrior `Misc/CApplication.cpp:168`) → `CMyApplication::HandleMessage` (`:341`); any `msg_DataArrived/DataTimeOut/ReadyToSend/ConnectionClosed/ConnectionRequest/Established/Refused` invokes the *entire* pipeline: `ProcessClients`, `ProcessTransferEstab`, `ProcessBannerDownloads`, `ProcessDownloads`, `ProcessFldrDownloads`, `ProcessUploads`, `ProcessFldrUploads`, `ProcessDownloadsWaiting` (`:352-359`). Each `Process*` is a `while` loop over a linked list advancing state machines.

**Accept loop** (`ProcessClients` `:4605`): drain `mListenTpt->Accept()` then `mListenHttpTpt->Accept()`, wrap each into `SMyConnect{tpt, ipAddress}` in `mConnectList` (`:4644-4650`). Handshake: `connect->tpt->ReceiveEstablish(prot, vers)`; must be `prot=='HOTL'` and `vers∈{2,3}` (`:4684`). `vers==2` → `SMyClient` in `mClientList` (`:4722-4728`); `vers==3` → raw `TTransport` into `mTransferEstabList` (`:4748-4755`). Reject/start-disconnect otherwise (`:4770`).

**Connection limits & flood** (`:4689-4747`): `nMaxConnectionsIP` (default 5, `:3853`) via `GetClientAddressCount`; >10 connects from one IP within 30 s → `AddTempBan` (`:4699`); 120 tx/sec (2/sec avg) → disconnect+temp-ban unless `myAcc_Broadcast` (`:4874`).

**Per-connection session state** `SMyClient` (`HotlineServ.h:646-688`): `tpt`, `access` (SMyUserAccess), `ipAddress`, `userID`, `iconID`, `logon[33]`, `realName/userName/accountName/accountLogin[32]`, `loginFailedSecs/activitySecs/lastMsgSecs/noflood*`, `lastPath[2048]/lastNewsPath[2048]`, refuse/auto-response flags+text, `vers`, dup-msg CRC, counters (`privMsgCount,dontChatCount,numdl,numul,numflddl,numfldul`), 8 bit-flags (`hasLoggedIn,isAway,isDownloading,isFakeRed,isVisible,isBlockDownload,isChatting,isPostting`).

**Receive loop** (`:4842-5115`): `client->tpt->NewReceiveSession(kAnyTransactType, kOnlyCompleteTransact)`; login timeout 60 s (`:4848`); auto-away after `nawaydelay` (`:4857`); pre-login only `Login`/`Agreed` accepted, anything else disconnects (`:4902-4918`). Post-login dispatches by `switch(type)` to `ProcessTran_*` (`:4922-5113`), with `myAcc_SpeakBefore/PostBefore` gating download (`:5019-5025`).

**Transfers** are state machines driven from separate lists (`mDownloadList`, `mDownloadFldrList`, `mUploadList`, `mUploadFldrList`, `mDownloadWaitingList`, `mDownloadBannerList`) with explicit `state` fields and `goto switchAgain` (`:5504`, `:6111`).

---

## 4. Transaction dispatch table (HotlineServTrans.cpp)

Access-check helpers `HasGeneralPriv`/`HasFolderPriv`/`HasBundlePriv` are **identical** — all return `inClient->HasAccess(priv)` (`HotlineServ.cpp:3604-3617`); "folder/bundle" granularity is nominal only.

| myTran_ | Handler (line) | In-fields | Access check | Reply / side effects |
|---|---|---|---|---|
| GetMsgs (101) | `:75` | — | NewsReadArt | Sends whole `MessageBoard.txt` in `myField_Data`. |
| PostMsg (103) | `:107` | Data | NewsPostArt | ≤8 KB; append to board (`AddToMsgBoard`); CRC-dup ban; broadcast `myTran_NewMsg`. |
| SendInstantMsg (108) | `:180` | UserID, Options, Data, QuotingMsg | SendMessage | Flood caps; auto-response/refuse; relays `myTran_ServerMsg`; **FLOOD-login backdoor floods 500×** (`:325`). |
| UserBroadcast (355) | `:351` | Data | Broadcast | Broadcasts `myTran_UserBroadcast` to all logged-in. |
| ChatSend (105) | `:443` | Data, ChatOptions, ChatID | SendChat | 8 KB cap, name-prefix per line, 10 KB/min flood; broadcast `myTran_ChatMsg` (channel or public). |
| IconChange (123) | `:620` | UserID, IconId | ChangeIcon | Sets icon, `BroadcastClientUserInfo`. |
| FakeRed (125) | `:662` | UserID, FakeRed | FakeRed | `SetClientFakeRed`. |
| Visible (129) | `:709` | UserID, Visible | Visible | `SetClientVisible`; stray `DebugBreak("visible")`. |
| AdminSpector (130) | `:751` | — | AdmInSpector | Builds ratio/speed text, replies `myTran_UserBroadcast`. |
| BlockDownload (128) | `:832` | UserID, BlockDownload | BlockDownload | `SetClientBlockDownload`. |
| Away (126) | `:876` | UserID, Away | Away | `SetClientAway`. |
| NickChange (124) | `:922` | UserID, NickName | ChangeNick | Rename (≤31), broadcast. |
| GetFileNameList (200) | `:973` | FilePath | — | `BuildFileList`; records `lastPath`; drop-box filtering. |
| GetNewsCatNameList (370) | `:1048` | NewsPath | — | `BuildNewsCatList` (v15 bundle/category structs or legacy). |
| PostNewsArt (410) | `:1082` | NewsPath, NewsArtID(parent), NewsArtTitle, NewsArtFlags, NewsArtDataFlav, NewsArtData | NewsPostArt (bundle) | `UMyNewsDatabase::AddArticle`+`AddData`. |
| DelNewsArt (411) | `:1149` | NewsPath, NewsArtID, NewsArtRecurseDel | NewsDeleteArt | `DeleteArticle`. |
| DelNewsItem (380) | `:1195` | NewsPath | NewsDeleteFldr/NewsDeleteCat | `MoveToTrash`. |
| NewNewsFldr (381) | `:1249` | FileName, NewsPath | NewsCreateFldr | `CreateFolder`. |
| NewNewsCat (382) | `:1311` | NewsCatName, NewsPath | NewsCreateCat | `UMyNewsDatabase::CreateNewGroup('3113')`; Win32 appends `.hnz`. |
| GetNewsArtNameList (371) | `:1377` | NewsPath | — | `BuildNewsArtList`; records `lastNewsPath`. |
| GetNewsArtData (400) | `:1415` | NewsPath, NewsArtID, NewsArtDataFlav | NewsReadArt (bundle) | Returns art data + prev/next/parent/child/title/poster/date. |
| GetUserNameList (300) | `:1468` | — | — | List of logged-in users (honors invisibility vs `myAcc_Canviewinvisible`). |
| Login (107) | `:1523` | UserLogin, UserPassword, MacAlg, C_CipherAlg, Vers, (legacy UserName/UserIconID) | ban checks | Verifies scrambled password; replies Vers=197, CommunityBannerID, ServerName; then agreement/banner/login-message. |
| Agreed (121) | `:1924` | (user info) | — | `SetClientUserInfo`, `hasLoggedIn=true`, sends UserAccess + banner. |
| DownloadFile (202) | `:2003` | FileName, FilePath, FileXferOptions, FileResumeData | DownloadFile | Queue/limit checks, drop-box guard, flatten or raw; replies TransferSize/FileSize/RefNum/WaitingCount; enqueues `SMyDownloadData`. |
| DownloadFldr (210) | `:2180` | FileName, FilePath, FileXferOptions, FileResumeData | DownloadFolder | `CMyDLFldr`; replies TransferSize/FldrItemCount/RefNum. |
| DownloadBanner (212) | `:2371` | — | (none) | Replies TransferSize/RefNum; enqueues banner download. |
| KillDownload (214) | `:2393` | RefNum | — | `KillDownloadByRefNum`. |
| UploadFile (203) | `:2404` | FileName, FilePath, FileXferOptions, TransferSize | UploadFile | `.LNK/.HPF` filters; Uploads/drop-box-only unless UploadAnywhere; disk-space; creates `HTft/HTLC`, StartUnflatten; replies RefNum. |
| UploadFldr (213) | `:2635` | FileName, FilePath, FldrItemCount, TransferSize, FileXferOptions | UploadFolder | Same guards; `CreateFolder` + task. |
| DeleteFile (204) | `:2824` | FileName, FilePath | DeleteFile/DeleteFolder | `MoveToTrash`. |
| NewFolder (205) | `:2927` | FileName, FilePath | CreateFolder | `CreateFolder` (validated name). |
| Unknown | `:3004` | — | — | `"Unknown transaction type!"`. |
| SendMessage (131) | `:3015` | StandardMessage | — | Joke reply `"Hahahaha!"` (dead code). |
| Dont_Chat / Dont_Post | `:3034/:3050` | — | — | Send "chat/post before download" message (SpeakBefore/PostBefore gate). |
| NewUser (350) | `:3082` | UserName, UserAlias, UserLogin, UserPassword, UserAccess | CreateUser | No-more-access-than-self check; `NewUser` writes folder+UserData. |
| DeleteUser (351) | `:3158` | UserLogin | DeleteUser | `DeleteUser` (MoveToTrash) + `DeleteOnlineAccount`. |
| GetUser (352) | `:3213` | UserLogin | OpenUser | Returns user data, password redacted to `x`. |
| SetUser (353) | `:3268` | UserName, UserAlias, UserLogin, UserPassword, UserAccess | ModifyUser | `SetUser`; keep old pass if empty; `SetOnlineAccount`. |
| GetUserList (348) | `:3355` | — | OpenUser | Serializes all accounts (passwords redacted). |
| SetUserList (349) | `:3438` | repeated Data fields | Create/Modify/DeleteUser | Bulk create/rename/modify/delete. |
| DisconnectUser (110) | `:3712` | UserID, Options(ban), Data(descr) | DisconUser | `CannotBeDiscon` guard; temp/perm ban; kick; optional chat broadcast (`myOpt_showKick`). |
| GetClientInfoText (303) | `:3865` | UserID | GetClientInfo | Text dump of transfers/queues; bot-mode formatting. |
| SetClientUserInfo (304) | `:4304` | user info | — | Name-change flood cap; `SetClientUserInfo` broadcast. |
| GetFileInfo (206) | `:4338` | FileName, FilePath | — | Type/creator/size/dates/comment; resolves aliases. |
| SetFileInfo (207) | `:4503` | FileName, FilePath, FileComment, FileNewName | SetFile/SetFolderComment, RenameFile/RenameFolder | Set comment and/or rename; `.LNK` protection; drop-box guard. |
| MoveFile (208) | `:4728` | FileName, FilePath, FileNewPath | MoveFile/MoveFolder | `fsItem->Move`. |
| MakeFileAlias (209) | `:4818` | FileName, FilePath, FileNewPath | MakeAlias | `CreateAlias`. |
| InviteNewChat (112) | `:4890` | ChatID, repeated UserID | CreateChat | Creates `SMyChat`, sends `myTran_InviteToChat`. |
| RejectChatInvite (114) | `:4996` | ChatID | — | `SendTextToChat` decline note. |
| JoinChat (115) | `:5004` | ChatID | — | Adds client, `myTran_NotifyChatChangeUser`, returns member list. |
| LeaveChat (116) | `:5080` | ChatID | — | Removes; deletes channel if empty; `myTran_NotifyChatDeleteUser`. |
| SetChatSubject (120) | `:5105` | ChatID, ChatSubject | — | Stores + `myTran_NotifyChatSubject`. |
| InviteToChat (113) | `:5143` | ChatID, repeated UserID | — | Sends `myTran_InviteToChat` (respects refuse/auto-response). |
| KeepConnectionAlive (500) | `:5232` | — | — | `SendNoError`. |

Not dispatched (declared but unused/absent from switch): `ProcessTran_Flood`, `ProcessTran_SendMessage` (commented `:5106`); `myTran_DisconnectMsg(111)`, `myTran_ServerMsg(104)`, `myTran_NotifyChangeUser/DeleteUser(301/302)`, legacy `NEW_NEWS` block are vestigial.

---

## 5. User database

Format: `struct SMyUserDataFile` (packed; `HotlineClientServerCommon.h:311`): `version`(1), `iconID`, `SMyUserAccess access` (2×Uint32), `maxSimulDownloads`, `rsvd[512]`, then pstring `name[64]/alias[64]/login[32]/password[32]`. One account = `Users/<login>/UserData` (type `HTud`, creator `HTLS`).

Load/save: `GetUser` opens `Users/<login>/UserData`, rejects hidden folders, checks size+version (`HotlineServ.cpp:3182`); `NewUser` creates folder+file (`:3130`); `SetUser` overwrites (`:3243`); `DeleteUser` moves folder to trash (`:3173`); `RenameUser` renames folder (`:3229`). Sample files confirm: admin/guest/test all `version=1, iconID=0`; access differs (admin `0xfff3cfff 0xfffffe00`, test `0x20708cb0 0x80801000`).

**Password storage**: trivially obfuscated plaintext. Wire scrambles login+password with bitwise NOT (`~`); login is un-scrambled before use (`HotlineServTrans.cpp:1623`, `:3123`), but the password is stored still-NOT'd ("leave password b/c we want it scrambled in the data file", `:3120`) and compared **directly** against the wire value (`:1676`). `SetAdmin` explicitly "scrambles" via NOT (`:3311-3316`). NOT is its own inverse → recoverable at rest. Sample admin/guest password byte `0xC5 == ~':'`. `GetUser` redacts password to `'x'` (`:3252`), but only the admin-client API.

Guest: empty `UserLogin` → `"guest"` (`:1601`). Admin: `CheckAdminAccount` prompts for password if missing (`:3374`, `:3393`); `SetAdmin` grants `SetAllPriv` (`:3325,:3339`). Login also reads but **does not use** `myField_MacAlg`/`myField_C_CipherAlg` (`:1562-1569`) — the advertised crypto fields are ignored in this path.

---

## 6. News database

`UMyNewsDatabase` (`HotlineServNewsDatabase.{h,cpp}`) implements a **single-file binary store per category**, type `'HLNZ'`, version **2** (`CreateNewGroup` sets `head->type=TB('HLNZ')`, `head->vers=2`, blockSize 256, `:77-97`). On-disk (`HotlineServNewsDatabase.h:7-117`): `_NZ_SDBHeader` (GUID id, add/del serials, blockSize, cached list offset/size, articleCount, first/last article offsets, nextArticleID, alloc-table info, ID→offset hash offset+mask, name/desc) followed by a bit-alloc-table and data blocks. `_NZ_SNewsItemBlock` body: `id`, `prev/next/parent/firstChild` offsets (threading), `SDateTimeStamp`, `flags`, `title[64]`, `poster[32]`, `externId[64]`; flavors (`_NZ_SGenericBlock`) chain via `nextFlav` with MIME pstrings (`application/x-hlnewsitem`, `text/plain`, …). Hash table `_NZ_SIDOffset{id,offset}` keyed by low bits of ID.

Category tree: `News/` folders = bundles; `.hnz` files = categories. `BuildNewsCatList` (`HotlineServ.cpp:2233`) emits `myField_NewsCatListData15` records (type 2 bundle / type 3 category with GUID + addSN/delSN for clients `vers>=15`; legacy 1/10/255 codes otherwise). Article list `GetArticleListing` → `myField_NewsArtListData`; article read (`BuildNewsArtData:2378`) returns prev/next/parent/1st-child IDs, title, poster, flavor, date. Sort/thread order derives from the doubly-linked `prev/next/parent/firstChild` chain; article `id` is a monotonic `nextArticleID` counter, and article GUIDs are `externId` (Base-36-generated; `GenerateExternID`).

`Resources/News DB Struct.txt` documents an earlier v1 layout (16-bit offsets, allocTableCRC) — the header struct in code supersedes it.

---

## 7. File service path & security

Root: `GetClientRootFolder` = `Users/<accountLogin>/Files` if it exists, else shared `Files/` (`:3354`); news root analogous (`:3364`). All file handlers build paths via `UFS::New(root, path, pathSize, name, options)` / `StFileSysRef` (`HotlineServTrans.cpp:2055`, `:2838`, `:4358`, `:4742`). **Path traversal is delegated to `UFS`/`StFileSysRef`** (path is an opaque blob; the server does not itself reject `..` — the framework must, and this is unverified). Filenames are sanitized by `UFS::ValidateFileName` on upload/new-folder/rename (`:2429,:2968,:4701`); `\r` is stripped from logins (`:1627`). No length bound is checked beyond the 2048-byte stack buffer and `GetField`'s copy limit.

Drop-box rule: a folder is a drop-box if its name contains "drop box" (case-insensitive) and the user lacks `myAcc_ViewDropBoxes`; `myAcc_ViewOwnDropBox` relaxes only when the account login appears in the path (`:1022-1035`, `:2065-2097`). Uploads without `myAcc_UploadAnywhere` must land in a folder named "upload"/"drop box" (`:2457-2467`).

Comments: stored via `TFSRefObj::GetComment/SetComment` (resource-fork/ADS at framework level) — **not** `.comment` sidecar files (`:4482,:4619`). Aliases: native FS aliases via `CreateAlias` (`:4858`). Deletes go to trash (`MoveToTrash`), not permanent.

---

## 8. ID translation (`HotlineServIDTranslate`)

**Not** a classic 1.2→1.5 transaction-ID remap (despite the name). It provides:
1. **HL Community ID** ↔ 6-char string, base-39 alphabet `" ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789._"` (`kHLCommID_IntToCharTab`, `:17`), little-endian digit order (`:45-49`); used by the Engage AdManager community-ID serial system, not the wire protocol.
2. **Base-36** (A–Z=0–25, 0–9=26–35) encode/decode with min-width padding (`TranslateToBase36`, `:198-248`) and `TranslateFromBase36` (max 7 chars, `:250`) — used for news `externId`/article IDs.

---

## 9. Security-sensitive patterns (top findings)

1. **Trivial password "encryption"** — bitwise NOT, reversible, compared in the clear: `HotlineServTrans.cpp:1676`, `:3120`; `HotlineServ.cpp:3311`.
2. **Flood-bot backdoor** — any account whose login begins `FLOOD` sends 500 copies of a private message: `HotlineServTrans.cpp:325-341`.
3. **Unused crypto fields** — `MacAlg`/`C_CipherAlg` read then ignored at login: `HotlineServTrans.cpp:1562-1572`.
4. **Fixed stack buffers with unbounded-ish copies** — `Uint8 path[2048]` + `GetField` in nearly every handler (e.g. `:2011`, `:2424`); relies on `GetField`/`_gTransactMaxReceiveSize=524288` (`HotlineServ.cpp:187`), not validated lengths.
5. **`sprintf`-style formatting** — `UText::Format` into fixed 256/512/2048 buffers without tight bounds (e.g. `:4290`, `:782`, `:789`); many `%#s` into `char[512]`.
6. **Path traversal delegation** — server passes raw client paths to `UFS::New`/`StFileSysRef` with no explicit `..` rejection: `:2055`, `:4358`, `:4742`, `:4770`.
7. **Admin password prompt & plaintext-at-rest** — `CheckAdminAccount`/`SetAdminAccount` (`:3374-3427`) gate only the *local* UI; remote admin uses same NOT-scrambled password.
8. **Flat privilege model** — `HasGeneralPriv/HasFolderPriv/HasBundlePriv` all equal `HasAccess` (`HotlineServ.cpp:3604-3617`); "folder/bundle" ACLs are fiction.
9. **Privilege-escalation guard present but only at create** — `NewUser`/`SetUserList` reject access ⊃ own access (`:3111-3118`, `:3669`); `SetUser` path (admin GUI) has no such guard.
10. **Rate/DoS counters are coarse** — 120 tx/min, 20 priv-msg/min, 10 KB chat/min (`HotlineServ.cpp:4874`; `Trans.cpp:231,460`); `myAcc_Canflood` bypasses.
11. **Duplicate-message ban via CRC** (`lastPrivMsgCRC`, `:204,:507`) — weak (CRC, not hash) and only triggers on exact repeat.
12. **`isVisible` field-ID collision** — `myField_Visible = 112` duplicates `myField_UserFlags` (`HotlineClientServerCommon.h:117`); `myField_number=114` duplicates `ChatID`.
13. **Serial-number licensing** — `NETWORK_SERVER` gates `mSimConnections` (`:1638-1644`); disabled here (`NETWORK_SERVER=0`), so connection cap is only per-IP.
14. **Legacy client fallback** — old clients send `myField_UserName/UserIconID` and bypass agreement flow in one branch (`:1699-1720`).
15. **DoS via unbounded list iteration** — broadcasts (`GetUserNameList`, `ChatSend`, `PostMsg`) walk the whole client list per event with no yield (`:168-175`, `:608-615`).

---

## 10. NewsSynch tool

`Utilities/NewsSynch` (identical in both trees) is a **standalone news synchronizer** between the Hotline HLNZ categories and external news sources. It contains its own HLNZ readers `CNewsDatabase.cpp` (v1), `CNewsDatabase_v2.cpp`, `CNewsDatabase_v3.cpp` — three format revisions — plus `SynchTasks.cpp` (the sync engine) and a full Mac/Win UI (`SynchViews/SynchWindows/Views`).

Verdict: **file-based sync, not NNTP at the core.** The server-side article store is HLNZ on disk; NewsSynch maps those onto the AppWarrior `UNntpTransact` (NNTP transport) to push/pull Usenet news (`legacy/AppWarrior/Source/Hardware/UNntpTransact.cpp` exists) and populate `.hnz` categories. Sample `News/news free/*.hnz` (e.g. `microsoft.public.z.newsmon.hnz`, `news.lists.filters.hnz`) are exactly the category-file names a NewsSynch/Usenet import would create. Relevance today: **historical/obsolete** — a pre-2000 NNTP/Usenet bridge; it shares the v2/v3 HLNZ layout with the server, so its readers are the only spec for the older on-disk formats.

---

## 11. Modernization recommendations

1. **Replace the polling core** with an explicit event loop (epoll/kqueue) or thread-per-connection; keep the `ProcessTran_*` handlers as pure `(session, request) → response` functions.
2. **Split the three fused concerns**: connection lifecycle (accept/disconnect/timeouts) vs session state (SMyClient) vs handler logic — currently all in `ProcessClients`.
3. **Introduce a protocol schema** generated from `HotlineClientServerCommon.h`; table-driven dispatch instead of the hand-written `switch`.
4. **Real auth**: bcrypt/argon2 password hashes; drop the NOT-scramble; implement the advertised `SessionKey/MacAlg/CipherAlg` handshake or remove the fields.
5. **Path sandboxing**: canonicalize + reject `..` and absolute paths at the service boundary; per-account chroot-like roots (`Users/<login>/Files`) already exist as a seam.
6. **Bounds-safe I/O**: replace fixed `Uint8[2048]` + `GetField` with length-checked readers; cap field sizes against `_gTransactMaxReceiveSize`.
7. **Permission model**: honor the nominal folder/bundle distinction or collapse the three helpers into one documented ACL; add per-folder ACLs if needed.
8. **Remove dead/abusive code**: the FLOOD backdoor, joke `SendMessage` handler, stray `DebugBreak`s, commented `NEW_NEWS` blocks.
9. **Logging**: centralize `LogChats`/`LogDownload`/etc. into a structured logger; sanitize injected user text (currently raw `%#s` into log files).
10. **Concurrency for transfers**: the flatten/unflatten + send state machines are the obvious candidate for async I/O, preserving the refNum-based protocol.

---

## 12. Risks / unknowns

- **Framework trust boundary unverified**: path traversal, alias resolution, and comment storage are delegated to `AppWarrior` `UFS`/`TFSRefObj`; the security of those primitives (esp. `ResolveAlias`, `GetPathTargetName`) is out of scope here and must be audited before any reuse.
- **`SetUser`/GUI admin path** may grant privileges without the create-time escalation guard; the GUI admin dialogs (Views/Windows) were only skimmed.
- **Encoding**: all sources are MacRoman; Windows `.rc`/`hls19.dat` resources and the `185Banner.gif`/`.ico` assets were not decoded.
- **Tracker protocol** (`UpdateTracker`, `SMyTrackInfo[10]`, `lorbac.net`/`dmp.fimble.com`) was not reverse-engineered; it appears UDP-based and now-dead.
- **Serial-number/community system** (`DecodeSerialNumber`, `HLCommID`) is disabled at compile time; semantics not fully verified.
- **NewsSynch v1/v2/v3 HLNZ dialects** were inventoried, not diffed against the server's v2 writer line-by-line.
- Exact `myTran_StandardMessage(131)`/`myTran_ServerMsg(104)` behavior from the *client* side is out of scope (client tree not audited here).
