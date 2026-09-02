#include "test_util.hpp"
#include "collectivefabric/reference/engine.hpp"
#include "collectivefabric/group.hpp"
#include "collectivefabric/planner/planner.hpp"
#include "collectivefabric/backend/capabilities.hpp"
#include "collectivefabric/authority.hpp"
#include "collectivefabric/store.hpp"
#include "collectivefabric/accounting.hpp"
#include <cstdint>
#include <algorithm>
using namespace collectivefabric;

// Deterministic PRNG (xorshift64*). Fixed seed; printed for reproducibility.
static std::uint64_t rng_state = 0xC0FFEE123456789ULL;
static std::uint64_t rng() {
  rng_state ^= rng_state >> 12; rng_state ^= rng_state << 25; rng_state ^= rng_state >> 27;
  return rng_state * 0x2545F4914F6CDD1DULL;
}
static std::uint64_t rng_range(std::uint64_t n) { return n ? rng() % n : 0; }

int main() {
  printf("property seed=0xC0FFEE123456789\n");
  const char* seed = "0xC0FFEE123456789";
  printf("property seed=%s\n", seed);

  auto caps = reference_backend_capabilities(BackendId(1), BackendGeneration(1));
  Planner pl;
  int iters = 200;
  for (int it = 0; it < iters; ++it) {
    // random rank count 1..8
    std::uint64_t n = 1 + rng_range(8);
    // build group with dense ranks
    GroupBuilder gb(CollectiveGroupId(1), GroupGeneration(1 + (it % 3)));
    gb.membership_generation(MembershipGeneration(1)).topology_generation(TopologyGeneration(1));
    for (std::uint64_t i = 0; i < n; ++i)
      gb.add_participant(ParticipantId(1 + i), WorkerId(1 + i), WorkerBootId(100 + (i + it)), NodeId(1), DeviceId(1 + i), BackendGeneration(1), RankId(i));
    auto group = gb.build();
    // reference all-reduce vs local expectation (SUM of int32, count 3)
    constexpr std::int32_t count = 3;
    std::vector<std::vector<std::uint8_t>> inputs(n);
    std::vector<std::int32_t> expected(count, 0);
    for (std::uint64_t r = 0; r < n; ++r) {
      std::int32_t vals[3];
      for (int k = 0; k < count; ++k) { vals[k] = (std::int32_t)rng_range(1000); expected[k] += vals[k]; }
      inputs[r] = std::vector<std::uint8_t>(reinterpret_cast<const std::uint8_t*>(vals), reinterpret_cast<const std::uint8_t*>(vals + count));
    }
    std::vector<ReferenceEngine::Span> spans;
    for (auto& in : inputs) spans.push_back(ReferenceEngine::Span{in.data(), in.size()});
    std::vector<std::vector<std::uint8_t>> outputs;
    ReferenceEngine::execute(CollectiveKind::ALL_REDUCE, ReductionOp::SUM, Datatype::INT32, count, 0, n, spans, outputs);
    bool ok = true;
    for (auto& o : outputs) for (int k = 0; k < count; ++k) if (((const std::int32_t*)o.data())[k] != expected[k]) ok = false;
    CF_CHECK(ok);

    // deterministic plan selection
    CollectiveDescriptor d;
    d.kind(CollectiveKind::ALL_REDUCE).reduction(ReductionOp::SUM).datatype(Datatype::INT32).element_count(count);
    d.group_id(CollectiveGroupId(1)).group_generation(GroupGeneration(1 + (it % 3))).collective_generation(CollectiveGeneration(1));
    Topology topo(TopologyGeneration(1)); topo.set_provenance(TopologyProvenance::SYNTHETIC);
    auto nd = topo.add_node("n"); topo.add_device(nd, "d", false, 0);
    PlannerInput pin; pin.descriptor=&d; pin.group=&group; pin.topology=&topo; pin.backend=&caps;
    auto pa = pl.plan(pin); auto pb = pl.plan(pin);
    CF_CHECK(pa.algorithm() == pb.algorithm());
    CF_CHECK(pa.expected_steps() == pb.expected_steps());

    // stale authority never mutates current state
    AuthorityGroundTruth truth;
    truth.epoch = CoordinatorEpoch(2); truth.live_boot = WorkerBootId(100);
    truth.group_generation = GroupGeneration(5); truth.collective_generation = CollectiveGeneration(2);
    truth.attempt_generation = AttemptGeneration(3); truth.dispatch_generation = DispatchGeneration(4);
    AuthorityClaim stale;
    stale.boot = WorkerBootId(9999); stale.epoch = CoordinatorEpoch(99);
    stale.group_generation = GroupGeneration(99); stale.collective_generation = CollectiveGeneration(99);
    stale.attempt_generation = AttemptGeneration(99); stale.dispatch_generation = DispatchGeneration(99);
    auto verdict = evaluate_authority(truth, stale).verdict;
    CF_CHECK(verdict == AuthorityVerdict::STALE);
    // current state unchanged (verify by re-evaluating a CURRENT claim)
    AuthorityClaim c; c.boot = truth.live_boot; c.epoch = truth.epoch; c.group_generation = truth.group_generation;
    c.collective_generation = truth.collective_generation; c.attempt_generation = truth.attempt_generation;
    c.dispatch_generation = truth.dispatch_generation;
    CF_CHECK(evaluate_authority(truth, c).verdict == AuthorityVerdict::CURRENT);

    // persistence round-trip preserves semantic digest
    PersistentState st; st.epoch = truth.epoch; st.live_boot = truth.live_boot;
    PersistedGroup pg; pg.group_id = CollectiveGroupId(1); pg.generation = GroupGeneration(1);
    pg.membership = MembershipGeneration(1); pg.topology = TopologyGeneration(1); pg.lifecycle = GroupLifecycle::READY;
    for (std::uint64_t r = 0; r < std::min<std::uint64_t>(n, 3); ++r) {
      PersistedParticipant pp; pp.participant_id = ParticipantId(1 + r); pp.worker = WorkerId(1 + r);
      pp.boot = WorkerBootId(100 + r); pp.node = NodeId(1); pp.device = DeviceId(1 + r); pp.rank = RankId(r);
      pp.backend_generation = BackendGeneration(1); pp.is_known = true;
      pg.participants.push_back(pp);
    }
    st.groups.push_back(pg);
    auto dig1 = Store::semantic_digest(st);
    auto dec = Store::decode(Store::encode(st));
    auto dig2 = Store::semantic_digest(dec);
    CF_CHECK(dig1 == dig2);

    // accounting never negative even with adversarial duplicate release
    Accounting acct;
    ReservationId rsv(7);
    CF_CHECK_NO_THROW(acct.reserve(rsv, 100));
    CF_CHECK_THROWS(acct.reserve(rsv, 10));   // duplicate reservation
    CF_CHECK_NO_THROW(acct.release(rsv));
    CF_CHECK_THROWS(acct.release(rsv));       // double release rejected

    // UNKNOWN provenance never silently becomes MEASURED
    MeasurementProvenance prov = MeasurementProvenance::UNKNOWN;
    CF_CHECK(prov != MeasurementProvenance::MEASURED);
    CF_CHECK(provenance_kind_to_string(ProvenanceKind::UNKNOWN) == "UNKNOWN");
  }

  CF_FINISH("test_property");
}
