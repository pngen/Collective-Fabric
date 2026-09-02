#pragma once
// Collective Fabric - deterministic collective planner. The planner consumes
// the descriptor, group, topology, backend capabilities and any available
// measurements, applies hard constraints first, then ranks remaining
// candidates by named factors with deterministic tie-breaking. Estimates are
// theoretical/derived and labelled as such; no measured value is invented.
#include "collectivefabric/collective/descriptor.hpp"
#include "collectivefabric/collective/plan.hpp"
#include "collectivefabric/group.hpp"
#include "collectivefabric/topology.hpp"
#include "collectivefabric/backend/capabilities.hpp"
#include "collectivefabric/foundation/enums.hpp"   // HealthState
#include <cstdint>
#include <optional>

namespace collectivefabric {

struct PlannerInput {
  const CollectiveDescriptor* descriptor = nullptr;
  const CollectiveGroup* group = nullptr;
  const Topology* topology = nullptr;
  const BackendCapabilities* backend = nullptr;
  std::optional<HealthState> path_health;             // collective-path health
  std::optional<std::uint64_t> measured_bandwidth_bytes_per_sec;
  std::optional<std::uint64_t> measured_latency_ns;
  bool deterministic_preference = true;
  bool prefer_device_buffers = false;
};

class Planner {
public:
  // Returns a plan. Throws Error(UNSUPPORTED or VALIDATION) if no feasible
  // algorithm exists for the requested constraints.
  CollectivePlan plan(const PlannerInput& input) const;

  // Deterministic candidate list with feasibility (used for explanation and
  // for tests). Returns (algorithm, feasible, hard_reject reason).
  struct Candidate {
    Algorithm algorithm = Algorithm::UNKNOWN;
    bool feasible = false;
    std::string reject_reason;
    std::uint64_t steps = 0;
    std::uint64_t byte_movement = 0;
  };
  std::vector<Candidate> candidates(const PlannerInput& input) const;
};

} // namespace collectivefabric
