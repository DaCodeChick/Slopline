// Hotline networking: the shared connection base.
//
// The role-neutral half of the UTransact replacement: transaction
// framing, multi-part reassembly, the historical receive policy,
// encrypted-transaction hooks, the send queue, and readiness servicing.
// Application code never instantiates ConnectionBase — the Hotline
// client derives ClientConnection from it (src/hotline/client) and the
// server derives ServerConnection (src/hotline/server), so each
// application project carries exactly one role's establish path and
// neither role's start entry point exists in the other's binary.
//
// Preserved legacy behavior (verified against UTransact.cpp):
//  * the client treats a reply error != 0 as version-unknown and closes;
//    the server accepts 'TRTP' or the legacy 'NICK' alias, replies
//    'TRTP' + error 0, and rejects version != 1 with reason 1;
//  * receive policy: a header with totalSize == 0, dataSize == 0, or
//    either above max_receive_size kills the connection (2 MB framework
//    default, 512 KB server override), enforced AFTER the crypto header
//    decode;
//  * multi-part reassembly dispatches once the accumulated data reaches
//    totalSize, using the last part's header;
//  * the tree's own keepalive carries a 2-byte empty field list, not an
//    empty body (see the modernization ledger, Phase 5).
//
// Readiness-driven: applications register descriptor() with
// aw::net::Poller and call service_readable()/service_writable() — no
// busy-waiting, no threads.

#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <functional>
#include <map>
#include <span>
#include <vector>

#include "appwarrior/net/socket.h"
#include "hotline/protocol/constants.h"
#include "hotline/protocol/transaction.h"

namespace hotline::net {

using protocol::TransactionHeader;
using protocol::TransactionType;
using protocol::kClientSubVersion;
using protocol::kFrameworkMaxTransactionReceiveSize;
using protocol::kSubProtocolHotl;
using protocol::kTransactionHeaderSize;

enum class ConnectionState : std::uint8_t {
  idle,
  awaiting_handshake_reply,  // client: sent handshake, waiting for reply
  awaiting_handshake,        // server: waiting for the client handshake
  established,
  closing,  // flushing the final outbound bytes before close
  dead,
};

struct ConnectionConfig {
  std::uint32_t max_receive_size = kFrameworkMaxTransactionReceiveSize;
};

struct ReceivedTransaction {
  TransactionHeader header;
  std::vector<std::byte> data;
};

struct ConnectionEvents {
  std::function<void()> on_established;
  std::function<void(const ReceivedTransaction&)> on_transaction;
  std::function<void()> on_closed;
};

// Encrypted-transaction hooks (wrap hotline::protocol::auth::
// TransactionCipher<H>). Legacy order: the header is decoded as soon as
// it arrives (decode_header, returning the flag), the receive policy is
// then enforced on the DECODED sizes, and the data is decoded at
// completion (decode_data with that flag). Without hooks the connection
// is plaintext (flag 0).
struct ConnectionCryptoHooks {
  std::function<std::uint8_t()> choose_flag;
  std::function<void(std::span<std::byte, kTransactionHeaderSize>, std::span<std::byte>,
                     std::uint8_t)>
      encode;
  std::function<std::uint8_t(std::span<std::byte, kTransactionHeaderSize>)> decode_header;
  std::function<void(std::span<std::byte>, std::uint8_t)> decode_data;
};

// The shared connection state machine. Abstract: the establish handshake
// is role-specific and lives in the derived classes (ClientConnection in
// the client application project, ServerConnection in the server
// application project). Everything below the handshake — transaction
// framing and reassembly, the receive policy, crypto hooks, the send
// queue — is shared and lives here.
class ConnectionBase {
 public:
  ConnectionBase(ConnectionConfig config, ConnectionEvents events);
  virtual ~ConnectionBase() = default;

  ConnectionBase(const ConnectionBase&) = delete;
  auto operator=(const ConnectionBase&) = delete;
  ConnectionBase(ConnectionBase&&) noexcept;
  auto operator=(ConnectionBase&&) noexcept -> ConnectionBase&;

  [[nodiscard]] auto state() const noexcept -> ConnectionState;
  [[nodiscard]] auto descriptor() const noexcept -> aw::net::NativeSocket;
  [[nodiscard]] auto is_dead() const noexcept -> bool;

  void service_readable();
  void service_writable();

  void queue_transaction(TransactionType type, std::uint32_t id,
                         std::span<const std::byte> data, bool is_reply = false,
                         std::uint32_t error = 0);
  void queue_keepalive();
  void set_crypto(ConnectionCryptoHooks hooks);
  void close() noexcept;

 protected:
  enum class ReceiveStage : std::uint8_t { handshake, header, data };

  struct PendingSend {
    std::vector<std::byte> bytes;
    std::size_t offset = 0;
  };

  struct PartialReceive {
    TransactionHeader header;
    std::vector<std::byte> data;
  };

  void queue_bytes(std::vector<std::byte> bytes);
  void mark_dead() noexcept;
  void parse_inbound();
  void complete_transaction(std::vector<std::byte> data);
  void flush_sends();
  void mark_established() noexcept {
    state_ = ConnectionState::established;
    if (events_.on_established) {
      events_.on_established();
    }
  }

  // Consumes exactly expected_bytes_ of inbound_ and advances the
  // handshake (role-specific; implemented by the derived classes).
  virtual void handle_handshake() = 0;

  ConnectionConfig config_;
  ConnectionEvents events_;
  aw::net::Socket socket_;
  ConnectionState state_ = ConnectionState::idle;
  ConnectionCryptoHooks crypto_;

  ReceiveStage receive_stage_ = ReceiveStage::handshake;
  std::size_t expected_bytes_ = 0;
  std::vector<std::byte> inbound_;
  std::array<std::byte, kTransactionHeaderSize> header_bytes_{};
  TransactionHeader pending_header_{};
  std::uint8_t pending_flag_ = 0;
  std::vector<std::byte> pending_data_;
  std::map<std::pair<std::uint8_t, std::uint32_t>, PartialReceive> partial_;

  std::deque<PendingSend> send_queue_;
};

}  // namespace hotline::net
