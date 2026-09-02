#pragma once
// Collective Fabric - a collective plan. Represents the chosen algorithm,
// transport path, phases, expected movement, expected critical path, rank
// ordering, hierarchy, overlap eligibility, hard constraints, named decision
// factors, and a deterministic explanation. There is no single opaque score;
// every selection is described by named factors.
#include "collectivefabric/foundation/identifier.hpp"
#include "collectivefabric/foundation/generation.hpp"
#include "collectivefabric/foundation/enums.hpp"
#include "collectivefabric/foundation/source.hpp"
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace collectivefabric {

struct DecisionFactor {
  std::string name;
  double value = 0.0;
  std::string unit;   // "ranks", "bytes", "us", "kib/s", "bool"
  bool hard = false;
  std::string reason;
};

class CollectivePlan {
public:
  // --- selection ----------------------------------------------------------
  Algorithm algorithm() const noexcept { return algorithm_; }
  void set_algorithm(Algorithm a) noexcept { algorithm_ = a; }
  BackendId backend() const noexcept { return backend_; }
  void set_backend(BackendId b) noexcept { backend_ = b; }
  TransportClass transport() const noexcept { return transport_; }
  void set_transport(TransportClass t) noexcept { transport_ = t; }

  // --- expected cost model (theoretical, derived) -------------------------
  void set_expected_byte_movement(std::uint64_t b) noexcept { expected_byte_movement_ = b; }
  std::uint64_t expected_byte_movement() const noexcept { return expected_byte_movement_; }
  void set_expected_steps(std::uint64_t s) noexcept { expected_steps_ = s; }
  std::uint64_t expected_steps() const noexcept { return expected_steps_; }
  void set_expected_critical_path_ns(std::optional<std::uint64_t> p) noexcept { expected_critical_path_ns_ = p; }
  std::optional<std::uint64_t> expected_critical_path_ns() const noexcept { return expected_critical_path_ns_; }

  // --- rank ordering ------------------------------------------------------
  void set_rank_order(std::vector<std::uint64_t> order) { rank_order_ = std::move(order); }
  const std::vector<std::uint64_t>& rank_order() const noexcept { return rank_order_; }

  // --- hierarchy ----------------------------------------------------------
  void set_hierarchy_depth(std::uint64_t d) noexcept { hierarchy_depth_ = d; }
  std::uint64_t hierarchy_depth() const noexcept { return hierarchy_depth_; }

  // --- overlap ------------------------------------------------------------
  void set_overlap_eligible(bool v, std::string reason) { overlap_eligible_ = v; overlap_reason_ = std::move(reason); }
  bool overlap_eligible() const noexcept { return overlap_eligible_; }
  const std::string& overlap_reason() const noexcept { return overlap_reason_; }

  // --- factors / explanation ----------------------------------------------
  void add_factor(DecisionFactor f) { factors_.push_back(std::move(f)); }
  const std::vector<DecisionFactor>& factors() const noexcept { return factors_; }
  void add_explanation(std::string s) { explanation_.push_back(std::move(s)); }
  const std::vector<std::string>& explanation() const noexcept { return explanation_; }
  void add_hard_constraint(std::string c) { hard_constraints_.push_back(std::move(c)); }
  const std::vector<std::string>& hard_constraints() const noexcept { return hard_constraints_; }

  // --- provenance / generations -------------------------------------------
  void set_provenance(ProvenanceKind p) noexcept { provenance_ = p; }
  ProvenanceKind provenance() const noexcept { return provenance_; }
  void set_policy_generation(PolicyGeneration g) noexcept { policy_generation_ = g; }
  PolicyGeneration policy_generation() const noexcept { return policy_generation_; }
  void set_topology_generation(TopologyGeneration g) noexcept { topology_generation_ = g; }
  TopologyGeneration topology_generation() const noexcept { return topology_generation_; }
  void set_health_generation(HealthGeneration g) noexcept { health_generation_ = g; }
  HealthGeneration health_generation() const noexcept { return health_generation_; }
  void set_collective_generation(CollectiveGeneration g) noexcept { collective_generation_ = g; }
  CollectiveGeneration collective_generation() const noexcept { return collective_generation_; }
  void set_backend_generation(BackendGeneration g) noexcept { backend_generation_ = g; }
  BackendGeneration backend_generation() const noexcept { return backend_generation_; }
  void set_source(Source s) { source_ = std::move(s); }
  const Source& source() const noexcept { return source_; }

private:
  Algorithm algorithm_ = Algorithm::UNKNOWN;
  BackendId backend_;
  TransportClass transport_ = TransportClass::UNKNOWN;
  std::uint64_t expected_byte_movement_ = 0;
  std::uint64_t expected_steps_ = 0;
  std::optional<std::uint64_t> expected_critical_path_ns_;
  std::vector<std::uint64_t> rank_order_;
  std::uint64_t hierarchy_depth_ = 0;
  bool overlap_eligible_ = false;
  std::string overlap_reason_;
  std::vector<DecisionFactor> factors_;
  std::vector<std::string> explanation_;
  std::vector<std::string> hard_constraints_;
  ProvenanceKind provenance_ = ProvenanceKind::UNKNOWN;
  PolicyGeneration policy_generation_;
  TopologyGeneration topology_generation_;
  HealthGeneration health_generation_;
  CollectiveGeneration collective_generation_;
  BackendGeneration backend_generation_;
  Source source_;
};

} // namespace collectivefabric
