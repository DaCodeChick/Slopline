#include "appwarrior/net/poller.h"

#include <cerrno>
#include <chrono>
#include <cstddef>
#include <vector>

#include <poll.h>

namespace aw::net {

namespace {

auto to_poll_mask(PollInterest interest) noexcept -> short {
  short mask = 0;
  if ((static_cast<std::uint8_t>(interest) & static_cast<std::uint8_t>(PollInterest::read)) != 0) {
    mask |= POLLIN;
  }
  if ((static_cast<std::uint8_t>(interest) & static_cast<std::uint8_t>(PollInterest::write)) != 0) {
    mask |= POLLOUT;
  }
  return mask;
}

}  // namespace

void Poller::add(int descriptor, PollInterest interest) noexcept {
  if (descriptor < 0) {
    return;
  }
  interests_[descriptor] = to_poll_mask(interest);
}

void Poller::remove(int descriptor) noexcept {
  interests_.erase(descriptor);
}

auto Poller::contains(int descriptor) const noexcept -> bool {
  return interests_.contains(descriptor);
}

auto Poller::wait(std::chrono::milliseconds timeout)
    -> std::expected<std::vector<PollEvent>, NetError> {
  std::vector<pollfd> descriptors;
  descriptors.reserve(interests_.size());
  for (const auto& [descriptor, events] : interests_) {
    descriptors.push_back(pollfd{descriptor, events, 0});
  }
  if (descriptors.empty()) {
    // Nothing registered: sleep the requested time so callers may still
    // use wait() as a loop heartbeat.
    poll(nullptr, 0, static_cast<int>(timeout.count()));
    return std::vector<PollEvent>{};
  }

  const int ready =
      ::poll(descriptors.data(), descriptors.size(), static_cast<int>(timeout.count()));
  if (ready < 0) {
    if (errno == EINTR) {
      return std::vector<PollEvent>{};
    }
    return std::unexpected(NetError::system);
  }

  std::vector<PollEvent> events;
  for (const pollfd& descriptor : descriptors) {
    if (descriptor.revents == 0) {
      continue;
    }
    PollEvent event;
    event.descriptor = descriptor.fd;
    event.readable = (descriptor.revents & POLLIN) != 0;
    event.writable = (descriptor.revents & POLLOUT) != 0;
    event.closed = (descriptor.revents & (POLLHUP | POLLERR | POLLNVAL)) != 0;
    events.push_back(event);
  }
  return events;
}

}  // namespace aw::net
