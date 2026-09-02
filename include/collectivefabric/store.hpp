#pragma once
// Collective Fabric - versioned binary persistence with integrity. Files carry
// a magic, version, bounded payload length, and CRC-32 checksum; a SHA-256
// semantic digest is computed over the canonical payload. Recovery never turns
// stale physical measurements back into CURRENT.
#include "collectivefabric/foundation/identifier.hpp"
#include "collectivefabric/foundation/generation.hpp"
#include "collectivefabric/foundation/enums.hpp"
#include "collectivefabric/digest/sha256.hpp"
#include <cstdint>
#include <string>
#include <vector>
#include <optional>

namespace collectivefabric {

struct PersistedParticipant {
  ParticipantId participant_id;
  WorkerId worker;
  WorkerBootId boot;
  NodeId node;
  DeviceId device;
  RankId rank;
  BackendGeneration backend_generation;
  bool is_known = true;
};

struct PersistedGroup {
  CollectiveGroupId group_id;
  GroupGeneration generation;
  MembershipGeneration membership;
  TopologyGeneration topology;
  HealthGeneration health;
  BackendGeneration backend;
  GroupLifecycle lifecycle = GroupLifecycle::READY;
  std::string provenance;
  std::vector<PersistedParticipant> participants;
};

struct PersistedMeasurement {
  MeasurementId id;
  CollectiveId collective;
  CollectiveGeneration collective_generation;
  GroupGeneration group_generation;
  Algorithm algorithm = Algorithm::UNKNOWN;
  BackendId backend;
  std::uint64_t rank_count = 0;
  std::uint64_t payload_bytes = 0;
  std::uint64_t wall_time_us = 0;
  MeasurementProvenance provenance = MeasurementProvenance::UNKNOWN;
  MeasurementFreshness freshness = MeasurementFreshness::REVALIDATION_REQUIRED;
  std::uint64_t timestamp_ns = 0;
  MeasurementGeneration measurement_generation;
  bool success = false;
};

struct PersistedHealth {
  HealthState state = HealthState::UNKNOWN;
  HealthGeneration generation;
  std::uint64_t observed_ns = 0;
  std::string narrative;
};

struct PersistedBackend {
  BackendId id;
  BackendGeneration generation;
  std::string name;
};

struct PersistentState {
  std::uint64_t format_version = 1;
  CoordinatorEpoch epoch;
  WorkerBootId live_boot;
  std::vector<PersistedGroup> groups;
  std::vector<PersistedMeasurement> measurements;
  std::optional<PersistedHealth> health;
  std::vector<PersistedBackend> backends;
  std::uint64_t policy_count = 0;
  std::uint64_t saved_ns = 0;
};

class Store {
public:
  static constexpr std::uint64_t kFormatVersion = 1;
  static constexpr std::uint32_t kMagic = 0x43464142u;  // "CFAB"

  struct SaveResult {
    Sha256::Digest digest;
    std::uint64_t bytes;
  };

  // Serialize state to a canonical byte payload (no file IO).
  static std::vector<std::uint8_t> encode(const PersistentState& s);
  // Decode a canonical payload with strict bounds/duplicate/enum checking.
  static PersistentState decode(std::span<const std::uint8_t> payload);
  static Sha256::Digest semantic_digest(const PersistentState& s);

  // Save to path with atomic temp -> flush -> close -> rename.
  static SaveResult save(const std::string& path, const PersistentState& s);
  // Load and verify magic/version/length/checksum. Throws Error(DECODE/IO).
  static std::pair<PersistentState, SaveResult> load(const std::string& path);
};

} // namespace collectivefabric
