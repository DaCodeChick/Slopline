# Hotline Wire Protocol Audit

Audit of the legacy `GLoarbLine`/Hotline implementation under `legacy/`. All findings are
derived from the actual source tree; the marker **[SOURCE]** denotes a fact quoted from a file
in this tree, **[EXTERNAL]** denotes historical Hotline protocol knowledge used only for
cross-checking, and **[UNKNOWN]** denotes something the source does not settle. File:line
references are to files under `legacy/` (paths are shortened to the `Apps/…`/`AppWarrior/…`
suffix for readability).

The most important cross-cutting finding is stated up front because it re-frames several
sections of the requested report:

> **[SOURCE]** The wire transport used by this tree is **not** the classic 18-byte Hotline
> 1.2/1.5 transaction header. It is the AppWarrior **TRTP** transaction framing (magic
> `'TRTP'`), whose header is 20 bytes and uses a **4-byte** transaction id, a **4-byte** error
> code, a single `flag`/`isReply` byte pair, and **no** per-packet version or sequence field.
> **[EXTERNAL]** The classic Hotline header is `UInt16 version=1, UInt16 transID, UInt16 flags,
> UInt16 sequence, UInt16 errorCode, UInt32 totalSize, UInt32 dataSize` (18 bytes). The two are
> not byte-compatible. The Hotline *transaction IDs* (100–500) and *field IDs* are carried
> *inside* the TRTP `type`/`data` fields, so the application-layer protocol is Hotline, but the
> framing layer is TRTP.

---

## 1. Framing

### 1.1 The transaction header (STranHdr)

**[SOURCE]** `AppWarrior/Source/Hardware/UTransact.cpp:13-24`:

```c
#pragma options align=packed
struct STranHdr {
	Uint8 flag;							// reserved, should be 0
	Uint8 isReply;						// is this transaction a reply?
	Uint16 type;						// type of transaction
	Uint32 id;							// arbitrary value used for reply
	Uint32 error;						// error code used for reply
	Uint32 totalSize;					// for splitting large transactions
	Uint32 dataSize;					// number of bytes in following string
	Uint8 data[];						// data for this transaction
};
#pragma options align=reset
```

Byte layout (packed, 20-byte header, then data):

| offset | size | field | endianness |
|---|---|---|---|
| 0 | 1 | `flag` | n/a (0 normally; used as key-permutation selector when crypto on) |
| 1 | 1 | `isReply` | n/a (0/1) |
| 2 | 2 | `type` | **big-endian** (Hotline transaction ID 100–500) |
| 4 | 4 | `id` | **big-endian** (reply matching) |
| 8 | 4 | `error` | **big-endian** (0 = OK) |
| 12 | 4 | `totalSize` | **big-endian** |
| 16 | 4 | `dataSize` | **big-endian** (bytes in this part) |
| 20 | … | `data` | raw |

### 1.2 Endianness of integers

**[SOURCE]** `AppWarrior/Headers/typedefs.h:173-208`. All multi-byte integers are converted with
`TB()` ("to big-endian") on send and `FB()` ("from big-endian") on receive. On Intel
(`__INTEL__`, `CONVERT_INTS=1`) these byte-swap; on PPC/68k they are identity:

```c
#if __INTEL__
	#define CONVERT_INTS	1
	inline Uint16 TB(Uint16 n)	{	return swap_int(n);			}
	inline Uint32 TB(Uint32 n)	{	return swap_int(n);			}
	inline Uint16 FB(Uint16 n)	{	return swap_int(n);			}
	inline Uint32 FB(Uint32 n)	{	return swap_int(n);			}
#elif __POWERPC__ || __MC68K__
	#define CONVERT_INTS	0
	inline Uint16 TB(Uint16 n)	{	return n;					}
	inline Uint32 TB(Uint32 n)	{	return n;					}
```

`swap_int` (typedefs.h:153-171) is a manual byte-reverse. So **every** multi-byte integer on
the wire is big-endian (network order), including transaction `type`, `id`, `error`,
`totalSize`, `dataSize`, field ids/sizes/counts, and every integer inside field payloads.

### 1.3 Sending (write path)

**[SOURCE]** `UTransact.cpp:907-975` (`_TNSendTran`):

```c
	hdr.flag = 0;
	hdr.isReply = (inReply != 0);
	hdr.type = TB(inType);
	hdr.id = TB(inID);
	hdr.error = TB(inError);
	hdr.totalSize = TB(inTotalSize);
	hdr.dataSize = TB(inDataSize);

	if(gCrypt) { /* ... derive hdr.flag, then: */ gCrypt->Encode((DataBuffer){(Uint8*)&hdr, sizeof(hdr)}); }
	...
	if (inDataSize == 0) UTransport::Send(tpt, &hdr, sizeof(hdr));
	else {
		Uint8 *buf = (Uint8 *)UTransport::NewBuffer(sizeof(hdr) + inDataSize);
		UMemory::Copy(buf, &hdr, sizeof(hdr));
		UMemory::Copy(buf+sizeof(hdr), inData, inDataSize);
		/* crypto path encrypts data here */
		UTransport::SendBuffer(tpt, buf);
	}
```

Header and data are concatenated into one buffer for send.

### 1.4 Receiving / byte counting / unknown-size

**[SOURCE]** `UTransact.cpp:977-1127` (`_TNProcessIncomingData`). The receive loop reads a full
`STranHdr` (20 bytes), then exactly `rcvHeader.dataSize` bytes of data. `dataSize` and
`totalSize` are validated (UTransact.cpp:1019-1026):

```c
	// allow max _gTransactMaxReceiveSize for totalSize and dataSize
	if (!rcvHeader.totalSize || rcvHeader.totalSize > _gTransactMaxReceiveSize
		|| !rcvHeader.dataSize || rcvHeader.dataSize > _gTransactMaxReceiveSize)
	{
		// kill the connection
		TRN->isDead = true;
		tpt->Disconnect();
		break;
	}
```

**[SOURCE]** `UTransact.cpp:86`: `Uint32 _gTransactMaxReceiveSize = 2097152;` (2 MB). So a single
part is capped at 2 MB, `dataSize==0` and `totalSize==0` are rejected (connection killed), and a
zero-length transaction body is represented by a header with no following data (the
`inDataSize == 0` send path sends the 20-byte header alone). "Unknown size" is not supported:
`totalSize` is mandatory and used by the caller (`SetSessionSendSize`) before sending; for
single-shot sends `SendTransaction` sets `totalSize = dataSize` (UTransact.cpp:746).

Split/multi-part transactions: each part carries its own `dataSize` and a constant `totalSize`;
the receiver reassembles by appending to the task's handle when a task with the matching
`(isReply, id)` already exists (UTransact.cpp:1093-1124), and creates a fresh inbound task if
not. The reply uses the **same `id`** as the request (UTransact.cpp:468, 505).

### 1.5 Connection establish (TRTP handshake)

**[SOURCE]** `UTransact.cpp:264-282` (client, in `GetConnectStatus`):

```c
	struct {
		Uint32 protocol;	// 'TRTP'
		Uint32 subProtocol;
		Uint16 version;		// 1
		Uint16 subVersion;
	} sndData = { TB((Uint32)0x54525450), TB(TRN->protocolTag), TB((Uint16)1), TB(TRN->versionTag) };
	UTransport::Send(tpt, &sndData, sizeof(sndData));
```

12 bytes, all big-endian. The client sets `protocolTag = 0x484F544C` (`'HOTL'`) and
`versionTag = 2` (`Apps/Client/Source/Hotline.cpp:8730`: `mTpt->SetVersion(0x484F544C, 2);`).
The server (`Apps/Server/Source/HotlineServ.cpp:4675-4690`) accepts `'HOTL'` version 2 (a
normal session) or version 3 (a transfer connection, routed to `mTransferEstabList`); it
rejects everything else. The server's reply (`UTransact.cpp:395-410`) is 8 bytes
`{ TB('TRTP'), TB(error=0) }`; `RejectEstablish` sends `{ TB('TRTP'), TB(reason) }`.

### 1.6 Transfer connection (HTXF)

**[SOURCE]** For file/folder/banner transfers the client opens a *second* TCP connection and
identifies with a different header (`Apps/Client/Source/HotlineTasks.cpp:2655-2663`):

```c
	struct { Uint32 protocol;	// 'HTXF'
	         Uint32 refNum; Uint32 dataSize; Uint32 rsvd; }
	sndData = { TB((Uint32)0x48545846), TB(mRefNum), 0, 0 };
```

