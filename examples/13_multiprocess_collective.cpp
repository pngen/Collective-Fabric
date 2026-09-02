// Host-staged distributed all-reduce using the reference engine and the
// collective runtime. The transport is HOST_STAGED (host memory, not
// GPUDirect/RDMA/NCCL).
#include "collectivefabric/runtime.hpp"
#include "collectivefabric/backend/reference_backend.hpp"
#include "collectivefabric/planner/planner.hpp"
#include <cstdio>
using namespace collectivefabric;
int main() {
  Runtime rt;
  auto backend = std::make_shared<ReferenceBackend>(BackendId(1), BackendGeneration(1));
  rt.register_backend(backend);
  Topology topo(TopologyGeneration(1)); topo.set_provenance(TopologyProvenance::SYNTHETIC);
  auto n = topo.add_node("localhost"); auto d0 = topo.add_device(n,"mem0",false,0); auto d1 = topo.add_device(n,"mem1",false,0);
  rt.adopt_topology(topo);
  GroupBuilder gb(CollectiveGroupId(1), GroupGeneration(1));
  gb.membership_generation(MembershipGeneration(1)).topology_generation(TopologyGeneration(1));
  gb.add_participant(ParticipantId(1),WorkerId(1),WorkerBootId(100),n,d0,BackendGeneration(1));
  gb.add_participant(ParticipantId(2),WorkerId(2),WorkerBootId(200),n,d1,BackendGeneration(1));
  auto g = gb.build(); rt.register_group(g);
  CollectiveDescriptor d; d.group_id(CollectiveGroupId(1)).group_generation(GroupGeneration(1)).collective_generation(CollectiveGeneration(1));
  d.kind(CollectiveKind::ALL_REDUCE).reduction(ReductionOp::SUM).datatype(Datatype::FLOAT32).element_count(4);
  d.collective_id(CollectiveId(1)).policy_generation(PolicyGeneration(1));
  auto eid = rt.create_collective(d);
  PlannerInput pin; pin.descriptor=&d; pin.group=&g; pin.topology=&topo; pin.backend=&backend->capabilities();
  rt.plan_collective(eid,pin); rt.reserve_collective(eid); rt.dispatch_collective(eid);
  float a[4]={1,2,3,4}, b[4]={10,20,30,40};
  std::vector<std::vector<std::uint8_t>> inputs;
  inputs.push_back(std::vector<std::uint8_t>(reinterpret_cast<std::uint8_t*>(a), reinterpret_cast<std::uint8_t*>(a+4)));
  inputs.push_back(std::vector<std::uint8_t>(reinterpret_cast<std::uint8_t*>(b), reinterpret_cast<std::uint8_t*>(b+4)));
  auto mid = rt.execute_collective(eid, inputs);
  auto outs = rt.outputs(eid);
  printf("state=%s result[0]=%f\n", std::string(collective_state_to_string(rt.execution_state(eid))).c_str(), ((float*)outs[0].data())[0]);
  printf("transport=HOST_STAGED (not GPUDirect/RDMA/NCCL)\n");
  return 0;
}
