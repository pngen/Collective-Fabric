#include "collectivefabric/authority.hpp"
#include <cstdio>
using namespace collectivefabric;
int main() {
  AuthorityGroundTruth truth;
  truth.epoch=CoordinatorEpoch(3); truth.live_boot=WorkerBootId(100);
  truth.group_generation=GroupGeneration(5); truth.collective_generation=CollectiveGeneration(2);
  truth.attempt_generation=AttemptGeneration(4); truth.dispatch_generation=DispatchGeneration(7);
  AuthorityClaim stale; stale.boot=WorkerBootId(100); stale.epoch=CoordinatorEpoch(2);
  stale.group_generation=GroupGeneration(5); stale.collective_generation=CollectiveGeneration(2);
  stale.attempt_generation=AttemptGeneration(4); stale.dispatch_generation=DispatchGeneration(7);
  auto d = evaluate_authority(truth, stale);
  printf("stale-epoch verdict=%s reason=%s\n", std::string(authority_verdict_to_string(d.verdict)).c_str(), d.reason.c_str());
  // Old incarnation with larger numeric generations is still stale (incarnation wins).
  AuthorityClaim old; old.boot=WorkerBootId(999); old.epoch=CoordinatorEpoch(3);
  old.group_generation=GroupGeneration(99); old.collective_generation=CollectiveGeneration(50);
  old.attempt_generation=AttemptGeneration(99); old.dispatch_generation=DispatchGeneration(99);
  auto d2 = evaluate_authority(truth, old);
  printf("old-incarnation verdict=%s reason=%s\n", std::string(authority_verdict_to_string(d2.verdict)).c_str(), d2.reason.c_str());
  return 0;
}
