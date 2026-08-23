// Hotline networking: connection state machine.
//
// The UTransact replacement: establishes the TRTP handshake in both
// roles, frames transactions (20-byte header + data), reassembles
// multi-part transactions by (isReply, id), enforces the historical
// receive policy, and optionally applies the encrypted-transaction
// stream (Phase 3b TransactionCipher) via injected hooks.
//
// Preserved legacy behavior (verified against UTransact.cpp):
//  * client handshake: 'TRTP' 'HOTL'/'HTXF' version 1 subVersion 2/3;
//  * server accepts 'TRTP' or the legacy 'NICK' alias, replies
//    'TRTP' + error 0, and rejects version != 1 with reason 1;
//  * the client treats a reply error != 0 as version-unknown and closes;
//  * receive policy: a header with totalSize == 0, dataSize == 0, or
//    either above max_receive_size kills the connection (2 MB framework
//    default, 512 KB server override);
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
#include <utility>
#include <vector>

#include "appwarrior/net/socket.h"
#include "hotline/protocol/constants.h"
#include "hotline/protocol/transaction.h"

namespace hotline::net {

using protocol::TransactionHeader;
using protocol::TransactionType;
using protocol::kClientSubVersion;
using protocol::kFrameworkMaxTransactionReceiveSize;
using protocol::kProtocolNick;
using protocol::kProtocolTrTp;
using protocol::kProtocolVersion;
using protocol::kSubProtocolHotl;
using protocol::kTransactionHeaderSize;

enum class ConnectionRole : std::uint8_t { client, server };

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

class Connection {
 public:
  Connection(ConnectionRole role, ConnectionConfig config = {}, ConnectionEvents events = {});

  // Client: attach a connected socket and send the handshake immediately.
  void start_client(aw::net::Socket socket, std::uint32_t sub_protocol = kSubProtocolHotl,
                    std::uint16_t sub_version = kClientSubVersion);
  // Server: attach a socket accepted from a listener; the handshake reply
  // is queued once the client's handshake validates.
  void start_server(aw::net::Socket socket);

  [[nodiscard]] auto state() const noexcept -> ConnectionState;
  [[nodiscard]] auto descriptor() const noexcept -> int;
  [[nodiscard]] auto is_dead() const noexcept -> bool;
  [[nodiscard]] auto remote_sub_protocol() const noexcept -> std::uint32_t;
  [[nodiscard]] auto remote_version() const noexcept -> std::uint16_t;

  void service_readable();
  void service_writable();

  // Queues a single-part transaction (totalSize == dataSize). The legacy
  // sender permitted empty bodies; the RECEIVER kills them — so callers
  // that need a no-op use queue_keepalive() instead.
  void queue_transaction(TransactionType type, std::uint32_t id,
                         std::span<const std::byte> data, bool is_reply = false,
                         std::uint32_t error = 0);
  // The historical keepalive: transaction 500 with a 2-byte empty field
  // list body.
  void queue_keepalive();

  // Encrypted-transaction hooks (wrap hotline::protocol::auth::
  // TransactionCipher<H>). Legacy order: the header is decoded as soon
  // as it arrives (decode_header, returning the flag), the receive
  // policy is then enforced on the DECODED sizes, and the data is
  // decoded at completion (decode_data with that flag). Without hooks
  // the connection is plaintext (flag 0).
  struct CryptoHooks {
    std::function<std::uint8_t()> choose_flag;
    std::function<void(std::span<std::byte, kTransactionHeaderSize>, std::span<std::byte>,
                       std::uint8_t)>
        encode;
    std::function<std::uint8_t(std::span<std::byte, kTransactionHeaderSize>)> decode_header;
    std::function<void(std::span<std::byte>, std::uint8_t)> decode_data;
  };
  void set_crypto(CryptoHooks hooks);

  void close() noexcept;

 private:
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

  ConnectionRole role_;
  ConnectionConfig config_;
  ConnectionEvents events_;
  aw::net::Socket socket_;
  ConnectionState state_ = ConnectionState::idle;
  CryptoHooks crypto_;

  ReceiveStage receive_stage_ = ReceiveStage::handshake;
  std::size_t expected_bytes_ = 0;
  std::vector<std::byte> inbound_;
  std::array<std::byte, kTransactionHeaderSize> header_bytes_{};
  TransactionHeader pending_header_{};
  std::uint8_t pending_flag_ = 0;
  std::vector<std::byte> pending_data_;
  std::map<std::pair<std::uint8_t, std::uint32_t>, PartialReceive> partial_;

  std::deque<PendingSend> send_queue_;

  std::uint32_t remote_sub_protocol_ = 0;
  std::uint16_t remote_version_ = 0;
};

}  // namespace hotline::net
