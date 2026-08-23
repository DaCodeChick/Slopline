#include "hotline/net/session.h"

#if HOTLINE_BUILD_SERVER


#include <cstddef>
#include <cstdint>
#include <utility>

#include "hotline/protocol/auth.h"

namespace hotline::net {

using namespace hotline::protocol;

namespace {

// ASCII lowercase (the legacy UText::MakeLowercase is encoding-aware and
// arrives with the text layer) plus the historical '\r' -> '-' rewrite.
auto normalize_login(std::string login) -> std::string {
  for (char& character : login) {
    if (character >= 'A' && character <= 'Z') {
      character = static_cast<char>(character - 'A' + 'a');
    } else if (character == '\r') {
      character = '-';
    }
  }
  return login;
}

}  // namespace

Session::Session(SessionConfig config) : config_(std::move(config)) {}

auto Session::process(const ReceivedTransaction& transaction) -> std::optional<OutgoingReply> {
  if (transaction.header.is_reply != 0 || state_ == SessionState::closed) {
    return std::nullopt;
  }

  switch (transaction.header.type) {
    case TransactionType::Login: {
      const auto fields = decode_field_list(transaction.data);
      if (!fields.has_value()) {
        state_ = SessionState::closed;
        OutgoingReply reply;
        reply.id = transaction.header.id;
        reply.error = 1;
        reply.fields.fields.push_back(make_string_field(FieldId::ErrorText, "Bad login."));
        return reply;
      }
      const Field* login_field = find_field(*fields, FieldId::UserLogin);
      const Field* password_field = find_field(*fields, FieldId::UserPassword);
      if (login_field == nullptr || password_field == nullptr || login_field->data.empty()) {
        state_ = SessionState::closed;
        OutgoingReply reply;
        reply.id = transaction.header.id;
        reply.error = 1;
        reply.fields.fields.push_back(make_string_field(FieldId::ErrorText, "Bad login."));
        return reply;
      }

      // Unscramble the login (bitwise NOT — self-inverse) and normalize.
      std::vector<std::byte> login_bytes = login_field->data;
      protocol::auth::scramble(std::span<std::byte>(login_bytes));
      std::string normalized;
      normalized.reserve(login_bytes.size());
      for (const std::byte byte : login_bytes) {
        normalized.push_back(static_cast<char>(std::to_integer<unsigned char>(byte)));
      }
      normalized = normalize_login(std::move(normalized));

      const auto user = config_.lookup_user ? config_.lookup_user(normalized)
                                            : std::expected<UserRecord, LoginError>{
                                                  std::unexpected(LoginError::unknown_user)};
      if (!user.has_value()) {
        state_ = SessionState::closed;
        OutgoingReply reply;
        reply.id = transaction.header.id;
        reply.error = 1;
        reply.fields.fields.push_back(
            make_string_field(FieldId::ErrorText, config_.incorrect_login_message));
        return reply;
      }

      // The password is compared exactly as stored: received (scrambled)
      // bytes against stored (scrambled) bytes — never unscrambled.
      if (password_field->data.size() != user->password.size() ||
          !std::ranges::equal(password_field->data, user->password)) {
        state_ = SessionState::closed;
        OutgoingReply reply;
        reply.id = transaction.header.id;
        reply.error = 1;
        reply.fields.fields.push_back(
            make_string_field(FieldId::ErrorText, config_.incorrect_login_message));
        return reply;
      }

      login_ = normalized;
      access_ = user->access;
      state_ = SessionState::awaiting_agreement;

      OutgoingReply reply;
      reply.id = transaction.header.id;
      reply.fields.fields.push_back(
          make_integer_field(FieldId::Vers, static_cast<std::int32_t>(config_.server_version)));
      reply.fields.fields.push_back(make_integer_field(
          FieldId::CommunityBannerId, static_cast<std::int32_t>(config_.community_banner_id)));
      reply.fields.fields.push_back(make_string_field(FieldId::ServerName, config_.server_name));
      return reply;
    }

    case TransactionType::Agreed: {
      if (state_ == SessionState::awaiting_agreement) {
        state_ = SessionState::active;
      }
      return std::nullopt;
    }

    default:
      return std::nullopt;
  }
}

auto Session::state() const noexcept -> SessionState { return state_; }

auto Session::access() const noexcept -> const AccessMask& { return access_; }

auto Session::login() const noexcept -> const std::string& { return login_; }

void Session::close() noexcept { state_ = SessionState::closed; }

}  // namespace hotline::net

#endif  // HOTLINE_BUILD_SERVER
