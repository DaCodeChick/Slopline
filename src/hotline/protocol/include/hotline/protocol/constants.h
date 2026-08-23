// Hotline wire protocol: constant tables.
//
// Transaction IDs, field IDs and access privileges are the wire values
// used by every Hotline peer. They are reproduced VERBATIM from the
// historical header "Apps/Common Files/HotlineClientServerCommon.h"
// ((c)2003 Hotsprings Inc., GPL — same project lineage, see legacy/).
//
// Interoperability invariants (do not "fix" without an explicit
// compatibility decision):
//  * Field ID 112 is used by BOTH UserFlags and Visible.
//  * Field ID 114 is used by BOTH ChatId and Number.
//    Existing peers depend on these collisions; they are preserved verbatim
//    and must not be silently deduplicated.
//  * Field IDs are not contiguous (324 is absent, 117 is IconId while
//    transaction 117 is NotifyChatChangeUser — the ID namespaces are
//    independent; a historical audit once misread this cross-namespace
//    coincidence as a third field-ID collision).

#pragma once

#include <cstddef>
#include <cstdint>

#include "appwarrior/core/endian.h"

namespace hotline::protocol {

// ---------------------------------------------------------------------------
// Protocol / connection tags
// ---------------------------------------------------------------------------

inline constexpr std::uint32_t kProtocolTrTp = aw::endian::four_cc('T', 'R', 'T', 'P');
inline constexpr std::uint32_t kProtocolNick = aw::endian::four_cc('N', 'I', 'C', 'K');  // legacy alternate tag
inline constexpr std::uint32_t kSubProtocolHotl = aw::endian::four_cc('H', 'O', 'T', 'L');
inline constexpr std::uint32_t kSubProtocolHtxf = aw::endian::four_cc('H', 'T', 'X', 'F');  // transfer channel

inline constexpr std::uint16_t kProtocolVersion = 1;
inline constexpr std::uint16_t kClientSubVersion = 2;      // main connection
inline constexpr std::uint16_t kTransferSubVersion = 3;    // HTXF transfer connections

// Historical receive-side caps (connection policy, not part of the codec):
inline constexpr std::uint32_t kFrameworkMaxTransactionReceiveSize = 2097152;  // UTransact.cpp: _gTransactMaxReceiveSize
inline constexpr std::uint32_t kServerMaxTransactionReceiveSize = 524288;      // HotlineServ.cpp: server override

// ---------------------------------------------------------------------------
// Transaction IDs (wire type: 16-bit big-endian header field)
// ---------------------------------------------------------------------------

enum class TransactionType : std::uint16_t {
  Error                = 100,
  GetMessages          = 101,
  NewMessage           = 102,
  PostMessage          = 103,
  ServerMessage        = 104,
  ChatSend             = 105,
  ChatMessage          = 106,
  Login                = 107,
  SendInstantMessage   = 108,
  ShowAgreement        = 109,
  DisconnectUser       = 110,
  DisconnectMessage    = 111,
  InviteNewChat        = 112,
  InviteToChat         = 113,
  RejectChatInvite     = 114,
  JoinChat             = 115,
  LeaveChat            = 116,
  NotifyChatChangeUser = 117,
  NotifyChatDeleteUser = 118,
  NotifyChatSubject    = 119,
  SetChatSubject       = 120,
  Agreed               = 121,
  ServerBanner         = 122,
  IconChange           = 123,
  NickChange           = 124,
  FakeRed              = 125,
  Away                 = 126,
  CrazyServer          = 127,
  BlockDownload        = 128,
  Visible              = 129,
  AdminSpector         = 130,  // historical spelling ("Spector") preserved
  StandardMessage      = 131,

  GetFileNameList      = 200,
  // 201 is absent from the historical table.
  DownloadFile         = 202,
  UploadFile           = 203,
  DeleteFile           = 204,
  NewFolder            = 205,
  GetFileInfo          = 206,
  SetFileInfo          = 207,
  MoveFile             = 208,
  MakeFileAlias        = 209,
  DownloadFolder       = 210,
  DownloadInfo         = 211,
  DownloadBanner       = 212,
  UploadFolder         = 213,
  KillDownload         = 214,

  GetUserNameList      = 300,
  NotifyChangeUser     = 301,
  NotifyDeleteUser     = 302,
  GetClientInfoText    = 303,
  SetClientUserInfo    = 304,

  GetUserList          = 348,
  SetUserList          = 349,
  NewUser              = 350,
  DeleteUser           = 351,
  GetUser              = 352,
  SetUser              = 353,
  UserAccess           = 354,
  UserBroadcast        = 355,

  GetNewsCatNameList   = 370,
  GetNewsArtNameList   = 371,
  DelNewsItem          = 380,
  NewNewsFolder        = 381,
  NewNewsCat           = 382,
  GetNewsArtData       = 400,
  PostNewsArt          = 410,
  DelNewsArt           = 411,

  KeepConnectionAlive  = 500,
};

// ---------------------------------------------------------------------------
// Field IDs (wire type: 16-bit big-endian per field entry)
// ---------------------------------------------------------------------------

enum class FieldId : std::uint16_t {
  // "HOPE" encrypted-login negotiation (client-only in the historical server).
  SessionKey        = 3587,  // 0x0E03
  MacAlg            = 3588,  // 0x0E04
  ServerCipherAlg   = 0x0EC1,  // 3777 (historical comment says "3771" — a typo)
  ClientCipherAlg   = 0x0EC2,  // 3778 (historical comment says "3772" — a typo)

