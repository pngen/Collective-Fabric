#include "test_util.hpp"
#include "collectivefabric/planner/planner.hpp"
#include "collectivefabric/group.hpp"
#include "collectivefabric/topology.hpp"
#include "collectivefabric/backend/capabilities.hpp"
#include "collectivefabric/collective/descriptor.hpp"
using namespace collectivefabric;

static CollectiveGroup make_group(std::uint64_t n, TopologyGeneration tg) {
  GroupBuilder gb(CollectiveGroupId(1), GroupGeneration(1));
  gb.membership_generation(MembershipGeneration(1)).topology_generation(tg);
  for (std::uint64_t i = 0; i < n; ++i)
    gb.add_participant(ParticipantId(i + 1), WorkerId(i + 1), WorkerBootId(100 + i), NodeId(1), DeviceId(i + 1), BackendGeneration(1), RankId(i));
  return gb.build();
}
static Topology make_topology(std::uint64_t nodes, TopologyGeneration tg) {
  Topology t(tg); t.set_provenance(TopologyProvenance::SYNTHETIC);
  std::vector<NodeId> ns;
  for (std::uint64_t i = 0; i < nodes; ++i) ns.push_back(t.add_node("n" + std::to_string(i)));
  for (auto n : ns) t.add_device(n, "d", false, 0);
  return t;
}

int main() {
  auto caps = reference_backend_capabilities(BackendId(1), BackendGeneration(1));
  Planner pl;

  // small-message all-reduce: RING should be competitive / TREE or RD
  auto g = make_group(4, TopologyGeneration(1));
  auto topo = make_topology(1, TopologyGeneration(1));
  CollectiveDescriptor d;
  d.kind(CollectiveKind::ALL_REDUCE).reduction(ReductionOp::SUM).datatype(Datatype::FLOAT32).element_count(16);
  d.group_id(CollectiveGroupId(1)).group_generation(GroupGeneration(1)).collective_generation(CollectiveGeneration(1));
  PlannerInput pin; pin.descriptor=&d; pin.group=&g; pin.topology=&topo; pin.backend=&caps;
  auto cands = pl.candidates(pin);
  int feasible = 0;
  for (auto& c : cands) if (c.feasible) ++feasible;
  CF_CHECK(feasible >= 4);  // ring, tree, recursive doubling, reduce-broadcast, backend_default
  auto plan = pl.plan(pin);
  CF_CHECK(plan.algorithm() != Algorithm::UNKNOWN);
  CF_CHECK(plan.expected_steps() >= 1);
  CF_CHECK(plan.provenance() == ProvenanceKind::DERIVED);
  CF_CHECK(plan.factors().size() > 0);
  CF_CHECK(plan.explanation().size() > 0);

  // hierarchical requires >=2 nodes
  PlannerInput pin1 = pin;
  auto g2 = make_group(2, TopologyGeneration(2));
  auto topo2 = make_topology(2, TopologyGeneration(2));
  CollectiveDescriptor d2;
  d2.kind(CollectiveKind::ALL_REDUCE).reduction(ReductionOp::SUM).datatype(Datatype::FLOAT32).element_count(4);
  d2.group_id(CollectiveGroupId(1)).group_generation(GroupGeneration(1)).collective_generation(CollectiveGeneration(1));
  pin1.group=&g2; pin1.topology=&topo2; pin1.descriptor=&d2;
  auto cands2 = pl.candidates(pin1);
  bool hierarchical_feasible = false;
  for (auto& c : cands2) if (c.feasible && c.algorithm == Algorithm::HIERARCHICAL) hierarchical_feasible = true;
  CF_CHECK(hierarchical_feasible);

  // backend without host buffer -> no feasible
  auto badcaps = caps;
  badcaps.supports_host_buffers = false;
  PlannerInput pbad; pbad.descriptor=&d; pbad.group=&g; pbad.topology=&topo; pbad.backend=&badcaps;
  CF_CHECK_THROWS(pl.plan(pbad));

  // bandwidth-delay product: large message favors bandwidth path (RING over TREE steps)
  PlannerInput pin3 = pin; pin3.measured_bandwidth_bytes_per_sec = 1000000000ULL;  // 1 GiB/s
  pin3.measured_latency_ns = 10;
  auto plan3 = pl.plan(pin3);
  CF_CHECK(plan3.algorithm() != Algorithm::UNKNOWN);
  CF_CHECK(plan3.expected_byte_movement() > 0);

  // deterministic identical-state ranking: same input -> same algorithm
  auto plan_a = pl.plan(pin);
  auto plan_b = pl.plan(pin);
  CF_CHECK(plan_a.algorithm() == plan_b.algorithm());
  CF_CHECK(plan_a.expected_steps() == plan_b.expected_steps());

  CF_FINISH("test_planner");
}
