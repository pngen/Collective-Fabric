#pragma once
// Collective Fabric - distributed failure model. Failure is distributed state,
// never a silent local flag. Records the class, affected participant/rank,
// generation, attempt, and whether side effects may have occurred, retry is
// permitted, reconfiguration is required, or prior results are invalid.
#include "collectivefabric/foundation/identifier.hpp"
#include "collectivefabric/foundation/generation.hpp"
#include "collectivefabric/foundation/enums.hpp"
#include "collectivefabric/foundation/source.hpp"
#include "collectivefabric/foundation/clock.hpp"
#include <cstdint>
#include <optional>
#include <string>

namespace collectivefabric {

struct CollectiveFailure {
  FailureClass class_ = FailureClass::UNKNOWN;
  std::optional<ParticipantId> participant;
  std::optional<RankId> rank;
  CollectiveId collective;
  CollectiveGeneration collective_generation;
  AttemptId attempt;
  AttemptGeneration attempt_generation;
  bool side_effects_may_have_occurred = false;
  bool retry_permitted = false;
  bool reconfiguration_required = false;
  bool prior_results_invalid = false;
  Source source;
  std::uint64_t observed_utc_ns = 0;
  std::string narrative;
};

} // namespace collectivefabric
