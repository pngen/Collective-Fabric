#pragma once
// Collective Fabric - monotonic and wall-clock time in nanoseconds.
#include <cstdint>
#include <chrono>

namespace collectivefabric {
namespace clock {

inline std::uint64_t wall_ns() noexcept {
  return static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(
      std::chrono::system_clock::now().time_since_epoch()).count());
}

inline std::uint64_t steady_ns() noexcept {
  return static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(
      std::chrono::steady_clock::now().time_since_epoch()).count());
}

// Microseconds (used by many measurements).
inline std::uint64_t wall_us() noexcept { return wall_ns() / 1000; }
inline std::uint64_t steady_us() noexcept { return steady_ns() / 1000; }

} // namespace clock
} // namespace collectivefabric
