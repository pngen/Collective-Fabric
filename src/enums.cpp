#include "collectivefabric/foundation/enums.hpp"

namespace collectivefabric {

std::optional<CollectiveKind> collective_kind_from_string(std::string_view s) {
  if (s == "BARRIER") return CollectiveKind::BARRIER;
  if (s == "BROADCAST") return CollectiveKind::BROADCAST;
  if (s == "REDUCE") return CollectiveKind::REDUCE;
  if (s == "ALL_REDUCE") return CollectiveKind::ALL_REDUCE;
  if (s == "ALL_GATHER") return CollectiveKind::ALL_GATHER;
  if (s == "REDUCE_SCATTER") return CollectiveKind::REDUCE_SCATTER;
  return std::nullopt;
}
std::string_view collective_kind_to_string(CollectiveKind k) {
  switch (k) {
    case CollectiveKind::BARRIER: return "BARRIER";
    case CollectiveKind::BROADCAST: return "BROADCAST";
    case CollectiveKind::REDUCE: return "REDUCE";
    case CollectiveKind::ALL_REDUCE: return "ALL_REDUCE";
    case CollectiveKind::ALL_GATHER: return "ALL_GATHER";
    case CollectiveKind::REDUCE_SCATTER: return "REDUCE_SCATTER";
  }
  return "UNKNOWN";
}

std::optional<ReductionOp> reduction_op_from_string(std::string_view s) {
  if (s == "SUM") return ReductionOp::SUM;
  if (s == "PRODUCT") return ReductionOp::PRODUCT;
  if (s == "MIN") return ReductionOp::MIN;
  if (s == "MAX") return ReductionOp::MAX;
  if (s == "NONE") return ReductionOp::NONE;
  return std::nullopt;
}
std::string_view reduction_op_to_string(ReductionOp r) {
  switch (r) {
    case ReductionOp::SUM: return "SUM";
    case ReductionOp::PRODUCT: return "PRODUCT";
    case ReductionOp::MIN: return "MIN";
    case ReductionOp::MAX: return "MAX";
    case ReductionOp::NONE: return "NONE";
  }
  return "UNKNOWN";
}

std::optional<Datatype> datatype_from_string(std::string_view s) {
  if (s == "INT32") return Datatype::INT32;
  if (s == "UINT32") return Datatype::UINT32;
  if (s == "INT64") return Datatype::INT64;
  if (s == "FLOAT32") return Datatype::FLOAT32;
  if (s == "FLOAT64") return Datatype::FLOAT64;
  if (s == "BYTE") return Datatype::BYTE;
  return std::nullopt;
}
std::string_view datatype_to_string(Datatype d) {
  switch (d) {
    case Datatype::INT32: return "INT32";
    case Datatype::UINT32: return "UINT32";
    case Datatype::INT64: return "INT64";
    case Datatype::FLOAT32: return "FLOAT32";
    case Datatype::FLOAT64: return "FLOAT64";
    case Datatype::BYTE: return "BYTE";
  }
  return "UNKNOWN";
}

std::optional<LinkClass> link_class_from_string(std::string_view s) {
  if (s == "INTRA_PROCESS") return LinkClass::INTRA_PROCESS;
  if (s == "HOST_MEMORY") return LinkClass::HOST_MEMORY;
  if (s == "PCIE") return LinkClass::PCIE;
  if (s == "NVLINK_CLASS") return LinkClass::NVLINK_CLASS;
  if (s == "NETWORK") return LinkClass::NETWORK;
  if (s == "RDMA_CLASS") return LinkClass::RDMA_CLASS;
  if (s == "SHARED_MEMORY") return LinkClass::SHARED_MEMORY;
  if (s == "UNKNOWN") return LinkClass::UNKNOWN;
  return std::nullopt;
}
std::string_view link_class_to_string(LinkClass l) {
  switch (l) {
    case LinkClass::INTRA_PROCESS: return "INTRA_PROCESS";
    case LinkClass::HOST_MEMORY: return "HOST_MEMORY";
    case LinkClass::PCIE: return "PCIE";
    case LinkClass::NVLINK_CLASS: return "NVLINK_CLASS";
    case LinkClass::NETWORK: return "NETWORK";
    case LinkClass::RDMA_CLASS: return "RDMA_CLASS";
    case LinkClass::SHARED_MEMORY: return "SHARED_MEMORY";
    case LinkClass::UNKNOWN: return "UNKNOWN";
  }
  return "UNKNOWN";
}

std::optional<TopologyProvenance> topology_provenance_from_string(std::string_view s) {
  if (s == "MEASURED") return TopologyProvenance::MEASURED;
  if (s == "REPORTED") return TopologyProvenance::REPORTED;
  if (s == "DERIVED") return TopologyProvenance::DERIVED;
  if (s == "SYNTHETIC") return TopologyProvenance::SYNTHETIC;
  if (s == "UNKNOWN") return TopologyProvenance::UNKNOWN;
  return std::nullopt;
}
std::string_view topology_provenance_to_string(TopologyProvenance p) {
  switch (p) {
    case TopologyProvenance::MEASURED: return "MEASURED";
    case TopologyProvenance::REPORTED: return "REPORTED";
    case TopologyProvenance::DERIVED: return "DERIVED";
    case TopologyProvenance::SYNTHETIC: return "SYNTHETIC";
    case TopologyProvenance::UNKNOWN: return "UNKNOWN";
  }
  return "UNKNOWN";
}

std::optional<GroupLifecycle> group_lifecycle_from_string(std::string_view s) {
  if (s == "CREATING") return GroupLifecycle::CREATING;
  if (s == "READY") return GroupLifecycle::READY;
  if (s == "DEGRADED") return GroupLifecycle::DEGRADED;
  if (s == "RECONFIGURING") return GroupLifecycle::RECONFIGURING;
  if (s == "FAILED") return GroupLifecycle::FAILED;
  if (s == "RETIRED") return GroupLifecycle::RETIRED;
  return std::nullopt;
}
std::string_view group_lifecycle_to_string(GroupLifecycle l) {
  switch (l) {
    case GroupLifecycle::CREATING: return "CREATING";
    case GroupLifecycle::READY: return "READY";
    case GroupLifecycle::DEGRADED: return "DEGRADED";
    case GroupLifecycle::RECONFIGURING: return "RECONFIGURING";
    case GroupLifecycle::FAILED: return "FAILED";
    case GroupLifecycle::RETIRED: return "RETIRED";
  }
  return "UNKNOWN";
}

std::optional<CollectiveState> collective_state_from_string(std::string_view s) {
  if (s == "CREATED") return CollectiveState::CREATED;
  if (s == "PLANNED") return CollectiveState::PLANNED;
  if (s == "RESERVED") return CollectiveState::RESERVED;
  if (s == "DISPATCHED") return CollectiveState::DISPATCHED;
  if (s == "RUNNING") return CollectiveState::RUNNING;
  if (s == "COMPLETING") return CollectiveState::COMPLETING;
  if (s == "SUCCEEDED") return CollectiveState::SUCCEEDED;
  if (s == "FAILED") return CollectiveState::FAILED;
  if (s == "ABORTED") return CollectiveState::ABORTED;
  if (s == "CANCELLED") return CollectiveState::CANCELLED;
  if (s == "STALE") return CollectiveState::STALE;
  return std::nullopt;
}
std::string_view collective_state_to_string(CollectiveState s) {
  switch (s) {
    case CollectiveState::CREATED: return "CREATED";
    case CollectiveState::PLANNED: return "PLANNED";
    case CollectiveState::RESERVED: return "RESERVED";
    case CollectiveState::DISPATCHED: return "DISPATCHED";
    case CollectiveState::RUNNING: return "RUNNING";
    case CollectiveState::COMPLETING: return "COMPLETING";
    case CollectiveState::SUCCEEDED: return "SUCCEEDED";
    case CollectiveState::FAILED: return "FAILED";
    case CollectiveState::ABORTED: return "ABORTED";
    case CollectiveState::CANCELLED: return "CANCELLED";
    case CollectiveState::STALE: return "STALE";
  }
  return "UNKNOWN";
}

std::optional<Algorithm> algorithm_from_string(std::string_view s) {
  if (s == "DIRECT") return Algorithm::DIRECT;
  if (s == "RING") return Algorithm::RING;
  if (s == "TREE") return Algorithm::TREE;
  if (s == "RECURSIVE_DOUBLING") return Algorithm::RECURSIVE_DOUBLING;
  if (s == "REDUCE_BROADCAST") return Algorithm::REDUCE_BROADCAST;
  if (s == "HIERARCHICAL") return Algorithm::HIERARCHICAL;
  if (s == "BACKEND_DEFAULT") return Algorithm::BACKEND_DEFAULT;
  if (s == "UNKNOWN") return Algorithm::UNKNOWN;
  return std::nullopt;
}
std::string_view algorithm_to_string(Algorithm a) {
  switch (a) {
    case Algorithm::DIRECT: return "DIRECT";
    case Algorithm::RING: return "RING";
    case Algorithm::TREE: return "TREE";
    case Algorithm::RECURSIVE_DOUBLING: return "RECURSIVE_DOUBLING";
    case Algorithm::REDUCE_BROADCAST: return "REDUCE_BROADCAST";
    case Algorithm::HIERARCHICAL: return "HIERARCHICAL";
    case Algorithm::BACKEND_DEFAULT: return "BACKEND_DEFAULT";
    case Algorithm::UNKNOWN: return "UNKNOWN";
  }
  return "UNKNOWN";
}

std::optional<MeasurementProvenance> measurement_provenance_from_string(std::string_view s) {
  if (s == "THEORETICAL") return MeasurementProvenance::THEORETICAL;
  if (s == "BACKEND_REPORTED") return MeasurementProvenance::BACKEND_REPORTED;
  if (s == "MEASURED") return MeasurementProvenance::MEASURED;
  if (s == "SYNTHETIC") return MeasurementProvenance::SYNTHETIC;
  if (s == "UNKNOWN") return MeasurementProvenance::UNKNOWN;
  return std::nullopt;
}
std::string_view measurement_provenance_to_string(MeasurementProvenance p) {
  switch (p) {
    case MeasurementProvenance::THEORETICAL: return "THEORETICAL";
    case MeasurementProvenance::BACKEND_REPORTED: return "BACKEND_REPORTED";
    case MeasurementProvenance::MEASURED: return "MEASURED";
    case MeasurementProvenance::SYNTHETIC: return "SYNTHETIC";
    case MeasurementProvenance::UNKNOWN: return "UNKNOWN";
  }
  return "UNKNOWN";
}

std::optional<MeasurementFreshness> measurement_freshness_from_string(std::string_view s) {
  if (s == "CURRENT") return MeasurementFreshness::CURRENT;
  if (s == "STALE") return MeasurementFreshness::STALE;
  if (s == "REVALIDATION_REQUIRED") return MeasurementFreshness::REVALIDATION_REQUIRED;
  if (s == "UNKNOWN") return MeasurementFreshness::UNKNOWN;
  return std::nullopt;
}
std::string_view measurement_freshness_to_string(MeasurementFreshness f) {
  switch (f) {
    case MeasurementFreshness::CURRENT: return "CURRENT";
    case MeasurementFreshness::STALE: return "STALE";
    case MeasurementFreshness::REVALIDATION_REQUIRED: return "REVALIDATION_REQUIRED";
    case MeasurementFreshness::UNKNOWN: return "UNKNOWN";
  }
  return "UNKNOWN";
}

std::optional<HealthState> health_state_from_string(std::string_view s) {
  if (s == "HEALTHY") return HealthState::HEALTHY;
  if (s == "DEGRADED") return HealthState::DEGRADED;
  if (s == "UNHEALTHY") return HealthState::UNHEALTHY;
  if (s == "UNKNOWN") return HealthState::UNKNOWN;
  return std::nullopt;
}
std::string_view health_state_to_string(HealthState h) {
  switch (h) {
    case HealthState::HEALTHY: return "HEALTHY";
    case HealthState::DEGRADED: return "DEGRADED";
    case HealthState::UNHEALTHY: return "UNHEALTHY";
    case HealthState::UNKNOWN: return "UNKNOWN";
  }
  return "UNKNOWN";
}

std::optional<FailureClass> failure_class_from_string(std::string_view s) {
  if (s == "PARTICIPANT_FAILURE") return FailureClass::PARTICIPANT_FAILURE;
  if (s == "WORKER_DEATH") return FailureClass::WORKER_DEATH;
  if (s == "TRANSPORT_FAILURE") return FailureClass::TRANSPORT_FAILURE;
  if (s == "BACKEND_FAILURE") return FailureClass::BACKEND_FAILURE;
  if (s == "DEVICE_FAILURE") return FailureClass::DEVICE_FAILURE;
  if (s == "CONTRIBUTION_MISSING") return FailureClass::CONTRIBUTION_MISSING;
  if (s == "DUPLICATE_CONTRIBUTION") return FailureClass::DUPLICATE_CONTRIBUTION;
  if (s == "CONFLICTING_CONTRIBUTION") return FailureClass::CONFLICTING_CONTRIBUTION;
  if (s == "STALE_RANK") return FailureClass::STALE_RANK;
  if (s == "STALE_BOOT") return FailureClass::STALE_BOOT;
  if (s == "STALE_EPOCH") return FailureClass::STALE_EPOCH;
  if (s == "STALE_GROUP_GENERATION") return FailureClass::STALE_GROUP_GENERATION;
  if (s == "STALE_COLLECTIVE_GENERATION") return FailureClass::STALE_COLLECTIVE_GENERATION;
  if (s == "STALE_DISPATCH") return FailureClass::STALE_DISPATCH;
  if (s == "ABORT") return FailureClass::ABORT;
  if (s == "RECONFIGURATION_REQUIRED") return FailureClass::RECONFIGURATION_REQUIRED;
  if (s == "AMBIGUOUS_LOCAL_EXECUTION") return FailureClass::AMBIGUOUS_LOCAL_EXECUTION;
  if (s == "VALIDATION") return FailureClass::VALIDATION;
  if (s == "UNKNOWN") return FailureClass::UNKNOWN;
  return std::nullopt;
}
std::string_view failure_class_to_string(FailureClass f) {
  switch (f) {
    case FailureClass::PARTICIPANT_FAILURE: return "PARTICIPANT_FAILURE";
    case FailureClass::WORKER_DEATH: return "WORKER_DEATH";
    case FailureClass::TRANSPORT_FAILURE: return "TRANSPORT_FAILURE";
    case FailureClass::BACKEND_FAILURE: return "BACKEND_FAILURE";
    case FailureClass::DEVICE_FAILURE: return "DEVICE_FAILURE";
    case FailureClass::CONTRIBUTION_MISSING: return "CONTRIBUTION_MISSING";
    case FailureClass::DUPLICATE_CONTRIBUTION: return "DUPLICATE_CONTRIBUTION";
    case FailureClass::CONFLICTING_CONTRIBUTION: return "CONFLICTING_CONTRIBUTION";
    case FailureClass::STALE_RANK: return "STALE_RANK";
    case FailureClass::STALE_BOOT: return "STALE_BOOT";
    case FailureClass::STALE_EPOCH: return "STALE_EPOCH";
    case FailureClass::STALE_GROUP_GENERATION: return "STALE_GROUP_GENERATION";
    case FailureClass::STALE_COLLECTIVE_GENERATION: return "STALE_COLLECTIVE_GENERATION";
    case FailureClass::STALE_DISPATCH: return "STALE_DISPATCH";
    case FailureClass::ABORT: return "ABORT";
    case FailureClass::RECONFIGURATION_REQUIRED: return "RECONFIGURATION_REQUIRED";
    case FailureClass::AMBIGUOUS_LOCAL_EXECUTION: return "AMBIGUOUS_LOCAL_EXECUTION";
    case FailureClass::VALIDATION: return "VALIDATION";
    case FailureClass::UNKNOWN: return "UNKNOWN";
  }
  return "UNKNOWN";
}

std::optional<TransportClass> transport_class_from_string(std::string_view s) {
  if (s == "SHARED_MEMORY") return TransportClass::SHARED_MEMORY;
  if (s == "HOST_MEMORY") return TransportClass::HOST_MEMORY;
  if (s == "TCP") return TransportClass::TCP;
  if (s == "RDMA") return TransportClass::RDMA;
  if (s == "NVLINK_CLASS") return TransportClass::NVLINK_CLASS;
  if (s == "GPU_DIRECT") return TransportClass::GPU_DIRECT;
  if (s == "PCIE") return TransportClass::PCIE;
  if (s == "HOST_STAGED_TCP") return TransportClass::HOST_STAGED_TCP;
  if (s == "UNKNOWN") return TransportClass::UNKNOWN;
  return std::nullopt;
}
std::string_view transport_class_to_string(TransportClass t) {
  switch (t) {
    case TransportClass::SHARED_MEMORY: return "SHARED_MEMORY";
    case TransportClass::HOST_MEMORY: return "HOST_MEMORY";
    case TransportClass::TCP: return "TCP";
    case TransportClass::RDMA: return "RDMA";
    case TransportClass::NVLINK_CLASS: return "NVLINK_CLASS";
    case TransportClass::GPU_DIRECT: return "GPU_DIRECT";
    case TransportClass::PCIE: return "PCIE";
    case TransportClass::HOST_STAGED_TCP: return "HOST_STAGED_TCP";
    case TransportClass::UNKNOWN: return "UNKNOWN";
  }
  return "UNKNOWN";
}

std::optional<AuthorityVerdict> authority_verdict_from_string(std::string_view s) {
  if (s == "CURRENT") return AuthorityVerdict::CURRENT;
  if (s == "STALE") return AuthorityVerdict::STALE;
  if (s == "FRESH") return AuthorityVerdict::FRESH;
  if (s == "DUPLICATE") return AuthorityVerdict::DUPLICATE;
  if (s == "CONFLICT") return AuthorityVerdict::CONFLICT;
  if (s == "UNKNOWN") return AuthorityVerdict::UNKNOWN;
  if (s == "INVALID") return AuthorityVerdict::INVALID;
  return std::nullopt;
}
std::string_view authority_verdict_to_string(AuthorityVerdict a) {
  switch (a) {
    case AuthorityVerdict::CURRENT: return "CURRENT";
    case AuthorityVerdict::STALE: return "STALE";
    case AuthorityVerdict::FRESH: return "FRESH";
    case AuthorityVerdict::DUPLICATE: return "DUPLICATE";
    case AuthorityVerdict::CONFLICT: return "CONFLICT";
    case AuthorityVerdict::UNKNOWN: return "UNKNOWN";
    case AuthorityVerdict::INVALID: return "INVALID";
  }
  return "UNKNOWN";
}

std::optional<SchedulingIntent> scheduling_intent_from_string(std::string_view s) {
  if (s == "DEFAULT") return SchedulingIntent::DEFAULT;
  if (s == "LOW_LATENCY") return SchedulingIntent::LOW_LATENCY;
  if (s == "HIGH_THROUGHPUT") return SchedulingIntent::HIGH_THROUGHPUT;
  if (s == "BACKGROUND") return SchedulingIntent::BACKGROUND;
  return std::nullopt;
}
std::string_view scheduling_intent_to_string(SchedulingIntent s) {
  switch (s) {
    case SchedulingIntent::DEFAULT: return "DEFAULT";
    case SchedulingIntent::LOW_LATENCY: return "LOW_LATENCY";
    case SchedulingIntent::HIGH_THROUGHPUT: return "HIGH_THROUGHPUT";
    case SchedulingIntent::BACKGROUND: return "BACKGROUND";
  }
  return "UNKNOWN";
}

std::optional<BufferPlacement> buffer_placement_from_string(std::string_view s) {
  if (s == "HOST") return BufferPlacement::HOST;
  if (s == "DEVICE") return BufferPlacement::DEVICE;
  if (s == "UNKNOWN") return BufferPlacement::UNKNOWN;
  return std::nullopt;
}
std::string_view buffer_placement_to_string(BufferPlacement p) {
  switch (p) {
    case BufferPlacement::HOST: return "HOST";
    case BufferPlacement::DEVICE: return "DEVICE";
    case BufferPlacement::UNKNOWN: return "UNKNOWN";
  }
  return "UNKNOWN";
}

std::size_t datatype_size_bytes(Datatype d) noexcept {
  switch (d) {
    case Datatype::INT32: return 4;
    case Datatype::UINT32: return 4;
    case Datatype::INT64: return 8;
    case Datatype::FLOAT32: return 4;
    case Datatype::FLOAT64: return 8;
    case Datatype::BYTE: return 1;
  }
  return 0;
}

bool reduction_is_applicable(CollectiveKind kind, ReductionOp op) noexcept {
  if (op == ReductionOp::NONE) {
    // BARRIER and BROADCAST carry no reduction; REDUCE-family ops require one.
    return kind == CollectiveKind::BARRIER || kind == CollectiveKind::BROADCAST;
  }
  // REDUCE, ALL_REDUCE, REDUCE_SCATTER require a reduction.
  return kind == CollectiveKind::REDUCE || kind == CollectiveKind::ALL_REDUCE ||
         kind == CollectiveKind::REDUCE_SCATTER;
}

bool datatype_is_supported(CollectiveKind kind, Datatype d) noexcept {
  (void)kind; (void)d;
  return true;  // all defined datatypes are supported by the reference backend
}

} // namespace collectivefabric
