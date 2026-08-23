#include "hotline/protocol/constants.h"

#include "appwarrior/testing.h"

using namespace hotline::protocol;
using namespace aw::test;

AW_TEST_CASE("transaction IDs match the historical table") {
  AW_CHECK(static_cast<std::uint16_t>(TransactionType::Error) == 100);
  AW_CHECK(static_cast<std::uint16_t>(TransactionType::Login) == 107);
  AW_CHECK(static_cast<std::uint16_t>(TransactionType::Agreed) == 121);
  AW_CHECK(static_cast<std::uint16_t>(TransactionType::AdminSpector) == 130);
  AW_CHECK(static_cast<std::uint16_t>(TransactionType::GetFileNameList) == 200);
  AW_CHECK(static_cast<std::uint16_t>(TransactionType::DownloadFolder) == 210);
  AW_CHECK(static_cast<std::uint16_t>(TransactionType::GetUserList) == 348);
  AW_CHECK(static_cast<std::uint16_t>(TransactionType::UserBroadcast) == 355);
  AW_CHECK(static_cast<std::uint16_t>(TransactionType::GetNewsCatNameList) == 370);
  AW_CHECK(static_cast<std::uint16_t>(TransactionType::GetNewsArtData) == 400);
  AW_CHECK(static_cast<std::uint16_t>(TransactionType::KeepConnectionAlive) == 500);
}

AW_TEST_CASE("field IDs match the historical table, including the two collisions") {
  AW_CHECK(static_cast<std::uint16_t>(FieldId::ErrorText) == 100);
  AW_CHECK(static_cast<std::uint16_t>(FieldId::Data) == 101);
  AW_CHECK(static_cast<std::uint16_t>(FieldId::Vers) == 160);
  AW_CHECK(static_cast<std::uint16_t>(FieldId::FileNameWithInfo) == 200);
  AW_CHECK(static_cast<std::uint16_t>(FieldId::UserNameWithInfo) == 300);
  AW_CHECK(static_cast<std::uint16_t>(FieldId::SessionKey) == 3587);
  AW_CHECK(static_cast<std::uint16_t>(FieldId::MacAlg) == 3588);
  // The historical header annotates these as "// 3771" and "// 3772" — a
  // typo: 0x0EC1 = 3777, 0x0EC2 = 3778. The hex literals are authoritative
  // (legacy code uses the hex constants), so the wire values are 3777/3778.
  AW_CHECK(static_cast<std::uint16_t>(FieldId::ServerCipherAlg) == 3777);
  AW_CHECK(static_cast<std::uint16_t>(FieldId::ClientCipherAlg) == 3778);

  // The two in-namespace collisions are preserved verbatim: existing
  // peers depend on them (HOTLINE_MODERNIZATION_REPORT.md risk #5).
  AW_CHECK(FieldId::UserFlags == FieldId::Visible);
  AW_CHECK(static_cast<std::uint16_t>(FieldId::UserFlags) == 112);
  AW_CHECK(static_cast<std::uint16_t>(FieldId::Visible) == 112);
  AW_CHECK(FieldId::ChatId == FieldId::Number);
  AW_CHECK(static_cast<std::uint16_t>(FieldId::ChatId) == 114);
  AW_CHECK(static_cast<std::uint16_t>(FieldId::Number) == 114);

  // 117 is NOT a field-ID collision: IconId is the only field 117
  // (transaction 117 is NotifyChatChangeUser — a different namespace).
  AW_CHECK(static_cast<std::uint16_t>(FieldId::IconId) == 117);
}

AW_TEST_CASE("access privilege bit positions match the historical table") {
  AW_CHECK(static_cast<std::uint8_t>(AccessPrivilege::DeleteFile) == 0);
  AW_CHECK(static_cast<std::uint8_t>(AccessPrivilege::UploadFile) == 1);
  AW_CHECK(static_cast<std::uint8_t>(AccessPrivilege::DownloadFile) == 2);
  AW_CHECK(static_cast<std::uint8_t>(AccessPrivilege::SendMessage) == 40);
  AW_CHECK(static_cast<std::uint8_t>(AccessPrivilege::CanFlood) == 50);
  AW_CHECK(static_cast<std::uint8_t>(AccessPrivilege::AdmInSpector) == 53);
  AW_CHECK(static_cast<std::uint8_t>(AccessPrivilege::PostBefore) == 54);
  AW_CHECK(kAccessPrivilegeCount == 55U);
}

AW_TEST_CASE("protocol tags, versions and wire sizes") {
  AW_CHECK(aw::endian::four_cc('T', 'R', 'T', 'P') == 0x54525450U);
  AW_CHECK(kProtocolTrTp == 0x54525450U);
  AW_CHECK(kProtocolNick == 0x4E49434BU);
  AW_CHECK(kSubProtocolHotl == 0x484F544CU);
  AW_CHECK(kSubProtocolHtxf == 0x48545846U);
  AW_CHECK(kProtocolVersion == 1);
  AW_CHECK(kClientSubVersion == 2);
  AW_CHECK(kTransferSubVersion == 3);
  AW_CHECK(kTransactionHeaderSize == 20U);
  AW_CHECK(kClientHandshakeSize == 12U);
  AW_CHECK(kServerHandshakeReplySize == 8U);
  AW_CHECK(kMaxFieldDataSize == 65535U);
  AW_CHECK(kMaxFieldCount == 65535U);
  AW_CHECK(kFrameworkMaxTransactionReceiveSize == 2097152U);
  AW_CHECK(kServerMaxTransactionReceiveSize == 524288U);
}
