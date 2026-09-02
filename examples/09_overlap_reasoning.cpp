#include "collectivefabric/planner/planner.hpp"
#include "collectivefabric/group.hpp"
#include "collectivefabric/topology.hpp"
#include "collectivefabric/backend/capabilities.hpp"
#include <cstdio>
using namespace collectivefabric;
int main() {
  auto caps = reference_backend_capabilities(BackendId(1), BackendGeneration(1));
  Topology topo(TopologyGeneration(1)); topo.set_provenance(TopologyProvenance::SYNTHETIC);
  auto n = topo.add_node("n"); auto d = topo.add_device(n,"gpu",true,120);
  GroupBuilder gb(CollectiveGroupId(1), GroupGeneration(1));
  gb.membership_generation(MembershipGeneration(1)).topology_generation(TopologyGeneration(1));
  for (int i=0;i<2;++i) gb.add_participant(ParticipantId(i+1),WorkerId(i+1),WorkerBootId(100+i),n,d,BackendGeneration(1));
  auto g = gb.build();
  CollectiveDescriptor dsc; dsc.kind(CollectiveKind::ALL_REDUCE).reduction(ReductionOp::SUM).datatype(Datatype::FLOAT32).element_count(64);
  dsc.group_id(CollectiveGroupId(1)).group_generation(GroupGeneration(1)).collective_generation(CollectiveGeneration(1));
  Planner pl; PlannerInput pin; pin.descriptor=&dsc; pin.group=&g; pin.topology=&topo; pin.backend=&caps;
  auto plan = pl.plan(pin);
  printf("overlap_eligible=%s\n", plan.overlap_eligible()?"true":"false");
  printf("overlap_reason=%s\n", plan.overlap_reason().c_str());
  // Deterministic overlap reasoning in the explanation.
  printf("explanation=%s\n", plan.explanation().empty()?"":plan.explanation()[0].c_str());
  return 0;
}
