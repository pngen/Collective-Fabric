#pragma once
// Collective Fabric - measurement model. Distinguishes theoretical/derived
// estimates, backend-reported values, directly measured values, synthetic
// measurements, and unknown values. Every throughput metric defines its exact
// numerator/denominator; no unqualified "bandwidth" is published.
#include "collectivefabric/foundation/identifier.hpp"
#include "collectivefabric/foundation/generation.hpp"
#include "collectivefabric/foundation/enums.hpp"
#include "collectivefabric/foundation/source.hpp"
#include "collectivefabric/foundation/clock.hpp"
#include <cstdint>
#include <optional>
#include <string>

namespace collectivefabric {

enum class BusBandwidthKind : std::uint8_t { NONE, PAYLOAD, LOGICAL, ESTIMATED_LINK };

struct CollectiveMeasurement {
  MeasurementId id;
  CollectiveId collective;
  CollectiveGeneration collective_generation;
  GroupGeneration group_generation;
  Algorithm algorithm = Algorithm::UNKNOWN;
  BackendId backend;
  std::uint64_t rank_count = 0;
  std::uint64_t payload_bytes = 0;               // per-rank payload, exact
  std::uint64_t logical_collective_bytes = 0;    // sum over ranks, exact when meaningful
  std::optional<std::uint64_t> estimated_network_bytes;  // derived estimate only
  std::uint64_t wall_time_us = 0;
  std::optional<std::uint64_t> per_rank_duration_us;
  std::optional<double> payload_throughput_bytes_per_sec;
  std::optional<double> logical_collective_bytes_per_sec;
  std::optional<double> estimated_link_bytes_per_sec;  // only if derivable & meaningful
  std::optional<std::uint64_t> latency_us;
  std::optional<std::uint64_t> overlap_duration_us;
  std::optional<std::uint64_t> queue_wait_duration_us;
  std::optional<std::uint64_t> execution_duration_us;
  bool success = false;
  TopologyGeneration topology_generation;
  MeasurementProvenance provenance = MeasurementProvenance::UNKNOWN;
  MeasurementFreshness freshness = MeasurementFreshness::UNKNOWN;
  std::optional<double> confidence;
  std::uint64_t timestamp_utc_ns = 0;
  std::uint64_t interval_ns = 0;
  MeasurementGeneration measurement_generation;
  Source source;
  std::string narrative;
};

} // namespace collectivefabric