  ErrorText         = 100,
  Data              = 101,
  UserName          = 102,
  UserId            = 103,
  UserIconId        = 104,
  UserLogin         = 105,
  UserPassword      = 106,
  RefNum            = 107,
  TransferSize      = 108,
  ChatOptions       = 109,
  UserAccess        = 110,
  UserAlias         = 111,
  UserFlags         = 112,
  Options           = 113,
  ChatId            = 114,
  ChatSubject       = 115,
  WaitingCount      = 116,
  IconId            = 117,
  NickName          = 118,
  FakeRed           = 119,
  Away              = 120,
  BlockDownload     = 121,
  // COLLISION: field ID 112 is also Visible (preserved verbatim).
  Visible           = 112,
  AdminSpector      = 122,  // historical spelling preserved
  StandardMessage   = 123,
  // COLLISION: field ID 114 is also Number (preserved verbatim).
  Number            = 114,
  ServerAgreement   = 150,
  ServerBanner      = 151,
  ServerBannerType  = 152,
  ServerBannerUrl   = 153,
  NoServerAgreement = 154,
  Vers              = 160,
  CommunityBannerId = 161,
  ServerName        = 162,

  FileNameWithInfo  = 200,
  FileName          = 201,
  FilePath          = 202,
  FileResumeData    = 203,
  FileXferOptions   = 204,
  FileTypeString    = 205,
  FileCreatorString = 206,
  FileSize          = 207,
  FileCreateDate    = 208,
  FileModifyDate    = 209,
  FileComment       = 210,
  FileNewName       = 211,
  FileNewPath       = 212,
  FileType          = 213,
  QuotingMsg        = 214,
  AutomaticResponse = 215,
  FldrItemCount     = 220,

  UserNameWithInfo  = 300,

  NewsCategoryGuid  = 319,
  NewsCatListData   = 320,  // pre-April-15 1.5 clients/servers
  NewsArtListData   = 321,
  NewsCatName       = 322,
  NewsCatListData15 = 323,
  // 324 is absent from the historical table.
  NewsPath          = 325,
  NewsArtId         = 326,
  NewsArtDataFlav   = 327,
  NewsArtTitle      = 328,
  NewsArtPoster     = 329,
  NewsArtDate       = 330,
  NewsArtPrevArt    = 331,
  NewsArtNextArt    = 332,
  NewsArtData       = 333,
  NewsArtFlags      = 334,
  NewsArtParentArt  = 335,
  NewsArt1stChildArt = 336,
  NewsArtRecurseDel = 337,
};

// ---------------------------------------------------------------------------
// Access privileges (bits 0..54 of the 64-bit SMyUserAccess mask)
// ---------------------------------------------------------------------------

enum class AccessPrivilege : std::uint8_t {
  DeleteFile        = 0,
  UploadFile        = 1,
  DownloadFile      = 2,
  RenameFile        = 3,
  MoveFile          = 4,
  CreateFolder      = 5,
  DeleteFolder      = 6,
  RenameFolder      = 7,
  MoveFolder        = 8,
  ReadChat          = 9,
  SendChat          = 10,
  CreateChat        = 11,
  CloseChat         = 12,
  ShowInList        = 13,
  CreateUser        = 14,
  DeleteUser        = 15,
  OpenUser          = 16,
  ModifyUser        = 17,
  ChangeOwnPass     = 18,
  SendPrivMsg       = 19,
  NewsReadArt       = 20,
  NewsPostArt       = 21,
  DisconUser        = 22,
  CannotBeDiscon    = 23,
  GetClientInfo     = 24,
  UploadAnywhere    = 25,
  AnyName           = 26,
  NoAgreement       = 27,
  SetFileComment    = 28,
  SetFolderComment  = 29,
  ViewDropBoxes     = 30,
  MakeAlias         = 31,
  Broadcast         = 32,
  NewsDeleteArt     = 33,
  NewsCreateCat     = 34,
  NewsDeleteCat     = 35,
  NewsCreateFldr    = 36,
  NewsDeleteFldr    = 37,
  UploadFolder      = 38,
  DownloadFolder    = 39,
  SendMessage       = 40,
  FakeRed           = 41,
  Away              = 42,
  ChangeNick        = 43,
  ChangeIcon        = 44,
  SpeakBefore       = 45,
  RefuseChat        = 46,
  BlockDownload     = 47,
  Visible           = 48,
  CanViewInvisible  = 49,
  CanFlood          = 50,
  ViewOwnDropBox    = 51,
  DontQueue         = 52,
  AdmInSpector      = 53,
  PostBefore        = 54,
};

inline constexpr std::size_t kAccessPrivilegeCount = 55;

// ---------------------------------------------------------------------------
// Auxiliary enums carried inside fields
// ---------------------------------------------------------------------------

enum class UserOption : std::uint16_t {
  UserMessage       = 1,
  RefuseMessage     = 2,
  RefuseChat        = 3,
  AutomaticResponse = 4,
};

enum class FolderDownloadAction : std::uint16_t {
  SendFile   = 1,
  ResumeFile = 2,
  NextFile   = 3,
  // NOTE: audit/06 §6.4 printed the SendFile/NextFile mapping swapped;
  // the verbatim enum and the server dispatch (it waits for
  // dlFldrAction_NextFile) confirm SendFile=1, ResumeFile=2, NextFile=3.
};

// ---------------------------------------------------------------------------
// Wire-size constants
// ---------------------------------------------------------------------------

inline constexpr std::size_t kTransactionHeaderSize = 20;      // STranHdr
inline constexpr std::size_t kClientHandshakeSize = 12;        // 'TRTP' 'HOTL' version subVersion
inline constexpr std::size_t kServerHandshakeReplySize = 8;    // 'TRTP' error
inline constexpr std::size_t kMaxFieldDataSize = 65535;        // UFieldData::AddField cap (max_Uint16)
inline constexpr std::size_t kMaxFieldCount = 65535;           // 16-bit count field

}  // namespace hotline::protocol