The server parses 16 bytes as `{ Uint32 protocol; Uint32 refNum; Uint32 dataSize; Uint16 type;
Uint16 rsvd }` (`HotlineServ.cpp:5137-5143`), where `type` = 0 file / 1 folder / 2 banner, and
`refNum & 1` selects download (odd) vs upload (even). The folder-download client appends a
`Uint16 nextMsg = dlFldrAction_NextFile` (18 bytes total, `HotlineTasks.cpp:3480`), which the
server's folder-download state machine consumes as its first "next file" command.

---

## 2. Field serialization

### 2.1 UFieldData wire layout

**[SOURCE]** `AppWarrior/Source/Data/UFieldData.cpp:2-11` (authoritative comment) and the
`AddField` implementation (426-514):

```
Uint16 count;                     // big-endian
struct {                          // no padding (ALIGN_FIELDS == 0)
	Uint16 id;                    // big-endian field ID
	Uint16 size;                  // big-endian data size
	Uint8  data[size];            // raw payload
} field[count];
```

Each field is a 4-byte header (`id` + `size`) followed by raw data. `ALIGN_FIELDS` is `0`
(`UFieldData.cpp:39`), so despite the comment mentioning 2-byte alignment, **fields are not
aligned** in this build. Field size is a `Uint16`, so a single field is capped at 65535 bytes
(`UFieldData::AddField` fails with `error_LimitReached` above that, `UFieldData.cpp:434-440`).
Field order is preserved; lookups are by ID via a sorted table.

### 2.2 Integer encoding

**[SOURCE]** `UFieldData.cpp:516-528` (`AddInteger`) — variable width:

```c
	if (inInteger & 0xFFFF0000) { Uint32 ln = TB((Uint32)inInteger); AddField(inRef, inID, &ln, sizeof(ln)); }
	else                        { Uint16 sn = TB((Uint16)inInteger); AddField(inRef, inID, &sn, sizeof(sn)); }
```

i.e. integers that fit in 16 bits (positive or negative within `Int16` range) are stored as
2 bytes; otherwise 4 bytes. The decoder (`GetInteger`, `UFieldData.cpp:554-600`) accepts **1, 2,
or 4** bytes and zero-extends otherwise. All big-endian.

### 2.3 CFlatten helpers

**[SOURCE]** `AppWarrior/Headers/CFlatten.h:18-19,62-63,119-140,167-173`:

- `WriteWord/WriteLong` → `TB()`; `ReadWord/ReadLong` → `FB()` (big-endian).
- `WritePString`: 1 length byte (≤255) + bytes (119-125).
- `WriteWString`: `Uint16` length (big-endian) + bytes (127-133).
- `WriteLString`: `Uint32` length (big-endian) + bytes (135-140).
- `WriteDateTimeStamp`/`ReadDateTimeStamp` (167-173, 260-266): `SDateTimeStamp` =
  `Uint16 year; Uint16 msecs; Uint32 seconds` (8 bytes, all big-endian;
  struct at `AppWarrior/Headers/UDateTime.h:80-84`).

### 2.4 Strings / script codes

**[SOURCE]** Names in file lists are raw bytes copied verbatim; `SMyFileInfo.nameScript` is
hard-coded to `0` (`HotlineServ.cpp:2212`: `fi.nameScript = 0;`). `SMyUserDataFile` carries
`nameScript/aliasScript/loginScript/passwordScript` as `Uint16` fields
(`HotlineClientServerCommon.h:318-327`) but the tree never sets them to any non-zero value.
There is **no** UTF-8 (256) script handling anywhere in this tree. **[EXTERNAL]** In the
historical protocol, script code 0 = MacRoman/system script and 256 = Unicode/UTF-8; this tree
only ever emits 0, so payloads are MacRoman/raw bytes. **[UNKNOWN]** whether any peer ever sets
256 and how this tree would decode it (it would be treated as an opaque name).

### 2.5 Message quoting (`myField_QuotingMsg`)

**[SOURCE]** The server treats `myField_QuotingMsg` as opaque bytes and relays it unchanged
(`HotlineServTrans.cpp:187-189, 337-339`). The quoting/escaping ("> " prefixes, "On <date> …")
is produced by the client and is not parsed server-side. There is no escaping scheme in the
wire format — messages are raw bytes (`myField_Data`), newlines are `\r` (CR), and the chat
composer inserts the sender name at the start of every line by replacing `\r`
(`HotlineServTrans.cpp:505-516`).

### 2.6 Nested structures

**SMyFileInfo** (filename-list item, `HotlineClientServerCommon.h:256-264`, packed):

```
Uint32 type;       // big-endian four-char code (e.g. 'HTft' incomplete, 'fldr' folder)
Uint32 creator;    // big-endian four-char code (e.g. 'HTLC')
Uint32 fileSize;   // big-endian
Uint32 rsvd;       // 0
Uint16 nameScript; // 0
Uint16 nameSize;   // big-endian (no length byte: size is in the header)
Uint8  nameData[]; // name bytes (MacRoman)
```

**[SOURCE]** `HotlineServ.cpp:2183-2221` (`BuildFileList`) emits one `myField_FileNameWithInfo`
(200) field per visible item: `fi.type = typeCode; fi.creator = creatorCode; fi.fileSize =
TB(size); fi.rsvd = 0; fi.nameScript = 0; fi.nameSize = TB(name[0]);` then
`AddField(..., sizeof(SMyFileInfo) + name[0])`. Note `type`/`creator` are already
big-endian when returned by `UFS::GetListNext`. `myField_FileName` (201) carries a bare
p-string name (used in several request/response handlers).

**SMyUserInfo** (online-user-list item, `HotlineServ.h:215-221`, packed):

```
Uint16 id;        // big-endian user ID
Int16  iconID;    // big-endian icon
Uint16 flags;     // big-endian
Uint16 nameSize;  // big-endian
Uint8  nameData[];// name bytes
```

**[SOURCE]** `HotlineServTrans.cpp:1468-1520` emits one `myField_UserNameWithInfo` (300) per
logged-in, visible user.

**SMyUserAccess** (`HotlineClientServerCommon.h:269-302`): 8 bytes = two `Uint32` bitmasks
(`data[0]`, `data[1]`), bit index = privilege (`myAcc_*` enum 0–54, `HotlineClientServerCommon.h:174-233`).
Carried as raw 8 bytes in `myField_UserAccess` (110). Bit order within the 8-byte blob is
platform-native `Uint32` array copied raw — see §9 for the endianness caveat.

**SMyUserDataFile** (account record; wire form for `myTran_GetUser`/`NewUser`/`SetUser`;
`HotlineClientServerCommon.h:310-330`, packed):

```
Uint16 version = 1 (big-endian); Int16 iconID;
SMyUserAccess access; Uint16 maxSimulDownloads; Uint8 rsvd[512];
Uint16 nameScript;  Uint16 nameSize;  Uint8 nameData[64];
Uint16 aliasScript; Uint16 aliasSize; Uint8 aliasData[64];
Uint16 loginScript; Uint16 loginSize; Uint8 loginData[32];
Uint16 passwordScript; Uint16 passwordSize; Uint8 passwordData[32];
```

The account record is **not** sent whole; the server serializes individual fields
(`myField_UserName`, `myField_UserLogin`, `myField_UserPassword`, `myField_UserAccess`,
`myField_UserAlias`) — see §7. On disk this full struct is the `UserData` file (stored
big-endian for the multi-byte fields; password kept scrambled, §4.4).

**News category list (v1.5+)** — `myField_NewsCatListData15` (323) items, built in
`HotlineServ.cpp:2233-2353`:

```
Uint16 type = 2 (folder) or 3 (category);   // big-endian
Uint16 count;                               // big-endian (item count)
[if type==3:] SGUID guid (16 bytes); Uint32 addSN; Uint32 delSN;  // all big-endian
Uint8  nameSize; Uint8 name[nameSize];
```

**News category list (legacy)** — `myField_NewsCatListData` (320), for `vers < 15`
(`HotlineServ.cpp:2336-2351`): each item is `Uint8 type` (1 = folder, 10 = category, 255 =
other) followed by the name bytes (the original name p-string's length byte is overwritten with
the type code).

**News article list** — `myField_NewsArtListData` (321); see §7.

### 2.7 `myField_Data` sub-list format (user list)

