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
  for (int i=0;i<4;++i) gb.add_participant(ParticipantId(i+1),WorkerId(i+1),WorkerBootId(100+i),n,d,BackendGeneration(1));
  auto g = gb.build();
  CollectiveDescriptor dsc; dsc.kind(CollectiveKind::ALL_REDUCE).reduction(ReductionOp::SUM).datatype(Datatype::FLOAT32).element_count(4096);
  dsc.group_id(CollectiveGroupId(1)).group_generation(GroupGeneration(1)).collective_generation(CollectiveGeneration(1));
  Planner pl; PlannerInput pin; pin.descriptor=&dsc; pin.group=&g; pin.topology=&topo; pin.backend=&caps;
  pin.measured_bandwidth_bytes_per_sec = 1000000000ULL; pin.measured_latency_ns = 10;  // measured (illustrative)
  auto plan = pl.plan(pin);
  printf("selected=%s\n", std::string(algorithm_to_string(plan.algorithm())).c_str());
  for (auto& f : plan.factors()) printf("  factor %s=%g (%s)\n", f.name.c_str(), f.value, f.unit.c_str());
  printf("explanation: %s\n", plan.explanation().empty()?"":plan.explanation()[0].c_str());
  return 0;
}
