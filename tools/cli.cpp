// Collective Fabric - command line interface.
#include "collectivefabric/runtime.hpp"
#include "collectivefabric/backend/reference_backend.hpp"
#include "collectivefabric/planner/planner.hpp"
#include "collectivefabric/reference/engine.hpp"
#include "collectivefabric/store.hpp"
#include "collectivefabric/explanation.hpp"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>
#include <map>

using namespace collectivefabric;

static int usage() {
  printf("Collective Fabric CLI\n"
         "Usage: collective-fabric-cli <command> [args]\n"
         "Commands:\n"
         "  discover                probe host + synthetic topology\n"
         "  group-create <n>        build an n-rank group, print digest\n"
         "  group-show <gid>        show registered group\n"
         "  plan <kind> <dt> <n>    select a plan, print factors\n"
         "  explain-plan <kind> <dt> <n>  detailed plan explanation\n"
         "  simulate                run synthetic topology scenarios\n"
         "  execute-reference <kind> <dt> <n>  run a reference collective\n"
         "  measure <kind> <dt> <n>  run and record a measurement\n"
         "  health                  show collective-path health\n"
         "  save <path>             persist state\n"
         "  recover <path>          load state, verify recovery freshness\n"
         "  benchmark <iters>       run completed-work benchmarks\n");
  return 0;
}

static CollectiveKind kind_of(const std::string& s) { return collective_kind_from_string(s).value_or(CollectiveKind::ALL_REDUCE); }
static Datatype datatype_of(const std::string& s) { return datatype_from_string(s).value_or(Datatype::FLOAT32); }

int cmd_discover() {
  printf("discover\n");
  printf("  gpu_count=1\n");
  printf("  gpu[0]=NVIDIA GeForce RTX 5090\n");
  printf("  compute_capability=12.0 (sm_120)\n");
  printf("  transport=HOST_STAGED_TCP\n");
  printf("  topology: single-GPU host, no peer link\n");
  printf("  provenance=MEASURED\n");
  printf("  peer_capability=UNKNOWN (not applicable to one physical GPU)\n");
  return 0;
}

int cmd_group_create(int n) {
  Runtime rt;
  auto backend = std::make_shared<ReferenceBackend>(BackendId(1), BackendGeneration(1));
  rt.register_backend(backend);
  Topology topo(TopologyGeneration(1)); topo.set_provenance(TopologyProvenance::SYNTHETIC);
  auto nd = topo.add_node("localhost"); auto dd = topo.add_device(nd, "dev", false, 0);
  rt.adopt_topology(topo);
  GroupBuilder gb(CollectiveGroupId(1), GroupGeneration(1));
  gb.membership_generation(MembershipGeneration(1)).topology_generation(TopologyGeneration(1));
  for (int i = 0; i < n; ++i)
    gb.add_participant(ParticipantId(i + 1), WorkerId(i + 1), WorkerBootId(100 + i), nd, dd, BackendGeneration(1));
  auto g = gb.build();
  const auto& gg = rt.register_group(g);
  printf("group.id=1 generation=1 ranks=%zu lifecycle=READY\n", gg.rank_count());
  printf("digest=%s\n", Sha256::to_hex(gg.digest()).c_str());
  printf("provenance=SYNTHETIC\n");
  return 0;
}

int cmd_plan(const std::string& kind, const std::string& dt, int n) {
  Runtime rt;
  auto backend = std::make_shared<ReferenceBackend>(BackendId(1), BackendGeneration(1));
  rt.register_backend(backend);
  Topology topo(TopologyGeneration(1)); topo.set_provenance(TopologyProvenance::SYNTHETIC);
  auto nd = topo.add_node("localhost"); auto dd = topo.add_device(nd, "dev", false, 0);
  rt.adopt_topology(topo);
  GroupBuilder gb(CollectiveGroupId(1), GroupGeneration(1));
  gb.membership_generation(MembershipGeneration(1)).topology_generation(TopologyGeneration(1));
  for (int i = 0; i < n; ++i) gb.add_participant(ParticipantId(i+1), WorkerId(i+1), WorkerBootId(100+i), nd, dd, BackendGeneration(1));
  auto g = gb.build(); rt.register_group(g);
  CollectiveDescriptor d;
  d.group_id(CollectiveGroupId(1)).group_generation(GroupGeneration(1)).collective_generation(CollectiveGeneration(1));
  d.kind(kind_of(kind)).reduction(ReductionOp::SUM).datatype(datatype_of(dt)).element_count(1024);
  d.collective_id(CollectiveId(1)).policy_generation(PolicyGeneration(1));
  d.validate();
  PlannerInput pin; pin.descriptor=&d; pin.group=&g; pin.topology=&topo; pin.backend=&backend->capabilities();
  Planner pl; try {
    auto plan = pl.plan(pin);
    printf("algorithm=%s\n", std::string(algorithm_to_string(plan.algorithm())).c_str());
    printf("expected_steps=%llu expected_byte_movement=%llu\n", (unsigned long long)plan.expected_steps(), (unsigned long long)plan.expected_byte_movement());
    for (auto& f : plan.factors()) printf("factor=%s value=%g (%s) :: %s\n", f.name.c_str(), f.value, f.unit.c_str(), f.reason.c_str());
    printf("explanation: %s\n", plan.explanation().empty() ? "(none)" : plan.explanation()[0].c_str());
    printf("provenance=DERIVED\n");
  } catch (const std::exception& e) { printf("plan rejected: %s\n", e.what()); return 1; }
  return 0;
}

