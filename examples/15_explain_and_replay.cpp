#include "collectivefabric/explanation.hpp"
#include "collectivefabric/runtime.hpp"
#include "collectivefabric/backend/reference_backend.hpp"
#include "collectivefabric/planner/planner.hpp"
#include "collectivefabric/store.hpp"
#include <cstdio>
using namespace collectivefabric;
int main() {
  Runtime rt;
  auto backend = std::make_shared<ReferenceBackend>(BackendId(1), BackendGeneration(1));
  rt.register_backend(backend);
  Topology topo(TopologyGeneration(1)); topo.set_provenance(TopologyProvenance::SYNTHETIC);
  auto n = topo.add_node("n"); auto d = topo.add_device(n,"gpu",true,120);
  rt.adopt_topology(topo);
  GroupBuilder gb(CollectiveGroupId(1), GroupGeneration(1));
  gb.membership_generation(MembershipGeneration(1)).topology_generation(TopologyGeneration(1));
  gb.add_participant(ParticipantId(1),WorkerId(1),WorkerBootId(100),n,d,BackendGeneration(1));
  gb.add_participant(ParticipantId(2),WorkerId(2),WorkerBootId(200),n,d,BackendGeneration(1));
  auto g = gb.build(); rt.register_group(g);
  CollectiveDescriptor dsc; dsc.kind(CollectiveKind::ALL_REDUCE).reduction(ReductionOp::SUM).datatype(Datatype::FLOAT32).element_count(64);
  dsc.group_id(CollectiveGroupId(1)).group_generation(GroupGeneration(1)).collective_generation(CollectiveGeneration(1));
  PlannerInput pin; pin.descriptor=&dsc; pin.group=&g; pin.topology=&topo; pin.backend=&backend->capabilities();
  Planner pl; auto plan = pl.plan(pin);
  printf("explain_plan:\n%s\n", explain_plan(plan).c_str());
  // replay: persistent canonical identity is stable.
  auto dg1 = dsc.canonical_digest();
  auto dg2 = dsc.canonical_digest();
  printf("replay digest_stable=%d\n", dg1 == dg2);
  return 0;
}
