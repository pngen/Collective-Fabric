#include "collectivefabric/runtime.hpp"
#include "collectivefabric/backend/reference_backend.hpp"
#include "collectivefabric/explanation.hpp"
#include <cstdio>
using namespace collectivefabric;
int main() {
  Runtime rt;
  auto backend = std::make_shared<ReferenceBackend>(BackendId(1), BackendGeneration(1));
  rt.register_backend(backend);
  HealthRecord h;
  h.state = HealthState::HEALTHY; h.generation = HealthGeneration(1);
  h.source = make_source("health probe", ProvenanceKind::MEASURED);
  rt.update_health(h);
  printf("path_health=%s gen=%llu\n", std::string(health_state_to_string(rt.path_health())).c_str(), (unsigned long long)rt.health_generation().value());
  // A single failed collective degrades, not permanent hardware failure.
  auto h2 = h; h2.generation = HealthGeneration(2); h2.state = advance_health(HealthState::HEALTHY, FailureClass::TRANSPORT_FAILURE);
  rt.update_health(h2);
  printf("after transient failure path_health=%s (one failure does not imply permanent failure)\n", std::string(health_state_to_string(rt.path_health())).c_str());
  // Stale health report overwrite rejected.
  HealthRecord stale; stale.state=HealthState::UNHEALTHY; stale.generation=HealthGeneration(1);
  try { rt.update_health(stale); printf("stale health overwrite NOT rejected (bug)\n"); } catch (const std::exception& e) {
    printf("stale health overwrite rejected: %s\n", e.what());
  }
  return 0;
}
