#include "hotline/protocol/constants.h"

#include "support/test_support.h"

using namespace hotline::protocol;
using namespace hotline_test;

TEST_CASE("transaction IDs match the historical table") {
  CHECK(static_cast<std::uint16_t>(TransactionType::Error) == 100);
  CHECK(static_cast<std::uint16_t>(TransactionType::Login) == 107);
  CHECK(static_cast<std::uint16_t>(TransactionType::Agreed) == 121);
  CHECK(static_cast<std::uint16_t>(TransactionType::AdminSpector) == 130);
  CHECK(static_cast<std::uint16_t>(TransactionType::GetFileNameList) == 200);
  CHECK(static_cast<std::uint16_t>(TransactionType::DownloadFolder) == 210);
  CHECK(static_cast<std::uint16_t>(TransactionType::GetUserList) == 348);
  CHECK(static_cast<std::uint16_t>(TransactionType::UserBroadcast) == 355);
  CHECK(static_cast<std::uint16_t>(TransactionType::GetNewsCatNameList) == 370);
  CHECK(static_cast<std::uint16_t>(TransactionType::GetNewsArtData) == 400);
  CHECK(static_cast<std::uint16_t>(TransactionType::KeepConnectionAlive) == 500);
}

TEST_CASE("field IDs match the historical table, including the two collisions") {
  CHECK(static_cast<std::uint16_t>(FieldId::ErrorText) == 100);
  CHECK(static_cast<std::uint16_t>(FieldId::Data) == 101);
  CHECK(static_cast<std::uint16_t>(FieldId::Vers) == 160);
  CHECK(static_cast<std::uint16_t>(FieldId::FileNameWithInfo) == 200);
  CHECK(static_cast<std::uint16_t>(FieldId::UserNameWithInfo) == 300);
  CHECK(static_cast<std::uint16_t>(FieldId::SessionKey) == 3587);
  CHECK(static_cast<std::uint16_t>(FieldId::MacAlg) == 3588);
  // The historical header annotates these as "// 3771" and "// 3772" — a
  // typo: 0x0EC1 = 3777, 0x0EC2 = 3778. The hex literals are authoritative
  // (legacy code uses the hex constants), so the wire values are 3777/3778.
  CHECK(static_cast<std::uint16_t>(FieldId::ServerCipherAlg) == 3777);
  CHECK(static_cast<std::uint16_t>(FieldId::ClientCipherAlg) == 3778);

  // The two in-namespace collisions are preserved verbatim: existing
  // peers depend on them (HOTLINE_MODERNIZATION_REPORT.md risk #5).
  CHECK(FieldId::UserFlags == FieldId::Visible);
  CHECK(static_cast<std::uint16_t>(FieldId::UserFlags) == 112);
  CHECK(static_cast<std::uint16_t>(FieldId::Visible) == 112);
  CHECK(FieldId::ChatId == FieldId::Number);
  CHECK(static_cast<std::uint16_t>(FieldId::ChatId) == 114);
  CHECK(static_cast<std::uint16_t>(FieldId::Number) == 114);

  // 117 is NOT a field-ID collision: IconId is the only field 117
  // (transaction 117 is NotifyChatChangeUser — a different namespace).
  CHECK(static_cast<std::uint16_t>(FieldId::IconId) == 117);
}

TEST_CASE("access privilege bit positions match the historical table") {
  CHECK(static_cast<std::uint8_t>(AccessPrivilege::DeleteFile) == 0);
  CHECK(static_cast<std::uint8_t>(AccessPrivilege::UploadFile) == 1);
  CHECK(static_cast<std::uint8_t>(AccessPrivilege::DownloadFile) == 2);
  CHECK(static_cast<std::uint8_t>(AccessPrivilege::SendMessage) == 40);
  CHECK(static_cast<std::uint8_t>(AccessPrivilege::CanFlood) == 50);
  CHECK(static_cast<std::uint8_t>(AccessPrivilege::AdmInSpector) == 53);
  CHECK(static_cast<std::uint8_t>(AccessPrivilege::PostBefore) == 54);
  CHECK(kAccessPrivilegeCount == 55U);
}

TEST_CASE("protocol tags, versions and wire sizes") {
  CHECK(four_cc('T', 'R', 'T', 'P') == 0x54525450U);
  CHECK(kProtocolTrTp == 0x54525450U);
  CHECK(kProtocolNick == 0x4E49434BU);
  CHECK(kSubProtocolHotl == 0x484F544CU);
  CHECK(kSubProtocolHtxf == 0x48545846U);
  CHECK(kProtocolVersion == 1);
  CHECK(kClientSubVersion == 2);
  CHECK(kTransferSubVersion == 3);
  CHECK(kTransactionHeaderSize == 20U);
  CHECK(kClientHandshakeSize == 12U);
  CHECK(kServerHandshakeReplySize == 8U);
  CHECK(kMaxFieldDataSize == 65535U);
  CHECK(kMaxFieldCount == 65535U);
  CHECK(kFrameworkMaxTransactionReceiveSize == 2097152U);
  CHECK(kServerMaxTransactionReceiveSize == 524288U);
}