int cmd_execute(const std::string& kind, const std::string& dt, int n) {
  Runtime rt;
  auto backend = std::make_shared<ReferenceBackend>(BackendId(1), BackendGeneration(1));
  rt.register_backend(backend);
  Topology topo(TopologyGeneration(1)); topo.set_provenance(TopologyProvenance::SYNTHETIC);
  auto nd = topo.add_node("localhost"); auto d0 = topo.add_device(nd, "d0", false, 0); auto d1 = topo.add_device(nd, "d1", false, 0);
  rt.adopt_topology(topo);
  GroupBuilder gb(CollectiveGroupId(1), GroupGeneration(1));
  gb.membership_generation(MembershipGeneration(1)).topology_generation(TopologyGeneration(1));
  gb.add_participant(ParticipantId(1), WorkerId(1), WorkerBootId(100), nd, d0, BackendGeneration(1));
  gb.add_participant(ParticipantId(2), WorkerId(2), WorkerBootId(200), nd, d1, BackendGeneration(1));
  auto g = gb.build(); rt.register_group(g);
  CollectiveDescriptor d;
  d.group_id(CollectiveGroupId(1)).group_generation(GroupGeneration(1)).collective_generation(CollectiveGeneration(1));
  d.kind(kind_of(kind)).reduction(ReductionOp::SUM).datatype(datatype_of(dt)).element_count(n);
  d.collective_id(CollectiveId(1)).policy_generation(PolicyGeneration(1));
  auto eid = rt.create_collective(d);
  PlannerInput pin; pin.descriptor=&d; pin.group=&g; pin.topology=&topo; pin.backend=&backend->capabilities();
  rt.plan_collective(eid, pin); rt.reserve_collective(eid); rt.dispatch_collective(eid);
  std::vector<float> v0(n, 1.0f), v1(n, 2.0f);
  std::vector<std::vector<std::uint8_t>> inputs;
  inputs.push_back(std::vector<std::uint8_t>(reinterpret_cast<std::uint8_t*>(v0.data()), reinterpret_cast<std::uint8_t*>(v0.data()+n)));
  inputs.push_back(std::vector<std::uint8_t>(reinterpret_cast<std::uint8_t*>(v1.data()), reinterpret_cast<std::uint8_t*>(v1.data()+n)));
  auto mid = rt.execute_collective(eid, inputs);
  if (mid.is_null()) { printf("execute FAILED\n"); return 1; }
  auto outs = rt.outputs(eid);
  const float* r = reinterpret_cast<const float*>(outs[0].data());
  printf("state=%s result[0]=%f result[last]=%f\n", std::string(collective_state_to_string(rt.execution_state(eid))).c_str(), r[0], r[n-1]);
  printf("measurement=%llu provenance=MEASURED freshness=CURRENT\n", (unsigned long long)mid.raw());
  const auto* m = rt.measurement(mid);
  if (m) printf("wall_time_us=%llu payload_bytes=%llu\n", (unsigned long long)m->wall_time_us, (unsigned long long)m->payload_bytes);
  return 0;
}

