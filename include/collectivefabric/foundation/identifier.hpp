#pragma once
// Collective Fabric - foundation: strong, non-interchangeable identities.
#include <cstdint>
#include <compare>
#include <functional>
#include <iosfwd>

namespace collectivefabric {

// A strong, non-interchangeable identifier. Each distinct identity is a
// distinct type; a value of one identity cannot be silently assigned to
// another. Value zero is reserved as "null"/unset.
template <typename Tag, typename UInt = std::uint64_t>
class StrongId {
public:
  using value_type = UInt;

  constexpr StrongId() = default;
  constexpr explicit StrongId(UInt v) noexcept : value_(v) {}

  constexpr UInt raw() const noexcept { return value_; }
  constexpr bool is_null() const noexcept { return value_ == UInt(0); }
  constexpr explicit operator bool() const noexcept { return !is_null(); }

  friend constexpr bool operator==(const StrongId&, const StrongId&) = default;
  friend constexpr auto operator<=>(const StrongId&, const StrongId&) = default;

private:
  UInt value_{0};
};

#ifndef __CUDACC__
template <typename Tag, typename UInt>
struct std::hash<collectivefabric::StrongId<Tag, UInt>> {
  std::size_t operator()(const collectivefabric::StrongId<Tag, UInt>& v) const noexcept {
    return std::hash<UInt>{}(v.raw());
  }
};
#endif

template <typename Tag, typename UInt>
std::ostream& operator<<(std::ostream& os, const ::collectivefabric::StrongId<Tag, UInt>& v);

template <typename Tag, typename UInt>
std::ostream& operator<<(std::ostream& os, const ::collectivefabric::StrongId<Tag, UInt>& v) {
  return os << v.raw();
}

#define COLLECTIVEFABRIC_DECLARE_ID(NAME) \
  struct NAME##Tag {}; \
  using NAME = ::collectivefabric::StrongId<NAME##Tag>;

// --- Identity model -------------------------------------------------------
COLLECTIVEFABRIC_DECLARE_ID(CollectiveId)
COLLECTIVEFABRIC_DECLARE_ID(CollectiveGroupId)
COLLECTIVEFABRIC_DECLARE_ID(ParticipantId)
COLLECTIVEFABRIC_DECLARE_ID(RankId)
COLLECTIVEFABRIC_DECLARE_ID(WorkerId)
COLLECTIVEFABRIC_DECLARE_ID(WorkerBootId)
COLLECTIVEFABRIC_DECLARE_ID(NodeId)
COLLECTIVEFABRIC_DECLARE_ID(DeviceId)
COLLECTIVEFABRIC_DECLARE_ID(BackendId)
COLLECTIVEFABRIC_DECLARE_ID(TransportId)
COLLECTIVEFABRIC_DECLARE_ID(AlgorithmId)
COLLECTIVEFABRIC_DECLARE_ID(BufferId)
COLLECTIVEFABRIC_DECLARE_ID(ExecutionId)
COLLECTIVEFABRIC_DECLARE_ID(AttemptId)
COLLECTIVEFABRIC_DECLARE_ID(DispatchId)
COLLECTIVEFABRIC_DECLARE_ID(ObservationId)
COLLECTIVEFABRIC_DECLARE_ID(MeasurementId)
COLLECTIVEFABRIC_DECLARE_ID(ReservationId)
COLLECTIVEFABRIC_DECLARE_ID(SourceId)

#undef COLLECTIVEFABRIC_DECLARE_ID

} // namespace collectivefabric
