#pragma once
// Collective Fabric - explicit collective execution lifecycle. Every
// transition is guarded. A collective cannot become SUCCEEDED because a single
// participant says it completed; authoritative quorum is enforced separately
// by the runtime. The state machine only guards legal state transitions.
#include "collectivefabric/foundation/enums.hpp"
#include "collectivefabric/foundation/errors.hpp"
#include <initializer_list>
#include <string>

namespace collectivefabric {

class CollectiveStateMachine {
public:
  CollectiveStateMachine() : state_(CollectiveState::CREATED) {}

  explicit CollectiveStateMachine(CollectiveState s) : state_(s) {}

  CollectiveState state() const noexcept { return state_; }
  bool is_terminal() const noexcept { return is_terminal(state_); }

  static bool is_terminal(CollectiveState s) noexcept {
    return s == CollectiveState::SUCCEEDED || s == CollectiveState::FAILED ||
           s == CollectiveState::ABORTED || s == CollectiveState::CANCELLED ||
           s == CollectiveState::STALE;
  }

  // Attempt a guarded transition. Throws Error(LIFECYCLE) on illegal move.
  void transition_to(CollectiveState to) {
    if (!allowed(state_, to)) {
      std::string msg("illegal lifecycle transition ");
      msg += std::string(collective_state_to_string(state_));
      msg += " -> ";
      msg += std::string(collective_state_to_string(to));
      throw Error(ErrorCode::LIFECYCLE, std::move(msg));
    }
    state_ = to;
  }

  // Non-throwing probe.
  bool can_transition_to(CollectiveState to) const noexcept { return allowed(state_, to); }

private:
  static bool allowed(CollectiveState from, CollectiveState to) noexcept {
    switch (from) {
      case CollectiveState::CREATED:
        return to == CollectiveState::PLANNED || to == CollectiveState::FAILED ||
               to == CollectiveState::CANCELLED || to == CollectiveState::ABORTED;
      case CollectiveState::PLANNED:
        return to == CollectiveState::RESERVED || to == CollectiveState::FAILED ||
               to == CollectiveState::CANCELLED;
      case CollectiveState::RESERVED:
        return to == CollectiveState::DISPATCHED || to == CollectiveState::FAILED ||
               to == CollectiveState::CANCELLED;
      case CollectiveState::DISPATCHED:
        return to == CollectiveState::RUNNING || to == CollectiveState::FAILED ||
               to == CollectiveState::ABORTED || to == CollectiveState::CANCELLED;
      case CollectiveState::RUNNING:
        return to == CollectiveState::COMPLETING || to == CollectiveState::FAILED ||
               to == CollectiveState::ABORTED || to == CollectiveState::CANCELLED;
      case CollectiveState::COMPLETING:
        return to == CollectiveState::SUCCEEDED || to == CollectiveState::FAILED ||
               to == CollectiveState::ABORTED;
      case CollectiveState::SUCCEEDED:
        return to == CollectiveState::STALE;  // a later stale event can retire
      case CollectiveState::FAILED:
        return to == CollectiveState::STALE;
      case CollectiveState::ABORTED:
      case CollectiveState::CANCELLED:
      case CollectiveState::STALE:
        return false;
    }
    return false;
  }

  CollectiveState state_;
};

} // namespace collectivefabric