**[SOURCE]** `ProcessTran_GetUserList` (`HotlineServTrans.cpp:3355-3437`) packs a *nested*
field-data blob into `myField_Data` (101): each account's `{UserName, UserLogin, UserPassword,
UserAccess}` fields are serialized as a complete UFieldData structure (its own `count` +
`{id,size,data}[]`) and appended as the value of one `myField_Data` field. So `myField_Data`
here is itself a recursive field container. The client (`HotlineTasks.cpp:4831`) reads it back
with `SetDataHandle`.

---

## 3. Transaction catalog

IDs from `HotlineClientServerCommon.h:6-82`; direction and fields from the server dispatch
(`HotlineServ.cpp:4890-5110`) and the client task constructors in `HotlineTasks.cpp`. All IDs
100–500. Direction: C→S = client request, S→C = server-initiated/reply.

### 100–139 (messaging / chat / login)

| ID | Name | Dir | Fields carried | Notes |
|---|---|---|---|---|
| 100 | Error | (reply) | — | Error is signalled by the header `error` field + `myField_ErrorText`(100). See §2.7 note. |
| 101 | GetMsgs | C→S | — | Reply: `myField_Data`(101) = whole message board (≤64 KB). |
| 102 | NewMsg | S→C | `myField_Data`(101) | Broadcast of a new board post. |
| 103 | PostMsg | C→S | `myField_Data`(101) | Post to message board (capped 8 KB server-side). |
| 104 | ServerMsg | S→C | `myField_UserID`(103), `myField_UserName`(102), `myField_Options`(113), `myField_Data`(101), optional `myField_QuotingMsg`(214) | Private message / server message. |
| 105 | ChatSend | C→S | `myField_ChatOptions`(109), `myField_ChatID`(114), `myField_Data`(101) | Chat. |
| 106 | ChatMsg | S→C | `myField_ChatID`(114), `myField_Data`(101) | Chat broadcast. |
| 107 | Login | C→S | see §4 | |
| 108 | SendInstantMsg | C→S | `myField_UserID`(103), `myField_Options`(113), `myField_Data`(101), optional `myField_QuotingMsg`(214) | |
| 109 | ShowAgreement | S→C | `myField_NoServerAgreement`(154) or `myField_Data`(101) | |
| 110 | DisconnectUser | C→S | `myField_UserID`(103) | Admin kick. |
| 111 | DisconnectMsg | S→C | `myField_Data`(101) | "You were disconnected". |
| 112 | InviteNewChat | C→S | `myField_UserID`(103) | Create private chat w/ user. |
| 113 | InviteToChat | S→C | `myField_ChatID`(114), `myField_UserID`(103), `myField_UserName`(102) | |
| 114 | RejectChatInvite | C→S | `myField_ChatID`(114) | |
| 115 | JoinChat | C→S | `myField_ChatID`(114) | |
| 116 | LeaveChat | C→S | `myField_ChatID`(114) | |
| 117 | NotifyChatChangeUser | S→C | `myField_ChatID`(114), `myField_UserNameWithInfo`(300) | |
| 118 | NotifyChatDeleteUser | S→C | `myField_ChatID`(114), `myField_UserID`(103) | |
| 119 | NotifyChatSubject | S→C | `myField_ChatID`(114), `myField_ChatSubject`(115) | |
| 120 | SetChatSubject | C→S | `myField_ChatID`(114), `myField_ChatSubject`(115) | |
| 121 | Agreed | C→S | `myField_UserName`(102), `myField_UserIconID`(104), `myField_Options`(113) | Post-agreement acknowledgement; completes login. |
| 122 | ServerBanner | S→C | `myField_ServerBannerType`(152), optional `myField_ServerBannerUrl`(153) | |
| 123 | IconChange | C→S | `myField_UserIconID`(104) | |
| 124 | NickChange | C→S | `myField_NickName`(118) | |
| 125 | FakeRed | C→S | `myField_FakeRed`(119) | "fake red light" (idle spoof). |
| 126 | Away | C→S | `myField_Away`(120) | Toggle away. |
| 127 | CrazyServer | — | — | declared, not dispatched. |
| 128 | BlockDownload | C→S | `myField_BlockDownload`(121) | |
| 129 | Visible | C→S | `myField_Visible`(117) (note: enum 117 duplicates `myField_IconId`) | Invisibility. |
| 130 | AdminSpector | C→S | `myField_AdminSpector`(122) | |
| 131 | StandardMessage | C→S | `myField_StandardMessage`(123) | Server logs it; stubbed. |

### 200–214 (file system)

| ID | Name | Dir | Fields carried |
|---|---|---|---|
| 200 | GetFileNameList | C→S | `myField_FilePath`(202). Reply: repeated `myField_FileNameWithInfo`(200). |
| 202 | DownloadFile | C→S | `myField_FileName`(201), `myField_FilePath`(202), `myField_FileXferOptions`(204), optional `myField_FileResumeData`(203). Reply: `myField_TransferSize`(108), `myField_FileSize`(207), `myField_RefNum`(107), optional `myField_WaitingCount`(116). |
| 203 | UploadFile | C→S | `myField_FilePath`(202), `myField_FileName`(201), `myField_FileXferOptions`(204), `myField_TransferSize`(108). Reply: `myField_RefNum`(107), optional `myField_FileResumeData`(203). |
| 204 | DeleteFile | C→S | `myField_FileName`(201), `myField_FilePath`(202) |
| 205 | NewFolder | C→S | `myField_FileName`(201), `myField_FilePath`(202) |
| 206 | GetFileInfo | C→S | `myField_FileName`(201), `myField_FilePath`(202). Reply: `myField_FileType`(213)/`myField_FileTypeString`(205)/`myField_FileCreatorString`(206), `myField_FileSize`(207), `myField_FileName`(201), `myField_FileCreateDate`(208), `myField_FileModifyDate`(209), `myField_FileComment`(210). |
| 207 | SetFileInfo | C→S | `myField_FileName`(201), `myField_FilePath`(202), `myField_FileNewName`(211), `myField_FileNewPath`(212), `myField_FileComment`(210) |
| 208 | MoveFile | C→S | `myField_FileName`(201), `myField_FilePath`(202), `myField_FileNewPath`(212) |
| 209 | MakeFileAlias | C→S | `myField_FileName`(201), `myField_FilePath`(202), `myField_FileNewPath`(212) |
| 210 | DownloadFldr | C→S | `myField_FileName`(201), `myField_FilePath`(202), `myField_FileXferOptions`(204), optional `myField_FileResumeData`(203). Reply: `myField_TransferSize`(108), `myField_FldrItemCount`(220), `myField_RefNum`(107), optional `myField_WaitingCount`(116). |
| 211 | DownloadInfo | S→C | `myField_RefNum`(107), `myField_TransferSize`(108) (progress notifications) |
| 212 | DownloadBanner | C→S | Reply: `myField_TransferSize`(108), `myField_RefNum`(107). Banner body sent on transfer connection. |
| 213 | UploadFldr | C→S | `myField_FileName`(201), `myField_FilePath`(202), `myField_FldrItemCount`(220) |
| 214 | KillDownload | C→S | `myField_RefNum`(107) |

`myField_FileXferOptions` (204) semantics: 1 = resume, 2 = "raw data fork only" (no file
package). See §6.

### 300–355 (users)

| ID | Name | Dir | Fields |
|---|---|---|---|
| 300 | GetUserNameList | C→S | Reply: repeated `myField_UserNameWithInfo`(300) (§2.6). |
| 301 | NotifyChangeUser | S→C | `myField_UserNameWithInfo`(300) |
| 302 | NotifyDeleteUser | S→C | `myField_UserID`(103) |
| 303 | GetClientInfoText | C→S | `myField_UserID`(103). Reply: `myField_Data`(101) = human-readable text (not binary). |
| 304 | SetClientUserInfo | C→S | `myField_UserName`(102), `myField_UserIconID`(104), `myField_Options`(113) |
| 348 | GetUserList | C→S | Reply: `myField_Data`(101) = nested field blobs (§2.7). |
| 349 | SetUserList | C→S | `myField_Data`(101) = nested field blobs. |
| 350 | NewUser | C→S | `myField_UserName`(102), `myField_UserAlias`(111), `myField_UserLogin`(105), `myField_UserPassword`(106), `myField_UserAccess`(110) |
| 351 | DeleteUser | C→S | `myField_UserLogin`(105) |
| 352 | GetUser | C→S | `myField_UserLogin`(105). Reply: `myField_UserName`, `myField_UserLogin` (scrambled), `myField_UserPassword` (masked 'x'), `myField_UserAccess`. |
| 353 | SetUser | C→S | `myField_UserName`, `myField_UserAlias`, `myField_UserLogin`, `myField_UserPassword`, `myField_UserAccess` |
| 354 | UserAccess | S→C | `myField_UserAccess`(110) (sent after login/agreed) |
| 355 | UserBroadcast | both | `myField_UserID`(103), `myField_UserName`(102), `myField_Data`(101) |

### 370–411 (news)

| ID | Name | Dir | Fields |
|---|---|---|---|
| 370 | GetNewsCatNameList | C→S | `myField_NewsPath`(325). Reply: `myField_NewsCatListData15`(323) or `myField_NewsCatListData`(320). |
| 371 | GetNewsArtNameList | C→S | `myField_NewsPath`(325). Reply: `myField_NewsArtListData`(321). |
| 380 | DelNewsItem | C→S | `myField_NewsPath`(325) |
| 381 | NewNewsFldr | C→S | `myField_FileName`(201), `myField_NewsPath`(325) |
| 382 | NewNewsCat | C→S | `myField_NewsCatName`(322), `myField_NewsPath`(325) |
| 400 | GetNewsArtData | C→S | `myField_NewsPath`(325), `myField_NewsArtID`(326), `myField_NewsArtDataFlav`(327). Reply: `myField_NewsArtData`(333), `myField_NewsArtPrevArt`(331), `myField_NewsArtNextArt`(332), `myField_NewsArtParentArt`(335), `myField_NewsArt1stChildArt`(336), `myField_NewsArtTitle`(328), `myField_NewsArtPoster`(329), `myField_NewsArtDataFlav`(327), `myField_NewsArtDate`(330). |
| 410 | PostNewsArt | C→S | `myField_NewsPath`(325), `myField_NewsArtID`(326) (parent), `myField_NewsArtTitle`(328), `myField_NewsArtFlags`(334), `myField_NewsArtDataFlav`(327), `myField_NewsArtData`(333) |
| 411 | DelNewsArt | C→S | `myField_NewsPath`(325), `myField_NewsArtID`(326), `myField_NewsArtRecurseDel`(337) |

### 500

| ID | Name | Dir | Fields |
|---|---|---|---|
| 500 | KeepConnectionAlive | C→S | — (no-op keepalive) |

Unknown types are answered with `ProcessTran_Unknown` → error reply
(`HotlineServTrans.cpp:3004-3012`).

---

## 4. Login / authentication

### 4.1 Handshake sequence

**[SOURCE]** `HotlineServ.cpp:4890-4910` (server), `HotlineTasks.cpp` (`CMyLoginTask`,
`CMyAgreedTask`).

1. TCP connect; TRTP establish (`'TRTP'`/`'HOTL'`/v2) as in §1.5.
2. C→S `myTran_Login` (107).
3. S→C reply: `myField_Vers`(160)=197, `myField_CommunityBannerID`(161), `myField_ServerName`(162)
   (server declares itself "1.9.7").
4. S→C `myTran_UserAccess` (354) — the account's privileges.
5. S→C `myTran_ShowAgreement` (109) — either `myField_NoServerAgreement`(154)=1 (no agreement
   needed) or `myField_Data`(101)=agreement text.
6. C→S `myTran_Agreed` (121) — `myField_UserName`(102) real name, `myField_UserIconID`(104)
   icon, `myField_Options`(113) (bit0 refuse private msg, bit1 refuse private chat, bit2
   automatic response). Server marks `hasLoggedIn`, re-sends `myTran_UserAccess`, and (if client
   vers ≥ 151 and a banner is configured) sends `myTran_ServerBanner` (122).

### 4.2 Normal (non-crypto) login

**[SOURCE]** `HotlineTasks.cpp:1496-1518` (client) and `HotlineServTrans.cpp:1554-1652` (server).

The client bitwise-inverts (one's-complement) each byte of both login and password before
putting them in `myField_UserLogin`/`myField_UserPassword` as p-strings:

```c
	s = str[0]; p = str + 1;
	while (s--) { *p = ~(*p); p++; }
	mFieldData->AddPString(myField_UserLogin, str);
