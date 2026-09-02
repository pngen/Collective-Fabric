#include "collectivefabric/group.hpp"
#include <cstdio>
using namespace collectivefabric;
int main() {
  // Strong non-interchangeable identities; deterministic dense ranks.
  GroupBuilder gb(CollectiveGroupId(7), GroupGeneration(1));
  gb.membership_generation(MembershipGeneration(1)).topology_generation(TopologyGeneration(1));
  gb.add_participant(ParticipantId(10), WorkerId(100), WorkerBootId(9000), NodeId(1), DeviceId(1), BackendGeneration(1));
  gb.add_participant(ParticipantId(20), WorkerId(200), WorkerBootId(9001), NodeId(1), DeviceId(2), BackendGeneration(1));
  gb.add_participant(ParticipantId(30), WorkerId(300), WorkerBootId(9002), NodeId(2), DeviceId(3), BackendGeneration(1));
  auto g = gb.build();
  printf("group id=%llu generation=%llu ranks=%zu\n", (unsigned long long)g.id().raw(), (unsigned long long)g.generation().value(), g.rank_count());
  for (auto& p : g.participants()) printf("  participant id=%llu rank=%llu boot=%llu\n", (unsigned long long)p.id.raw(), (unsigned long long)p.rank.raw(), (unsigned long long)p.boot.raw());
  printf("digest=%s\n", Sha256::to_hex(g.digest()).c_str());
  return 0;
}
