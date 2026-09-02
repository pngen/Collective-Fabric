#pragma once
// Collective Fabric - source/provenance record. Every authoritative decision
// carries where it came from and whether that provenance is measured, derived,
// reported, synthetic, or unknown.
#include "collectivefabric/foundation/identifier.hpp"
#include "collectivefabric/foundation/generation.hpp"
#include "collectivefabric/foundation/enums.hpp"
#include "collectivefabric/foundation/clock.hpp"
#include <string>
#include <string_view>
#include <atomic>

namespace collectivefabric {

enum class ProvenanceKind : std::uint8_t {
  MEASURED, REPORTED, DERIVED, SYNTHETIC, UNKNOWN
};

inline std::string_view provenance_kind_to_string(ProvenanceKind k) {
  switch (k) {
    case ProvenanceKind::MEASURED: return "MEASURED";
    case ProvenanceKind::REPORTED: return "REPORTED";
    case ProvenanceKind::DERIVED: return "DERIVED";
    case ProvenanceKind::SYNTHETIC: return "SYNTHETIC";
    case ProvenanceKind::UNKNOWN: return "UNKNOWN";
  }
  return "UNKNOWN";
}
inline std::optional<ProvenanceKind> provenance_kind_from_string(std::string_view s) {
  if (s == "MEASURED") return ProvenanceKind::MEASURED;
  if (s == "REPORTED") return ProvenanceKind::REPORTED;
  if (s == "DERIVED") return ProvenanceKind::DERIVED;
  if (s == "SYNTHETIC") return ProvenanceKind::SYNTHETIC;
  if (s == "UNKNOWN") return ProvenanceKind::UNKNOWN;
  return std::nullopt;
}

// A source identifier plus the generation that produced the claim and the
// provenance kind. Used to answer "where did this come from, and is it known?"
struct Source {
  SourceId id;
  SourceGeneration generation;
  ProvenanceKind provenance = ProvenanceKind::UNKNOWN;
  std::uint64_t created_utc_ns = 0;
  std::string description;

  bool is_null() const noexcept { return id.is_null(); }
  bool is_measured() const noexcept { return provenance == ProvenanceKind::MEASURED; }
  bool is_synthetic() const noexcept { return provenance == ProvenanceKind::SYNTHETIC; }
  bool is_unknown_provenance() const noexcept { return provenance == ProvenanceKind::UNKNOWN; }
};

inline Source make_source(std::string description, ProvenanceKind provenance = ProvenanceKind::UNKNOWN) {
  static std::atomic<std::uint64_t> counter{0};
  Source s;
  s.id = SourceId(++counter);
  s.generation = SourceGeneration(1);
  s.provenance = provenance;
  s.created_utc_ns = clock::wall_ns();
  s.description = std::move(description);
  return s;
}

} // namespace collectivefabric
