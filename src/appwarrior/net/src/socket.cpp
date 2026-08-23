#include "appwarrior/net/socket.h"

#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstring>

#if defined(_WIN32)

#include <winsock2.h>
#include <ws2tcpip.h>

namespace aw::net {

namespace {

// WSAStartup must run before any WinSock call; a process-lifetime static
// is the simple, correct answer for the framework (cleanup happens at
// process exit, after all sockets are gone).
struct WsaRuntime {
  WsaRuntime() {
    WSADATA data{};
    ::WSAStartup(MAKEWORD(2, 2), &data);
  }
  ~WsaRuntime() { ::WSACleanup(); }
};

auto wsa_runtime() -> WsaRuntime& {
  static WsaRuntime runtime;
  return runtime;
}

auto last_error() noexcept -> int { return ::WSAGetLastError(); }

auto from_error(int error) noexcept -> NetError {
  switch (error) {
    case WSAEWOULDBLOCK:
    case WSAEINPROGRESS:
      return NetError::would_block;
    case WSAEINTR:
      return NetError::interrupted;
    case WSAECONNRESET:
    case WSAENOTCONN:
      return NetError::connection_closed;
    case WSAECONNREFUSED:
      return NetError::connection_refused;
    case WSAEADDRINUSE:
      return NetError::address_in_use;
    case WSAEINVAL:
    case WSAEAFNOSUPPORT:
      return NetError::invalid_argument;
    default:
      return NetError::system;
  }
}

auto set_non_blocking(NativeSocket descriptor) -> bool {
  u_long enabled = 1;
  return ::ioctlsocket(descriptor, FIONBIO, &enabled) == 0;
}

void to_sockaddr(const IpAddress& address, sockaddr_storage& out, int& out_size) noexcept {
  std::memset(&out, 0, sizeof(out));
  if (address.family() == IpAddress::Family::ipv4) {
    auto& v4 = reinterpret_cast<sockaddr_in&>(out);
    v4.sin_family = AF_INET;
    v4.sin_port = htons(address.port());
    v4.sin_addr.s_addr = htonl(address.network_address());
    out_size = sizeof(v4);
  } else {
    auto& v6 = reinterpret_cast<sockaddr_in6&>(out);
    v6.sin6_family = AF_INET6;
    v6.sin6_port = htons(address.port());
    const std::array<std::uint8_t, 16> bytes = address.ipv6_bytes();
    std::memcpy(v6.sin6_addr.s6_addr, bytes.data(), 16);
    v6.sin6_scope_id = address.scope_id();
    out_size = sizeof(v6);
  }
}

auto from_sockaddr(const sockaddr_storage& in) -> IpAddress {
  if (in.ss_family == AF_INET6) {
    const auto& v6 = reinterpret_cast<const sockaddr_in6&>(in);
    std::array<std::uint8_t, 16> bytes{};
    std::memcpy(bytes.data(), v6.sin6_addr.s6_addr, 16);
    return IpAddress::ipv6(bytes, ntohs(v6.sin6_port), v6.sin6_scope_id);
  }
  const auto& v4 = reinterpret_cast<const sockaddr_in&>(in);
  const std::uint32_t network = v4.sin_addr.s_addr;
  return IpAddress{static_cast<std::uint8_t>(network & 0xFFU),
                   static_cast<std::uint8_t>((network >> 8) & 0xFFU),
                   static_cast<std::uint8_t>((network >> 16) & 0xFFU),
                   static_cast<std::uint8_t>((network >> 24) & 0xFFU), ntohs(v4.sin_port)};
}

}  // namespace

auto Socket::create_tcp(IpAddress::Family family) -> std::expected<Socket, NetError> {
  wsa_runtime();
  const int af = family == IpAddress::Family::ipv4 ? AF_INET : AF_INET6;
  const NativeSocket descriptor = ::socket(af, SOCK_STREAM, IPPROTO_TCP);
  if (descriptor == INVALID_SOCKET) {
    return std::unexpected(from_error(last_error()));
  }
  if (!set_non_blocking(descriptor)) {
    const int error = last_error();
    ::closesocket(descriptor);
    return std::unexpected(from_error(error));
  }
  return Socket{descriptor};
}

auto Socket::create_udp(IpAddress::Family family) -> std::expected<Socket, NetError> {
  wsa_runtime();
  const int af = family == IpAddress::Family::ipv4 ? AF_INET : AF_INET6;
  const NativeSocket descriptor = ::socket(af, SOCK_DGRAM, IPPROTO_UDP);
  if (descriptor == INVALID_SOCKET) {
    return std::unexpected(from_error(last_error()));
  }
  if (!set_non_blocking(descriptor)) {
    const int error = last_error();
    ::closesocket(descriptor);
    return std::unexpected(from_error(error));
  }
  // Dual-stack IPv6 datagram sockets receive v4-mapped datagrams.
  if (af == AF_INET6) {
    const DWORD disabled = 0;
    ::setsockopt(descriptor, IPPROTO_IPV6, IPV6_V6ONLY,
                 reinterpret_cast<const char*>(&disabled), sizeof(disabled));
  }
  return Socket{descriptor};
}

auto Socket::bind(const IpAddress& address) -> std::expected<void, NetError> {
  if (!is_open()) {
    return std::unexpected(NetError::invalid_argument);
  }
  sockaddr_storage bind_address{};
  int bind_size = 0;
  to_sockaddr(address, bind_address, bind_size);
  if (::bind(descriptor_, reinterpret_cast<const sockaddr*>(&bind_address), bind_size) != 0) {
    return std::unexpected(from_error(last_error()));
  }
  return {};
}

auto Socket::connect(const IpAddress& address) -> std::expected<void, NetError> {
  if (!is_open()) {
    return std::unexpected(NetError::invalid_argument);
  }
  sockaddr_storage target{};
  int target_size = 0;
  to_sockaddr(address, target, target_size);
  if (::connect(descriptor_, reinterpret_cast<const sockaddr*>(&target), target_size) != 0) {
    return std::unexpected(from_error(last_error()));
  }
  return {};
}

auto Socket::send(std::span<const std::byte> buffer) -> std::expected<std::size_t, NetError> {
  if (!is_open() || buffer.empty()) {
    return std::unexpected(NetError::invalid_argument);
  }
  const int written = ::send(descriptor_, reinterpret_cast<const char*>(buffer.data()),
                             static_cast<int>(buffer.size()), 0);
  if (written == SOCKET_ERROR) {
    return std::unexpected(from_error(last_error()));
  }
  return static_cast<std::size_t>(written);
}

auto Socket::receive(std::span<std::byte> buffer) -> std::expected<std::size_t, NetError> {
  if (!is_open() || buffer.empty()) {
    return std::unexpected(NetError::invalid_argument);
  }
  const int read = ::recv(descriptor_, reinterpret_cast<char*>(buffer.data()),
                          static_cast<int>(buffer.size()), 0);
  if (read == SOCKET_ERROR) {
    return std::unexpected(from_error(last_error()));
  }
  if (read == 0) {
    return std::unexpected(NetError::connection_closed);
  }
  return static_cast<std::size_t>(read);
}

auto Socket::send_to(const IpAddress& destination, std::span<const std::byte> buffer)
    -> std::expected<std::size_t, NetError> {
  if (!is_open()) {
    return std::unexpected(NetError::invalid_argument);
  }
  sockaddr_storage target{};
  int target_size = 0;
  to_sockaddr(destination, target, target_size);
  const int sent = ::sendto(descriptor_, reinterpret_cast<const char*>(buffer.data()),
                            static_cast<int>(buffer.size()), 0,
                            reinterpret_cast<const sockaddr*>(&target), target_size);
  if (sent == SOCKET_ERROR) {
    return std::unexpected(from_error(last_error()));
  }
  return static_cast<std::size_t>(sent);
}

auto Socket::receive_from(std::span<std::byte> buffer) -> std::expected<Datagram, NetError> {
  if (!is_open() || buffer.empty()) {
    return std::unexpected(NetError::invalid_argument);
  }
  sockaddr_storage source{};
  std::memset(&source, 0, sizeof(source));
  int source_size = sizeof(source);
  const int read = ::recvfrom(descriptor_, reinterpret_cast<char*>(buffer.data()),
                              static_cast<int>(buffer.size()), 0,
                              reinterpret_cast<sockaddr*>(&source), &source_size);
  if (read == SOCKET_ERROR) {
    return std::unexpected(from_error(last_error()));
  }
  // Zero-length datagrams are valid on UDP: report them, don't fold them
  // into the stream-only connection_closed signal.
  return Datagram{static_cast<std::size_t>(read), from_sockaddr(source)};
}

auto Socket::local_address() const -> std::expected<IpAddress, NetError> {
  sockaddr_storage address{};
  int length = sizeof(address);
  if (::getsockname(descriptor_, reinterpret_cast<sockaddr*>(&address), &length) != 0) {
    return std::unexpected(from_error(last_error()));
  }
  return from_sockaddr(address);
}

auto Socket::remote_address() const -> std::expected<IpAddress, NetError> {
  sockaddr_storage address{};
  int length = sizeof(address);
  if (::getpeername(descriptor_, reinterpret_cast<sockaddr*>(&address), &length) != 0) {
    return std::unexpected(from_error(last_error()));
  }
  return from_sockaddr(address);
}

void Socket::set_tcp_no_delay(bool enabled) noexcept {
  const BOOL value = enabled ? TRUE : FALSE;
  ::setsockopt(descriptor_, IPPROTO_TCP, TCP_NODELAY,
               reinterpret_cast<const char*>(&value), sizeof(value));
}

void Socket::shutdown() noexcept {
  if (is_open()) {
    ::shutdown(descriptor_, SD_BOTH);
  }
}

void Socket::close() noexcept {
  if (is_open()) {
    ::closesocket(descriptor_);
    descriptor_ = kInvalid;
  }
}

auto Listener::create_tcp(IpAddress::Family family) -> std::expected<Listener, NetError> {
  wsa_runtime();
  const int af = family == IpAddress::Family::ipv4 ? AF_INET : AF_INET6;
  const NativeSocket descriptor = ::socket(af, SOCK_STREAM, IPPROTO_TCP);
  if (descriptor == INVALID_SOCKET) {
    return std::unexpected(from_error(last_error()));
  }
  if (!set_non_blocking(descriptor)) {
    const int error = last_error();
    ::closesocket(descriptor);
    return std::unexpected(from_error(error));
  }
  // Dual-stack IPv6 listeners accept v4-mapped connections.
  if (af == AF_INET6) {
    const DWORD disabled = 0;
    ::setsockopt(descriptor, IPPROTO_IPV6, IPV6_V6ONLY,
                 reinterpret_cast<const char*>(&disabled), sizeof(disabled));
  }
  return Listener{descriptor};
}

auto Listener::listen(const IpAddress& address, int backlog) -> std::expected<void, NetError> {
  if (descriptor_ == kInvalid || backlog < 1) {
    return std::unexpected(NetError::invalid_argument);
  }
  sockaddr_storage bind_address{};
  int bind_size = 0;
  to_sockaddr(address, bind_address, bind_size);
  const BOOL reuse = TRUE;
  ::setsockopt(descriptor_, SOL_SOCKET, SO_REUSEADDR,
               reinterpret_cast<const char*>(&reuse), sizeof(reuse));
  if (::bind(descriptor_, reinterpret_cast<const sockaddr*>(&bind_address), bind_size) != 0) {
    return std::unexpected(from_error(last_error()));
  }
  if (::listen(descriptor_, backlog) != 0) {
    return std::unexpected(from_error(last_error()));
  }
  return {};
}

auto Listener::accept() -> std::expected<Socket, NetError> {
  const NativeSocket accepted = ::accept(descriptor_, nullptr, nullptr);
  if (accepted == INVALID_SOCKET) {
    return std::unexpected(from_error(last_error()));
  }
  if (!set_non_blocking(accepted)) {
    const int error = last_error();
    ::closesocket(accepted);
    return std::unexpected(from_error(error));
  }
  return Socket{accepted};
}

auto Listener::local_address() const -> std::expected<IpAddress, NetError> {
  sockaddr_storage address{};
  int length = sizeof(address);
  if (::getsockname(descriptor_, reinterpret_cast<sockaddr*>(&address), &length) != 0) {
    return std::unexpected(from_error(last_error()));
  }
  return from_sockaddr(address);
}

void Listener::close() noexcept {
  if (descriptor_ != kInvalid) {
    ::closesocket(descriptor_);
    descriptor_ = kInvalid;
  }
}

auto make_socket_pair() -> std::expected<std::pair<Socket, Socket>, NetError> {
  wsa_runtime();
  // Windows has no socketpair: build a loopback TCP pair by hand.
  const auto listener = Listener::create_tcp(IpAddress::Family::ipv4);
  if (!listener.has_value()) {
    return std::unexpected(listener.error());
  }
  auto listener_value = std::move(*listener);
  const auto bound = listener_value.listen(IpAddress{127, 0, 0, 1, 0});
  if (!bound.has_value()) {
    return std::unexpected(bound.error());
  }
  const auto address = listener_value.local_address();
  if (!address.has_value()) {
    return std::unexpected(address.error());
  }

  const auto client = Socket::create_tcp(IpAddress::Family::ipv4);
  if (!client.has_value()) {
    return std::unexpected(client.error());
  }
  auto client_value = std::move(*client);

  // Blocking connect/accept on a loopback pair completes immediately;
  // switch both to non-blocking afterwards.
  u_long disabled = 0;
  ::ioctlsocket(client_value.descriptor(), FIONBIO, &disabled);
  const auto connecting = client_value.connect(*address);
  if (!connecting.has_value()) {
    return std::unexpected(connecting.error());
  }
  const auto accepted = listener_value.accept();
  if (!accepted.has_value()) {
    return std::unexpected(accepted.error());
  }
  auto server_value = std::move(*accepted);
  ::ioctlsocket(client_value.descriptor(), FIONBIO, &disabled);
  ::ioctlsocket(server_value.descriptor(), FIONBIO, &disabled);
  return std::pair<Socket, Socket>{std::move(client_value), std::move(server_value)};
}

}  // namespace aw::net

