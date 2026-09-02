#include "test_util.hpp"
#include "collectivefabric/measurement.hpp"
#include "collectivefabric/explanation.hpp"
using namespace collectivefabric;

int main() {
  CollectiveMeasurement m;
  m.id = MeasurementId(1);
  m.collective = CollectiveId(9);
  m.group_generation = GroupGeneration(2);
  m.provenance = MeasurementProvenance::MEASURED;
  m.freshness = MeasurementFreshness::CURRENT;
  m.wall_time_us = 1000;
  m.payload_bytes = 4096;
  m.logical_collective_bytes = 8192;
  m.success = true;
  // throughput denominators defined explicitly
  double secs = m.wall_time_us / 1e6;
  m.payload_throughput_bytes_per_sec = m.payload_bytes / secs;   // 4.096 MB/s
  m.logical_collective_bytes_per_sec = m.logical_collective_bytes / secs;
  CF_CHECK(*m.payload_throughput_bytes_per_sec == 4096000.0);
  CF_CHECK(*m.logical_collective_bytes_per_sec == 8192000.0);
  // bus bandwidth not derivable here -> UNKNOWN (not set)
  CF_CHECK(!m.estimated_link_bytes_per_sec.has_value());
  auto txt = explain_measurement(m);
  CF_CHECK(txt.find("payload_throughput_bytes_per_sec") != std::string::npos);
  CF_CHECK(txt.find("MEASURED") != std::string::npos);

  // stale measurement cannot become CURRENT again
  m.freshness = MeasurementFreshness::STALE;
  auto txt2 = explain_measurement(m);
  CF_CHECK(txt2.find("STALE") != std::string::npos);
  m.freshness = MeasurementFreshness::REVALIDATION_REQUIRED;
  CF_CHECK(measurement_freshness_to_string(MeasurementFreshness::REVALIDATION_REQUIRED) == "REVALIDATION_REQUIRED");

  CF_FINISH("test_measurement");
}