```

The server unscrambles the login with `*p = ~(*p)` (`HotlineServTrans.cpp:1603-1610`) and
lower-cases it, then compares the **received password bytes** directly against the stored
password bytes:

```c
	if ((psUserPass[0] != FB(info.passwordSize)) || !UMemory::Equal(psUserPass+1, info.passwordData, FB(info.passwordSize)))
		goto loginIncorrect;
```

So the password is **plaintext compared** (over the scrambled-inverted wire form) — the server
stores the inverted bytes in the account file and compares byte-for-byte. **[SOURCE]**
`HotlineServ.cpp:3307` stores `stUserInfo.version` big-endian; the password is stored still
scrambled (inverted) — `NewUser`/`SetUser` only unscramble the *login* before storing
(`HotlineServTrans.cpp:3141-3144`, "leave password b/c we want it scrambled in the data file").

### 4.3 HOPE crypto login (client-side; server side absent)

**[SOURCE]** `HotlineTasks.cpp:1462-1650`. When `gApp->mUseCrypt` is set, the client uses a
two-stage "HOPE" login:

**Stage 1** C→S `myTran_Login` with:
- `myField_UserLogin` = 1 zero byte, `myField_UserPassword` = 1 zero byte,
- `myField_MacAlg`(3588) = `00 02 09 "HMAC-SHA1"` (list of MAC algorithms: byte0=0 reserved,
  byte1=count=2, byte2=len=9, "HMAC-SHA1"; only the first entry is actually sent in the 12-byte
  buffer),
- `myField_C_CipherAlg`(3778) = `00 01 08 "BLOWFISH"` (byte0=0, byte1=count=1, byte2=len=8,
  "BLOWFISH").

**Stage 2** S→C reply is expected to carry `myField_SessionKey`(3587), `myField_MacAlg`(3588)
(server's choice: p-string "HMAC-SHA1" or "HMAC-MD5" after a 2-byte prefix), and
`myField_S_CipherAlg`(3777) (p-string "BLOWFISH"). The client then computes
`MacLogin = HMAC(login, sessionKey)`, `MacPassword = HMAC(password, sessionKey)` (using the
negotiated hash) and sends a second `myTran_Login` with those 16/20-byte digests as
`myField_UserLogin`/`myField_UserPassword`, echoing `myField_C_CipherAlg` and `myField_Vers`=197.

**[SOURCE, critical]** The **server never generates a session key**: a tree-wide grep finds
`myField_SessionKey` only in the client and in `HLCrypt.cpp`. The server's `ProcessTran_Login`
reads `myField_MacAlg`/`myField_C_CipherAlg` into local buffers but never uses them (the
handling is commented out, `HotlineServTrans.cpp:1575-1585`). `gCrypt` is declared
`extern HLCrypt *gCrypt;` in `HotlineServ.h:1032` but never assigned on the server. Therefore
**this server build only implements the normal (non-encrypted) login path**; the HOPE path is
dead on the server side. The crypto primitives are nonetheless fully implemented in
`AppWarrior/Source/Crypt/` and are documented below because a rewrite must preserve them.

### 4.4 Key derivation (HLCrypt::Init)

**[SOURCE]** `AppWarrior/Source/Crypt/HLCrypt.cpp:16-54`:

```c
	mMacLen = mHash->GetMacLen();                 // 16 (MD5) or 20 (SHA1)
	mSessionKey = sessionKey;
	temp1 = HMAC(password, sessionKey);
	temp1 = HMAC(password, temp1);
	temp2 = HMAC(password, temp1);
	if (isClient) { mEncodeKey = temp2; mDecodeKey = temp1; }
	else          { mEncodeKey = temp1; mDecodeKey = temp2; }
```

i.e. both directions derive from `password` and `sessionKey`; client encode key is the third
HMAC, server encode key is the double HMAC (they are the inverse pair). `PermEncodeKey(n)` /
`PermDecodeKey(n)` (`HLCrypt.cpp:56-74`) iterate `HMAC(key, sessionKey)` `n` times to permute
the cipher key — this is what the 1-byte `flag`/`rand` in `STranHdr` selects (§1.1): on send,
`_TNSendTran` draws a random 0..15 and maps 2,7,13 to a small permutation count (or 0),
writes it into `hdr.flag`, and on receive the peer permutes the key by `flag` before
decrypting the body after the first 2 bytes (`UTransact.cpp:922-939, 1077-1091`).

### 4.5 HMAC-MD5 / HMAC-SHA1

**[SOURCE]** `HLMD5.cpp:4-45` and `HLSha1.cpp:444-484` are standard **RFC 2104 HMAC** (block
size 64, ipad 0x36 / opad 0x5C, key hashed if longer than 64 bytes). `HLMD5::GetMacLen()`=16,
`HLSha1::GetMacLen()`=20. MD5 is the RFC 1321 algorithm (`UDigest.cpp:435-661`); SHA1 is the
A.M. Kuchling "fixed" SHA (`HLSha1.cpp`). MD5 output is little-endian byte order per RFC
(`_MD5::Encode` writes `mState[i] & 0xFF` first).

### 4.6 Blowfish

**[SOURCE]** `HLBlowfish.cpp`. Standard Blowfish (PI-derived P-array/S-boxes from
`HLBlowfishData.h`), key schedule `ExpandKey` (119-167), **OFB-64** stream mode (`OFB64`,
170-196) with a 64-bit IV initialized to zero (`Init`, 4-16) and a byte offset `n` (0..7) so
the stream cipher is continuous across separate `Encode`/`Decode` calls. `SelfTest` (199-237)
runs the standard Eric Young variable/set-key vectors.

### 4.7 HLRand

**[SOURCE]** `HLRand.cpp:109-131`. `HLRand::GetBytes` is a **deterministic counter-mode
HMAC-SHA1 PRNG**:

```c
	while(outBytes.len) {
		sTheHash.HMAC_XXX(temp, (DataBuffer){(Uint8*)&sRandCounter, sizeof(sRandCounter)},
		                        (DataBuffer){sRandPool, HLRAND_HASHLEN});
		sRandCounter++;
		...copy out...
	}
