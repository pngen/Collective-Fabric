#include "test_util.hpp"
#include "collectivefabric/store.hpp"
#include "collectivefabric/digest/canonical.hpp"
#include <cstdio>
#include <fstream>
#include <vector>
using namespace collectivefabric;

static PersistentState make_state() {
  PersistentState s;
  s.format_version = Store::kFormatVersion;
  s.epoch = CoordinatorEpoch(2);
  s.live_boot = WorkerBootId(100);
  PersistedGroup g;
  g.group_id = CollectiveGroupId(7); g.generation = GroupGeneration(2);
  g.membership = MembershipGeneration(2); g.topology = TopologyGeneration(1);
  g.health = HealthGeneration(1); g.backend = BackendGeneration(1);
  g.lifecycle = GroupLifecycle::READY; g.provenance = "recovered";
  PersistedParticipant p;
  p.participant_id = ParticipantId(1); p.worker = WorkerId(1); p.boot = WorkerBootId(100);
  p.node = NodeId(1); p.device = DeviceId(1); p.rank = RankId(0); p.backend_generation = BackendGeneration(1); p.is_known = true;
  g.participants.push_back(p);
  PersistedParticipant p2;
  p2.participant_id = ParticipantId(2); p2.worker = WorkerId(2); p2.boot = WorkerBootId(200);
  p2.node = NodeId(1); p2.device = DeviceId(2); p2.rank = RankId(1); p2.backend_generation = BackendGeneration(1); p2.is_known = true;
  g.participants.push_back(p2);
  s.groups.push_back(g);
  PersistedMeasurement m;
  m.id = MeasurementId(5); m.collective = CollectiveId(1); m.collective_generation = CollectiveGeneration(2);
  m.group_generation = GroupGeneration(2); m.algorithm = Algorithm::RING; m.backend = BackendId(1);
  m.rank_count = 2; m.payload_bytes = 16; m.wall_time_us = 5; m.provenance = MeasurementProvenance::MEASURED;
  m.freshness = MeasurementFreshness::CURRENT; m.timestamp_ns = 999; m.measurement_generation = MeasurementGeneration(1);
  m.success = true;
  s.measurements.push_back(m);
  return s;
}

int main() {
  const char* path = "test_state.bin";
  auto s = make_state();
  auto before = Store::semantic_digest(s);
  auto saved = Store::save(path, s);
  CF_CHECK(saved.bytes > 0);
  auto [loaded, res] = Store::load(path);
  auto after = Store::semantic_digest(loaded);
  CF_CHECK(before == after);            // semantics preserved exactly
  CF_CHECK(loaded.epoch == CoordinatorEpoch(2));
  CF_CHECK(loaded.live_boot == WorkerBootId(100));
  CF_CHECK(loaded.groups.size() == 1);
  CF_CHECK(loaded.groups[0].participants.size() == 2);
  CF_CHECK(loaded.measurements.size() == 1);
  CF_CHECK(loaded.measurements[0].id == MeasurementId(5));
  CF_CHECK(res.bytes == saved.bytes);

  // truncation rejection
  remove("trunc.bin");
  std::vector<std::uint8_t> t = Store::encode(s);
  { std::ofstream o("trunc.bin", std::ios::binary); o.write(reinterpret_cast<const char*>(t.data()), t.size()/2); }
  CF_CHECK_THROWS(Store::load("trunc.bin"));

  // corruption / checksum mismatch
  Store::save("corrupt.bin", s);
  { std::fstream f("corrupt.bin", std::ios::in | std::ios::out | std::ios::binary); f.seekg(0, std::ios::end); auto len = f.tellg(); f.seekg(20, std::ios::beg); char c; f.read(&c,1); c ^= 0x55; f.seekp(20, std::ios::beg); f.write(&c,1); }
  CF_CHECK_THROWS(Store::load("corrupt.bin"));

  // bad magic
  { std::fstream f("badmagic.bin", std::ios::out | std::ios::binary); f.write("XXXX", 4); std::uint32_t v=1; f.write(reinterpret_cast<const char*>(&v), 4); }
  CF_CHECK_THROWS(Store::load("badmagic.bin"));

  // trailing garbage rejection: append bytes after a valid frame
  Store::save("trailing.bin", s);
  { std::ofstream o("trailing.bin", std::ios::binary | std::ios::app); std::uint8_t extra = 0xFF; o.write(reinterpret_cast<const char*>(&extra), 1); }
  CF_CHECK_THROWS(Store::load("trailing.bin"));

  // impossible count (duplicate group id/gen) rejection
  auto dupc = s;
  dupc.groups.push_back(dupc.groups[0]);  // duplicate (id, gen)
  CF_CHECK_THROWS(Store::decode(Store::encode(dupc)));

  // invalid enum rejection
  auto bad = s;
  // corrupt the lifecycle byte of the first group by re-encoding with a bad value
  CanonicalWriter w;
  w.u64(bad.format_version); w.u64(bad.epoch.value()); w.u64(bad.live_boot.raw());
  w.u64(1);  // group count
  auto& g0 = bad.groups[0];
  w.u64(g0.group_id.raw()); w.u64(g0.generation.value()); w.u64(g0.membership.value()); w.u64(g0.topology.value());
  w.u64(g0.health.value()); w.u64(g0.backend.value());
  w.u8(0xFE);  // invalid lifecycle
  w.string(g0.provenance);
  std::vector<std::uint8_t> payload(w.data().begin(), w.data().end());
  CF_CHECK_THROWS(Store::decode(payload));

  remove(path); remove("trunc.bin"); remove("corrupt.bin"); remove("badmagic.bin"); remove("trailing.bin");
  CF_FINISH("test_persistence");
}
