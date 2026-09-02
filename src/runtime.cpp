#include "collectivefabric/runtime.hpp"
#include "collectivefabric/foundation/checked.hpp"

namespace collectivefabric {

Runtime::Runtime() { authority_.epoch = CoordinatorEpoch(1); }

void Runtime::register_backend(std::shared_ptr<CollectiveBackend> b) {
  if (!b) throw Error(ErrorCode::INVALID_ARGUMENT, "register_backend: null backend");
  std::lock_guard<std::mutex> l(mu_);
  backends_[b->id()] = std::move(b);
}
const CollectiveBackend* Runtime::find_backend(BackendId id) const noexcept {
  std::lock_guard<std::mutex> l(mu_);
  auto it = backends_.find(id);
  return it == backends_.end() ? nullptr : it->second.get();
}
std::vector<BackendId> Runtime::backend_ids() const {
  std::lock_guard<std::mutex> l(mu_);
  std::vector<BackendId> ids;
  for (const auto& [id, _] : backends_) ids.push_back(id);
  return ids;
}

void Runtime::adopt_topology(const Topology& t) {
  std::lock_guard<std::mutex> l(mu_);
  topology_ = t;
}
const Topology* Runtime::topology() const noexcept {
  std::lock_guard<std::mutex> l(mu_);
  return topology_ ? &*topology_ : nullptr;
}
TopologyGeneration Runtime::topology_generation() const noexcept {
  std::lock_guard<std::mutex> l(mu_);
  return topology_ ? topology_->generation() : TopologyGeneration{};
}

const CollectiveGroup& Runtime::register_group(CollectiveGroup g) {
  std::lock_guard<std::mutex> l(mu_);
  auto key = std::make_pair(g.id(), g.generation());
  if (groups_.count(key)) throw Error(ErrorCode::ALREADY_EXISTS, "group generation already registered");
  auto [it, _] = groups_.emplace(key, std::move(g));
  return it->second;
}
const CollectiveGroup* Runtime::find_group(CollectiveGroupId id, GroupGeneration gen) const noexcept {
  std::lock_guard<std::mutex> l(mu_);
  return find_group_unlocked(id, gen);
}
const CollectiveGroup* Runtime::find_group_by_boot(CollectiveGroupId id, WorkerBootId boot) const noexcept {
  std::lock_guard<std::mutex> l(mu_);
  for (const auto& [key, g] : groups_) {
    if (key.first == id && g.is_live_authority(boot)) return &g;
  }
  return nullptr;
}
const CollectiveGroup* Runtime::find_group_unlocked(CollectiveGroupId id, GroupGeneration gen) const noexcept {
  auto it = groups_.find(std::make_pair(id, gen));
  return it == groups_.end() ? nullptr : &it->second;
}

ExecutionId Runtime::create_collective(CollectiveDescriptor d) {
  std::lock_guard<std::mutex> l(mu_);
  if (d.group_id().is_null() || d.group_generation().is_null()) {
    throw Error(ErrorCode::VALIDATION, "collective requires a group id and group generation");
  }
  if (!groups_.count(std::make_pair(d.group_id(), d.group_generation()))) {
    throw Error(ErrorCode::VALIDATION, "collective references an unknown group generation");
  }
  d.validate();
  ExecutionId eid(next_id_++);
  Execution ex;
  ex.descriptor = std::move(d);
  ex.collective_id = ex.descriptor.collective_id();
  ex.attempt = AttemptId(next_id_++);
  ex.attempt_generation = AttemptGeneration(1);
  executions_[eid] = std::move(ex);
  return eid;
}

CollectivePlan Runtime::plan_collective(ExecutionId e, const PlannerInput& in) {
  std::lock_guard<std::mutex> l(mu_);
  auto& ex = require(e);
  ex.lifecycle.transition_to(CollectiveState::PLANNED);
  Planner pl;
  PlannerInput pin = in;
  pin.descriptor = &ex.descriptor;
  ex.plan = pl.plan(pin);
  return ex.plan;
}

ReservationId Runtime::reserve_collective(ExecutionId e) {
  std::lock_guard<std::mutex> l(mu_);
  auto& ex = require(e);
  ex.lifecycle.transition_to(CollectiveState::RESERVED);
  auto bytes = ex.descriptor.element_byte_count();
  ex.reservation = ReservationId(next_id_++);
  accounting_.reserve(ex.reservation, bytes);
  accounting_.collective_started();
  return ex.reservation;
}

DispatchId Runtime::dispatch_collective(ExecutionId e) {
  std::lock_guard<std::mutex> l(mu_);
  auto& ex = require(e);
  ex.lifecycle.transition_to(CollectiveState::DISPATCHED);
  ex.dispatch = DispatchId(next_id_++);
  return ex.dispatch;
}

std::uint64_t Runtime::per_rank_bytes(const CollectiveDescriptor& d, std::uint64_t rank_count) const {
  const bool aggregate = (d.kind() == CollectiveKind::ALL_GATHER || d.kind() == CollectiveKind::REDUCE_SCATTER);
  return aggregate ? d.aggregate_byte_count(rank_count) : d.element_byte_count();
}

MeasurementId Runtime::execute_collective(ExecutionId e, std::vector<std::vector<std::uint8_t>> inputs) {
  const auto* d = execution_descriptor(e);
  if (!d) return MeasurementId{};
  const auto* grp = find_group(d->group_id(), d->group_generation());
  const std::uint64_t rank_count = grp ? grp->rank_count() : 0;
  const std::uint64_t start = clock::steady_us();

  std::vector<std::vector<std::uint8_t>> outputs;
  CollectiveFailure failure;
  bool ok = true;
  try {
    if (!grp) throw Error(ErrorCode::VALIDATION, "execute: group not found");
    if (inputs.size() < rank_count) throw Error(ErrorCode::VALIDATION, "execute: missing rank inputs");
    const std::uint64_t expected = per_rank_bytes(*d, rank_count);
    for (std::uint64_t r = 0; r < rank_count; ++r)
      if (inputs[r].size() != expected) throw Error(ErrorCode::VALIDATION, "execute: rank input size mismatch");
    std::vector<ReferenceEngine::Span> spans;
    for (const auto& in : inputs) spans.push_back(ReferenceEngine::Span{in.data(), in.size()});
    const std::uint64_t root = d->root().is_null() ? 0 : d->root().raw();
    {
      std::lock_guard<std::mutex> l(mu_);
      auto& ex = require(e);
      ex.lifecycle.transition_to(CollectiveState::RUNNING);
    }
    ReferenceEngine::execute(d->kind(), d->reduction(), d->datatype(), d->element_count(), root, rank_count, spans, outputs);
  } catch (const std::exception& err) {
    failure.class_ = FailureClass::WORKER_DEATH;  // placeholder; refined below
    failure.class_ = FailureClass::UNKNOWN;
    failure.collective = d->collective_id();
    failure.collective_generation = d->collective_generation();
    failure.attempt = AttemptId{};
    failure.observed_utc_ns = clock::wall_ns();
    failure.narrative = err.what();
    failure.side_effects_may_have_occurred = true;
    ok = false;
  }

  const std::uint64_t end = clock::steady_us();
  std::lock_guard<std::mutex> l(mu_);
  auto& ex = require(e);
  if (!ok) {
    try { ex.lifecycle.transition_to(CollectiveState::FAILED); } catch (const Error&) {}
    ex.failure = failure;
    accounting_.failed_bytes(d->element_byte_count());
    return MeasurementId{};
  }
  for (std::uint64_t r = 0; r < rank_count; ++r) ex.rank_completions[RankId(r)] = true;
  ex.outputs = std::move(outputs);
  try { ex.lifecycle.transition_to(CollectiveState::COMPLETING); } catch (const Error&) {}
  apply_quorum_unlocked(ex);
  CollectiveMeasurement m;
  m.id = MeasurementId(next_id_++);
  m.collective = ex.collective_id;
  m.collective_generation = ex.descriptor.collective_generation();
  m.group_generation = ex.descriptor.group_generation();
  m.algorithm = ex.plan.algorithm();
  m.backend = ex.plan.backend();
  m.rank_count = rank_count;
  m.payload_bytes = ex.descriptor.element_byte_count();
  m.wall_time_us = end - start;
  m.execution_duration_us = m.wall_time_us;
  m.success = true;
  m.topology_generation = topology_ ? topology_->generation() : TopologyGeneration{};
  m.provenance = MeasurementProvenance::MEASURED;
  m.freshness = MeasurementFreshness::CURRENT;
  m.timestamp_utc_ns = clock::wall_ns();
  m.measurement_generation = MeasurementGeneration(1);
  m.source = make_source("reference in-process execution", ProvenanceKind::MEASURED);
  auto lc = checked_mul(m.payload_bytes, rank_count);
  m.logical_collective_bytes = lc ? *lc : 0;
  if (m.wall_time_us > 0) {
    const double secs = static_cast<double>(m.wall_time_us) / 1e6;
    m.payload_throughput_bytes_per_sec = static_cast<double>(m.payload_bytes) / secs;
    if (m.logical_collective_bytes > 0)
      m.logical_collective_bytes_per_sec = static_cast<double>(m.logical_collective_bytes) / secs;
  }
  measurements_[m.id] = m;
  ex.measurement = m.id;
  accounting_.collective_completed();
  auto cb = checked_mul(m.payload_bytes, rank_count);
  accounting_.completed_bytes(cb ? *cb : 0);
  return m.id;
}

void Runtime::complete_rank(ExecutionId e, RankId r) {
  std::lock_guard<std::mutex> l(mu_);
  auto& ex = require(e);
  if (ex.rank_completions.count(r)) throw Error(ErrorCode::CONFLICT, "duplicate rank completion");
  ex.rank_completions[r] = true;
  accounting_.rank_contribution_complete();
  if (ex.lifecycle.state() == CollectiveState::RUNNING) {
    try { ex.lifecycle.transition_to(CollectiveState::COMPLETING); } catch (const Error&) {}
  }
  apply_quorum_unlocked(ex);
}

void Runtime::fail_collective(ExecutionId e, CollectiveFailure f) {
  std::lock_guard<std::mutex> l(mu_);
  auto& ex = require(e);
  if (ex.lifecycle.state() == CollectiveState::FAILED) return;  // idempotent
  try { ex.lifecycle.transition_to(CollectiveState::FAILED); } catch (const Error&) {
    throw Error(ErrorCode::LIFECYCLE, "cannot fail collective in current state");
  }
  ex.failure = f;
  accounting_.failed_bytes(ex.descriptor.element_byte_count());
}

void Runtime::abort_collective(ExecutionId e) {
  std::lock_guard<std::mutex> l(mu_);
  auto& ex = require(e);
  ex.lifecycle.transition_to(CollectiveState::ABORTED);
  accounting_.collective_aborted();
}

void Runtime::cancel_collective(ExecutionId e) {
  std::lock_guard<std::mutex> l(mu_);
  auto& ex = require(e);
  ex.lifecycle.transition_to(CollectiveState::CANCELLED);
}

void Runtime::mark_stale_collective(ExecutionId e) {
  std::lock_guard<std::mutex> l(mu_);
  auto& ex = require(e);
  ex.lifecycle.transition_to(CollectiveState::STALE);
}

CollectiveState Runtime::execution_state(ExecutionId e) const {
  std::lock_guard<std::mutex> l(mu_);
  return require(e).lifecycle.state();
}
const CollectivePlan* Runtime::execution_plan(ExecutionId e) const {
  std::lock_guard<std::mutex> l(mu_);
  auto& ex = require(e);
  return &ex.plan;
}
const CollectiveDescriptor* Runtime::execution_descriptor(ExecutionId e) const {
  std::lock_guard<std::mutex> l(mu_);
  auto it = executions_.find(e);
  return it == executions_.end() ? nullptr : &it->second.descriptor;
}
std::vector<std::vector<std::uint8_t>> Runtime::outputs(ExecutionId e) const {
  std::lock_guard<std::mutex> l(mu_);
  return require(e).outputs;
}

void Runtime::ingest_measurement(CollectiveMeasurement m) {
  std::lock_guard<std::mutex> l(mu_);
  measurements_[m.id] = std::move(m);
}
std::size_t Runtime::measurement_count() const {
  std::lock_guard<std::mutex> l(mu_);
  return measurements_.size();
}
std::vector<MeasurementId> Runtime::measurement_ids() const {
  std::lock_guard<std::mutex> l(mu_);
  std::vector<MeasurementId> ids;
  for (const auto& [id, _] : measurements_) ids.push_back(id);
  return ids;
}
const CollectiveMeasurement* Runtime::measurement(MeasurementId id) const {
  std::lock_guard<std::mutex> l(mu_);
  auto it = measurements_.find(id);
  return it == measurements_.end() ? nullptr : &it->second;
}

void Runtime::update_health(HealthRecord h) {
  std::lock_guard<std::mutex> l(mu_);
  if (!health_.is_zero() && compare_generations(h.generation, health_.generation) == std::partial_ordering::less) {
    accounting_.stale_message_rejected();
    throw Error(ErrorCode::STALE_AUTHORITY, "old health report rejected");
  }
  health_ = std::move(h);
}
HealthState Runtime::path_health() const noexcept {
  std::lock_guard<std::mutex> l(mu_);
  return health_.state;
}
HealthGeneration Runtime::health_generation() const noexcept {
  std::lock_guard<std::mutex> l(mu_);
  return health_.generation;
}

void Runtime::advance_epoch() {
  std::lock_guard<std::mutex> l(mu_);
  authority_.epoch = authority_.epoch.next();
}
void Runtime::set_live_boot(WorkerBootId b) noexcept {
  std::lock_guard<std::mutex> l(mu_);
  authority_.live_boot = b;
}

Runtime::Execution& Runtime::require(ExecutionId e) {
  auto it = executions_.find(e);
  if (it == executions_.end()) throw Error(ErrorCode::NOT_FOUND, "unknown execution");
  return it->second;
}
const Runtime::Execution& Runtime::require(ExecutionId e) const {
  auto it = executions_.find(e);
  if (it == executions_.end()) throw Error(ErrorCode::NOT_FOUND, "unknown execution");
  return it->second;
}

void Runtime::apply_quorum_unlocked(Execution& ex) {
  const auto* grp = find_group_unlocked(ex.descriptor.group_id(), ex.descriptor.group_generation());
  if (!grp) { try { ex.lifecycle.transition_to(CollectiveState::FAILED); } catch (const Error&) {} return; }
  std::size_t done = 0;
  for (const auto& p : grp->participants()) {
    if (ex.rank_completions.count(p.rank)) ++done;
  }
  if (done == grp->rank_count()) {
    if (ex.lifecycle.state() == CollectiveState::COMPLETING) {
      try { ex.lifecycle.transition_to(CollectiveState::SUCCEEDED); } catch (const Error&) {}
    }
  }
}

} // namespace collectivefabric