```

The 20-byte `sRandPool` is initialized in `ReadSeed` (41-75) by `HMAC(seed, seed)` where `seed`
is the 256-byte `rand_seed` file (or uninitialized memory on first run). So HLRand is a
hash-based PRNG seeded from a persisted 256-byte file; deterministic given the seed file, but
seeded with OS-uninitialized memory if no seed exists. `sRandCounter` is not persisted
separately (the whole pool is rewritten on `Cleanup` via `WriteSeed`).

### 4.8 Exact digest formula

**[SOURCE]** The only digest computed over *login/password* is in the HOPE path:
`HMAC_H(login, sessionKey)` and `HMAC_H(password, sessionKey)` where `H` is the negotiated
`HMAC-SHA1`/`HMAC-MD5` (`HotlineTasks.cpp:1627-1628`). The cipher keys (§4.4) are
`HMAC_H(password, sessionKey)` applied 2 and 3 times. **[EXTERNAL]** This matches the known
Hotline "HOPE"/encryption extension key schedule. **[UNKNOWN]** whether this tree ever
implemented a `login+password+salt` MD5 pre-digest: no such concatenation exists anywhere in
`legacy/AppWarrior/Source/Crypt` or the login code — the source hashes login and password
separately, never a concatenation with salt. The only "salt" is the server-provided
`sessionKey` used as the HMAC *message*.

---

## 5. Version / ID translation

**[SOURCE]** The premise that `HotlineServIDTranslate` maps legacy Hotline 1.2 transaction/field
IDs is **incorrect for this tree**. `Apps/Server/Source/HotlineServIDTranslate.h` and `.cpp`
implement **community-ID ⇄ base-36 string** conversion for the Ad System, not a transaction-ID
table:

```c
const char kHLCommID_IntToCharTab[] = " ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789._";
const int  kHLCommID_IDBase = sizeof(kHLCommID_IntToCharTab);   // 39
const int  kHLCommID_MaxIDLen = 6;
```

`HLCommID_IntToChar` and `HLCommID_CharToInt` convert a 32-bit community ID to/from a
little-endian base-39 string (most-significant character last). `TranslateToBase36` /
`TranslateFromBase36` handle serial-number sub-fields (used in
`HotlineServ.cpp:1216,1245-1246` for license/serial parsing).

**There is no Hotline 1.2 → 1.5 transaction/field ID translation table anywhere in the tree.**
The "legacy client" compatibility that exists is field-level, not ID-level:

- `ProcessTran_Login` accepts `myField_UserName`/`myField_UserIconID` "for compatibility with
  old clients" (`HotlineServTrans.cpp:1587-1594`).
- `BuildNewsCatList` branches on `inClientVers >= 15` to emit the v1.5 `myField_NewsCatListData15`
  vs the pre-April-15 `myField_NewsCatListData` (320) format (`HotlineServ.cpp:2277-2353`).
- Client/server version negotiation is the `'HOTL'` subVersion (2 = normal, 3 = transfer) in the
  TRTP establish (§1.5); `myField_Vers`(160) carries the peer's app version string integer
  (server sends 197 = "1.9.7").

**[EXTERNAL]** In the real historical protocol, Hotline 1.2 used different transaction IDs
(login=1, etc.) mapped to the 100–500 scheme by 1.5-era servers. This tree predates that
mapping work or dropped it; it only speaks the 100–500 scheme.

---

## 6. File transfer

### 6.1 `myField_FileResumeData` (resume data)

**[SOURCE]** `AppWarrior/Source/Files/UFileSys(W).cpp:2216-2226`:

```c
struct SFlatFileResumeEntry {
	Uint32 fork;
	Uint32 dataSize;
	Uint32 rsvdA, rsvdB;
};
struct SFlatFileResumeData {
	Uint32 format;              // always 'RFLT' (0x52464C54)
	Uint16 version;             // 1
	Uint8  rsvd[34];
	Uint16 count;
	SFlatFileResumeEntry forkInfo[];
};
```

All big-endian. `ResumeFlatten` (`UFileSys(W).cpp:2310-2332`) validates `format=='RFLT'`,
`version==1`, and for each `fork=='DATA'` entry sets `dataResumeSize = FB(entry.dataSize)`. This
is what the client sends in `myField_FileResumeData` when resuming (`HotlineTasks.cpp:2443`
via `ResumeUnflatten`, sent at `2583`), and what the server sends back when resuming an upload
(`HotlineServTrans.cpp:2539-2550`). `myField_FileXferOptions` = 1 = resume, 2 = raw data fork.

### 6.2 Single-file payload — the "FILP" flat file

**[SOURCE]** `UFileSys(W).cpp:2186-2198`:

```c
struct SFlatFileHeader      { Uint32 format; Uint16 version; Uint8 rsvd[16]; Uint16 forkCount; };
struct SFlatFileForkHeader  { Uint32 forkType; Uint32 compType; Uint32 rsvd; Uint32 dataSize; };
struct SFlatFileInfoFork    { Uint32 platform; Uint32 typeSig; Uint32 creatorSig; Uint32 flags;
                              Uint32 platFlags; Uint8 rsvd[32]; SDateTimeStamp createDate;
                              SDateTimeStamp modifyDate; Uint16 nameScript; Uint16 nameSize;
                              Uint8 nameData[]; /* + Uint16 commentSize + commentData */ };
```

Windows build (`StartFlatten`, 2274-2305): `format='FILP'` (0x46494C50), `version=1`,
`forkCount=2`, `infoHdr.forkType='INFO'` (0x494E464F), `dataHdr.forkType='DATA'` (0x44415441),
`platform='MWIN'`. Stream order (`ProcessFlatten`, 2356+): `SFlatFileHeader` (24 bytes) →
`infoHdr` (16) → `info` data → `dataHdr` (16) → raw data fork. `GetFlattenSize` (2345-2353):
`sizeof(SFlatFileHeader) + 2*sizeof(SFlatFileForkHeader) + infoDataSize + dataSize`. The Mac
build (`UFileSys(M).cpp:1911-1950, 2160-2166`) uses `forkCount=3` (data/rsrc/info) and
`platform='AMAC'`. `opts==2` ("raw") bypasses the package and streams the bare data fork
(`HotlineServTrans.cpp:2079-2088`).

### 6.3 DownloadFile / DownloadFldr / UploadFile exchange

**[SOURCE]** `HotlineServTrans.cpp:2003-2634`. Both downloads return `myField_TransferSize`,
`myField_RefNum` (odd = download), and `myField_FileSize` (file) / `myField_FldrItemCount`
(folder); `myField_WaitingCount` when queued. The actual bytes flow on the `'HTXF'` transfer
connection (§1.6). Transfer completion/error: the transfer connection is closed by the server
when done; the client tracks `DownloadedItems == TotalItems` for folders and
`mFileDlSize >= mFileTotalSize` for files (`HotlineTasks.cpp:3694-3740`). Errors during the
request phase are the `error` header + `myField_ErrorText`.

### 6.4 Folder download item header

**[SOURCE]** server `HotlineServ.cpp:5516-5560` and client `HotlineTasks.cpp:3490-3530`. Each
item (after the root) is:

```
Uint16 size;       // bytes following this field (== 2 + 2 + pathData)
Uint16 type;       // bit0: 1 = folder, 0 = file
Uint16 pathCount;  // number of path components (root excluded)
component[pathCount] = { Uint16 script (=0); Uint8 namelen; Uint8 name[namelen]; }
```

The path is built back-to-front by `CMyDLFldr::GetNextItem` (`HotlineFolderDownload.cpp`); each
component is `{ Uint16 script=0, p-string name }`. The client drives it with 16-bit
`dlFldrAction_*` commands (`HotlineClientServerCommon.h:243-248`): 1 = SendFile, 2 =
ResumeFile, 3 = NextFile. *(Corrected during Phase 4 implementation: this section originally
swapped SendFile/NextFile; the verbatim enum and the server dispatch — it waits for
`dlFldrAction_NextFile` before sending the next item — confirm SendFile=1, ResumeFile=2,
NextFile=3.)* On ResumeFile the client sends `{Uint16 action=2; Uint16 size; RFLT
resume data}` (`HotlineTasks.cpp:3592-3603`). Server replies to SendFile with a `Uint32`
flattened size, then streams the FILP file (§6.2).

### 6.5 Archive container ("harc") — the update-package codec

**[SOURCE]** `Apps/Common Files/HotlineArchiveStruct.h:56-132` + `HotlineArchiveDecoder.cpp`.
This is a *separate* container from FILP, used for the client self-update resource
(`Hotline.cpp:6230-6252`) and encoded by `Rez2Dat`. Layout (all multi-byte fields big-endian):

```
SMyArcHead:
  Uint32 sig  = 'harc' (0x68617263)
  Uint32 vers = 1
  Uint32 archiveSize        // size of everything below this Uint32
  Uint32 rsvd[4]
  Uint8  archiveName[64]    // p-string
  Uint16 fileCount
  Uint16 fileAutoLaunch     // 0 = none
  Uint16 rsvd3size
  Uint8  rsvd3data[rsvd3size]

