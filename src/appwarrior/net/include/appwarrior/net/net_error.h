// AppWarrior networking: error codes.
//
// Transport operations return std::expected<T, NetError>; they never throw
// (project error-handling policy). `system` carries an unspecified OS
// failure — the platform backends translate to portable values where
// practical; the raw errno remains inspectable via the C library.

#pragma once

namespace aw::net {

enum class NetError {
  would_block,         // operation would block (EAGAIN/EWOULDBLOCK)
  interrupted,         // EINTR — retry is safe
  connection_closed,   // peer closed (zero-length receive) or reset (ECONNRESET/EPIPE)
  connection_refused,  // ECONNREFUSED during connect
  address_in_use,      // EADDRINUSE during bind/listen
  invalid_argument,    // malformed address, port, or buffer arguments
  system,              // any other OS failure
};

}  // namespace aw::net
