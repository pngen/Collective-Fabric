#pragma once
// Collective Fabric - checked arithmetic. Detect overflow in element-count *
// datatype-size, rank-count * contribution-size, offsets, serialization
// lengths, and aggregation totals. No silent wraparound.
#include <cstdint>
#include <optional>
#include <limits>

namespace collectivefabric {

constexpr std::optional<std::uint64_t> checked_mul(std::uint64_t a, std::uint64_t b) noexcept {
  if (a == 0 || b == 0) return std::uint64_t(0);
  if (a > std::numeric_limits<std::uint64_t>::max() / b) return std::nullopt;
  return a * b;
}

constexpr std::optional<std::uint64_t> checked_add(std::uint64_t a, std::uint64_t b) noexcept {
  if (a > std::numeric_limits<std::uint64_t>::max() - b) return std::nullopt;
  return a + b;
}

constexpr bool would_overflow_mul(std::uint64_t a, std::uint64_t b) noexcept { return !checked_mul(a, b).has_value(); }
constexpr bool would_overflow_add(std::uint64_t a, std::uint64_t b) noexcept { return !checked_add(a, b).has_value(); }

} // namespace collectivefabric
