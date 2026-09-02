#pragma once
// Collective Fabric - vendor-neutral backend contract. Backends describe
// capabilities and expose a reference execution surface. Adapters for
// NCCL/RCCL/oneCCL/MPI-style engines can be implemented behind this contract
// without changing core collective semantics. The runtime only furthers this
// interface; a backend may also be a pure capability/measurement descriptor.
#include "collectivefabric/foundation/identifier.hpp"
#include "collectivefabric/foundation/enums.hpp"
#include "collectivefabric/backend/capabilities.hpp"
#include <string_view>

namespace collectivefabric {

class CollectiveBackend {
public:
  virtual ~CollectiveBackend() = default;

  virtual const BackendCapabilities& capabilities() const = 0;
  virtual BackendId id() const = 0;
  virtual std::string_view name() const = 0;
  virtual bool is_reference() const = 0;
};

} // namespace collectivefabric