file[fileCount]:
  SMyArcPathHead: Uint32 type ('file'/'fldr'/...), Uint32 rsvd, Uint16 pathSize,
                  Uint8 pathData[pathSize] =
                    { Uint16 pathCount;
                      pathCount × { Uint16 script; Uint8 namelen; Uint8 name[namelen]; } }
  Uint16 rsvdSize; Uint8 rsvd[rsvdSize]
  SMyArcFileHead: Uint32 compressionType ('zlib' 0x7A6C6962 or 'raw ' 0x72617720),
                  Uint32 decompressedSize, Uint32 compressedSize,
                  Uint8 compressedFileFlatData[compressedSize]
```

The per-file payload is a **zlib**-compressed (or raw) **FILP** flat file, fed to
`ProcessUnflatten` by the decoder (`HotlineArchiveDecoder.cpp:297-360`). The decoder is a
state machine (`arcState_*`, `HotlineArchiveDecoder.cpp:53-63`).

---

## 7. News

### 7.1 Article list layout (`myField_NewsArtListData`)

**[SOURCE]** `HotlineServNewsDatabase.h:121-138` (comment) and
`HotlineServNewsDatabase.cpp:1308-1422` (`CNZArticleList`). Header (10 bytes) + one entry per
article:

```
Uint32 groupID = 0     // dummy, backward compat
Uint32 count           // article count (big-endian)
Uint8  name  = 0       // dummy p-string (empty)
Uint8  desc  = 0       // dummy p-string (empty)

article[count]:
  Uint32 id                // big-endian
  SDateTimeStamp date      // { Uint16 year; Uint16 msecs; Uint32 seconds } big-endian
  Uint32 parentID          // 0 = top-level thread; big-endian
  Uint32 flags             // big-endian
  Uint16 flavorCount       // big-endian
  Uint8  titleSize; title[titleSize]
  Uint8  posterSize; poster[posterSize]
  [optional] Uint8 externIdSize; externId[externIdSize]   // only when inGetExternID
  flavor[flavorCount] = { Uint8 flavorSize; flavor[flavorSize]; Uint16 size; }
```

`AddArticle` writes id/date/parentID/flags directly from the (already network-order)
`_NZ_SNewsItemBlock.body`; title/poster are p-strings; each flavor is a MIME-type p-string +
big-endian `Uint16` data size. Threading is parent/child via `parentID` (and `parentOffset`/
`firstChildOffset` internally), and `myField_NewsArtParentArt`(335) / `myField_NewsArt1stChildArt`(336)
plus `prev/next` (331/332) link articles when fetching data.

### 7.2 Dates and GUIDs

**[SOURCE]** Article dates are `SDateTimeStamp` = `{year Uint16, msecs Uint16, seconds Uint32}`
(`UDateTime.h:80-84`), i.e. **seconds since midnight 1/1 of `year`**, not an epoch
(`HotlineServTrans.cpp:2403-2410` converts each field with `TB` under `#if CONVERT_INTS`).
Category GUIDs are `SGUID` (16 bytes, `UGUID.h:30-46`); flattened big-endian for the three
multi-byte fields (`UGUID(W).cpp:63-69`), matching standard UUID byte order on the wire.

---

## 8. Tracker protocol

The tracker does **not** use the TRTP transaction layer; it uses raw UDP/TCP with
`UTransport`. Ports (`TrackerServ.cpp:147-162`): UDP 5499 (registration), TCP 5498 (server
list), TCP 5497 (HTTP-tunnel list).

### 8.1 Registration (server → tracker, UDP 5499)

**[SOURCE]** `TrackerServ.cpp:585-643` (`ProcessRegister`):

```
Uint16 type = 1     // "add" (2 = "remove", ignored)
Uint16 port         // server's Hotline port
Uint16 userCount
Uint16 flags        // 1 = don't show in list (name only)
Uint32 passID       // password ID
pstring name
pstring desc
pstring password
```

All big-endian (parsed with `CUnflatten`).

### 8.2 Client handshake (TCP)

**[SOURCE]** `HotlineTasks.cpp:5939-5959`: client sends `"HTRK"` + `Uint16 version` (1 or 2)
+ (version 2 only) 32-byte padded login p-string + 32-byte padded password p-string. The
tracker echoes 6 bytes `"HTRK"` + version (`HotlineTasks.cpp:6057-6065`).

### 8.3 Server list (tracker → client, TCP)

**[SOURCE]** `TrackerServ.cpp:1055-1135` (`SendServerList`):

```
Uint16 type = 1
Uint16 size        // bytes following (totalCount + count + entries)
Uint16 totalCount  // total servers
Uint16 count       // servers in this message
entry[count] = { Uint32 address; Uint16 port; Uint16 userCount; Uint16 flags;
                 pstring name; pstring desc; }
```

`address` is the 4 raw IP octets (written without `TB` — already network order); port/userCount/
flags are big-endian. The client parses this in `HotlineTracker.cpp:995-1039`
(`AddListFromData`) and `HotlineTasks.cpp:6088-6115`.

### 8.4 Lookup

**[SOURCE]** `TrackerServ.cpp:1137-1170`: `{ Uint16 type=4 (found); Uint16 size; entry }` or
`{ Uint16 type=5 (not found); Uint16 size=0 }`. Category codes are the tree's category/tracker
IDs in the client's tracker list (the tracker itself is category-agnostic; the client groups
by tracker ID).

---

## 9. Endianness audit

Mechanism: `TB()`/`FB()` in `AppWarrior/Headers/typedefs.h:173-208` (identity on big-endian
PPC/68k, byte-swap on Intel; `CONVERT_INTS=1` on Intel). `swap_int` is defined at
typedefs.h:153-171. Approximately 2084 `TB(`/`FB(` call sites exist. The explicit `swap_int`
call sites (outside the macro) and their format class:

| File:line | Applies to | Format class |
|---|---|---|
| `typedefs.h:153-171` | `swap_int` definition | n/a |
| `Apps/Server/Source/HotlineServ.cpp:4003-4334` | ban-list IP ranges/counts (`BanRecord::Match`, perm-ban file load) | **file** (network-order IPs in the ban data file) |
| `Apps/Tracker/New Tracker/Sources/TrackerServ.cpp:1756,1816,1872,1990,2051,2124` | tracker login count / perm-ban counts | **file** |
| `AppWarrior/Source/Images/CDecompressPict.cpp:1953,1967` | PICT image `LongField`/`Rect` decoding | **resource** (Mac PICT) |
| `HotlineServ.cpp:3947-3949` (ServerOLD only) | (same ban-file pattern) | **file** |

Everything else goes through `TB`/`FB`. The **network** formats and their byte order:

- TRTP `STranHdr` (type/id/error/totalSize/dataSize): big-endian — `UTransact.cpp:915-919, 1009-1016`.
- TRTP/HTXF establish `protocol/refNum/version`: big-endian — `UTransact.cpp:264-270`, `HotlineTasks.cpp:2658-2662`.
- UFieldData count/id/size: big-endian — `UFieldData.cpp` throughout (`TB`/`FB`).
- Integer field values (2/4-byte): big-endian — `UFieldData.cpp:516-528, 554-600`.
- `SMyFileInfo`, `SMyUserInfo`, `SMyUserAccess`, `SMyUserDataFile`, `SDateTimeStamp`,
  `SGUID`, FILP/flat file, harc archive, tracker messages, news article list: all big-endian
  (quoted per-section above).

