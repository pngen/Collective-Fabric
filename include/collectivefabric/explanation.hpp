#pragma once
// Collective Fabric - explanation API. Every selection, rejection, failure,
// health, measurement, and recovery decision is inspectable through named
// factors rather than a single unexplained scalar.
#include "collectivefabric/collective/plan.hpp"
#include "collectivefabric/collective/descriptor.hpp"
#include "collectivefabric/health.hpp"
#include "collectivefabric/measurement.hpp"
#include "collectivefabric/foundation/enums.hpp"
#include <sstream>
#include <string>

namespace collectivefabric {

inline std::string explain_plan(const CollectivePlan& plan) {
  std::ostringstream os;
  os << "plan algorithm=" << algorithm_to_string(plan.algorithm())
     << " backend=" << plan.backend().raw()
     << " transport=" << transport_class_to_string(plan.transport())
     << " expected_steps=" << plan.expected_steps()
     << " expected_byte_movement=" << plan.expected_byte_movement()
     << " overlap_eligible=" << (plan.overlap_eligible() ? "true" : "false")
     << " provenance=" << provenance_kind_to_string(plan.provenance());
  for (const auto& f : plan.factors()) {
    os << "\n  factor " << f.name << "=" << f.value << " (" << f.unit << ")";
    if (!f.reason.empty()) os << " :: " << f.reason;
  }
  for (const auto& e : plan.explanation()) os << "\n  " << e;
  return os.str();
}

inline std::string explain_rejection(const std::string& reason, const CollectiveDescriptor& d) {
  std::ostringstream os;
  os << "rejected collective " << collective_kind_to_string(d.kind()) << " "
     << "datatype=" << datatype_to_string(d.datatype());
  if (!reason.empty()) os << " :: " << reason;
  return os.str();
}

inline std::string explain_collective(const CollectiveDescriptor& d, const CollectivePlan& p) {
  std::ostringstream os;
  os << "collective " << collective_kind_to_string(d.kind())
     << " group=" << d.group_id().raw() << " gen=" << d.group_generation().value()
     << " collective_gen=" << d.collective_generation().value()
     << " ranks=" << p.rank_order().size()
     << " payload_bytes=" << d.element_byte_count()
     << " algorithm=" << algorithm_to_string(p.algorithm());
  return os.str();
}

inline std::string explain_health(const HealthRecord& h) {
  std::ostringstream os;
  os << "health=" << health_state_to_string(h.state)
     << " generation=" << h.generation.value();
  if (!h.narrative.empty()) os << " :: " << h.narrative;
  return os.str();
}

inline std::string explain_measurement(const CollectiveMeasurement& m) {
  std::ostringstream os;
  os << "measurement collective=" << m.collective.raw()
     << " provenance=" << measurement_provenance_to_string(m.provenance)
     << " freshness=" << measurement_freshness_to_string(m.freshness)
     << " wall_time_us=" << m.wall_time_us
     << " payload_bytes=" << m.payload_bytes;
  if (m.payload_throughput_bytes_per_sec)
    os << " payload_throughput_bytes_per_sec=" << *m.payload_throughput_bytes_per_sec;
  if (m.estimated_link_bytes_per_sec)
    os << " estimated_link_bytes_per_sec=" << *m.estimated_link_bytes_per_sec;
  if (!m.narrative.empty()) os << " :: " << m.narrative;
  return os.str();
}

inline std::string explain_recovery(const std::string& what, MeasurementFreshness freshness) {
  std::ostringstream os;
  os << "recovery " << what << " freshness=" << measurement_freshness_to_string(freshness);
  return os.str();
}

} // namespace collectivefabric
