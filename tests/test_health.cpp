#include "test_util.hpp"
#include "collectivefabric/health.hpp"
#include "collectivefabric/explanation.hpp"
using namespace collectivefabric;

int main() {
  HealthRecord h;
  h.state = HealthState::HEALTHY;
  h.generation = HealthGeneration(1);
  h.observed_utc_ns = 12345;
  h.source = make_source("health probe", ProvenanceKind::MEASURED);
  h.narrative = "all good";
  auto txt = explain_health(h);
  CF_CHECK(txt.find("HEALTHY") != std::string::npos);

  // health progression is monotone toward degraded
  HealthRecord h2 = h;
  h2.state = advance_health(HealthState::HEALTHY, FailureClass::TRANSPORT_FAILURE);
  CF_CHECK(h2.state == HealthState::DEGRADED);
  HealthRecord h3 = h2;
  h3.state = advance_health(HealthState::DEGRADED, FailureClass::WORKER_DEATH);
  CF_CHECK(h3.state == HealthState::DEGRADED);

  // a single transient failure does not imply permanent hardware failure
  HealthRecord h4;
  h4.state = advance_health(HealthState::UNKNOWN, FailureClass::TRANSPORT_FAILURE);
  CF_CHECK(h4.state == HealthState::DEGRADED);

  CF_FINISH("test_health");
}