**Caveat (potential portability bug)**: `SMyUserAccess` (`data[2]` of `Uint32`) is copied with
raw `UMemory::Copy` into `myField_UserAccess` and compared with native `Uint32` bit ops
(`HotlineServTrans.cpp:1719, 1961`; `HotlineClientServerCommon.h:270-302`). If both peers are
little-endian the 8 bytes are little-endian on the wire; this is **not** normalized via `TB` and
would break mixed-endian peers. **[SOURCE]** the code does `data->AddField(myField_UserAccess,
inClient->access.data, sizeof(inClient->access.data))` with no byte-swap. **[UNKNOWN]** whether
any big-endian peer existed in practice.

Also note `_FSCreateFlatFileInfo` writes `platform/typeSig/creatorSig` without `TB` on the Mac
path (`UFileSys(M).cpp:1967-1970`), relying on the Mac being big-endian; the Windows path uses
`TB` (`UFileSys(W).cpp:2212-2214`).

---

## 10. Malformed-input hazards

A security-focused list of places where network-supplied lengths/sizes are used with fixed
buffers or arithmetic. (Read-only observation; nothing was modified.)

1. **1-byte over-read + over-write in path caching** — `ProcessTran_GetFileNameList`
   (`HotlineServTrans.cpp:985-990`) and `GetNewsCatNameList`/`GetNewsArtNameList`:
   `pathSize` can be up to `sizeof(path)` = 2048, then
   `UMemory::Copy(inClient->lastPath+1, path+1, pathSize)` copies 2048 bytes from `path+1`
   (i.e. `path[1..2048]`, one past the 2048-byte `path` array) into `lastPath+1` (a 2047-byte
   tail of `lastPath[2048]`, `HotlineServ.h:660`). 1-byte stack over-read + 1-byte overflow.
2. **`SendErrorMsg` (format-string overload) drops the error body** — `HotlineServTrans.cpp:8-25`
   calls `SetSendError(1)` but never `SendData(data)`, unlike the other two overloads. Replies
   on that path are sent empty (or not at all); error text is lost. Behavioral, not a memory
   bug, but affects many dynamic error messages.
3. **`myField_FileResumeData` size** — `ProcessTran_DownloadFile` copies at most 512 bytes
   (`resumeData[512]`) but `ResumeFlatten` only requires `>= sizeof(SFlatFileResumeData)` (42);
   a 42–512 byte buffer whose `count` claims more `forkInfo` entries than present is iterated
   without a length check (`UFileSys(W).cpp:2310-2332`) — a short buffer could read past the
   end. **[SOURCE]** no bound check against `inDataSize` beyond the header size.
4. **TRTP receive caps** are enforced (`dataSize`/`totalSize` ≤ 2 MB, non-zero; connection
   killed otherwise — `UTransact.cpp:1019-1026`), and allocation is `try`-wrapped
   (1029-1042). Good. However `rcvHeader.id` matching trusts `dataSize` for reassembly
   `UMemory::Append`; cumulative size is bounded only by `totalSize` ≤ 2 MB.
5. **`UFieldData::_FDBuildTables`** bounds-checks each field header and payload
   (`if (p+4 > ep) break; if (p+4+s > ep) break`, `UFieldData.cpp:799-801`) and the field
   count is capped at `dataSize`, so it is robust. `_FDManualGetFieldOffset(ByIndex)` also
   bounds-checks.
