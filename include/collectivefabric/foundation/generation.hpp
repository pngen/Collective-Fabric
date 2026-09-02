#pragma once
// Collective Fabric - foundation: strong generation types.
// Generations are incarnation-scoped. A higher numeric generation belonging
// to an old incarnation must never fence a fresh incarnation; authority is
// always evaluated against the current (incarnation, generation) ground
// truth, never from the numeric generation alone.
#include <cstdint>
#include <compare>
#include <functional>

namespace collectivefabric {

template <typename Tag>
class Generation {
public:
  using value_type = std::uint64_t;

  constexpr Generation() = default;
  constexpr explicit Generation(std::uint64_t v) noexcept : value_(v) {}

  constexpr std::uint64_t value() const noexcept { return value_; }
  constexpr bool is_null() const noexcept { return value_ == 0; }
  constexpr explicit operator bool() const noexcept { return !is_null(); }
  constexpr bool is_valid() const noexcept { return true; }

  constexpr Generation next() const noexcept {
    if (value_ == 0) return Generation(std::uint64_t(1));
    if (value_ == UINT64_MAX) return *this;
    return Generation(value_ + 1);
  }

  friend constexpr bool operator==(const Generation&, const Generation&) = default;
  friend constexpr auto operator<=>(const Generation&, const Generation&) = default;

private:
  std::uint64_t value_{0};
};

template <typename Tag>
constexpr bool generation_is_valid(const Generation<Tag>& g) noexcept { return g.is_valid(); }

template <typename Tag>
constexpr bool generation_is_null(const Generation<Tag>& g) noexcept { return g.is_null(); }

template <typename Tag>
constexpr std::partial_ordering compare_generations(const Generation<Tag>& a, const Generation<Tag>& b) noexcept {
  return a <=> b;
}

template <typename Tag>
constexpr bool generation_is_newer(const Generation<Tag>& a, const Generation<Tag>& b) noexcept {
  return compare_generations(a, b) == std::partial_ordering::greater;
}

template <typename Tag>
constexpr bool generation_is_older(const Generation<Tag>& a, const Generation<Tag>& b) noexcept {
  return compare_generations(a, b) == std::partial_ordering::less;
}

template <typename Tag>
constexpr Generation<Tag> make_generation(std::uint64_t v) noexcept {
  return Generation<Tag>(v);
}

template <typename Tag>
struct std::hash<collectivefabric::Generation<Tag>> {
  std::size_t operator()(const collectivefabric::Generation<Tag>& g) const noexcept {
    return std::hash<std::uint64_t>{}(g.value());
  }
};

#define COLLECTIVEFABRIC_GEN(TAG) \
  struct TAG##Tag {}; \
  using TAG = ::collectivefabric::Generation<TAG##Tag>;

COLLECTIVEFABRIC_GEN(CoordinatorEpoch)
COLLECTIVEFABRIC_GEN(GroupGeneration)
COLLECTIVEFABRIC_GEN(MembershipGeneration)
COLLECTIVEFABRIC_GEN(TopologyGeneration)
COLLECTIVEFABRIC_GEN(BackendGeneration)
COLLECTIVEFABRIC_GEN(CollectiveGeneration)
COLLECTIVEFABRIC_GEN(AttemptGeneration)
COLLECTIVEFABRIC_GEN(DispatchGeneration)
COLLECTIVEFABRIC_GEN(BufferGeneration)
COLLECTIVEFABRIC_GEN(HealthGeneration)
COLLECTIVEFABRIC_GEN(MeasurementGeneration)
COLLECTIVEFABRIC_GEN(PolicyGeneration)
COLLECTIVEFABRIC_GEN(SourceGeneration)

#undef COLLECTIVEFABRIC_GEN

} // namespace collectivefabric
