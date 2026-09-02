#pragma once
// Collective Fabric - collective-path health model. Kept separate from generic
// device health. Health changes carry a HealthGeneration and provenance; old
// reports cannot overwrite fresh state. One failed collective is not treated as
// a permanent hardware failure.
#include "collectivefabric/foundation/identifier.hpp"
#include "collectivefabric/foundation/generation.hpp"
#include "collectivefabric/foundation/enums.hpp"
#include "collectivefabric/foundation/source.hpp"
#include "collectivefabric/foundation/clock.hpp"
#include <cstdint>
#include <optional>
#include <string>

namespace collectivefabric {

struct HealthRecord {
  HealthState state = HealthState::UNKNOWN;
  HealthGeneration generation;
  Source source;
  std::uint64_t observed_utc_ns = 0;
  std::string narrative;
  bool is_zero() const noexcept { return state == HealthState::UNKNOWN; }
};

// Deterministic health progression: a single transient failure degrades, and a
// repeated failure degrades further; only fresh observations alter the record.
inline HealthState advance_health(HealthState current, FailureClass ev) {
  switch (ev) {
    case FailureClass::PARTICIPANT_FAILURE:
    case FailureClass::WORKER_DEATH:
    case FailureClass::TRANSPORT_FAILURE:
    case FailureClass::DEVICE_FAILURE:
      return current == HealthState::UNHEALTHY ? HealthState::UNHEALTHY : HealthState::DEGRADED;
    default:
      return current == HealthState::UNKNOWN ? HealthState::DEGRADED : current;
  }
}

} // namespace collectivefabric
