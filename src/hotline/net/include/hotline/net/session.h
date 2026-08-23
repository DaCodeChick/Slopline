// Hotline networking: login session state machine.
//
// The server-side login/agreement choreography extracted from
// ProcessTran_Login (HotlineServTrans.cpp:1605-1700, :1886-1900) and
// ProcessTran_Agreed, using the Phase 3 auth codecs:
//
//  * the login name is unscrambled (bitwise NOT), lowercased (ASCII —
//    the legacy MakeLowercase is encoding-aware and arrives with the text
//    layer), and '\r' is replaced with '-';
//  * the user is looked up by that normalized login (injected callback —
//    the server core will back it with the user database);
//  * the password is compared EXACTLY as stored: the received field bytes
//    (still scrambled) must equal the stored (still scrambled) bytes —
//    the server never unscrambles the password;
//  * success replies with Vers = server version, CommunityBannerID, and
//    the server name; failure replies with error 1 + ErrorText (fixing
//    the legacy format-string SendErrorMsg bug that sent no data);
//  * the Agreed transaction moves the session from awaiting_agreement to
//    active.
//
// The agreement/banner choreography itself belongs to the server core
// (Phase 6); this unit owns verification and state transitions.

#pragma once

#include <cstdint>
#include <expected>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "hotline/net/connection.h"
#include "hotline/protocol/field_list.h"
#include "hotline/protocol/payload.h"

#if !defined(HOTLINE_BUILD_SERVER)
#define HOTLINE_BUILD_SERVER 0
#endif

#if HOTLINE_BUILD_SERVER

namespace hotline::net {

using protocol::AccessMask;
using protocol::FieldList;

enum class SessionState : std::uint8_t { fresh, awaiting_agreement, active, closed };

enum class LoginError : std::uint8_t { unknown_user, wrong_password, malformed_login };

struct UserRecord {
  std::string name;                // account display name (raw bytes)
  std::vector<std::byte> password;  // STORED form: still scrambled
  AccessMask access;
};

using UserLookup =
    std::function<std::expected<UserRecord, LoginError>(std::string_view normalized_login)>;

struct SessionConfig {
  std::string server_name;
  std::uint16_t server_version = 197;
  std::uint32_t community_banner_id = 0;
  UserLookup lookup_user;
  std::string incorrect_login_message = "Incorrect login.";
};

// A reply the caller should queue on the connection for the transaction
// that was processed.
struct OutgoingReply {
  std::uint32_t id = 0;
  std::uint32_t error = 0;
  FieldList fields;
};

class Session {
 public:
  explicit Session(SessionConfig config);

  [[nodiscard]] auto process(const ReceivedTransaction& transaction)
      -> std::optional<OutgoingReply>;

  [[nodiscard]] auto state() const noexcept -> SessionState;
  [[nodiscard]] auto access() const noexcept -> const AccessMask&;
  [[nodiscard]] auto login() const noexcept -> const std::string&;
  void close() noexcept;

 private:
  SessionConfig config_;
  SessionState state_ = SessionState::fresh;
  std::string login_;
  AccessMask access_;
};

}  // namespace hotline::net

#endif  // HOTLINE_BUILD_SERVER
