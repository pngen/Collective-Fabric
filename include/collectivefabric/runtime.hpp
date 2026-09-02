#pragma once
// Collective Fabric - runtime facade. Coordinates groups, topology, backends,
// the planner, the reference collective engine, lifecycle transitions, quorum
// completion, measurement ingest, health, accounting, and authority. This is
// the session object used by examples, the CLI, tests, and the coordinator.
#include "collectivefabric/collective/descriptor.hpp"
#include "collectivefabric/collective/plan.hpp"
#include "collectivefabric/group.hpp"
#include "collectivefabric/topology.hpp"
#include "collectivefabric/backend/backend.hpp"
#include "collectivefabric/reference/engine.hpp"
#include "collectivefabric/planner/planner.hpp"
#include "collectivefabric/lifecycle.hpp"
#include "collectivefabric/measurement.hpp"
#include "collectivefabric/health.hpp"
#include "collectivefabric/failure.hpp"
#include "collectivefabric/accounting.hpp"
#include "collectivefabric/authority.hpp"
#include "collectivefabric/foundation/errors.hpp"
#include "collectivefabric/foundation/clock.hpp"
#include <cstdint>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

namespace collectivefabric {

// --- support structure -----------------------------------------------------
struct EntryPartition {
  std::vector<std::uint8_t> bytes;
  bool success = false;
};

class Runtime {
public:
  Runtime();

  // ---------------- Backends ----------------------------------------------
  void register_backend(std::shared_ptr<CollectiveBackend> b);
  const CollectiveBackend* find_backend(BackendId id) const noexcept;
  std::vector<BackendId> backend_ids() const;

  // ---------------- Topology ----------------------------------------------
  void adopt_topology(const Topology& t);
  const Topology* topology() const noexcept;
  TopologyGeneration topology_generation() const noexcept;

  // ---------------- Groups -------------------------------------------------
  // Validates and registers an immutable group. Throws on validation failure.
  const CollectiveGroup& register_group(CollectiveGroup g);
  const CollectiveGroup* find_group(CollectiveGroupId id, GroupGeneration gen) const noexcept;
  // Member lookup by boot identity among registered groups.
  const CollectiveGroup* find_group_by_boot(CollectiveGroupId id, WorkerBootId boot) const noexcept;

  // ---------------- Executions --------------------------------------------
  ExecutionId create_collective(CollectiveDescriptor d);             // CREATED
  CollectivePlan plan_collective(ExecutionId e, const PlannerInput& in);  // PLANNED
  ReservationId reserve_collective(ExecutionId e);                    // RESERVED
  DispatchId dispatch_collective(ExecutionId e);                      // DISPATCHED
  // In-process reference execution of a dispatched collective. 'inputs' is
  // indexed by rank order. Applies quorum completion and returns the resulting
  // measurement id.
  MeasurementId execute_collective(ExecutionId e, std::vector<std::vector<std::uint8_t>> inputs);
  // Outputs produced by the last executed collective, ordered by rank.
  std::vector<std::vector<std::uint8_t>> outputs(ExecutionId e) const;
  // Per-rank completion reporting (used when quorum is authoritative).
  void complete_rank(ExecutionId e, RankId r);
  void fail_collective(ExecutionId e, CollectiveFailure f);
  void abort_collective(ExecutionId e);
  void cancel_collective(ExecutionId e);
  void mark_stale_collective(ExecutionId e);

  CollectiveState execution_state(ExecutionId e) const;
  const CollectivePlan* execution_plan(ExecutionId e) const;
  const CollectiveDescriptor* execution_descriptor(ExecutionId e) const;

  // ---------------- Measurements & health --------------------------------
  void ingest_measurement(CollectiveMeasurement m);
  std::size_t measurement_count() const;
  std::vector<MeasurementId> measurement_ids() const;
  const CollectiveMeasurement* measurement(MeasurementId id) const;
  void update_health(HealthRecord h);  // stale generation rejected
  HealthState path_health() const noexcept;
  HealthGeneration health_generation() const noexcept;

  // ---------------- Accounting & authority --------------------------------
  Accounting& accounting() noexcept { return accounting_; }
  const Accounting& accounting() const noexcept { return accounting_; }
  AuthorityGroundTruth& authority() noexcept { return authority_; }
  const AuthorityGroundTruth& authority() const noexcept { return authority_; }
  void advance_epoch();
  void set_live_boot(WorkerBootId b) noexcept;

private:
  struct Execution {
    CollectiveDescriptor descriptor;
    CollectivePlan plan;
    CollectiveStateMachine lifecycle{CollectiveState::CREATED};
    AttemptId attempt;
    AttemptGeneration attempt_generation;
    DispatchId dispatch;
    ReservationId reservation;
    std::map<RankId, bool> rank_completions;
    std::optional<CollectiveFailure> failure;
    std::optional<MeasurementId> measurement;
    std::vector<std::vector<std::uint8_t>> outputs;
    CollectiveId collective_id;
  };

  Execution& require(ExecutionId e);
  const Execution& require(ExecutionId e) const;
  // Lock-free helpers; callers must already hold mu_.
  const CollectiveGroup* find_group_unlocked(CollectiveGroupId id, GroupGeneration gen) const noexcept;
  void apply_quorum_unlocked(Execution& ex);
  std::uint64_t per_rank_bytes(const CollectiveDescriptor& d, std::uint64_t rank_count) const;
  std::uint64_t next_id_ = 1;

  mutable std::mutex mu_;
  std::map<BackendId, std::shared_ptr<CollectiveBackend>> backends_;
  std::optional<Topology> topology_;
  std::map<std::pair<CollectiveGroupId, GroupGeneration>, CollectiveGroup> groups_;
  std::map<ExecutionId, Execution> executions_;
  std::map<MeasurementId, CollectiveMeasurement> measurements_;
  HealthRecord health_;
  Accounting accounting_;
  AuthorityGroundTruth authority_;
};

} // namespace collectivefabric
