#pragma once
// Collective Fabric - incarnation-scoped authority and fencing. Authority is
// scoped to a WorkerBootId (incarnation) plus the current epoch and
// generations. A higher numeric generation from an old incarnation NEVER
// fences a fresh incarnation: the boot identity is checked first. Generation
// comparisons are explicit, never implicit numeric ordering alone.
#include "collectivefabric/foundation/identifier.hpp"
#include "collectivefabric/foundation/generation.hpp"
#include "collectivefabric/foundation/enums.hpp"
#include <string>

namespace collectivefabric {

// The current authoritative ground truth held by the coordinator/session.
struct AuthorityGroundTruth {
  CoordinatorEpoch epoch;
  WorkerBootId live_boot;               // current live incarnation; null if none
  GroupGeneration group_generation;
  CollectiveGeneration collective_generation;
  AttemptGeneration attempt_generation;
  DispatchGeneration dispatch_generation;

  bool boot_is_authoritative(WorkerBootId b) const noexcept {
    return !live_boot.is_null() && b == live_boot;
  }
};

// A candidate authority claim carried by an incoming message/mutation.
struct AuthorityClaim {
  WorkerBootId boot;                   // incarnation identifier
  CoordinatorEpoch epoch;
  GroupGeneration group_generation;
  CollectiveGeneration collective_generation;
  AttemptGeneration attempt_generation;
  DispatchGeneration dispatch_generation;
  bool is_completion = false;          // message is a completion report
};

struct AuthorityDecision {
  AuthorityVerdict verdict = AuthorityVerdict::UNKNOWN;
  std::string reason;
};

// Evaluate a claim against the ground truth. Incarnation (boot) is checked
// first; generations are only compared when the incarnations match.
inline AuthorityDecision evaluate_authority(const AuthorityGroundTruth& truth, const AuthorityClaim& claim) {
  AuthorityDecision d;

  // 1. Incarnation mismatch dominates everything. A different boot identity is
  //    stale even if its numeric generations are larger.
  if (!truth.live_boot.is_null() && claim.boot != truth.live_boot) {
    d.verdict = AuthorityVerdict::STALE;
    d.reason = "WorkerBootId does not match current participant incarnation";
    return d;
  }
  if (truth.live_boot.is_null()) {
    // No live incarnation yet; a claim is not yet attributable.
    if (claim.boot.is_null()) {
      d.verdict = AuthorityVerdict::INVALID;
      d.reason = "claim carries no boot identity";
      return d;
    }
  }

  // 2. Coordinator epoch. A stale epoch is rejected even for a matching boot.
  if (claim.epoch != truth.epoch) {
    if (compare_generations(claim.epoch, truth.epoch) == std::partial_ordering::less) {
      d.verdict = AuthorityVerdict::STALE;
      d.reason = "stale coordinator epoch";
      return d;
    }
    d.verdict = AuthorityVerdict::UNKNOWN;
    d.reason = "claim epoch is ahead of the coordinator epoch";
    return d;
  }

  // 3. Incarnation matches and epoch matches; compare generations explicitly.
  if (compare_generations(claim.group_generation, truth.group_generation) == std::partial_ordering::less) {
    d.verdict = AuthorityVerdict::STALE;
    d.reason = "stale group generation";
    return d;
  }
  if (compare_generations(claim.collective_generation, truth.collective_generation) == std::partial_ordering::less) {
    d.verdict = AuthorityVerdict::STALE;
    d.reason = "stale collective generation";
    return d;
  }
  if (compare_generations(claim.attempt_generation, truth.attempt_generation) == std::partial_ordering::less) {
    d.verdict = AuthorityVerdict::STALE;
    d.reason = "stale attempt generation";
    return d;
  }
  if (compare_generations(claim.dispatch_generation, truth.dispatch_generation) == std::partial_ordering::less) {
    d.verdict = AuthorityVerdict::STALE;
    d.reason = "stale dispatch generation";
    return d;
  }

  // Claim is not stale. Distinguish duplicate vs current vs newer.
  const bool same = claim.group_generation == truth.group_generation &&
                    claim.collective_generation == truth.collective_generation &&
                    claim.attempt_generation == truth.attempt_generation &&
                    claim.dispatch_generation == truth.dispatch_generation;
  if (same) {
    d.verdict = claim.is_completion ? AuthorityVerdict::DUPLICATE : AuthorityVerdict::CURRENT;
    d.reason = claim.is_completion ? "generation matches but completion already recorded" : "claim is current";
  } else {
    d.verdict = AuthorityVerdict::FRESH;
    d.reason = "claim is newer than the recorded generation";
  }
  return d;
}

} // namespace collectivefabric
