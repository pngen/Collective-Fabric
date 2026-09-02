#include "test_util.hpp"
#include "collectivefabric/group.hpp"
using namespace collectivefabric;

int main() {
  // Valid group: dense ranks, deterministic assignment.
  GroupBuilder gb(CollectiveGroupId(1), GroupGeneration(1));
  gb.membership_generation(MembershipGeneration(1)).topology_generation(TopologyGeneration(1))
    .lifecycle(GroupLifecycle::READY).provenance(Source{});
  gb.add_participant(ParticipantId(10), WorkerId(1), WorkerBootId(100), NodeId(1), DeviceId(1), BackendGeneration(1), RankId(0));
  gb.add_participant(ParticipantId(20), WorkerId(2), WorkerBootId(200), NodeId(1), DeviceId(2), BackendGeneration(1), RankId(1));
  auto g = gb.build();
  CF_CHECK(g.rank_count() == 2);
  CF_CHECK(g.participant_at(RankId(1))->boot == WorkerBootId(200));
  CF_CHECK(g.rank_of(ParticipantId(10)) == RankId(0));

  // Deterministic rank assignment by participant id (ranks not explicit).
  GroupBuilder gb2(CollectiveGroupId(2), GroupGeneration(1));
  gb2.membership_generation(MembershipGeneration(1)).topology_generation(TopologyGeneration(1));
  gb2.add_participant(ParticipantId(50), WorkerId(1), WorkerBootId(100), NodeId(1), DeviceId(1), BackendGeneration(1));
  gb2.add_participant(ParticipantId(30), WorkerId(2), WorkerBootId(200), NodeId(1), DeviceId(2), BackendGeneration(1));
  auto g2 = gb2.build();
  // sorted by participant id: 30 first -> rank0
  CF_CHECK(g2.participant_at(RankId(0))->id == ParticipantId(30));
  CF_CHECK(g2.participant_at(RankId(1))->id == ParticipantId(50));

  // zero-rank group rejected
  GroupBuilder gb3(CollectiveGroupId(3), GroupGeneration(1));
  gb3.membership_generation(MembershipGeneration(1)).topology_generation(TopologyGeneration(1));
  CF_CHECK_THROWS(gb3.build());

  // duplicate participant id rejected
  GroupBuilder gb4(CollectiveGroupId(4), GroupGeneration(1));
  gb4.membership_generation(MembershipGeneration(1)).topology_generation(TopologyGeneration(1));
  gb4.add_participant(ParticipantId(1), WorkerId(1), WorkerBootId(100), NodeId(1), DeviceId(1), BackendGeneration(1), RankId(0));
  gb4.add_participant(ParticipantId(1), WorkerId(2), WorkerBootId(200), NodeId(1), DeviceId(2), BackendGeneration(1), RankId(1));
  CF_CHECK_THROWS(gb4.build());

  // rank hole / non-dense rejected (explicit non-null ranks with a hole)
  GroupBuilder gb5(CollectiveGroupId(5), GroupGeneration(1));
  gb5.membership_generation(MembershipGeneration(1)).topology_generation(TopologyGeneration(1));
  gb5.add_participant(ParticipantId(1), WorkerId(1), WorkerBootId(100), NodeId(1), DeviceId(1), BackendGeneration(1), RankId(1));
  gb5.add_participant(ParticipantId(2), WorkerId(2), WorkerBootId(200), NodeId(1), DeviceId(2), BackendGeneration(1), RankId(2));  // hole at rank 0
  CF_CHECK_THROWS(gb5.build());

  // null membership gen rejected
  GroupBuilder gb6(CollectiveGroupId(6), GroupGeneration(1));
  gb6.membership_generation(MembershipGeneration(0)).topology_generation(TopologyGeneration(1));
  gb6.add_participant(ParticipantId(1), WorkerId(1), WorkerBootId(100), NodeId(1), DeviceId(1), BackendGeneration(1), RankId(0));
  CF_CHECK_THROWS(gb6.build());

  // digest stable and generation-scoped
  auto dig1 = g.digest();
  auto gb7 = gb;  // copy builder, same inputs
  auto g7 = GroupBuilder(CollectiveGroupId(1), GroupGeneration(1))
    .membership_generation(MembershipGeneration(1)).topology_generation(TopologyGeneration(1)).lifecycle(GroupLifecycle::READY)
    .add_participant(ParticipantId(10), WorkerId(1), WorkerBootId(100), NodeId(1), DeviceId(1), BackendGeneration(1), RankId(0))
    .add_participant(ParticipantId(20), WorkerId(2), WorkerBootId(200), NodeId(1), DeviceId(2), BackendGeneration(1), RankId(1)).build();
  CF_CHECK(g.digest() == g7.digest());
  (void)gb7;

  // membership change -> new generation, immutable
  auto g8 = GroupBuilder(CollectiveGroupId(1), GroupGeneration(2))
    .membership_generation(MembershipGeneration(2)).topology_generation(TopologyGeneration(1))
    .add_participant(ParticipantId(10), WorkerId(1), WorkerBootId(100), NodeId(1), DeviceId(1), BackendGeneration(1), RankId(0))
    .add_participant(ParticipantId(20), WorkerId(2), WorkerBootId(200), NodeId(1), DeviceId(2), BackendGeneration(1), RankId(1)).build();
  CF_CHECK(g8.generation().value() == 2);
  CF_CHECK(g8.digest() != g.digest());

  CF_FINISH("test_group_membership");
}
