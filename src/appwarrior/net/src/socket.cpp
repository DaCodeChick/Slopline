#include "appwarrior/net/socket.h"

#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstring>

#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/socket.h>
#include <unistd.h>

namespace aw::net {

namespace {

auto from_errno(int error) noexcept -> NetError {
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
      return NetError::invalid_argument;
    default:
      return NetError::system;
  }
}

auto set_non_blocking(int descriptor) -> bool {
  const int flags = ::fcntl(descriptor, F_GETFL, 0);
  if (flags < 0) {
    return false;
  }
  return ::fcntl(descriptor, F_SETFL, flags | O_NONBLOCK) >= 0;
}

auto to_sockaddr(const IpAddress& address, sockaddr_in& out) noexcept -> void {
  std::memset(&out, 0, sizeof(out));
  out.sin_family = AF_INET;
  out.sin_addr.s_addr = htonl(address.network_address());
  out.sin_port = htons(address.port());
}

auto from_sockaddr(const sockaddr_in& in) -> IpAddress {
  const std::uint32_t network = in.sin_addr.s_addr;
  return IpAddress{static_cast<std::uint8_t>(network & 0xFFU),
                   static_cast<std::uint8_t>((network >> 8) & 0xFFU),
                   static_cast<std::uint8_t>((network >> 16) & 0xFFU),
                   static_cast<std::uint8_t>((network >> 24) & 0xFFU), ntohs(in.sin_port)};
}

}  // namespace

auto Socket::create_tcp() -> std::expected<Socket, NetError> {
  const int descriptor = ::socket(AF_INET, SOCK_STREAM, 0);
  if (descriptor < 0) {
    return std::unexpected(from_errno(errno));
  }
  if (!set_non_blocking(descriptor)) {
    const int error = errno;
    ::close(descriptor);
    return std::unexpected(from_errno(error));
  }
  return Socket{descriptor};
}

auto Socket::connect(const IpAddress& address) -> std::expected<void, NetError> {
  if (!is_open()) {
    return std::unexpected(NetError::invalid_argument);
  }
  sockaddr_in target{};
  to_sockaddr(address, target);
  if (::connect(descriptor_, reinterpret_cast<const sockaddr*>(&target), sizeof(target)) < 0) {
    return std::unexpected(from_errno(errno));
  }
  return {};
}

auto Socket::send(std::span<const std::byte> buffer) -> std::expected<std::size_t, NetError> {
  if (!is_open() || buffer.empty()) {
    return std::unexpected(NetError::invalid_argument);
  }
  const ssize_t written = ::send(descriptor_, buffer.data(), buffer.size(), MSG_NOSIGNAL);
  if (written < 0) {
    return std::unexpected(from_errno(errno));
  }
  return static_cast<std::size_t>(written);
}

auto Socket::receive(std::span<std::byte> buffer) -> std::expected<std::size_t, NetError> {
  if (!is_open() || buffer.empty()) {
    return std::unexpected(NetError::invalid_argument);
  }
  const ssize_t read = ::recv(descriptor_, buffer.data(), buffer.size(), 0);
  if (read < 0) {
    return std::unexpected(from_errno(errno));
  }
  if (read == 0) {
    return std::unexpected(NetError::connection_closed);
  }
  return static_cast<std::size_t>(read);
}

auto Socket::local_address() const -> std::expected<IpAddress, NetError> {
  sockaddr_in address{};
  socklen_t length = sizeof(address);
  if (::getsockname(descriptor_, reinterpret_cast<sockaddr*>(&address), &length) < 0) {
    return std::unexpected(from_errno(errno));
  }
  return from_sockaddr(address);
}

auto Socket::remote_address() const -> std::expected<IpAddress, NetError> {
  sockaddr_in address{};
  socklen_t length = sizeof(address);
  if (::getpeername(descriptor_, reinterpret_cast<sockaddr*>(&address), &length) < 0) {
    return std::unexpected(from_errno(errno));
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
    descriptor_ = -1;
  }
}

auto Listener::create_tcp() -> std::expected<Listener, NetError> {
  const int descriptor = ::socket(AF_INET, SOCK_STREAM, 0);
  if (descriptor < 0) {
    return std::unexpected(from_errno(errno));
  }
  if (!set_non_blocking(descriptor)) {
    const int error = errno;
    ::close(descriptor);
    return std::unexpected(from_errno(error));
  }
  return Listener{descriptor};
}

auto Listener::listen(const IpAddress& address, int backlog) -> std::expected<void, NetError> {
  if (descriptor_ < 0 || backlog < 1) {
    return std::unexpected(NetError::invalid_argument);
  }
  sockaddr_in bind_address{};
  to_sockaddr(address, bind_address);
  const int reuse = 1;
  ::setsockopt(descriptor_, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));
  if (::bind(descriptor_, reinterpret_cast<const sockaddr*>(&bind_address), sizeof(bind_address)) < 0) {
    return std::unexpected(from_errno(errno));
  }
  if (::listen(descriptor_, backlog) < 0) {
    return std::unexpected(from_errno(errno));
  }
  return {};
}

auto Listener::accept() -> std::expected<Socket, NetError> {
  const int accepted = ::accept(descriptor_, nullptr, nullptr);
  if (accepted < 0) {
    return std::unexpected(from_errno(errno));
  }
  if (!set_non_blocking(accepted)) {
    const int error = errno;
    ::close(accepted);
    return std::unexpected(from_errno(error));
  }
  return Socket{accepted};
}

auto Listener::local_address() const -> std::expected<IpAddress, NetError> {
  sockaddr_in address{};
  socklen_t length = sizeof(address);
  if (::getsockname(descriptor_, reinterpret_cast<sockaddr*>(&address), &length) < 0) {
    return std::unexpected(from_errno(errno));
  }
  return from_sockaddr(address);
}

void Listener::close() noexcept {
  if (descriptor_ >= 0) {
    ::close(descriptor_);
    descriptor_ = -1;
  }
}

auto make_socket_pair() -> std::expected<std::pair<Socket, Socket>, NetError> {
  int descriptors[2] = {-1, -1};
  if (::socketpair(AF_UNIX, SOCK_STREAM, 0, descriptors) < 0) {
    return std::unexpected(from_errno(errno));
  }
  if (!set_non_blocking(descriptors[0]) || !set_non_blocking(descriptors[1])) {
    const int error = errno;
    ::close(descriptors[0]);
    ::close(descriptors[1]);
    return std::unexpected(from_errno(error));
  }
  return std::pair<Socket, Socket>{Socket{descriptors[0]}, Socket{descriptors[1]}};
}

}  // namespace aw::net
