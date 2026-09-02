#pragma once
// Collective Fabric - concrete deterministic in-process reference backend.
// Provides the capability description used by the planner and executes the
// reference collective engine for all in-phase data.
#include "collectivefabric/backend/backend.hpp"
#include "collectivefabric/foundation/enums.hpp"
#include <string_view>

namespace collectivefabric {

class ReferenceBackend : public CollectiveBackend {
public:
  ReferenceBackend(BackendId id, BackendGeneration generation)
      : id_(id), caps_(reference_backend_capabilities(id, generation)) {}

  const BackendCapabilities& capabilities() const override { return caps_; }
  BackendId id() const override { return id_; }
  std::string_view name() const override { return caps_.name; }
  bool is_reference() const override { return true; }

private:
  BackendId id_;
  BackendCapabilities caps_;
};

} // namespace collectivefabric
