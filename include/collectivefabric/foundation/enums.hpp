#pragma once
// Collective Fabric - foundation: canonical enums and their bounded
// string / value conversions. All parsing is bounded and rejects unknown
// values rather than silently coercing.
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <array>

namespace collectivefabric {

// ---------------- Collective kind ---------------------------------------
enum class CollectiveKind : std::uint8_t {
  BARRIER,
  BROADCAST,
  REDUCE,
  ALL_REDUCE,
  ALL_GATHER,
  REDUCE_SCATTER,
};

using CollectiveKinds = std::array<CollectiveKind, 6>;
inline constexpr CollectiveKinds all_collective_kinds{CollectiveKind::BARRIER, CollectiveKind::BROADCAST,
  CollectiveKind::REDUCE, CollectiveKind::ALL_REDUCE, CollectiveKind::ALL_GATHER, CollectiveKind::REDUCE_SCATTER};

// ---------------- Reduction op -------------------------------------------
enum class ReductionOp : std::uint8_t { SUM, PRODUCT, MIN, MAX, NONE };
using ReductionOps = std::array<ReductionOp, 5>;
inline constexpr ReductionOps all_reduction_ops{ReductionOp::SUM, ReductionOp::PRODUCT, ReductionOp::MIN,
  ReductionOp::MAX, ReductionOp::NONE};

// ---------------- Datatype ----------------------------------------------
enum class Datatype : std::uint8_t { INT32, UINT32, INT64, FLOAT32, FLOAT64, BYTE };
using Datatypes = std::array<Datatype, 6>;
inline constexpr Datatypes all_datatypes{Datatype::INT32, Datatype::UINT32, Datatype::INT64, Datatype::FLOAT32,
  Datatype::FLOAT64, Datatype::BYTE};

// ---------------- Link class ---------------------------------------------
enum class LinkClass : std::uint8_t {
  INTRA_PROCESS, HOST_MEMORY, PCIE, NVLINK_CLASS, NETWORK, RDMA_CLASS, SHARED_MEMORY, UNKNOWN
};

// ---------------- Topology provenance ------------------------------------
enum class TopologyProvenance : std::uint8_t { MEASURED, REPORTED, DERIVED, SYNTHETIC, UNKNOWN };

// ---------------- Group lifecycle ----------------------------------------
enum class GroupLifecycle : std::uint8_t { CREATING, READY, DEGRADED, RECONFIGURING, FAILED, RETIRED };

// ---------------- Collective state ---------------------------------------
enum class CollectiveState : std::uint8_t {
  CREATED, PLANNED, RESERVED, DISPATCHED, RUNNING, COMPLETING, SUCCEEDED, FAILED, ABORTED, CANCELLED, STALE
};

// ---------------- Algorithm class ----------------------------------------
enum class Algorithm : std::uint8_t {
  DIRECT, RING, TREE, RECURSIVE_DOUBLING, REDUCE_BROADCAST, HIERARCHICAL, BACKEND_DEFAULT, UNKNOWN
};

// ---------------- Measurement provenance ---------------------------------
enum class MeasurementProvenance : std::uint8_t {
  THEORETICAL, BACKEND_REPORTED, MEASURED, SYNTHETIC, UNKNOWN
};

// ---------------- Measurement freshness ----------------------------------
enum class MeasurementFreshness : std::uint8_t { CURRENT, STALE, REVALIDATION_REQUIRED, UNKNOWN };

// ---------------- Health state -------------------------------------------
enum class HealthState : std::uint8_t { HEALTHY, DEGRADED, UNHEALTHY, UNKNOWN };

// ---------------- Failure class ------------------------------------------
enum class FailureClass : std::uint8_t {
  PARTICIPANT_FAILURE,
  WORKER_DEATH,
  TRANSPORT_FAILURE,
  BACKEND_FAILURE,
  DEVICE_FAILURE,
  CONTRIBUTION_MISSING,
  DUPLICATE_CONTRIBUTION,
  CONFLICTING_CONTRIBUTION,
  STALE_RANK,
  STALE_BOOT,
  STALE_EPOCH,
  STALE_GROUP_GENERATION,
  STALE_COLLECTIVE_GENERATION,
  STALE_DISPATCH,
  ABORT,
  RECONFIGURATION_REQUIRED,
  AMBIGUOUS_LOCAL_EXECUTION,
  VALIDATION,
  UNKNOWN,
};

// ---------------- Transport class ----------------------------------------
enum class TransportClass : std::uint8_t {
  SHARED_MEMORY, HOST_MEMORY, TCP, RDMA, NVLINK_CLASS, GPU_DIRECT, PCIE, HOST_STAGED_TCP, UNKNOWN
};

// ---------------- Replay / authority verdict -----------------------------
enum class AuthorityVerdict : std::uint8_t { CURRENT, STALE, FRESH, DUPLICATE, CONFLICT, UNKNOWN, INVALID };

// ---------------- Occupancy / concurrency intent -------------------------
enum class SchedulingIntent : std::uint8_t { DEFAULT, LOW_LATENCY, HIGH_THROUGHPUT, BACKGROUND };

// ---------------- Buffer placement ---------------------------------------
enum class BufferPlacement : std::uint8_t { HOST, DEVICE, UNKNOWN };

// ---------------- Serialization ------------------------------------------
// Bounded conversion helpers. Return nullopt on malformed input.
std::optional<CollectiveKind> collective_kind_from_string(std::string_view s);
std::string_view collective_kind_to_string(CollectiveKind k);

std::optional<ReductionOp> reduction_op_from_string(std::string_view s);
std::string_view reduction_op_to_string(ReductionOp r);

std::optional<Datatype> datatype_from_string(std::string_view s);
std::string_view datatype_to_string(Datatype d);

std::optional<LinkClass> link_class_from_string(std::string_view s);
std::string_view link_class_to_string(LinkClass l);

std::optional<TopologyProvenance> topology_provenance_from_string(std::string_view s);
std::string_view topology_provenance_to_string(TopologyProvenance p);

std::optional<GroupLifecycle> group_lifecycle_from_string(std::string_view s);
std::string_view group_lifecycle_to_string(GroupLifecycle l);

std::optional<CollectiveState> collective_state_from_string(std::string_view s);
std::string_view collective_state_to_string(CollectiveState s);

std::optional<Algorithm> algorithm_from_string(std::string_view s);
std::string_view algorithm_to_string(Algorithm a);

std::optional<MeasurementProvenance> measurement_provenance_from_string(std::string_view s);
std::string_view measurement_provenance_to_string(MeasurementProvenance p);

std::optional<MeasurementFreshness> measurement_freshness_from_string(std::string_view s);
std::string_view measurement_freshness_to_string(MeasurementFreshness f);

std::optional<HealthState> health_state_from_string(std::string_view s);
std::string_view health_state_to_string(HealthState h);

std::optional<FailureClass> failure_class_from_string(std::string_view s);
std::string_view failure_class_to_string(FailureClass f);

std::optional<TransportClass> transport_class_from_string(std::string_view s);
std::string_view transport_class_to_string(TransportClass t);

std::optional<AuthorityVerdict> authority_verdict_from_string(std::string_view s);
std::string_view authority_verdict_to_string(AuthorityVerdict a);

std::optional<SchedulingIntent> scheduling_intent_from_string(std::string_view s);
std::string_view scheduling_intent_to_string(SchedulingIntent s);

std::optional<BufferPlacement> buffer_placement_from_string(std::string_view s);
std::string_view buffer_placement_to_string(BufferPlacement p);

// Sized, bounded mapping helpers. Datatype byte size is exact; returns 0 for
// invalid/unknown. Reduction applicability is validated explicitly.
std::size_t datatype_size_bytes(Datatype d) noexcept;
bool reduction_is_applicable(CollectiveKind kind, ReductionOp op) noexcept;
bool datatype_is_supported(CollectiveKind kind, Datatype d) noexcept;

} // namespace collectivefabric
