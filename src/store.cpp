#include "collectivefabric/store.hpp"
#include "collectivefabric/digest/canonical.hpp"
#include "collectivefabric/digest/crc32.hpp"
#include "collectivefabric/foundation/errors.hpp"
#include "collectivefabric/foundation/clock.hpp"
#include <cstdio>
#include <cstring>
#include <fstream>
#include <filesystem>
#include <unordered_set>
#include <set>
#ifdef _WIN32
#include <windows.h>
#endif

namespace collectivefabric {

namespace {
constexpr std::uint64_t kMaxCount = 10'000'000;
constexpr std::uint64_t kHeaderBytes = 4 + 4 + 8 + 4;  // magic + version + length + crc

bool valid_lifecycle(std::uint8_t v) { return v <= static_cast<std::uint8_t>(GroupLifecycle::RETIRED); }
bool valid_algorithm(std::uint8_t v) { return v <= static_cast<std::uint8_t>(Algorithm::UNKNOWN); }
bool valid_prov(std::uint8_t v) { return v <= static_cast<std::uint8_t>(MeasurementProvenance::UNKNOWN); }
bool valid_fresh(std::uint8_t v) { return v <= static_cast<std::uint8_t>(MeasurementFreshness::UNKNOWN); }
bool valid_health(std::uint8_t v) { return v <= static_cast<std::uint8_t>(HealthState::UNKNOWN); }

void write_group(CanonicalWriter& w, const PersistedGroup& g) {
  w.u64(g.group_id.raw()); w.u64(g.generation.value()); w.u64(g.membership.value());
  w.u64(g.topology.value()); w.u64(g.health.value()); w.u64(g.backend.value());
  w.u8(static_cast<std::uint8_t>(g.lifecycle)); w.string(g.provenance);
  w.u64(g.participants.size());
  for (const auto& p : g.participants) {
    w.u64(p.participant_id.raw()); w.u64(p.worker.raw()); w.u64(p.boot.raw());
    w.u64(p.node.raw()); w.u64(p.device.raw()); w.u64(p.rank.raw());
    w.u64(p.backend_generation.value()); w.boolean(p.is_known);
  }
}

void write_measurement(CanonicalWriter& w, const PersistedMeasurement& m) {
  w.u64(m.id.raw()); w.u64(m.collective.raw()); w.u64(m.collective_generation.value());
  w.u64(m.group_generation.value()); w.u8(static_cast<std::uint8_t>(m.algorithm));
  w.u64(m.backend.raw()); w.u64(m.rank_count); w.u64(m.payload_bytes); w.u64(m.wall_time_us);
  w.u8(static_cast<std::uint8_t>(m.provenance)); w.u8(static_cast<std::uint8_t>(m.freshness));
  w.u64(m.timestamp_ns); w.u64(m.measurement_generation.value()); w.boolean(m.success);
}

} // namespace

std::vector<std::uint8_t> Store::encode(const PersistentState& s) {
  CanonicalWriter w;
  w.u64(s.format_version);
  w.u64(s.epoch.value());
  w.u64(s.live_boot.raw());
  w.u64(s.groups.size());
  for (const auto& g : s.groups) write_group(w, g);
  w.u64(s.measurements.size());
  for (const auto& m : s.measurements) write_measurement(w, m);
  w.boolean(s.health.has_value());
  if (s.health) {
    w.u8(static_cast<std::uint8_t>(s.health->state)); w.u64(s.health->generation.value());
    w.u64(s.health->observed_ns); w.string(s.health->narrative);
  }
  w.u64(s.backends.size());
  for (const auto& b : s.backends) { w.u64(b.id.raw()); w.u64(b.generation.value()); w.string(b.name); }
  w.u64(s.policy_count);
  w.u64(s.saved_ns);
  return std::vector<std::uint8_t>(w.data().begin(), w.data().end());
}

PersistentState Store::decode(std::span<const std::uint8_t> payload) {
  CanonicalReader r(payload);
  PersistentState s;
  if (!r.u64(s.format_version)) throw Error(ErrorCode::DECODE, "store: truncated format version");
  if (s.format_version > kFormatVersion) throw Error(ErrorCode::DECODE, "store: unsupported version");
  std::uint64_t epoch_v, live_boot_v;
  if (!r.u64(epoch_v) || !r.u64(live_boot_v)) throw Error(ErrorCode::DECODE, "store: truncated epoch/boot");
  s.epoch = make_generation<CoordinatorEpochTag>(epoch_v);
  s.live_boot = WorkerBootId(live_boot_v);

  std::uint64_t gcount, mcount, bcount;
  if (!r.u64(gcount) || gcount > kMaxCount) throw Error(ErrorCode::DECODE, "store: invalid group count");
  std::set<std::pair<CollectiveGroupId, GroupGeneration>> seen_groups;
  for (std::uint64_t i = 0; i < gcount; ++i) {
    PersistedGroup g;
    std::uint64_t t;
    if (!r.u64(t)) throw Error(ErrorCode::DECODE, "store: truncated group id");
    g.group_id = CollectiveGroupId(t);
    if (!r.u64(t)) throw Error(ErrorCode::DECODE, "store: truncated group generation");
    g.generation = make_generation<GroupGenerationTag>(t);
    if (!r.u64(t)) throw Error(ErrorCode::DECODE, "store: truncated membership");
    g.membership = make_generation<MembershipGenerationTag>(t);
    if (!r.u64(t)) throw Error(ErrorCode::DECODE, "store: truncated topology");
    g.topology = make_generation<TopologyGenerationTag>(t);
    if (!r.u64(t)) throw Error(ErrorCode::DECODE, "store: truncated health");
    g.health = make_generation<HealthGenerationTag>(t);
    if (!r.u64(t)) throw Error(ErrorCode::DECODE, "store: truncated backend");
    g.backend = make_generation<BackendGenerationTag>(t);
    std::uint8_t lc; if (!r.u8(lc)) throw Error(ErrorCode::DECODE, "store: truncated lifecycle");
    if (!valid_lifecycle(lc)) throw Error(ErrorCode::DECODE, "store: invalid lifecycle enum");
    g.lifecycle = static_cast<GroupLifecycle>(lc);
    if (!r.string(g.provenance)) throw Error(ErrorCode::DECODE, "store: truncated provenance");
    std::uint64_t pc; if (!r.u64(pc) || pc > kMaxCount) throw Error(ErrorCode::DECODE, "store: invalid participant count");
    std::unordered_set<ParticipantId> pid_seen;
    std::unordered_set<RankId> rank_seen;
    for (std::uint64_t j = 0; j < pc; ++j) {
      PersistedParticipant p;
      std::uint64_t v;
      if (!r.u64(v)) throw Error(ErrorCode::DECODE, "store: truncated participant id");
      p.participant_id = ParticipantId(v);
      if (!r.u64(v)) throw Error(ErrorCode::DECODE, "store: truncated worker");
      p.worker = WorkerId(v);
      if (!r.u64(v)) throw Error(ErrorCode::DECODE, "store: truncated boot");
      p.boot = WorkerBootId(v);
      if (!r.u64(v)) throw Error(ErrorCode::DECODE, "store: truncated node");
      p.node = NodeId(v);
      if (!r.u64(v)) throw Error(ErrorCode::DECODE, "store: truncated device");
      p.device = DeviceId(v);
      if (!r.u64(v)) throw Error(ErrorCode::DECODE, "store: truncated rank");
      p.rank = RankId(v);
      if (!r.u64(v)) throw Error(ErrorCode::DECODE, "store: truncated backend gen");
      p.backend_generation = make_generation<BackendGenerationTag>(v);
      if (!r.boolean(p.is_known)) throw Error(ErrorCode::DECODE, "store: truncated known");
      if (!pid_seen.insert(p.participant_id).second) throw Error(ErrorCode::DECODE, "store: duplicate participant id");
      if (!rank_seen.insert(p.rank).second) throw Error(ErrorCode::DECODE, "store: duplicate participant rank");
      g.participants.push_back(p);
    }
    if (seen_groups.count({g.group_id, g.generation})) throw Error(ErrorCode::DECODE, "store: duplicate group id/generation");
    seen_groups.insert({g.group_id, g.generation});
    s.groups.push_back(g);
  }

  if (!r.u64(mcount) || mcount > kMaxCount) throw Error(ErrorCode::DECODE, "store: invalid measurement count");
  std::unordered_set<MeasurementId> mid_seen;
  for (std::uint64_t i = 0; i < mcount; ++i) {
    PersistedMeasurement m;
    std::uint64_t v;
    if (!r.u64(v)) throw Error(ErrorCode::DECODE, "store: truncated measurement id");
    m.id = MeasurementId(v);
    if (!r.u64(v)) throw Error(ErrorCode::DECODE, "store: truncated collective");
    m.collective = CollectiveId(v);
    if (!r.u64(v)) throw Error(ErrorCode::DECODE, "store: truncated collective gen");
    m.collective_generation = make_generation<CollectiveGenerationTag>(v);
    if (!r.u64(v)) throw Error(ErrorCode::DECODE, "store: truncated group gen");
    m.group_generation = make_generation<GroupGenerationTag>(v);
    std::uint8_t al; if (!r.u8(al)) throw Error(ErrorCode::DECODE, "store: truncated algorithm");
    if (!valid_algorithm(al)) throw Error(ErrorCode::DECODE, "store: invalid algorithm enum");
    m.algorithm = static_cast<Algorithm>(al);
    if (!r.u64(v)) throw Error(ErrorCode::DECODE, "store: truncated backend");
    m.backend = BackendId(v);
    if (!r.u64(m.rank_count) || !r.u64(m.payload_bytes) || !r.u64(m.wall_time_us)) throw Error(ErrorCode::DECODE, "store: truncated measurement counters");
    std::uint8_t pr, fr;
    if (!r.u8(pr) || !r.u8(fr)) throw Error(ErrorCode::DECODE, "store: truncated provenance/freshness");
    if (!valid_prov(pr) || !valid_fresh(fr)) throw Error(ErrorCode::DECODE, "store: invalid measurement enum");
    m.provenance = static_cast<MeasurementProvenance>(pr);
    m.freshness = static_cast<MeasurementFreshness>(fr);
    if (!r.u64(m.timestamp_ns)) throw Error(ErrorCode::DECODE, "store: truncated timestamp");
    if (!r.u64(v)) throw Error(ErrorCode::DECODE, "store: truncated measurement gen");
    m.measurement_generation = make_generation<MeasurementGenerationTag>(v);
    if (!r.boolean(m.success)) throw Error(ErrorCode::DECODE, "store: truncated success");
    if (!mid_seen.insert(m.id).second) throw Error(ErrorCode::DECODE, "store: duplicate measurement id");
    s.measurements.push_back(m);
  }

  bool has_health; if (!r.boolean(has_health)) throw Error(ErrorCode::DECODE, "store: truncated health flag");
  if (has_health) {
    PersistedHealth h;
    std::uint8_t st; if (!r.u8(st)) throw Error(ErrorCode::DECODE, "store: truncated health state");
    if (!valid_health(st)) throw Error(ErrorCode::DECODE, "store: invalid health enum");
    h.state = static_cast<HealthState>(st);
    std::uint64_t v;
    if (!r.u64(v)) throw Error(ErrorCode::DECODE, "store: truncated health gen");
    h.generation = make_generation<HealthGenerationTag>(v);
    if (!r.u64(h.observed_ns)) throw Error(ErrorCode::DECODE, "store: truncated health time");
    if (!r.string(h.narrative)) throw Error(ErrorCode::DECODE, "store: truncated health narrative");
    s.health = h;
  }

  if (!r.u64(bcount) || bcount > kMaxCount) throw Error(ErrorCode::DECODE, "store: invalid backend count");
  for (std::uint64_t i = 0; i < bcount; ++i) {
    PersistedBackend b;
    std::uint64_t v;
    if (!r.u64(v)) throw Error(ErrorCode::DECODE, "store: truncated backend id");
    b.id = BackendId(v);
    if (!r.u64(v)) throw Error(ErrorCode::DECODE, "store: truncated backend gen");
    b.generation = make_generation<BackendGenerationTag>(v);
    if (!r.string(b.name)) throw Error(ErrorCode::DECODE, "store: truncated backend name");
    s.backends.push_back(b);
  }
  if (!r.u64(s.policy_count)) throw Error(ErrorCode::DECODE, "store: truncated policy count");
  if (!r.u64(s.saved_ns)) throw Error(ErrorCode::DECODE, "store: truncated saved time");
  if (!r.at_end()) throw Error(ErrorCode::DECODE, "store: trailing garbage in payload");
  return s;
}

Sha256::Digest Store::semantic_digest(const PersistentState& s) {
  auto payload = encode(s);
  return Sha256::digest(payload);
}

Store::SaveResult Store::save(const std::string& path, const PersistentState& s) {
  auto payload = encode(s);
  std::vector<std::uint8_t> frame;
  frame.reserve(kHeaderBytes + payload.size());
  frame.push_back(static_cast<std::uint8_t>(kMagic >> 24));
  frame.push_back(static_cast<std::uint8_t>(kMagic >> 16));
  frame.push_back(static_cast<std::uint8_t>(kMagic >> 8));
  frame.push_back(static_cast<std::uint8_t>(kMagic));
  for (int i = 3; i >= 0; --i) frame.push_back(static_cast<std::uint8_t>((kFormatVersion >> (i * 8)) & 0xff));
  for (int i = 7; i >= 0; --i) frame.push_back(static_cast<std::uint8_t>((payload.size() >> (i * 8)) & 0xff));
  std::uint32_t crc = Crc32::compute(payload);
  for (int i = 3; i >= 0; --i) frame.push_back(static_cast<std::uint8_t>((crc >> (i * 8)) & 0xff));
  frame.insert(frame.end(), payload.begin(), payload.end());

  const std::string tmp = path + ".tmp";
  {
    std::ofstream os(tmp, std::ios::binary | std::ios::trunc);
    if (!os) throw Error(ErrorCode::IO, "store: cannot open temp file");
    os.write(reinterpret_cast<const char*>(frame.data()), static_cast<std::streamsize>(frame.size()));
    os.flush();
    if (!os) throw Error(ErrorCode::IO, "store: write failed");
  }
#ifdef _WIN32
  std::filesystem::path src(tmp);
  std::filesystem::path dst(path);
  if (!MoveFileExW(src.c_str(), dst.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
    throw Error(ErrorCode::IO, "store: atomic rename failed");
  }
#else
  std::filesystem::rename(tmp, path);
#endif
  return SaveResult{Sha256::digest(payload), frame.size()};
}

std::pair<PersistentState, Store::SaveResult> Store::load(const std::string& path) {
  std::ifstream is(path, std::ios::binary | std::ios::ate);
  if (!is) throw Error(ErrorCode::IO, "store: cannot open state file");
  std::streamoff sz = is.tellg();
  if (sz < static_cast<std::streamoff>(kHeaderBytes)) throw Error(ErrorCode::DECODE, "store: file too small");
  std::vector<std::uint8_t> buf(static_cast<std::size_t>(sz));
  is.seekg(0); is.read(reinterpret_cast<char*>(buf.data()), static_cast<std::streamsize>(sz));
  if (!is) throw Error(ErrorCode::IO, "store: read failed");

  std::uint32_t magic = (std::uint32_t(buf[0]) << 24) | (std::uint32_t(buf[1]) << 16) | (std::uint32_t(buf[2]) << 8) | buf[3];
  if (magic != kMagic) throw Error(ErrorCode::DECODE, "store: bad magic");
  std::uint64_t version = 0;
  for (int i = 0; i < 4; ++i) version = (version << 8) | buf[4 + i];
  if (version > kFormatVersion) throw Error(ErrorCode::DECODE, "store: unsupported version");
  std::uint64_t plen = 0;
  for (int i = 0; i < 8; ++i) plen = (plen << 8) | buf[8 + i];
  if (plen != buf.size() - kHeaderBytes) throw Error(ErrorCode::DECODE, "store: payload length mismatch");
  std::uint32_t stored_crc = 0;
  for (int i = 0; i < 4; ++i) stored_crc = (stored_crc << 8) | buf[16 + i];
  std::span<const std::uint8_t> payload(buf.data() + kHeaderBytes, plen);
  std::uint32_t calc = Crc32::compute(payload);
  if (calc != stored_crc) throw Error(ErrorCode::DECODE, "store: checksum mismatch");

  PersistentState s = decode(payload);
  return {std::move(s), SaveResult{Sha256::digest(payload), buf.size()}};
}

} // namespace collectivefabric
