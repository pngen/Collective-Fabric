#include "collectivefabric/store.hpp"
#include <cstdio>
using namespace collectivefabric;
int main() {
  PersistentState st; st.epoch = CoordinatorEpoch(2); st.live_boot = WorkerBootId(100); st.saved_ns = 111;
  PersistedGroup pg; pg.group_id=CollectiveGroupId(1); pg.generation=GroupGeneration(1); pg.membership=MembershipGeneration(1);
  pg.topology=TopologyGeneration(1); pg.health=HealthGeneration(0); pg.backend=BackendGeneration(1); pg.lifecycle=GroupLifecycle::READY;
  PersistedParticipant pp; pp.participant_id=ParticipantId(1); pp.worker=WorkerId(1); pp.boot=WorkerBootId(100); pp.node=NodeId(1); pp.device=DeviceId(1); pp.rank=RankId(0); pp.backend_generation=BackendGeneration(1); pp.is_known=true;
  pg.participants.push_back(pp);
  st.groups.push_back(pg);
  auto res = Store::save("example_state.bin", st);
  printf("saved bytes=%llu digest=%s\n", (unsigned long long)res.bytes, Sha256::to_hex(res.digest).c_str());
  auto [loaded, res2] = Store::load("example_state.bin");
  printf("loaded groups=%zu digest_match=%d\n", loaded.groups.size(), res2.digest==res.digest);
  return 0;
}
