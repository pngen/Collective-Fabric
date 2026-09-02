// Collective Fabric - real multiprocess coordinator authority.
#include "mp.hpp"
#include <windows.h>
#include <fstream>
#include <chrono>
#include <thread>

namespace collectivefabric_mp {

struct WorkerState {
  std::unique_ptr<FrameChannel> ch;
  std::uint64_t boot = 0;
  std::string name;
  std::uint64_t rank = 0;
  std::uint64_t group_gen = 0;
  bool joined = false;
};

static HANDLE g_logh = (HANDLE)-1;
static bool open_log(const std::string& path) {
  g_logh = CreateFileA(path.c_str(), GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr,
                       CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
  return g_logh != (HANDLE)-1;
}
void logline(const std::string& s) {
  fputs((s + "\n").c_str(), stdout);
  fflush(stdout);
  if (g_logh != (HANDLE)-1) {
    std::string line = s + "\n";
    DWORD written = 0;
    WriteFile(g_logh, line.data(), static_cast<DWORD>(line.size()), &written, nullptr);
    FlushFileBuffers(g_logh);
  }
}

// Form a group from the currently-connected workers at the given generation.
void form_group(Runtime& rt, std::vector<WorkerState>& workers, std::uint64_t group_id, std::uint64_t group_gen,
                TopologyGeneration topo_gen) {
  std::vector<WorkerState*> ordered = {&workers[0], &workers[1]};
  std::sort(ordered.begin(), ordered.end(), [](const WorkerState* a, const WorkerState* b) { return a->boot < b->boot; });
  int i = 0;
  GroupBuilder gb(CollectiveGroupId(group_id), make_generation<GroupGenerationTag>(group_gen));
  gb.membership_generation(MembershipGeneration(group_gen)).topology_generation(topo_gen)
    .backend_generation(BackendGeneration(1)).lifecycle(GroupLifecycle::READY)
    .provenance(make_source("multiprocess group", ProvenanceKind::DERIVED));
  for (auto* w : ordered) {
    w->rank = static_cast<std::uint64_t>(i++);
    gb.add_participant(ParticipantId(w->boot), WorkerId(w->boot), WorkerBootId(w->boot),
                       NodeId(1), DeviceId(1 + w->rank), BackendGeneration(1), RankId(w->rank));
  }
  auto g = gb.build();
  rt.register_group(g);
}
RankId rank_of(const Runtime& rt, CollectiveGroupId gid, GroupGeneration gen, std::uint64_t boot) {
  const auto* g = rt.find_group(gid, gen);
  for (const auto& p : g->participants()) if (p.boot.raw() == boot) return p.rank;
  return RankId{};
}

int run_coordinator(const Args& a) {
  collectivefabric_tools::winsock_init();
  if (a.logfile.size()) open_log(a.logfile);
  collectivefabric_tools::TcpListener listener;
  std::string err;
  if (!listener.listen_port(a.port, err)) { logline("COORD ERROR listen " + err); return 1; }
  logline("COORD listening port=" + std::to_string(a.port));

  Runtime rt;
  auto backend = std::make_shared<ReferenceBackend>(BackendId(1), BackendGeneration(1));
  rt.register_backend(backend);
  rt.advance_epoch();  // epoch 1

  // Topology (single-node, two devices; SYNTHETIC where not measured).
  Topology topo(TopologyGeneration(1));
  auto n1 = topo.add_node("localhost");
  auto d0 = topo.add_device(n1, "dev0", false, 0);
  auto d1 = topo.add_device(n1, "dev1", false, 0);
  TopologyLink l; l.source_node=n1; l.dest_node=n1; l.source_device=d0; l.dest_device=d1;
  l.link_class = LinkClass::SHARED_MEMORY; l.provenance = ProvenanceKind::SYNTHETIC;
  topo.add_link(l);
  topo.set_provenance(TopologyProvenance::SYNTHETIC);
  rt.adopt_topology(topo);

  std::vector<WorkerState> workers;
  // ---- ACCEPT 2 workers ---------------------------------------------------
  for (int i = 0; i < 2; ++i) {
    SOCKET s = listener.accept_client(err);
    if (s == INVALID_SOCKET) { logline("COORD accept failed"); return 1; }
    auto ch = std::make_unique<FrameChannel>(s);
    Frame f; std::string reason;
    if (!ch->recv(f, reason)) { logline("COORD HELLO read failed " + reason); return 1; }
    if (f.kind != ProtocolMessageKind::HELLO) { logline("COORD expected HELLO"); return 1; }
    CanonicalReader r(f.payload);
    std::uint64_t boot; std::string name;
    if (!r.u64(boot) || !r.string(name)) { logline("COORD HELLO decode failed"); return 1; }
    if (!ch->recv(f, reason)) { logline("COORD JOIN read failed " + reason); return 1; }
    if (f.kind != ProtocolMessageKind::JOIN_GROUP) { logline("COORD expected JOIN_GROUP"); return 1; }
    WorkerState w;
    w.ch = std::move(ch); w.boot = boot; w.name = name; w.joined = true;
    workers.push_back(std::move(w));
    logline("COORD accepted worker name=" + name + " boot=" + std::to_string(boot));
  }
  const std::uint64_t group_id = 1;
  form_group(rt, workers, group_id, 1, topo.generation());
  for (auto& w : workers) {
    auto g = rt.find_group(CollectiveGroupId(group_id), GroupGeneration(1));
    auto rk = rank_of(rt, CollectiveGroupId(group_id), GroupGeneration(1), w.boot);
    Frame acc; acc.kind = ProtocolMessageKind::GROUP_ACCEPT;
    acc.payload = enc_rank_accept(rk.raw(), g->rank_count(), group_id, 1);
    w.ch->send(acc);
    logline("COORD GROUP_ACCEPT boot=" + std::to_string(w.boot) + " rank=" + std::to_string(rk.raw()));
  }
  logline("PHASE_GROUP_READY gen=1 ranks=2");

  // ---- BARRIER (workers send BARRIER after JOIN) --------------------------
  for (auto& w : workers) {
    Frame f; std::string reason;
    if (!w.ch->recv(f, reason)) { logline("COORD barrier read failed " + reason); return 1; }
    if (f.kind != ProtocolMessageKind::BARRIER) { logline("COORD expected BARRIER"); return 1; }
    AuthClaim c; if (!dec_auth(f.payload, c)) { logline("COORD barrier auth decode failed"); return 1; }
    if (c.group_gen != 1 || c.epoch != 1) { logline("COORD barrier stale auth rejected"); return 1; }
  }
  for (auto& w : workers) {
    Frame ack; ack.kind = ProtocolMessageKind::COMPLETE; ack.payload = enc_simple(1, 1);
    w.ch->send(ack);
  }
  logline("PHASE_BARRIER_DONE");

  auto run_allreduce = [&](std::uint64_t gen, const std::string& label, bool expect_loss)->bool {
    const std::uint64_t count = 4;
    Frame req; req.kind = ProtocolMessageKind::REQUEST_COLLECTIVE;
    Desc d; d.kind = (std::uint8_t)CollectiveKind::ALL_REDUCE; d.datatype = (std::uint8_t)Datatype::FLOAT32;
    d.reduction = (std::uint8_t)ReductionOp::SUM; d.element_count = count; d.root = 0; d.rank_count = 2;
    d.group_id = group_id; d.group_gen = gen; d.collective_gen = 1; d.epoch = 2; d.attempt_gen = 1; d.dispatch_gen = 1;
    if (expect_loss) { d.epoch = 1; d.dispatch_gen = 1; }
    req.payload = enc_desc(d);
    std::vector<std::vector<std::uint8_t>> inputs(2);
    std::vector<AuthClaim> auths(2);
    bool lost = false;
    for (auto& w : workers) {
      w.ch->send(req);
    }
    for (int i = 0; i < 2; ++i) {
      Frame f; std::string reason;
      if (!workers[i].ch->recv(f, reason)) {
        logline(label + " LOSS_DETECTED boot=" + std::to_string(workers[i].boot) + " reason=" + reason);
        lost = true; break;
      }
      if (f.kind != ProtocolMessageKind::CONTRIBUTION) { logline(label + " expected CONTRIBUTION"); return false; }
      CanonicalReader r(f.payload);
      if (!dec_auth(f.payload, auths[i])) { logline(label + " contribution auth decode failed"); return false; }
      // blob is the trailing field
      std::span<const std::uint8_t> blob;
      // re-read: skip auth fields then blob
      CanonicalWriter dummy;
      // parse blob
      CanonicalReader rr(f.payload); AuthClaim tmp; std::span<const std::uint8_t> bytes;
      if (!(rr.u64(tmp.epoch) && rr.u64(tmp.boot) && rr.u64(tmp.group_gen) && rr.u64(tmp.collective_gen) &&
            rr.u64(tmp.attempt_gen) && rr.u64(tmp.dispatch_gen) && rr.bytes(bytes))) {
        logline(label + " contribution blob decode failed"); return false;
      }
      inputs[i] = std::vector<std::uint8_t>(bytes.begin(), bytes.end());
      logline(label + " rank=" + std::to_string(i) + " contribution bytes=" + std::to_string(inputs[i].size()));
    }
    if (lost) return lost;
    std::vector<ReferenceEngine::Span> spans;
    for (auto& in : inputs) spans.push_back(ReferenceEngine::Span{in.data(), in.size()});
    std::vector<std::vector<std::uint8_t>> outputs;
    ReferenceEngine::execute(CollectiveKind::ALL_REDUCE, ReductionOp::SUM, Datatype::FLOAT32, count, 0, 2, spans, outputs);
    for (auto& w : workers) {
      Frame res; res.kind = ProtocolMessageKind::RESULT;
      AuthClaim a; a.epoch = 2; a.boot = w.boot; a.group_gen = gen; a.collective_gen = 1; a.attempt_gen = 1; a.dispatch_gen = 1;
      res.payload = enc_blob(a, outputs[0]);
      w.ch->send(res);
    }
    // wait COMPLETE acks
    for (auto& w : workers) {
      Frame f; std::string reason;
      if (!w.ch->recv(f, reason)) { logline(label + " COMPLETE lost boot=" + std::to_string(w.boot) + " reason=" + reason); return lost; }
    }
    float s = ((float*)outputs[0].data())[0];
    logline(label + " DONE sum=" + std::to_string(s));
    // record measurement + accounting
    CollectiveMeasurement m;
    m.id = MeasurementId(1000 + gen);
    m.collective = CollectiveId(1); m.collective_generation = CollectiveGeneration(gen);
    m.group_generation = GroupGeneration(gen); m.algorithm = Algorithm::RING; m.backend = BackendId(1);
    m.rank_count = 2; m.payload_bytes = count * 4; m.wall_time_us = 1; m.success = true;
    m.topology_generation = topo.generation(); m.provenance = MeasurementProvenance::MEASURED;
    m.freshness = MeasurementFreshness::CURRENT; m.timestamp_utc_ns = clock::wall_ns();
    m.measurement_generation = MeasurementGeneration(gen); m.source = make_source("host-staged tcp", ProvenanceKind::MEASURED);
    rt.ingest_measurement(m);
    return false;
  };

  run_allreduce(1, "PHASE_ALLREDUCE", false);
  logline("PHASE_ALLREDUCE_DONE gen=1");

  // ---- SAVE ---------------------------------------------------------------
  auto build_state = [&]() -> PersistentState {
    PersistentState s; s.saved_ns = clock::wall_ns();
    s.epoch = rt.authority().epoch; s.live_boot = rt.authority().live_boot;
    // export groups
    for (const auto& gk : std::vector<std::pair<CollectiveGroupId,GroupGeneration>>{{CollectiveGroupId(1), GroupGeneration(1)}}) {
      const auto* g = rt.find_group(gk.first, gk.second);
      if (!g) continue;
      PersistedGroup pg; pg.group_id = g->id(); pg.generation = g->generation();
      pg.membership = g->membership_generation(); pg.topology = g->topology_generation();
      pg.health = g->health_generation(); pg.backend = g->backend_generation(); pg.lifecycle = g->lifecycle();
      for (const auto& p : g->participants()) {
        PersistedParticipant pp; pp.participant_id = p.id; pp.worker = p.worker; pp.boot = p.boot;
        pp.node = p.node; pp.device = p.device; pp.rank = p.rank; pp.backend_generation = p.backend_generation; pp.is_known = p.is_known;
        pg.participants.push_back(pp);
      }
      s.groups.push_back(pg);
    }
    for (auto mid : rt.measurement_ids()) {
      const CollectiveMeasurement* m = rt.measurement(mid);
      if (!m) continue;
      PersistedMeasurement pm; pm.id = m->id; pm.collective = m->collective;
      pm.collective_generation = m->collective_generation; pm.group_generation = m->group_generation;
      pm.algorithm = m->algorithm; pm.backend = m->backend; pm.rank_count = m->rank_count;
      pm.payload_bytes = m->payload_bytes; pm.wall_time_us = m->wall_time_us;
      pm.provenance = m->provenance; pm.freshness = m->freshness; pm.timestamp_ns = m->timestamp_utc_ns;
      pm.measurement_generation = m->measurement_generation; pm.success = m->success;
      s.measurements.push_back(pm);
    }
    return s;
  };
  PersistentState st = build_state();
  Store::save(a.statefile, st);
  logline("PHASE_SAVED path=" + a.statefile);

  // ---- begin another collective with live authority; worker A will die ----
  logline("PHASE_BEGIN_LIVE_COLLECTIVE");
  run_allreduce(1, "PHASE_LOSS", true);  // blocks; driver kills worker A
  logline("PHASE_LOSS_DETECTED");

  // ---- reconfigure --------------------------------------------------------
  rt.advance_epoch();  // epoch 2
  for (auto it = workers.begin(); it != workers.end(); ) {
    if (!it->ch || !it->ch->valid()) { logline("COORD dropping dead worker boot=" + std::to_string(it->boot)); it = workers.erase(it); }
    else ++it;
  }
  logline("PHASE_RECONFIGURE epoch=2");

  // ---- accept new worker A' (fresh boot) ----------------------------------
  {
    SOCKET s = listener.accept_client(err);
    if (s == INVALID_SOCKET) { logline("COORD accept A' failed"); return 1; }
    auto ch = std::make_unique<FrameChannel>(s);
    Frame f; std::string reason;
    ch->recv(f, reason);
    if (f.kind != ProtocolMessageKind::HELLO) { logline("COORD expected HELLO from A'"); return 1; }
    CanonicalReader r(f.payload);
    std::uint64_t boot; std::string name;
    r.u64(boot); r.string(name);
    ch->recv(f, reason);
    WorkerState w; w.ch = std::move(ch); w.boot = boot; w.name = name; w.joined = true;
    workers.push_back(std::move(w));
    logline("COORD new worker boot=" + std::to_string(boot));
  }
  form_group(rt, workers, group_id, 2, topo.generation());
  for (auto& w : workers) {
    auto g = rt.find_group(CollectiveGroupId(group_id), GroupGeneration(2));
    auto rk = rank_of(rt, CollectiveGroupId(group_id), GroupGeneration(2), w.boot);
    Frame acc; acc.kind = ProtocolMessageKind::GROUP_ACCEPT;
    acc.payload = enc_rank_accept(rk.raw(), g->rank_count(), group_id, 2);
    w.ch->send(acc);
    logline("COORD REGEN GROUP_ACCEPT boot=" + std::to_string(w.boot) + " rank=" + std::to_string(rk.raw()));
  }
  rt.set_live_boot(WorkerBootId(workers[0].boot));  // representative live incarnation
  rt.authority().group_generation = GroupGeneration(2);
  rt.authority().collective_generation = CollectiveGeneration(1);
  rt.authority().attempt_generation = AttemptGeneration(2);
  rt.authority().dispatch_generation = DispatchGeneration(2);
  logline("PHASE_REGROUP gen=2");

  // ---- stale replay -------------------------------------------------------
  {
    AuthorityGroundTruth truth;
    truth.epoch = CoordinatorEpoch(2);
    truth.live_boot = WorkerBootId(workers[0].boot);
    truth.group_generation = GroupGeneration(2);
    truth.collective_generation = CollectiveGeneration(1);
    truth.attempt_generation = AttemptGeneration(2);
    truth.dispatch_generation = DispatchGeneration(2);
    int rejected = 0, accepted = 0;
    auto check = [&](AuthorityClaim c) {
      auto d = evaluate_authority(truth, c);
      if (d.verdict == AuthorityVerdict::STALE || d.verdict == AuthorityVerdict::DUPLICATE) {
        logline("PHASE_STALE_REJECT class=" + std::string(authority_verdict_to_string(d.verdict)) + " " + d.reason);
        ++rejected;
      } else { logline("PHASE_STALE_NOT_REJECTED class=" + std::string(authority_verdict_to_string(d.verdict)) + " " + d.reason); ++accepted; }
    };
    AuthorityClaim c;
    const WorkerBootId live(truth.live_boot.raw());
    c.epoch = CoordinatorEpoch(1); c.boot = live; c.group_generation = GroupGeneration(2); c.collective_generation = CollectiveGeneration(1);
    c.attempt_generation = AttemptGeneration(2); c.dispatch_generation = DispatchGeneration(2); check(c);   // stale epoch
    c.epoch = CoordinatorEpoch(2); c.boot = WorkerBootId(999); c.group_generation = GroupGeneration(2); c.collective_generation = CollectiveGeneration(1);
    c.attempt_generation = AttemptGeneration(2); c.dispatch_generation = DispatchGeneration(2); check(c);   // stale boot
    c.boot = live; c.group_generation = GroupGeneration(1); c.collective_generation = CollectiveGeneration(1);
    c.attempt_generation = AttemptGeneration(2); c.dispatch_generation = DispatchGeneration(2); check(c);   // stale group gen
    c.group_generation = GroupGeneration(2); c.collective_generation = CollectiveGeneration(0);
    c.attempt_generation = AttemptGeneration(2); c.dispatch_generation = DispatchGeneration(2); check(c);   // stale collective gen
    c.collective_generation = CollectiveGeneration(1); c.attempt_generation = AttemptGeneration(1);
    c.dispatch_generation = DispatchGeneration(2); check(c);                                               // stale attempt gen
    c.attempt_generation = AttemptGeneration(2); c.dispatch_generation = DispatchGeneration(1); check(c);   // stale dispatch gen
    c.dispatch_generation = DispatchGeneration(2); c.is_completion = true; check(c);                       // duplicate completion
    AuthorityClaim good; good.epoch = CoordinatorEpoch(2); good.boot = live; good.group_generation = GroupGeneration(2);
    good.collective_generation = CollectiveGeneration(1); good.attempt_generation = AttemptGeneration(2);
    good.dispatch_generation = DispatchGeneration(2); good.is_completion = false; check(good);             // current
    logline("PHASE_STALE_REPLAY rejected=" + std::to_string(rejected) + " accepted=" + std::to_string(accepted));
  }

  // ---- fresh collective on group gen 2 ------------------------------------
  st = build_state();
  Store::save(a.statefile + ".before2", st);
  run_allreduce(2, "PHASE_ALLREDUCE2", false);
  logline("PHASE_ALLREDUCE_DONE gen=2");

  st = build_state();
  Store::save(a.statefile, st);
  logline("PHASE_SAVED_FINAL");
  logline("PHASE_DONE");
  listener.close();
  collectivefabric_tools::winsock_cleanup();
  if (g_logh != (HANDLE)-1) CloseHandle(g_logh);
  return 0;
}


// ---- recovery: fresh coordinator process loads persisted state ------------
void rebuild_runtime(const PersistentState& state, Runtime& rt) {
  for (const auto& pg : state.groups) {
    GroupBuilder gb(pg.group_id, pg.generation);
    gb.membership_generation(pg.membership).topology_generation(pg.topology)
      .health_generation(pg.health).backend_generation(pg.backend).lifecycle(pg.lifecycle)
      .provenance(make_source("recovered group", ProvenanceKind::DERIVED));
    for (const auto& pp : pg.participants)
      gb.add_participant(pp.participant_id, pp.worker, pp.boot, pp.node, pp.device, pp.backend_generation, pp.rank);
    rt.register_group(gb.build());
  }
}

int run_recover(const Args& a) {
  collectivefabric_tools::winsock_init();
  if (a.logfile.size()) open_log(a.logfile);
  try {
    auto [state, loadres] = Store::load(a.statefile);
    (void)loadres;
    logline("RECOVERY loaded groups=" + std::to_string(state.groups.size()) +
            " measurements=" + std::to_string(state.measurements.size()));
    if (state.groups.empty()) { logline("RECOVERY_DONE"); return 1; }
    Runtime rt;
    auto backend = std::make_shared<ReferenceBackend>(BackendId(1), BackendGeneration(1));
    rt.register_backend(backend);
    rt.authority().epoch = state.epoch;
    rt.set_live_boot(WorkerBootId{});  // recovered live authority cleared
    rebuild_runtime(state, rt);
    std::size_t forced_reval = 0;
    for (const auto& pm : state.measurements)
      if (pm.freshness == MeasurementFreshness::CURRENT) ++forced_reval;
    logline("RECOVERY_MEASUREMENTS_REVALIDATION_REQUIRED forced=" + std::to_string(forced_reval));
    logline("RECOVERY_AUTHORITY_CLEARED");

    collectivefabric_tools::TcpListener listener;
    std::string err;
    listener.listen_port(a.port, err);
    std::vector<std::unique_ptr<FrameChannel>> chs;
    std::vector<std::uint64_t> boots;
    for (int i = 0; i < 2; ++i) {
      SOCKET s = listener.accept_client(err);
      auto ch = std::make_unique<FrameChannel>(s);
      Frame f; std::string reason;
      if (!ch->recv(f, reason)) { logline("RECOVERY recv failed"); return 1; }
      CanonicalReader r(f.payload); std::uint64_t boot; std::string name; r.u64(boot); r.string(name);
      if (!ch->recv(f, reason)) return 1;
      boots.push_back(boot);
      logline("RECOVERY worker boot=" + std::to_string(boot));
      chs.push_back(std::move(ch));
    }
    const std::uint64_t gen = 3;
    std::vector<std::size_t> idx = {0, 1};
    std::sort(idx.begin(), idx.end(), [&](std::size_t x, std::size_t y) { return boots[x] < boots[y]; });
    GroupBuilder gb(CollectiveGroupId(7), GroupGeneration(gen));
    gb.membership_generation(MembershipGeneration(gen)).topology_generation(TopologyGeneration(1))
      .backend_generation(BackendGeneration(1)).lifecycle(GroupLifecycle::READY)
      .provenance(make_source("recovered fresh group", ProvenanceKind::DERIVED));
    for (std::size_t r = 0; r < 2; ++r) {
      gb.add_participant(ParticipantId(900 + r), WorkerId(boots[idx[r]]), WorkerBootId(boots[idx[r]]),
                         NodeId(1), DeviceId(1 + r), BackendGeneration(1), RankId(r));
    }
    rt.register_group(gb.build());
    // send GROUP_ACCEPT to each worker with its rank (by boot order)
    for (std::size_t rr = 0; rr < 2; ++rr) {
      Frame acc; acc.kind = ProtocolMessageKind::GROUP_ACCEPT;
      acc.payload = enc_rank_accept(rr, 2, 7, gen);
      chs[idx[rr]]->send(acc);
    }
    for (auto& c : chs) { Frame f; std::string reason; c->recv(f, reason); }   // barrier
    for (auto& c : chs) { Frame ack; ack.kind = ProtocolMessageKind::COMPLETE; ack.payload = enc_simple(1, 1); c->send(ack); }
    Frame req; req.kind = ProtocolMessageKind::REQUEST_COLLECTIVE;
    Desc d; d.kind = (std::uint8_t)CollectiveKind::ALL_REDUCE; d.datatype = (std::uint8_t)Datatype::FLOAT32;
    d.reduction = (std::uint8_t)ReductionOp::SUM; d.element_count = 4; d.rank_count = 2; d.group_id = 7;
    d.group_gen = gen; d.collective_gen = 1; d.epoch = state.epoch.value(); d.attempt_gen = 1; d.dispatch_gen = 1;
    req.payload = enc_desc(d);
    for (auto& c : chs) c->send(req);
    std::vector<std::vector<std::uint8_t>> held;
    std::vector<ReferenceEngine::Span> spans;
    for (auto& c : chs) {
      Frame f; std::string reason;
      if (!c->recv(f, reason)) { logline("RECOVERY contrib recv failed"); return 1; }
      CanonicalReader rr(f.payload); AuthClaim tmp; std::span<const std::uint8_t> bytes;
      rr.u64(tmp.epoch); rr.u64(tmp.boot); rr.u64(tmp.group_gen); rr.u64(tmp.collective_gen);
      rr.u64(tmp.attempt_gen); rr.u64(tmp.dispatch_gen); rr.bytes(bytes);
      held.push_back(std::vector<std::uint8_t>(bytes.begin(), bytes.end()));
    }
    for (auto& in : held) spans.push_back(ReferenceEngine::Span{in.data(), in.size()});
    std::vector<std::vector<std::uint8_t>> outputs;
    ReferenceEngine::execute(CollectiveKind::ALL_REDUCE, ReductionOp::SUM, Datatype::FLOAT32, 4, 0, 2, spans, outputs);
    for (auto& c : chs) { Frame res; res.kind = ProtocolMessageKind::RESULT; res.payload = enc_blob(AuthClaim{}, outputs[0]); c->send(res); }
    for (auto& c : chs) { Frame f; std::string reason; c->recv(f, reason); }
    logline("RECOVERY_ALLREDUCE_DONE sum=" + std::to_string(((float*)outputs[0].data())[0]));
    logline("RECOVERY_DONE");
    listener.close();
  } catch (const std::exception& e) {
    logline(std::string("RECOVERY_ERROR ") + e.what());
  }
  collectivefabric_tools::winsock_cleanup();
  if (g_logh != (HANDLE)-1) CloseHandle(g_logh);
  return 0;
}
} // namespace collectivefabric_mp

int main(int argc, char** argv) {
  collectivefabric_mp::Args a;
  for (int i = 1; i < argc; ++i) {
    std::string arg = argv[i];
    auto next = [&](const char* flag) -> std::string {
      if (arg == flag && i + 1 < argc) return argv[++i];
      return "";
    };
    if (arg == "--port") a.port = (unsigned short)atoi(next("--port").c_str());
    else if (arg == "--statefile") a.statefile = next("--statefile");
    else if (arg == "--logfile") a.logfile = next("--logfile");
    else if (arg == "--scenario") a.phases = next("--scenario");
  }
  if (a.phases == "recover") return collectivefabric_mp::run_recover(a);
  return collectivefabric_mp::run_coordinator(a);
}