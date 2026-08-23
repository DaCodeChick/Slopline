// AppWarrior networking: readiness poller.
//
// poll(2) on POSIX, WSAPoll on Windows (conditional compilation). An
// application registers its socket descriptors with an interest mask and
// calls wait() from its event loop — readiness-driven, never busy-waiting.
// Interruption is swallowed: wait() returns an empty list and the caller
// simply waits again.

#pragma once

#include <chrono>
#include <cstdint>
#include <expected>
#include <map>
#include <vector>

#include "appwarrior/export.h"
#include "appwarrior/net/net_error.h"
#include "appwarrior/net/socket.h"

namespace aw::net {

enum class PollInterest : std::uint8_t {
  read = 1,
  write = 2,
  read_write = 3,
};

struct PollEvent {
  NativeSocket descriptor = 0;
  bool readable = false;
  bool writable = false;
  bool closed = false;  // hang-up, error, or invalid descriptor
};

class AW_API Poller {
 public:
  // Registers (or replaces) the interest for a descriptor.
  void add(NativeSocket descriptor, PollInterest interest) noexcept;
  void remove(NativeSocket descriptor) noexcept;
  [[nodiscard]] auto contains(NativeSocket descriptor) const noexcept -> bool;

  // Blocks up to `timeout` (negative = indefinitely) for readiness.
  // Returns an empty list on interruption.
  auto wait(std::chrono::milliseconds timeout)
      -> std::expected<std::vector<PollEvent>, NetError>;

 private:
  std::map<NativeSocket, short> interests_;  // descriptor -> poll(2) event mask
};

}  // namespace aw::net
