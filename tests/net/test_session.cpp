#include "hotline/server/session.h"

#include <cstddef>
#include <cstdint>
#include <span>
#include <utility>
#include <vector>

#include "appwarrior/testing.h"
#include "hotline/protocol/auth.h"
#include "hotline/protocol/constants.h"
#include "hotline/protocol/field_list.h"

using namespace hotline::server;
using namespace hotline::net;
using namespace hotline::protocol;
using namespace aw::test;

namespace {

// Builds the wire body of a login transaction: scrambled login, scrambled
// password, Vers field.
auto make_login_body(std::string_view login, std::string_view password) -> std::vector<std::byte> {
  FieldList fields;
  fields.fields.push_back(
      Field{FieldId::UserLogin, auth::scramble(bytes_from_ascii(login))});
  fields.fields.push_back(
      Field{FieldId::UserPassword, auth::scramble(bytes_from_ascii(password))});
  fields.fields.push_back(make_integer_field(FieldId::Vers, 197));
  return *encode_field_list(fields);
}

auto make_transaction(TransactionType type, std::uint32_t id, std::vector<std::byte> data)
    -> ReceivedTransaction {
  ReceivedTransaction transaction;
  transaction.header.type = type;
  transaction.header.id = id;
  transaction.data = std::move(data);
  return transaction;
}

}  // namespace

AW_TEST_CASE("login succeeds with the legacy scrambled-password comparison") {
  SessionConfig config;
  config.server_name = "My Server";
  config.community_banner_id = 5;
  config.lookup_user = [](std::string_view login) -> std::expected<UserRecord, LoginError> {
    if (login == "alice") {
      UserRecord record;
      record.name = "Alice";
      // Stored form: password STILL SCRAMBLED (the legacy data file form).
      record.password = auth::scramble(bytes_from_ascii("secret"));
      record.access.set(AccessPrivilege::DownloadFile);
      return record;
    }
    return std::unexpected(LoginError::unknown_user);
  };

  Session session(std::move(config));
  const auto reply =
      session.process(make_transaction(TransactionType::Login, 3, make_login_body("Alice", "secret")));
  AW_CHECK(reply.has_value());
  AW_CHECK(reply->id == 3U);
  AW_CHECK(reply->error == 0U);
  AW_CHECK(session.state() == SessionState::awaiting_agreement);
  AW_CHECK(session.login() == "alice");
  AW_CHECK(session.access().has(AccessPrivilege::DownloadFile));

  AW_REQUIRE_BYTES(field_data(reply->fields, FieldId::Vers), "00 c5");
  AW_CHECK(find_field(reply->fields, FieldId::CommunityBannerId) != nullptr);
  AW_CHECK(find_field(reply->fields, FieldId::ServerName) != nullptr);
  AW_CHECK(decode_string_field(*find_field(reply->fields, FieldId::ServerName)) == "My Server");
}

AW_TEST_CASE("agreed moves the session to active") {
  SessionConfig config;
  config.lookup_user = [](std::string_view) -> std::expected<UserRecord, LoginError> {
    UserRecord record;
    record.password = auth::scramble(bytes_from_ascii("secret"));
    return record;
  };
  Session session(std::move(config));
  AW_CHECK(session.process(make_transaction(TransactionType::Login, 1,
                                            make_login_body("alice", "secret"))));
  AW_CHECK(session.state() == SessionState::awaiting_agreement);

  const auto agreed = session.process(make_transaction(TransactionType::Agreed, 2, {}));
  AW_CHECK(!agreed.has_value());
  AW_CHECK(session.state() == SessionState::active);
}

AW_TEST_CASE("wrong password and unknown user produce error replies") {
  SessionConfig config;
  config.lookup_user = [](std::string_view login) -> std::expected<UserRecord, LoginError> {
    if (login == "alice") {
      UserRecord record;
      record.password = auth::scramble(bytes_from_ascii("secret"));
      return record;
    }
    return std::unexpected(LoginError::unknown_user);
  };

  {
    Session session(config);
    const auto reply = session.process(
        make_transaction(TransactionType::Login, 4, make_login_body("alice", "wrong!")));
    AW_CHECK(reply.has_value());
    AW_CHECK(reply->error == 1U);
    AW_CHECK(find_field(reply->fields, FieldId::ErrorText) != nullptr);
    AW_CHECK(session.state() == SessionState::closed);
  }
  {
    Session session(config);
    const auto reply = session.process(
        make_transaction(TransactionType::Login, 5, make_login_body("mallory", "whatever")));
    AW_CHECK(reply.has_value());
    AW_CHECK(reply->error == 1U);
    AW_CHECK(session.state() == SessionState::closed);
  }
}

AW_TEST_CASE("malformed login fields produce an error reply") {
  SessionConfig config;
  Session session(std::move(config));

  ReceivedTransaction transaction;
  transaction.header.type = TransactionType::Login;
  transaction.header.id = 6;
  transaction.data = bytes_from_hex("00 00");  // valid field list, no login fields
  const auto reply = session.process(transaction);
  AW_CHECK(reply.has_value());
  AW_CHECK(reply->error == 1U);
  AW_CHECK(session.state() == SessionState::closed);
}

AW_TEST_CASE("non-login transactions are ignored") {
  SessionConfig config;
  Session session(std::move(config));
  const auto reply =
      session.process(make_transaction(TransactionType::ChatSend, 7, bytes_from_ascii("hi")));
  AW_CHECK(!reply.has_value());
  AW_CHECK(session.state() == SessionState::fresh);
}
