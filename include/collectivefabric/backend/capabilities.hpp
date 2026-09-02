#pragma once
// Collective Fabric - vendor-neutral backend capability description. The
// runtime reasons about backends through typed capabilities, never through
// hard-coded vendor assumptions.
#include "collectivefabric/foundation/identifier.hpp"
#include "collectivefabric/foundation/generation.hpp"
#include "collectivefabric/foundation/enums.hpp"
#include "collectivefabric/foundation/source.hpp"
#include <cstdint>
#include <optional>
#include <string>
#include <vector>
#include <algorithm>

namespace collectivefabric {

struct BackendCapabilities {
  BackendId id;
  BackendGeneration generation;
  std::string name;
  std::vector<CollectiveKind> supported_collective_kinds;
  std::vector<Datatype> supported_datatypes;
  std::vector<ReductionOp> supported_reductions;
  bool supports_device_buffers = false;
  bool supports_host_buffers = false;
  bool supports_in_place = false;
  bool supports_nonblocking_progress = false;
  bool supports_stream_binding = false;
  bool supports_graph_capture = false;
  bool supports_async_error_query = false;
  bool supports_abort = false;
  bool supports_multi_process = false;
  bool supports_multi_node = false;
  std::vector<TransportClass> supported_transport_classes;
  std::optional<std::uint64_t> maximum_rank_count;
  Source provenance;
  bool is_reference = false;

  bool supports_kind(CollectiveKind k) const noexcept {
    return std::find(supported_collective_kinds.begin(), supported_collective_kinds.end(), k) != supported_collective_kinds.end();
  }
  bool supports_datatype(Datatype d) const noexcept {
    return std::find(supported_datatypes.begin(), supported_datatypes.end(), d) != supported_datatypes.end();
  }
  bool supports_reduction(ReductionOp r) const noexcept {
    return std::find(supported_reductions.begin(), supported_reductions.end(), r) != supported_reductions.end();
  }
  bool supports_transport(TransportClass t) const noexcept {
    return std::find(supported_transport_classes.begin(), supported_transport_classes.end(), t) != supported_transport_classes.end();
  }
};

// Reference (deterministic in-process) backend capabilities.
inline BackendCapabilities reference_backend_capabilities(BackendId id, BackendGeneration generation) {
  BackendCapabilities c;
  c.id = id;
  c.generation = generation;
  c.name = "deterministic-reference";
  c.supported_collective_kinds = {CollectiveKind::BARRIER, CollectiveKind::BROADCAST, CollectiveKind::REDUCE,
                                  CollectiveKind::ALL_REDUCE, CollectiveKind::ALL_GATHER, CollectiveKind::REDUCE_SCATTER};
  c.supported_datatypes = {Datatype::INT32, Datatype::UINT32, Datatype::INT64, Datatype::FLOAT32,
                           Datatype::FLOAT64, Datatype::BYTE};
  c.supported_reductions = {ReductionOp::SUM, ReductionOp::PRODUCT, ReductionOp::MIN, ReductionOp::MAX, ReductionOp::NONE};
  c.supports_device_buffers = false;
  c.supports_host_buffers = true;
  c.supports_in_place = true;
  c.supports_nonblocking_progress = true;
  c.supports_stream_binding = false;
  c.supports_graph_capture = false;
  c.supports_async_error_query = false;
  c.supports_abort = true;
  c.supports_multi_process = false;
  c.supports_multi_node = false;
  c.supported_transport_classes = {TransportClass::HOST_MEMORY};
  c.maximum_rank_count = std::uint64_t(1u << 20);
  c.is_reference = true;
  return c;
}

} // namespace collectivefabric
