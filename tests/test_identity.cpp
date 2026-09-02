#include "test_util.hpp"
#include "collectivefabric/foundation/identifier.hpp"
#include "collectivefabric/foundation/generation.hpp"
#include "collectivefabric/authority.hpp"
using namespace collectivefabric;

// Strong identities: distinct types, no accidental cross-assignment.
// (Cross-assignment is a compile error; we verify distinct type identity here.)
static_assert(!std::is_convertible_v<CollectiveId, CollectiveGroupId>);
static_assert(!std::is_convertible_v<RankId, ParticipantId>);
static_assert(!std::is_same_v<WorkerBootId, WorkerId>);

int main() {
  // Generation values preserved exactly.
  GroupGeneration g1(1), g2(2);
  CF_CHECK(g1.value() == 1);
  CF_CHECK(g2.value() == 2);
  CF_CHECK(compare_generations(g1, g2) == std::partial_ordering::less);
  CF_CHECK(compare_generations(g2, g1) == std::partial_ordering::greater);
  CF_CHECK(generation_is_newer(g2, g1));
  CF_CHECK(generation_is_older(g1, g2));
  CF_CHECK(Generation<GroupGenerationTag>{} .is_null());

  // next() saturates; unset advances to 1.
  CF_CHECK(GroupGeneration{}.next().value() == 1);
  CF_CHECK(GroupGeneration(UINT64_MAX).next().value() == UINT64_MAX);

  // Authority is incarnation-scoped: a higher generation from an old boot
  // never fences a fresh incarnation.
  AuthorityGroundTruth truth;
  truth.epoch = CoordinatorEpoch(3);
  truth.live_boot = WorkerBootId(100);
  truth.group_generation = GroupGeneration(5);
  truth.collective_generation = CollectiveGeneration(2);
  truth.attempt_generation = AttemptGeneration(4);
  truth.dispatch_generation = DispatchGeneration(7);

  // Same boot, same generations -> CURRENT (or DUPLICATE for a completion).
  AuthorityClaim cur;
  cur.boot = WorkerBootId(100); cur.epoch = CoordinatorEpoch(3);
  cur.group_generation = GroupGeneration(5); cur.collective_generation = CollectiveGeneration(2);
  cur.attempt_generation = AttemptGeneration(4); cur.dispatch_generation = DispatchGeneration(7);
  CF_CHECK(evaluate_authority(truth, cur).verdict == AuthorityVerdict::CURRENT);

  cur.is_completion = true;
  CF_CHECK(evaluate_authority(truth, cur).verdict == AuthorityVerdict::DUPLICATE);

  // Higher numeric generation from an OLD boot is still stale (incarnation wins).
  AuthorityClaim old_boot;
  old_boot.boot = WorkerBootId(999);  // different incarnation
  old_boot.epoch = CoordinatorEpoch(3);
  old_boot.group_generation = GroupGeneration(99);   // numerically much larger
  old_boot.collective_generation = CollectiveGeneration(50);
  old_boot.attempt_generation = AttemptGeneration(99);
  old_boot.dispatch_generation = DispatchGeneration(99);
  CF_CHECK(evaluate_authority(truth, old_boot).verdict == AuthorityVerdict::STALE);

  // Stale epoch.
  AuthorityClaim stale_epoch; stale_epoch.boot = WorkerBootId(100); stale_epoch.epoch = CoordinatorEpoch(2);
  stale_epoch.group_generation = GroupGeneration(5); stale_epoch.collective_generation = CollectiveGeneration(2);
  stale_epoch.attempt_generation = AttemptGeneration(4); stale_epoch.dispatch_generation = DispatchGeneration(7);
  CF_CHECK(evaluate_authority(truth, stale_epoch).verdict == AuthorityVerdict::STALE);

  // Stale group generation.
  AuthorityClaim stale_grp; stale_grp.boot = WorkerBootId(100); stale_grp.epoch = CoordinatorEpoch(3);
  stale_grp.group_generation = GroupGeneration(4); stale_grp.collective_generation = CollectiveGeneration(2);
  stale_grp.attempt_generation = AttemptGeneration(4); stale_grp.dispatch_generation = DispatchGeneration(7);
  CF_CHECK(evaluate_authority(truth, stale_grp).verdict == AuthorityVerdict::STALE);

  CF_FINISH("test_identity");
}