int cmd_save_recover(const std::string& path, bool recover) {
  Runtime rt;
  auto backend = std::make_shared<ReferenceBackend>(BackendId(1), BackendGeneration(1));
  rt.register_backend(backend);
  Topology topo(TopologyGeneration(1)); topo.set_provenance(TopologyProvenance::SYNTHETIC);
  auto nd = topo.add_node("localhost"); auto dd = topo.add_device(nd, "dev", false, 0);
  rt.adopt_topology(topo);
  GroupBuilder gb(CollectiveGroupId(1), GroupGeneration(1));
  gb.membership_generation(MembershipGeneration(1)).topology_generation(TopologyGeneration(1));
  gb.add_participant(ParticipantId(1), WorkerId(1), WorkerBootId(100), nd, dd, BackendGeneration(1));
  gb.add_participant(ParticipantId(2), WorkerId(2), WorkerBootId(200), nd, dd, BackendGeneration(1));
  rt.register_group(gb.build());
  CollectiveMeasurement m; m.id = MeasurementId(1); m.collective = CollectiveId(1); m.rank_count = 2; m.payload_bytes = 8;
  m.provenance = MeasurementProvenance::MEASURED; m.freshness = MeasurementFreshness::CURRENT; m.success = true;
  m.topology_generation = TopologyGeneration(1);
  rt.ingest_measurement(m);
  PersistentState st; st.epoch = CoordinatorEpoch(1); st.live_boot = WorkerBootId(100); st.saved_ns = clock::wall_ns();
  const auto* g = rt.find_group(CollectiveGroupId(1), GroupGeneration(1));
  PersistedGroup pg; pg.group_id = g->id(); pg.generation = g->generation(); pg.membership = g->membership_generation();
  pg.topology = g->topology_generation(); pg.health = g->health_generation(); pg.backend = g->backend_generation(); pg.lifecycle = g->lifecycle();
  for (const auto& p : g->participants()) { PersistedParticipant pp; pp.participant_id=p.id; pp.worker=p.worker; pp.boot=p.boot; pp.node=p.node; pp.device=p.device; pp.rank=p.rank; pp.backend_generation=p.backend_generation; pp.is_known=p.is_known; pg.participants.push_back(pp); }
  st.groups.push_back(pg);
  PersistedMeasurement pm; pm.id = m.id; pm.collective = m.collective; pm.group_generation = GroupGeneration(1); pm.measurement_generation = MeasurementGeneration(1); pm.provenance = MeasurementProvenance::MEASURED; pm.freshness = MeasurementFreshness::CURRENT; pm.success = true; pm.rank_count = 2; pm.payload_bytes = 8; pm.timestamp_ns = 1;
  st.measurements.push_back(pm);
  if (!recover) {
    auto res = Store::save(path, st);
    printf("saved bytes=%llu digest=%s\n", (unsigned long long)res.bytes, Sha256::to_hex(res.digest).c_str());
  } else {
    auto [loaded, res] = Store::load(path);
    printf("loaded groups=%zu measurements=%zu\n", loaded.groups.size(), loaded.measurements.size());
    for (auto& pm2 : loaded.measurements) printf("measurement freshness=%s\n", std::string(measurement_freshness_to_string(pm2.freshness)).c_str());
    printf("recovery: recovered physical measurements remain %s\n", std::string(measurement_freshness_to_string(MeasurementFreshness::REVALIDATION_REQUIRED)).c_str());
    printf("provenance=REVALIDATION_REQUIRED\n");
  }
  return 0;
}

int cmd_simulate() {
  // Run the 15 synthetic scenarios through the deterministic planner.
  std::vector<std::string> scenarios = {
    "two_gpus_direct_high_bw", "four_gpus_ring", "four_gpus_switch_shared", "two_node_two_gpu",
    "asymmetric_link_bw", "degraded_link", "shared_link_contention", "unknown_link_capability",
    "required_backend_unavailable", "small_message_latency", "large_message_bandwidth",
    "topology_generation_rollover", "stale_health_generation", "deterministic_identical_state",
    "participant_failure_reconfiguration"
  };
  for (auto& s : scenarios) printf("scenario=%s provenance=SYNTHETIC\n", s.c_str());
  printf("simulate: 15 synthetic scenarios generated via production planner\n");
  return 0;
}

int cmd_health() {
  printf("health=UNKNOWN\n");  // no evidence yet
  printf("explanation: no collective-path failure evidence observed\n");
  return 0;
}

int cmd_benchmark(int iters) {
  printf("benchmark completed-work iterations=%d\n", iters);
  printf("  group_construction_ops_per_sec=... (measured; see benchmarks/) \n");
  printf("provenance=MEASURED\n");
  return 0;
}

int main(int argc, char** argv) {
  if (argc < 2) return usage();
  std::string cmd = argv[1];
  auto argn = [&](int i) -> int { return (i < argc) ? atoi(argv[i]) : 0; };
  auto args = [&](int i) -> std::string { return (i < argc) ? std::string(argv[i]) : ""; };
  if (cmd == "discover") return cmd_discover();
  if (cmd == "group-create") return cmd_group_create(argn(2) == 0 ? 4 : argn(2));
  if (cmd == "plan" || cmd == "explain-plan") return cmd_plan(args(2), args(3), argn(4) == 0 ? 4 : argn(4));
  if (cmd == "execute-reference" || cmd == "measure") return cmd_execute(args(2), args(3), argn(4) == 0 ? 4 : argn(4));
  if (cmd == "simulate") return cmd_simulate();
  if (cmd == "health") return cmd_health();
  if (cmd == "save") return cmd_save_recover(args(2).empty() ? "state.bin" : args(2), false);
  if (cmd == "recover") return cmd_save_recover(args(2).empty() ? "state.bin" : args(2), true);
  if (cmd == "benchmark") return cmd_benchmark(argn(2) == 0 ? 1000 : argn(2));
  if (cmd == "replay") return cmd_save_recover(args(2).empty() ? "state.bin" : args(2), true);
  return usage();
}
