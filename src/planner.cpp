#include "collectivefabric/planner/planner.hpp"
#include "collectivefabric/foundation/checked.hpp"
#include <algorithm>
#include <cmath>
#include <limits>
#include <string>
#include <vector>

namespace collectivefabric {

namespace {

std::uint64_t ceil_log2(std::uint64_t n) {
  std::uint64_t s = 0;
  std::uint64_t x = 1;
  while (x < n) { x <<= 1; ++s; }
  return s;
}

struct PathModel {
  std::uint64_t steps = 0;
  std::uint64_t movement = 0;
};

bool is_reduce_family(CollectiveKind k) {
  return k == CollectiveKind::REDUCE || k == CollectiveKind::ALL_REDUCE || k == CollectiveKind::REDUCE_SCATTER;
}

std::uint64_t sat_mul(std::uint64_t a, std::uint64_t b) {
  auto r = checked_mul(a, b);
  return r ? *r : std::numeric_limits<std::uint64_t>::max();
}

PathModel model_path(CollectiveKind kind, Algorithm algo, std::uint64_t n, std::uint64_t p) {
  PathModel m;
  const std::uint64_t logn = ceil_log2(n);
  switch (algo) {
    case Algorithm::DIRECT:
      m.steps = 1;
      m.movement = sat_mul(p, n > 0 ? (n - 1) : 0);
      break;
    case Algorithm::RING:
      m.steps = n > 0 ? (n - 1) : 0;
      m.movement = sat_mul(p, m.steps);
      break;
    case Algorithm::TREE:
      m.steps = (kind == CollectiveKind::BROADCAST || kind == CollectiveKind::REDUCE) ? logn : sat_mul(2, logn);
      m.movement = sat_mul(p, m.steps);
      break;
    case Algorithm::RECURSIVE_DOUBLING:
      m.steps = logn;
      m.movement = sat_mul(sat_mul(p, logn), 2);
      break;
    case Algorithm::REDUCE_BROADCAST:
      m.steps = sat_mul(2, n > 0 ? (n - 1) : 0);
      m.movement = sat_mul(p, m.steps);
      break;
    case Algorithm::HIERARCHICAL:
      m.steps = sat_mul(2, logn);
      m.movement = sat_mul(p, m.steps);
      break;
    case Algorithm::BACKEND_DEFAULT:
      m.steps = 1;
      m.movement = p;
      break;
    case Algorithm::UNKNOWN:
      m.steps = 0;
      m.movement = 0;
      break;
  }
  return m;
}

bool algorithm_feasible(CollectiveKind kind, Algorithm a) {
  switch (a) {
    case Algorithm::DIRECT:
      return kind == CollectiveKind::BROADCAST || kind == CollectiveKind::REDUCE;
    case Algorithm::RING:
      return kind == CollectiveKind::ALL_REDUCE || kind == CollectiveKind::REDUCE_SCATTER ||
             kind == CollectiveKind::ALL_GATHER || kind == CollectiveKind::REDUCE || kind == CollectiveKind::BROADCAST;
    case Algorithm::TREE:
      return kind == CollectiveKind::BARRIER || kind == CollectiveKind::BROADCAST || kind == CollectiveKind::REDUCE ||
             kind == CollectiveKind::ALL_REDUCE || kind == CollectiveKind::ALL_GATHER || kind == CollectiveKind::REDUCE_SCATTER;
    case Algorithm::RECURSIVE_DOUBLING:
      return kind == CollectiveKind::ALL_REDUCE || kind == CollectiveKind::ALL_GATHER;
    case Algorithm::REDUCE_BROADCAST:
      return kind == CollectiveKind::ALL_REDUCE || kind == CollectiveKind::REDUCE;
    case Algorithm::HIERARCHICAL:
      return kind == CollectiveKind::ALL_REDUCE || kind == CollectiveKind::ALL_GATHER || kind == CollectiveKind::REDUCE_SCATTER;
    case Algorithm::BACKEND_DEFAULT:
      return true;
    case Algorithm::UNKNOWN:
      return false;
  }
  return false;
}

std::string build_explanation(const CollectiveDescriptor& d, const PathModel& chosen,
                              std::uint64_t n, std::uint64_t p, const PlannerInput& in,
                              Algorithm selected) {
  std::string s = std::string(algorithm_to_string(selected)) + " selected for " +
      std::string(collective_kind_to_string(d.kind())) + " with " + std::to_string(n) + " ranks and " +
      std::to_string(p) + " payload bytes";
  if (chosen.steps <= 1) s += "; direct/one-shot path has minimal critical path";
  double bdp = 0;
  if (in.measured_bandwidth_bytes_per_sec && in.measured_latency_ns && *in.measured_latency_ns > 0)
    bdp = (static_cast<double>(*in.measured_bandwidth_bytes_per_sec) * static_cast<double>(*in.measured_latency_ns)) / 1e9;
  if (bdp > 0) {
    if (static_cast<double>(p) < bdp) s += "; message is small relative to bandwidth-delay product, so latency-favoring choice dominates";
    else s += "; message is large relative to bandwidth-delay product, so bandwidth-favoring choice dominates";
  } else {
    s += "; no measured bandwidth/latency, so the planner ranks on structural steps and byte movement";
  }
  return s;
}

} // namespace

std::vector<Planner::Candidate> Planner::candidates(const PlannerInput& in) const {
  std::vector<Candidate> out;
  if (!in.descriptor || !in.group || !in.backend) return out;
  const auto& d = *in.descriptor;
  const std::uint64_t n = in.group->rank_count();
  const std::uint64_t p = (d.kind() == CollectiveKind::BROADCAST || d.kind() == CollectiveKind::REDUCE ||
                           d.kind() == CollectiveKind::ALL_REDUCE)
                          ? d.element_byte_count()
                          : d.aggregate_byte_count(n);
  const bool any_transport_ok = in.backend->supports_transport(TransportClass::HOST_MEMORY);
  const bool rank_ok = !in.backend->maximum_rank_count || n <= *in.backend->maximum_rank_count;
  const bool buffer_ok = d.input_placement() == BufferPlacement::HOST && in.backend->supports_host_buffers;

  for (Algorithm a : {Algorithm::DIRECT, Algorithm::RING, Algorithm::TREE, Algorithm::RECURSIVE_DOUBLING,
                      Algorithm::REDUCE_BROADCAST, Algorithm::HIERARCHICAL, Algorithm::BACKEND_DEFAULT, Algorithm::UNKNOWN}) {
    Candidate c;
    c.algorithm = a;
    c.feasible = true;
    if (!algorithm_feasible(d.kind(), a)) {
      c.feasible = false;
      c.reject_reason = "algorithm not applicable to collective kind";
    } else if (!in.backend->supports_kind(d.kind())) {
      c.feasible = false;
      c.reject_reason = "backend does not support collective kind";
    } else if (!in.backend->supports_datatype(d.datatype())) {
      c.feasible = false;
      c.reject_reason = "backend does not support datatype";
    } else if (is_reduce_family(d.kind()) && !in.backend->supports_reduction(d.reduction())) {
      c.feasible = false;
      c.reject_reason = "backend does not support reduction";
    } else if (!any_transport_ok) {
      c.feasible = false;
      c.reject_reason = "no compatible host transport";
    } else if (!rank_ok) {
      c.feasible = false;
      c.reject_reason = "rank count exceeds backend maximum";
    } else if (!buffer_ok) {
      c.feasible = false;
      c.reject_reason = "backend does not support requested buffer placement";
    } else if (a == Algorithm::HIERARCHICAL && in.topology && in.topology->node_count() < 2) {
      c.feasible = false;
      c.reject_reason = "topology contains no inter-node subgroup boundary";
    }
    if (c.feasible) {
      auto pm = model_path(d.kind(), a, n, p);
      c.steps = pm.steps;
      c.byte_movement = pm.movement;
    }
    out.push_back(c);
  }
  return out;
}

CollectivePlan Planner::plan(const PlannerInput& in) const {
  if (!in.descriptor || !in.group || !in.backend) {
    throw Error(ErrorCode::VALIDATION, "planner requires descriptor, group, and backend");
  }
  const auto& d = *in.descriptor;
  const std::uint64_t n = in.group->rank_count();
  const std::uint64_t p = (d.kind() == CollectiveKind::BROADCAST || d.kind() == CollectiveKind::REDUCE ||
                           d.kind() == CollectiveKind::ALL_REDUCE)
                          ? d.element_byte_count()
                          : d.aggregate_byte_count(n);
  const auto cands = candidates(in);

  auto est_time = [&](const Candidate& c) -> std::optional<double> {
    double t = 0.0;
    bool any = false;
    if (in.measured_bandwidth_bytes_per_sec && *in.measured_bandwidth_bytes_per_sec > 0) {
      t += (static_cast<double>(c.byte_movement) / static_cast<double>(*in.measured_bandwidth_bytes_per_sec)) * 1e9;
      any = true;
    }
    if (in.measured_latency_ns) {
      t += static_cast<double>(c.steps) * static_cast<double>(*in.measured_latency_ns);
      any = true;
    }
    if (!any) return std::nullopt;
    return t;
  };

  const Candidate* best = nullptr;
  for (const auto& c : cands) {
    if (!c.feasible) continue;
    if (!best) { best = &c; continue; }
    bool better = false;
    auto ta = est_time(c);
    auto tb = est_time(*best);
    if (ta.has_value() && tb.has_value()) {
      better = *ta < *tb - 1e-12;
      if (!better && std::abs(*ta - *tb) < 1e-12) {
        // deterministic tie-break
        if (c.steps != best->steps) better = c.steps < best->steps;
        else if (c.byte_movement != best->byte_movement) better = c.byte_movement < best->byte_movement;
        else better = static_cast<int>(c.algorithm) < static_cast<int>(best->algorithm);
      }
    } else {
      if (c.steps != best->steps) better = c.steps < best->steps;
      else if (c.byte_movement != best->byte_movement) better = c.byte_movement < best->byte_movement;
      else better = static_cast<int>(c.algorithm) < static_cast<int>(best->algorithm);
    }
    if (better) best = &c;
  }

  if (!best) throw Error(ErrorCode::UNSUPPORTED, "no feasible algorithm for the requested collective");

  auto pm = model_path(d.kind(), best->algorithm, n, p);
  CollectivePlan plan;
  plan.set_algorithm(best->algorithm);
  plan.set_backend(in.backend->id);
  plan.set_transport(TransportClass::HOST_MEMORY);
  plan.set_expected_steps(pm.steps);
  plan.set_expected_byte_movement(pm.movement);
  {
    std::vector<std::uint64_t> o(n);
    for (std::uint64_t i = 0; i < n; ++i) o[i] = i;
    plan.set_rank_order(std::move(o));
  }
  plan.set_hierarchy_depth(in.topology && in.topology->node_count() >= 2 ? 2 : 0);
  plan.set_provenance(ProvenanceKind::DERIVED);
  plan.set_policy_generation(d.policy_generation());
  plan.set_topology_generation(in.topology ? in.topology->generation() : TopologyGeneration{});
  plan.set_collective_generation(d.collective_generation());
  plan.set_backend_generation(in.backend->generation);
  plan.set_health_generation(in.path_health ? HealthGeneration(1) : HealthGeneration{});
  plan.set_source(make_source("deterministic planner selection", ProvenanceKind::DERIVED));

  plan.add_factor(DecisionFactor{"collective_kind", static_cast<double>(static_cast<int>(d.kind())), "enum", true,
    std::string(collective_kind_to_string(d.kind()))});
  plan.add_factor(DecisionFactor{"rank_count", static_cast<double>(n), "ranks", true, "participant count at group generation"});
  plan.add_factor(DecisionFactor{"payload_bytes", static_cast<double>(p), "bytes", true, "per-rank payload"});
  plan.add_factor(DecisionFactor{"expected_steps", static_cast<double>(pm.steps), "steps", false,
    "critical path length in communication rounds"});
  plan.add_factor(DecisionFactor{"expected_byte_movement", static_cast<double>(pm.movement), "bytes", false,
    "total bytes that cross at least one link"});
  if (in.measured_bandwidth_bytes_per_sec)
    plan.add_factor(DecisionFactor{"measured_bandwidth_bytes_per_sec", static_cast<double>(*in.measured_bandwidth_bytes_per_sec),
      "bytes/s", false, "measured"});
  else
    plan.add_factor(DecisionFactor{"measured_bandwidth_bytes_per_sec", 0, "unknown", false,
      "not measured; link bandwidth UNKNOWN"});
  if (in.measured_latency_ns)
    plan.add_factor(DecisionFactor{"measured_latency_ns", static_cast<double>(*in.measured_latency_ns), "ns", false, "measured"});
  else
    plan.add_factor(DecisionFactor{"measured_latency_ns", 0, "unknown", false, "not measured; latency UNKNOWN"});
  if (in.topology) {
    plan.add_factor(DecisionFactor{"topology_diameter_nodes", static_cast<double>(in.topology->node_count()), "nodes", false,
      "distinct nodes in topology"});
    plan.add_factor(DecisionFactor{"intra_node_device_pairs", static_cast<double>(in.topology->intra_node_device_pairs()),
      "pairs", false, "intra-node locality"});
  }
  plan.add_factor(DecisionFactor{"device_buffer_support", in.backend->supports_device_buffers ? 1.0 : 0.0, "bool", false,
    in.backend->supports_device_buffers ? "backend supports device buffers" : "backend is host-buffer only"});

  for (const auto& c : cands) if (!c.feasible) plan.add_hard_constraint(c.reject_reason);

  plan.add_explanation(build_explanation(d, pm, n, p, in, best->algorithm));

  bool overlap = true;
  std::string overlap_reason = "independent stream/queue semantics; no in-place hazard identified";
  if (!in.backend->supports_nonblocking_progress) { overlap = false; overlap_reason = "backend has no nonblocking progress"; }
  if (!in.backend->supports_host_buffers && d.input_placement() == BufferPlacement::DEVICE) {
    overlap = false; overlap_reason = "device buffer backend requires binding";
  }
  plan.set_overlap_eligible(overlap, overlap_reason);

  return plan;
}

} // namespace collectivefabric