#else  // POSIX

#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/socket.h>
#include <unistd.h>

namespace aw::net {

namespace {

auto last_error() noexcept -> int { return errno; }

auto from_error(int error) noexcept -> NetError {
  switch (error) {
    case EAGAIN:
#if EWOULDBLOCK != EAGAIN
    case EWOULDBLOCK:
#endif
    case EINPROGRESS:
      return NetError::would_block;
    case EINTR:
      return NetError::interrupted;
    case ECONNRESET:
    case EPIPE:
    case ENOTCONN:
      return NetError::connection_closed;
    case ECONNREFUSED:
      return NetError::connection_refused;
    case EADDRINUSE:
      return NetError::address_in_use;
    case EINVAL:
    case EAFNOSUPPORT:
      return NetError::invalid_argument;
    default:
      return NetError::system;
  }
}

auto set_non_blocking(NativeSocket descriptor) -> bool {
  const int flags = ::fcntl(descriptor, F_GETFL, 0);
  if (flags < 0) {
    return false;
  }
  return ::fcntl(descriptor, F_SETFL, flags | O_NONBLOCK) >= 0;
}

void to_sockaddr(const IpAddress& address, sockaddr_storage& out, socklen_t& out_size) noexcept {
  std::memset(&out, 0, sizeof(out));
  if (address.family() == IpAddress::Family::ipv4) {
    auto& v4 = reinterpret_cast<sockaddr_in&>(out);
    v4.sin_family = AF_INET;
    v4.sin_addr.s_addr = htonl(address.network_address());
    v4.sin_port = htons(address.port());
    out_size = sizeof(v4);
  } else {
    auto& v6 = reinterpret_cast<sockaddr_in6&>(out);
    v6.sin6_family = AF_INET6;
    v6.sin6_port = htons(address.port());
    const std::array<std::uint8_t, 16> bytes = address.ipv6_bytes();
    std::memcpy(v6.sin6_addr.s6_addr, bytes.data(), 16);
    v6.sin6_scope_id = address.scope_id();
    out_size = sizeof(v6);
  }
}

auto from_sockaddr(const sockaddr_storage& in) -> IpAddress {
  if (in.ss_family == AF_INET6) {
    const auto& v6 = reinterpret_cast<const sockaddr_in6&>(in);
    std::array<std::uint8_t, 16> bytes{};
    std::memcpy(bytes.data(), v6.sin6_addr.s6_addr, 16);
    return IpAddress::ipv6(bytes, ntohs(v6.sin6_port), v6.sin6_scope_id);
  }
  const auto& v4 = reinterpret_cast<const sockaddr_in&>(in);
  const std::uint32_t network = v4.sin_addr.s_addr;
  return IpAddress{static_cast<std::uint8_t>(network & 0xFFU),
                   static_cast<std::uint8_t>((network >> 8) & 0xFFU),
                   static_cast<std::uint8_t>((network >> 16) & 0xFFU),
                   static_cast<std::uint8_t>((network >> 24) & 0xFFU), ntohs(v4.sin_port)};
}

}  // namespace

auto Socket::create_tcp(IpAddress::Family family) -> std::expected<Socket, NetError> {
  const int af = family == IpAddress::Family::ipv4 ? AF_INET : AF_INET6;
  const NativeSocket descriptor = ::socket(af, SOCK_STREAM, 0);
  if (descriptor < 0) {
    return std::unexpected(from_error(last_error()));
  }
  if (!set_non_blocking(descriptor)) {
    const int error = last_error();
    ::close(descriptor);
    return std::unexpected(from_error(error));
  }
  return Socket{descriptor};
}

auto Socket::create_udp(IpAddress::Family family) -> std::expected<Socket, NetError> {
  const int af = family == IpAddress::Family::ipv4 ? AF_INET : AF_INET6;
  const NativeSocket descriptor = ::socket(af, SOCK_DGRAM, 0);
  if (descriptor < 0) {
    return std::unexpected(from_error(last_error()));
  }
  if (!set_non_blocking(descriptor)) {
    const int error = last_error();
    ::close(descriptor);
    return std::unexpected(from_error(error));
  }
  // Dual-stack IPv6 datagram sockets receive v4-mapped datagrams.
  if (af == AF_INET6) {
    const int disabled = 0;
    ::setsockopt(descriptor, IPPROTO_IPV6, IPV6_V6ONLY, &disabled, sizeof(disabled));
  }
  return Socket{descriptor};
}

auto Socket::bind(const IpAddress& address) -> std::expected<void, NetError> {
  if (!is_open()) {
    return std::unexpected(NetError::invalid_argument);
  }
  sockaddr_storage bind_address{};
  socklen_t bind_size = 0;
  to_sockaddr(address, bind_address, bind_size);
  if (::bind(descriptor_, reinterpret_cast<const sockaddr*>(&bind_address), bind_size) < 0) {
    return std::unexpected(from_error(last_error()));
  }
  return {};
}

auto Socket::connect(const IpAddress& address) -> std::expected<void, NetError> {
  if (!is_open()) {
    return std::unexpected(NetError::invalid_argument);
  }
  sockaddr_storage target{};
  socklen_t target_size = 0;
  to_sockaddr(address, target, target_size);
  if (::connect(descriptor_, reinterpret_cast<const sockaddr*>(&target), target_size) < 0) {
    return std::unexpected(from_error(last_error()));
  }
  return {};
}

auto Socket::send(std::span<const std::byte> buffer) -> std::expected<std::size_t, NetError> {
  if (!is_open() || buffer.empty()) {
    return std::unexpected(NetError::invalid_argument);
  }
  const ssize_t written = ::send(descriptor_, buffer.data(), buffer.size(), MSG_NOSIGNAL);
  if (written < 0) {
    return std::unexpected(from_error(last_error()));
  }
  return static_cast<std::size_t>(written);
}

auto Socket::receive(std::span<std::byte> buffer) -> std::expected<std::size_t, NetError> {
  if (!is_open() || buffer.empty()) {
    return std::unexpected(NetError::invalid_argument);
  }
  const ssize_t read = ::recv(descriptor_, buffer.data(), buffer.size(), 0);
  if (read < 0) {
    return std::unexpected(from_error(last_error()));
  }
  if (read == 0) {
    return std::unexpected(NetError::connection_closed);
  }
  return static_cast<std::size_t>(read);
}

auto Socket::send_to(const IpAddress& destination, std::span<const std::byte> buffer)
    -> std::expected<std::size_t, NetError> {
  if (!is_open()) {
    return std::unexpected(NetError::invalid_argument);
  }
  sockaddr_storage target{};
  socklen_t target_size = 0;
  to_sockaddr(destination, target, target_size);
  // MSG_NOSIGNAL: a connected datagram socket may see SIGPIPE on ICMP
  // errors; our sockets are unconnected, but be safe by construction.
  const ssize_t sent = ::sendto(descriptor_, buffer.data(), buffer.size(), MSG_NOSIGNAL,
                                reinterpret_cast<const sockaddr*>(&target), target_size);
  if (sent < 0) {
    return std::unexpected(from_error(last_error()));
  }
  return static_cast<std::size_t>(sent);
}

auto Socket::receive_from(std::span<std::byte> buffer) -> std::expected<Datagram, NetError> {
  if (!is_open() || buffer.empty()) {
    return std::unexpected(NetError::invalid_argument);
  }
  sockaddr_storage source{};
  std::memset(&source, 0, sizeof(source));
  socklen_t source_size = sizeof(source);
  const ssize_t read = ::recvfrom(descriptor_, buffer.data(), buffer.size(), 0,
                                  reinterpret_cast<sockaddr*>(&source), &source_size);
  if (read < 0) {
    return std::unexpected(from_error(last_error()));
  }
  // Zero-length datagrams are valid on UDP: report them, don't fold them
  // into the stream-only connection_closed signal.
  return Datagram{static_cast<std::size_t>(read), from_sockaddr(source)};
}

auto Socket::local_address() const -> std::expected<IpAddress, NetError> {
  sockaddr_storage address{};
  socklen_t length = sizeof(address);
  if (::getsockname(descriptor_, reinterpret_cast<sockaddr*>(&address), &length) < 0) {
    return std::unexpected(from_error(last_error()));
  }
  return from_sockaddr(address);
}

auto Socket::remote_address() const -> std::expected<IpAddress, NetError> {
  sockaddr_storage address{};
  socklen_t length = sizeof(address);
  if (::getpeername(descriptor_, reinterpret_cast<sockaddr*>(&address), &length) < 0) {
    return std::unexpected(from_error(last_error()));
  }
  return from_sockaddr(address);
}

void Socket::set_tcp_no_delay(bool enabled) noexcept {
  const int value = enabled ? 1 : 0;
  ::setsockopt(descriptor_, IPPROTO_TCP, TCP_NODELAY, &value, sizeof(value));
}

void Socket::shutdown() noexcept {
  if (is_open()) {
    ::shutdown(descriptor_, SHUT_RDWR);
  }
}

void Socket::close() noexcept {
  if (is_open()) {
    ::close(descriptor_);
    descriptor_ = kInvalid;
  }
}

auto Listener::create_tcp(IpAddress::Family family) -> std::expected<Listener, NetError> {
  const int af = family == IpAddress::Family::ipv4 ? AF_INET : AF_INET6;
  const NativeSocket descriptor = ::socket(af, SOCK_STREAM, 0);
  if (descriptor < 0) {
    return std::unexpected(from_error(last_error()));
  }
  if (!set_non_blocking(descriptor)) {
    const int error = last_error();
    ::close(descriptor);
    return std::unexpected(from_error(error));
  }
  // Dual-stack IPv6 listeners accept v4-mapped connections.
  if (af == AF_INET6) {
    const int disabled = 0;
    ::setsockopt(descriptor, IPPROTO_IPV6, IPV6_V6ONLY, &disabled, sizeof(disabled));
  }
  return Listener{descriptor};
}

auto Listener::listen(const IpAddress& address, int backlog) -> std::expected<void, NetError> {
  if (descriptor_ == kInvalid || backlog < 1) {
    return std::unexpected(NetError::invalid_argument);
  }
  sockaddr_storage bind_address{};
  socklen_t bind_size = 0;
  to_sockaddr(address, bind_address, bind_size);
  const int reuse = 1;
  ::setsockopt(descriptor_, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));
  if (::bind(descriptor_, reinterpret_cast<const sockaddr*>(&bind_address), bind_size) < 0) {
    return std::unexpected(from_error(last_error()));
  }
  if (::listen(descriptor_, backlog) < 0) {
    return std::unexpected(from_error(last_error()));
  }
  return {};
}