6. **Client archive decoder** (`HotlineArchiveDecoder.cpp`) trusts `head->fileCount`,
   `pathSize`, `rsvdSize`, `compressedSize` from the archive; `compressedSize` is only
   bounded by `min(..., inDataSize)` per call and `decompressedSize` is not checked against a
   cap — a malicious 'harc' with a huge `decompressedSize`/`compressedSize` could drive
   unbounded decompression/disk writes (the decoder itself has "should do some checks on
   compressedSize and all" as a TODO at line 254).
7. **Folder-download `mHeaderSize`** — client reads a `Uint16` size with no `>= 2` check before
   `ValidateFilePath(p, mHeaderSize - 2)`; if the server sent 0 or 1, `mHeaderSize-2` wraps,
   but `ValidateFilePath` fails with `error_OutOfRange` for sizes > 2048 (`UFileSys.cpp:118-124`),
   so it throws rather than overruns. Mild.
8. **Tracker client `AddListFromData`** — p-string walks are guarded (`if (p >= ep) break` /
   `if (p > ep) break`, `HotlineTracker.cpp:1022-1029`), and a minimum 12-byte window is
   required before reading the fixed fields. Reasonably robust.
9. **Login buffers** are bounded (`psUserLogin[33]`, `GetPString(..., 33)` etc.);
   `GetUser`/`SetUser`/`NewUser` copy `loginSize` (≤32) and `nameSize` (≤64) with explicit
   clamping. `SMyUserDataFile` is read with an exact `sizeof` check (`HotlineServ.cpp:3199`).
10. **Chat/board/private-message sizes** are clamped server-side (chat 8192, post 8192,
    `SendInstantMsg` 2048 fixed buffer via `GetField(..., sizeof(msg))`).

---

## 11. Protocol test vectors and behaviors needing tests

### 11.1 Golden byte-level examples derivable from source

- **TRTP establish (client→server)** — 12 bytes, big-endian:
  `54 52 54 50  48 4F 54 4C  00 01  00 02`  (`'TRTP'`, `'HOTL'`, version 1, subVersion 2).
- **TRTP establish accept (server→client)** — 8 bytes: `54 52 54 50 00 00 00 00`.
- **Transaction header for `KeepConnectionAlive`** — 20-byte header:
  `00 00 01 F4 00 00 00 01 00 00 00 00 00 00 00 00 00 00 00 00`
  (flag=0, isReply=0, type=500, id=1, error=0, totalSize=0, dataSize=0). *(Corrected during
  Phase 5 implementation: this tree's own keepalive carries a 2-byte empty field list body
  (`00 00` — UFieldData::GetDataHandle creates the zero count), because the receive policy
  kills any transaction with dataSize == 0. The header above is a valid header shape but does
  not describe the tree's keepalive transaction.)*
- **Single-field body `myField_ErrorText`="x"** — `00 01  00 64  00 01  78`
  (count=1, id=100, size=1, 'x').
- **`myField_Vers`=197 as an integer field** — 2 bytes: `00 C5` (fits Int16 → 2-byte encoding).
- **HMAC-MD5("", "")** (the "key"/"text" empty) — derivable from `HLMD5.cpp`/`UDigest.cpp`;
  `MD5("")` = `d4 1d 8c d9 8f 00 b2 04 e9 80 09 98 ec f8 42 7e` (standard RFC 1321 vector,
  cross-checkable with `_MD5::Report`).
- **Blowfish self-test vectors** — the Eric Young `variable_key`/`set_key`/`plaintext_l/r`/
  `ciphertext_l/r` tables in `HLBlowfishData.h` (executed by `HLBlowfish::SelfTest`).
- **`SFlatFileResumeData`** for a 100-byte data fork resume: `52 46 4C 54 00 01` + 34 zero
  bytes + `00 01` (count=1) + `44 41 54 41 00 00 00 64 00 00 00 00 00 00 00 00`.
- **FILP header** (Windows): `46 49 4C 50 00 01` + 16 zero bytes + `00 02` (forkCount=2).
- **Folder-download item** for a single top-level file "a.txt": `[size] 00 00 [type] [pathCount]
  00 01  00 00 05 "a.txt"` where pathCount=1, script=0, namelen=5.

### 11.2 Behaviors that require behavioral tests before rewriting

- `SendErrorMsg` overload #1 (format-string) sends no data — confirm and fix the reply semantics.
- Crypto (HOPE) login: the server never emits `myField_SessionKey`; decide whether the rewrite
  restores the encrypted path, and capture a known session-key/HMAC/Blowfish trace from the
  client code (which is complete) as the golden vector.
- `SMyUserAccess` endianness normalization (currently raw copy).
- Login/password scrambling (bitwise-NOT) must round-trip exactly (server unscrambles login but
  stores password still scrambled).
- Multi-part transaction reassembly (`totalSize` vs `dataSize`) and the `id`-matched reply.
- `flag`-based key permutation (`rand` in {2,7,13}) behavior across the encrypted data path.
- Folder-download resume (`dlFldrAction_ResumeFile`) round-trip and the `.hpf` suffix handling.
- News article-list `count`/`flavorCount` consistency with the cached-list path
  (`GetArticleListing` reads a cached list when `head->listOffset` is nonzero).
- Tracker multi-message server-list splitting (the 8192-byte buffer boundary) and the
  `totalCount`/`count` semantics.

---

## Protocol constants that must be preserved verbatim

### Transaction IDs (numeric table)

```
100 Error       101 GetMsgs    102 NewMsg    103 PostMsg   104 ServerMsg
105 ChatSend    106 ChatMsg    107 Login     108 SendInstantMsg
109 ShowAgreement 110 DisconnectUser 111 DisconnectMsg 112 InviteNewChat
113 InviteToChat 114 RejectChatInvite 115 JoinChat 116 LeaveChat
117 NotifyChatChangeUser 118 NotifyChatDeleteUser 119 NotifyChatSubject
120 SetChatSubject 121 Agreed 122 ServerBanner 123 IconChange 124 NickChange
125 FakeRed 126 Away 127 CrazyServer 128 BlockDownload 129 Visible
130 AdminSpector 131 StandardMessage
200 GetFileNameList 202 DownloadFile 203 UploadFile 204 DeleteFile
205 NewFolder 206 GetFileInfo 207 SetFileInfo 208 MoveFile 209 MakeFileAlias
210 DownloadFldr 211 DownloadInfo 212 DownloadBanner 213 UploadFldr 214 KillDownload
300 GetUserNameList 301 NotifyChangeUser 302 NotifyDeleteUser 303 GetClientInfoText
304 SetClientUserInfo
348 GetUserList 349 SetUserList 350 NewUser 351 DeleteUser 352 GetUser 353 SetUser
354 UserAccess 355 UserBroadcast
370 GetNewsCatNameList 371 GetNewsArtNameList 380 DelNewsItem 381 NewNewsFldr
382 NewNewsCat 400 GetNewsArtData 410 PostNewsArt 411 DelNewsArt
500 KeepConnectionAlive
```

### Field IDs (numeric table)

```
100 ErrorText 101 Data 102 UserName 103 UserID 104 UserIconID 105 UserLogin
106 UserPassword 107 RefNum 108 TransferSize 109 ChatOptions 110 UserAccess
111 UserAlias 112 UserFlags 113 Options 114 ChatID (= number) 115 ChatSubject
116 WaitingCount 117 IconId 118 NickName 119 FakeRed 120 Away 121 BlockDownload
122 AdminSpector 123 StandardMessage 150 ServerAgreement 151 ServerBanner
152 ServerBannerType 153 ServerBannerUrl 154 NoServerAgreement 160 Vers
161 CommunityBannerID 162 ServerName
200 FileNameWithInfo 201 FileName 202 FilePath 203 FileResumeData 204 FileXferOptions
205 FileTypeString 206 FileCreatorString 207 FileSize 208 FileCreateDate
209 FileModifyDate 210 FileComment 211 FileNewName 212 FileNewPath 213 FileType
214 QuotingMsg 215 AutomaticResponse 220 FldrItemCount
300 UserNameWithInfo
319 NewsCatGUID 320 NewsCatListData 321 NewsArtListData 322 NewsCatName
323 NewsCatListData15 325 NewsPath 326 NewsArtID 327 NewsArtDataFlav
328 NewsArtTitle 329 NewsArtPoster 330 NewsArtDate 331 NewsArtPrevArt
332 NewsArtNextArt 333 NewsArtData 334 NewsArtFlags 335 NewsArtParentArt
336 NewsArt1stChildArt 337 NewsArtRecurseDel
3587 SessionKey (0x0E03)  3588 MacAlg (0x0E04)
3777 S_CipherAlg (0x0EC1) 3778 C_CipherAlg (0x0EC2)
```

Note the duplicate enum value `117` (both `myField_IconId` and `myField_Visible`) and `114`
(both `myField_ChatID` and `myField_number`) in `HotlineClientServerCommon.h:116-121` — a
pre-existing collision that must be preserved (or consciously resolved) because it is
observable on the wire.

### Privilege bit indices (`myAcc_*`)

Preserve the 0–54 enumeration in `HotlineClientServerCommon.h:174-233` verbatim (bit positions
inside the two `Uint32` access words define account capability and are persisted in account
files).

---

## Serialization decisions for the modern codec (proposed explicit rules)

1. **Integers on the wire are big-endian, always.** Use the `TB`/`FB` convention; never write
   host-order multi-byte values. Normalize `SMyUserAccess` (8-byte privilege blob) to two
   big-endian `Uint32` on send and back on receive.
2. **Field container**: `Uint16 count` (BE) then `count × { Uint16 id (BE), Uint16 size (BE),
   bytes }`, no padding, max field size 65535. Integer *values* encode as 2 bytes when they fit
   a signed 16-bit range, else 4 bytes; the decoder must accept 1/2/4-byte forms.
3. **Framing**: if interoperability with this legacy tree is required, preserve the 20-byte
   TRTP `STranHdr` (`flag,isReply,type,id,error,totalSize,dataSize`) and the `'TRTP'`/`'HOTL'`
   establish; document explicitly that this is *not* the classic 18-byte Hotline header.
4. **Strings**: p-strings (1 length byte) for names/titles; raw length-prefixed
   (`nameSize` in a struct) for `SMyFileInfo`/`SMyUserInfo` names; `\r` as the text newline;
   script code 0 = MacRoman/raw. No escaping is applied to message bodies.
5. **File payloads**: keep the `FILP` flat-file layout (header 24B, fork headers 16B each,
   INFO then DATA forks) and the `RFLT` resume record; keep `'harc'` (zlib-wrapped FILP files)
   as a distinct container.
6. **Dates** are `{year, msecs, seconds}` triples (seconds since Jan 1 of `year`), not epochs;
   preserve this rather than silently converting to epoch.
7. **Flags/options** (`myField_Options`, `myField_FileXferOptions`, `myField_ChatOptions`,
   `myField_NewsArtFlags`) keep their current integer semantics (§3, §6.1).

## Required golden tests

1. TRTP establish + accept byte vectors (§11.1).
2. Header encode/decode round-trip for a representative of each transaction ID class (empty,
   single-part, multi-part), including the 2 MB cap and the `dataSize==0` rejection.
3. Field-codec round-trip: variable-width integers (0, 1, 0x7FFF, 0x8000, ±1, 2^31-1), p-strings,
   empty fields, field ordering, duplicate-ID lookup, and the 65535-byte field limit.
4. `SMyFileInfo` and `SMyUserInfo` byte layouts (golden hex for a known name/type/creator/size).
5. `SMyUserAccess` bit semantics and byte order for all 55 privileges.
6. News article-list golden encoding (header + one article with one flavor) and category-list
   v1.5 vs legacy encodings.
7. `SDateTimeStamp` and `SGUID` byte order.
8. FILP flat-file and `RFLT` resume-data golden bytes; folder-download item header and the
   `dlFldrAction_*` state machine.
9. `harc` archive golden header + one zlib file.
10. Crypto: `HLMD5::HMAC_XXX`/`HLSha1::HMAC_XXX` against RFC 4231-style vectors; `HLBlowfish`
    self-test vectors; `HLCrypt::Init` key derivation for a fixed `(password, sessionKey)` pair
    (client and server directions); `HLRand` determinism given a fixed seed file.
11. Login round-trip: inverted-login/inverted-password scrambling, and the HOPE two-stage
    exchange (client side) with a captured session key.
12. Tracker: UDP registration message and TCP server-list message (single and split) golden bytes.

> [SOURCE] = quoted/derived from files in this tree; [EXTERNAL] = historical Hotline knowledge
> used for cross-check; [UNKNOWN] = not settled by the source. Key source files:
> `AppWarrior/Source/Hardware/UTransact.cpp`, `AppWarrior/Source/Data/UFieldData.cpp`,
> `AppWarrior/Headers/CFlatten.h`, `AppWarrior/Headers/typedefs.h`,
> `Apps/Common Files/HotlineClientServerCommon.h`, `Apps/Server/Source/HotlineServ.cpp`,
> `Apps/Server/Source/HotlineServTrans.cpp`, `Apps/Server/Source/HotlineServNewsDatabase.cpp`,
> `Apps/Client/Source/HotlineTasks.cpp`, `Apps/Client/Source/HotlineTracker.cpp`,
> `Apps/Tracker/New Tracker/Sources/TrackerServ.cpp`,
> `Apps/Common Files/HotlineArchiveDecoder.cpp`, `Apps/Common Files/HotlineArchiveStruct.h`,
> `AppWarrior/Source/Crypt/*`, `AppWarrior/Source/Files/UFileSys(W).cpp`.
