// Hotline networking: connection state machines.
//
// The UTransact replacement, split by role so the Hotline client links a
// client-only API and the Hotline server a server-only API (neither
// application carries the other role's entry point). Both classes share a
// private connection core.
//
// Preserved legacy behavior (verified against UTransact.cpp):
//  * client handshake: 'TRTP' 'HOTL'/'HTXF' version 1 subVersion 2/3;
//  * server accepts 'TRTP' or the legacy 'NICK' alias, replies
//    'TRTP' + error 0, and rejects version != 1 with reason 1;
//  * the client treats a reply error != 0 as version-unknown and closes;
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
#include <functional>
#include <memory>
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

namespace detail {
class ClientCore;
class ServerCore;
}  // namespace detail

// Client-side connection (the Hotline client's entry point).
class ClientConnection {
 public:
  ClientConnection(ConnectionConfig config = {}, ConnectionEvents events = {});
  ~ClientConnection();
  ClientConnection(ClientConnection&&) noexcept;
  auto operator=(ClientConnection&&) noexcept -> ClientConnection&;

  ClientConnection(const ClientConnection&) = delete;
  auto operator=(const ClientConnection&) = delete;

  // Attaches a connected socket and sends the handshake immediately.
  void start(aw::net::Socket socket, std::uint32_t sub_protocol = kSubProtocolHotl,
             std::uint16_t sub_version = kClientSubVersion);

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

 private:
  std::unique_ptr<detail::ClientCore> core_;
};

// Server-side connection (the Hotline server's entry point).
class ServerConnection {
 public:
  ServerConnection(ConnectionConfig config = {}, ConnectionEvents events = {});
  ~ServerConnection();
  ServerConnection(ServerConnection&&) noexcept;
  auto operator=(ServerConnection&&) noexcept -> ServerConnection&;

  ServerConnection(const ServerConnection&) = delete;
  auto operator=(const ServerConnection&) = delete;

  // Attaches a socket accepted from a listener; the handshake reply is
  // queued once the client's handshake validates.
  void start(aw::net::Socket socket);

  [[nodiscard]] auto state() const noexcept -> ConnectionState;
  [[nodiscard]] auto descriptor() const noexcept -> aw::net::NativeSocket;
  [[nodiscard]] auto is_dead() const noexcept -> bool;
  [[nodiscard]] auto remote_sub_protocol() const noexcept -> std::uint32_t;
  [[nodiscard]] auto remote_version() const noexcept -> std::uint16_t;

  void service_readable();
  void service_writable();

  void queue_transaction(TransactionType type, std::uint32_t id,
                         std::span<const std::byte> data, bool is_reply = false,
                         std::uint32_t error = 0);
  void queue_keepalive();
  void set_crypto(ConnectionCryptoHooks hooks);
  void close() noexcept;

 private:
  std::unique_ptr<detail::ServerCore> core_;
};

}  // namespace hotline::net