auto Listener::accept() -> std::expected<Socket, NetError> {
  const NativeSocket accepted = ::accept(descriptor_, nullptr, nullptr);
  if (accepted < 0) {
    return std::unexpected(from_error(last_error()));
  }
  if (!set_non_blocking(accepted)) {
    const int error = last_error();
    ::close(accepted);
    return std::unexpected(from_error(error));
  }
  return Socket{accepted};
}

auto Listener::local_address() const -> std::expected<IpAddress, NetError> {
  sockaddr_storage address{};
  socklen_t length = sizeof(address);
  if (::getsockname(descriptor_, reinterpret_cast<sockaddr*>(&address), &length) < 0) {
    return std::unexpected(from_error(last_error()));
  }
  return from_sockaddr(address);
}

void Listener::close() noexcept {
  if (descriptor_ != kInvalid) {
    ::close(descriptor_);
    descriptor_ = kInvalid;
  }
}

auto make_socket_pair() -> std::expected<std::pair<Socket, Socket>, NetError> {
  int descriptors[2] = {-1, -1};
  if (::socketpair(AF_UNIX, SOCK_STREAM, 0, descriptors) < 0) {
    return std::unexpected(from_error(last_error()));
  }
  if (!set_non_blocking(descriptors[0]) || !set_non_blocking(descriptors[1])) {
    const int error = last_error();
    ::close(descriptors[0]);
    ::close(descriptors[1]);
    return std::unexpected(from_error(error));
  }
  return std::pair<Socket, Socket>{Socket{descriptors[0]}, Socket{descriptors[1]}};
}

}  // namespace aw::net

#endif
