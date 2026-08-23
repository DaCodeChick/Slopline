// AppWarrior networking: RAII non-blocking sockets.
//
// Implemented with conditional compilation: WinSock2 on Windows, POSIX
// sockets elsewhere (per project decision — see the modernization ledger,
// Phase 5 follow-up). Sockets are non-blocking and move-only; every
// operation returns std::expected<T, NetError>. Applications drive
// readiness through aw::net::Poller — no busy-waiting.

#pragma once

#include <cstddef>
#include <cstdint>
#include <expected>
#include <span>
#include <utility>

#include "appwarrior/export.h"
#include "appwarrior/net/address.h"
#include "appwarrior/net/net_error.h"

namespace aw::net {

// Native socket handle: int on POSIX, SOCKET (uintptr) on Windows.
#if defined(_WIN32)
using NativeSocket = std::uintptr_t;
#else
using NativeSocket = int;
#endif

// One received UDP datagram. Zero-length datagrams are valid on UDP (they
// are NOT connection_closed — that distinction belongs to streams).
struct Datagram {
  std::size_t bytes_received = 0;
  IpAddress from;
};

class AW_API Socket {
 public:
  Socket() = default;
  explicit Socket(NativeSocket descriptor) noexcept : descriptor_(descriptor) {}
  Socket(Socket&& other) noexcept : descriptor_(other.descriptor_) {
    other.descriptor_ = kInvalid;
  }
  auto operator=(Socket&& other) noexcept -> Socket& {
    if (this != &other) {
      close();
      descriptor_ = other.descriptor_;
      other.descriptor_ = kInvalid;
    }
    return *this;
  }
  ~Socket() { close(); }

  Socket(const Socket&) = delete;
  auto operator=(const Socket&) = delete;

  // Creates a non-blocking stream socket for the given address family.
  [[nodiscard]] static auto create_tcp(IpAddress::Family family)
      -> std::expected<Socket, NetError>;

  // Creates a non-blocking datagram socket for the given address family.
  // IPv6 sockets are dual-stack: they also receive v4-mapped datagrams.
  // Bind it with bind(); send and receive with send_to()/receive_from().
  [[nodiscard]] static auto create_udp(IpAddress::Family family)
      -> std::expected<Socket, NetError>;

  [[nodiscard]] auto descriptor() const noexcept -> NativeSocket { return descriptor_; }
  [[nodiscard]] auto is_open() const noexcept -> bool { return descriptor_ != kInvalid; }

  // Non-blocking connect: returns would_block while in progress (poll for
  // writability, then call connect again to collect the result).
  auto connect(const IpAddress& address) -> std::expected<void, NetError>;

  // Binds the socket to a local address. Port 0 selects an ephemeral port
  // (query local_address()). Used for UDP sockets and any socket that
  // needs an explicit local endpoint.
  auto bind(const IpAddress& address) -> std::expected<void, NetError>;

  // Partial sends are normal; the returned value is the byte count written.
  auto send(std::span<const std::byte> buffer) -> std::expected<std::size_t, NetError>;
  // Returns the byte count read; a zero-length read reports
  // NetError::connection_closed instead (stream semantics).
  auto receive(std::span<std::byte> buffer) -> std::expected<std::size_t, NetError>;

  // Sends one datagram to `destination`. An empty buffer sends a valid
  // zero-length datagram (unlike send(), which rejects empty buffers).
  auto send_to(const IpAddress& destination, std::span<const std::byte> buffer)
      -> std::expected<std::size_t, NetError>;

  // Receives one datagram; `datagram.from` is the sender's address. A
  // zero-length datagram succeeds with bytes_received == 0 (UDP has no
  // stream-close semantics).
  auto receive_from(std::span<std::byte> buffer) -> std::expected<Datagram, NetError>;

  auto local_address() const -> std::expected<IpAddress, NetError>;
  auto remote_address() const -> std::expected<IpAddress, NetError>;

  void set_tcp_no_delay(bool enabled) noexcept;  // best effort
  void shutdown() noexcept;                      // half-close both directions
  void close() noexcept;

 private:
#if defined(_WIN32)
  static constexpr NativeSocket kInvalid = static_cast<NativeSocket>(~static_cast<std::uintptr_t>(0));
#else
  static constexpr NativeSocket kInvalid = -1;
#endif
  NativeSocket descriptor_ = kInvalid;
};

// A non-blocking stream listener bound to an address. Port 0 selects an
// ephemeral port (query local_address()).
class AW_API Listener {
 public:
  Listener() = default;
  explicit Listener(NativeSocket descriptor) noexcept : descriptor_(descriptor) {}
  Listener(Listener&& other) noexcept : descriptor_(other.descriptor_) {
    other.descriptor_ = kInvalid;
  }
  auto operator=(Listener&& other) noexcept -> Listener& {
    if (this != &other) {
      close();
      descriptor_ = other.descriptor_;
      other.descriptor_ = kInvalid;
    }
    return *this;
  }
  ~Listener() { close(); }

  Listener(const Listener&) = delete;
  auto operator=(const Listener&) = delete;

  [[nodiscard]] static auto create_tcp(IpAddress::Family family)
      -> std::expected<Listener, NetError>;
  auto listen(const IpAddress& address, int backlog = 16) -> std::expected<void, NetError>;
  auto accept() -> std::expected<Socket, NetError>;  // would_block when none pending

  [[nodiscard]] auto descriptor() const noexcept -> NativeSocket { return descriptor_; }
  auto local_address() const -> std::expected<IpAddress, NetError>;
  void close() noexcept;

 private:
#if defined(_WIN32)
  static constexpr NativeSocket kInvalid = static_cast<NativeSocket>(~static_cast<std::uintptr_t>(0));
#else
  static constexpr NativeSocket kInvalid = -1;
#endif
  NativeSocket descriptor_ = kInvalid;
};

// A connected pair of non-blocking stream sockets — the deterministic
// transport for integration tests, and useful for in-process plumbing.
// (AF_UNIX socketpair on POSIX; a loopback TCP pair on Windows, which has
// no socketpair.)
[[nodiscard]] auto make_socket_pair() -> std::expected<std::pair<Socket, Socket>, NetError>;

}  // namespace aw::net
