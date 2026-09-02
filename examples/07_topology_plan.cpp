#include "collectivefabric/runtime.hpp"
#include "collectivefabric/backend/reference_backend.hpp"
#include "collectivefabric/planner/planner.hpp"
#include <cstdio>
using namespace collectivefabric;
int main() {
  Runtime rt;
  auto backend = std::make_shared<ReferenceBackend>(BackendId(1), BackendGeneration(1));
  rt.register_backend(backend);
  // Synthetic two-node topology with a network link.
  Topology topo(TopologyGeneration(1)); topo.set_provenance(TopologyProvenance::SYNTHETIC);
  auto n1 = topo.add_node("node0"); auto n2 = topo.add_node("node1");
  auto d0 = topo.add_device(n1,"gpu0",true,120); auto d1 = topo.add_device(n2,"gpu1",true,120);
  TopologyLink l; l.source_node=n1; l.dest_node=n2; l.source_device=d0; l.dest_device=d1;
  l.link_class=LinkClass::NETWORK; l.provenance=ProvenanceKind::SYNTHETIC; topo.add_link(l);
  rt.adopt_topology(topo);
  GroupBuilder gb(CollectiveGroupId(1), GroupGeneration(1));
  gb.membership_generation(MembershipGeneration(1)).topology_generation(topo.generation());
  gb.add_participant(ParticipantId(1),WorkerId(1),WorkerBootId(100),n1,d0,BackendGeneration(1));
  gb.add_participant(ParticipantId(2),WorkerId(2),WorkerBootId(200),n2,d1,BackendGeneration(1));
  auto g = gb.build(); rt.register_group(g);
  printf("topology nodes=%zu devices=%zu links=%zu provenance=SYNTHETIC\n", topo.node_count(), topo.device_count(), topo.link_count());
  CollectiveDescriptor d; d.group_id(CollectiveGroupId(1)).group_generation(GroupGeneration(1)).collective_generation(CollectiveGeneration(1));
  d.kind(CollectiveKind::ALL_REDUCE).reduction(ReductionOp::SUM).datatype(Datatype::FLOAT32).element_count(1024);
  d.collective_id(CollectiveId(1)).policy_generation(PolicyGeneration(1));
  PlannerInput pin; pin.descriptor=&d; pin.group=&g; pin.topology=&topo; pin.backend=&backend->capabilities();
  Planner pl; auto plan = pl.plan(pin);
  printf("plan algorithm=%s steps=%llu\n", std::string(algorithm_to_string(plan.algorithm())).c_str(), (unsigned long long)plan.expected_steps());
  return 0;
}
