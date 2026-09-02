// Collective Fabric - completed-work benchmarks with explicit units.
// Every reported metric is ops/s (completed semantic operations per second),
// µs/op, or bytes/s. Loop iterations are never reported as throughput unless
// each iteration performs a complete semantic operation.
#include "collectivefabric/runtime.hpp"
#include "collectivefabric/backend/reference_backend.hpp"
#include "collectivefabric/planner/planner.hpp"
#include "collectivefabric/reference/engine.hpp"
#include "collectivefabric/store.hpp"
#include "collectivefabric/protocol.hpp"
#include "collectivefabric/explanation.hpp"
#include "collectivefabric/foundation/clock.hpp"
#include <cstdio>
#include <vector>
#include <string>

using namespace collectivefabric;

static double ns_per_op(std::uint64_t start, std::uint64_t iters) {
  return (clock::steady_ns() - start) / (double)iters;
}

int main() {
  const std::uint64_t N = 4096;
  Runtime rt;
  auto backend = std::make_shared<ReferenceBackend>(BackendId(1), BackendGeneration(1));
  rt.register_backend(backend);
  Topology topo(TopologyGeneration(1)); topo.set_provenance(TopologyProvenance::SYNTHETIC);
  auto nd = topo.add_node("n"); auto d0 = topo.add_device(nd,"d0",false,0); auto d1 = topo.add_device(nd,"d1",false,0);
  rt.adopt_topology(topo);
  GroupBuilder gb(CollectiveGroupId(1), GroupGeneration(1));
  gb.membership_generation(MembershipGeneration(1)).topology_generation(TopologyGeneration(1));
  gb.add_participant(ParticipantId(1),WorkerId(1),WorkerBootId(100),nd,d0,BackendGeneration(1));
  gb.add_participant(ParticipantId(2),WorkerId(2),WorkerBootId(200),nd,d1,BackendGeneration(1));
  auto g = gb.build(); rt.register_group(g);

  printf("Collective Fabric benchmarks (units explicit)\n");

  // group construction
  {
    const std::uint64_t iters = 20000;
    auto t0 = clock::steady_ns();
    for (std::uint64_t i=0;i<iters;++i) {
      GroupBuilder x(CollectiveGroupId(1), GroupGeneration(1));
      x.membership_generation(MembershipGeneration(1)).topology_generation(TopologyGeneration(1));
      x.add_participant(ParticipantId(1),WorkerId(1),WorkerBootId(100),nd,d0,BackendGeneration(1));
      x.add_participant(ParticipantId(2),WorkerId(2),WorkerBootId(200),nd,d1,BackendGeneration(1));
      auto gg = x.build();
      if (gg.rank_count() != 2) return 1;
    }
    double nsp = ns_per_op(t0, iters);
    printf("  group_construction  %.3f us/op   %.0f ops/s\n", nsp/1000.0, 1e9/nsp);
  }

  // descriptor canonicalization
  CollectiveDescriptor dsc;
  dsc.group_id(CollectiveGroupId(1)).group_generation(GroupGeneration(1)).collective_generation(CollectiveGeneration(1));
  dsc.kind(CollectiveKind::ALL_REDUCE).reduction(ReductionOp::SUM).datatype(Datatype::FLOAT32).element_count(N);
  dsc.collective_id(CollectiveId(1)).policy_generation(PolicyGeneration(1));
  {
    const std::uint64_t iters = 100000;
    auto t0 = clock::steady_ns();
    Sha256::Digest dg;
    for (std::uint64_t i=0;i<iters;++i) dg = dsc.canonical_digest();
    double nsp = ns_per_op(t0, iters);
    printf("  descriptor_canonicalize  %.3f us/op   %.0f ops/s\n", nsp/1000.0, 1e9/nsp);
    (void)dg;
  }

  // plan selection
  Planner pl; PlannerInput pin; pin.descriptor=&dsc; pin.group=&g; pin.topology=&topo; pin.backend=&backend->capabilities();
  {
    const std::uint64_t iters = 20000;
    auto t0 = clock::steady_ns();
    CollectivePlan plan;
    for (std::uint64_t i=0;i<iters;++i) plan = pl.plan(pin);
    double nsp = ns_per_op(t0, iters);
    printf("  plan_selection  %.3f us/op   %.0f ops/s\n", nsp/1000.0, 1e9/nsp);
    (void)plan;
  }

  // all-reduce reference execution (4096 floats x 2 ranks)
  {
    const std::uint64_t iters = 2000;
    std::vector<float> a(N,1.0f), b(N,2.0f);
    std::vector<std::vector<std::uint8_t>> inputs;
    inputs.push_back(std::vector<std::uint8_t>(reinterpret_cast<std::uint8_t*>(a.data()), reinterpret_cast<std::uint8_t*>(a.data()+N)));
    inputs.push_back(std::vector<std::uint8_t>(reinterpret_cast<std::uint8_t*>(b.data()), reinterpret_cast<std::uint8_t*>(b.data()+N)));
    std::vector<ReferenceEngine::Span> spans{{inputs[0].data(),inputs[0].size()},{inputs[1].data(),inputs[1].size()}};
    std::vector<std::vector<std::uint8_t>> out;
    auto t0 = clock::steady_ns();
    for (std::uint64_t i=0;i<iters;++i) ReferenceEngine::execute(CollectiveKind::ALL_REDUCE, ReductionOp::SUM, Datatype::FLOAT32, N, 0, 2, spans, out);
    double nsp = ns_per_op(t0, iters);
    double bytes_per_sec = (double)(N*4*2) / (nsp/1e9);
    printf("  allreduce_reference  %.3f us/op   %.0f ops/s   %.3f GiB/s (logical %llu bytes/op)\n", nsp/1000.0, 1e9/nsp, bytes_per_sec/1e9, (unsigned long long)(N*4*2));
  }

  // all-gather reference
  {
    const std::uint64_t iters = 2000;
    std::vector<float> a(N,7.0f), b(N,8.0f);
    std::vector<std::vector<std::uint8_t>> inputs;
    inputs.push_back(std::vector<std::uint8_t>(reinterpret_cast<std::uint8_t*>(a.data()), reinterpret_cast<std::uint8_t*>(a.data()+N)));
    inputs.push_back(std::vector<std::uint8_t>(reinterpret_cast<std::uint8_t*>(b.data()), reinterpret_cast<std::uint8_t*>(b.data()+N)));
    std::vector<ReferenceEngine::Span> spans{{inputs[0].data(),inputs[0].size()},{inputs[1].data(),inputs[1].size()}};
    std::vector<std::vector<std::uint8_t>> out;
    auto t0 = clock::steady_ns();
    for (std::uint64_t i=0;i<iters;++i) ReferenceEngine::execute(CollectiveKind::ALL_GATHER, ReductionOp::NONE, Datatype::FLOAT32, N, 0, 2, spans, out);
    double nsp = ns_per_op(t0, iters);
    printf("  allgather_reference  %.3f us/op   %.0f ops/s\n", nsp/1000.0, 1e9/nsp);
  }

  // explanation generation
  {
    const std::uint64_t iters = 20000;
    auto plan = pl.plan(pin);
    auto t0 = clock::steady_ns();
    std::string s;
    for (std::uint64_t i=0;i<iters;++i) s = explain_plan(plan);
    double nsp = ns_per_op(t0, iters);
    printf("  explanation_generation  %.3f us/op   %.0f ops/s\n", nsp/1000.0, 1e9/nsp);
    (void)s;
  }

  // persistence serialization + recovery
  {
    PersistedGroup pg; pg.group_id=CollectiveGroupId(1); pg.generation=GroupGeneration(1); pg.membership=MembershipGeneration(1);
    pg.topology=TopologyGeneration(1); pg.health=HealthGeneration(0); pg.backend=BackendGeneration(1); pg.lifecycle=GroupLifecycle::READY;
    for (int i=0;i<4;++i){ PersistedParticipant pp; pp.participant_id=ParticipantId(i+1); pp.worker=WorkerId(i+1); pp.boot=WorkerBootId(100+i); pp.node=NodeId(1); pp.rank=RankId(i); pp.backend_generation=BackendGeneration(1); pp.is_known=true; pg.participants.push_back(pp); }
    PersistentState st; st.epoch=CoordinatorEpoch(1); st.live_boot=WorkerBootId(100); st.groups.push_back(pg);
    const std::uint64_t iters = 5000;
    auto t0 = clock::steady_ns();
    std::vector<std::uint8_t> payload;
    for (std::uint64_t i=0;i<iters;++i) payload = Store::encode(st);
    double nsp = ns_per_op(t0, iters);
    printf("  persistence_serialize  %.3f us/op   %.0f ops/s   %llu bytes/op\n", nsp/1000.0, 1e9/nsp, (unsigned long long)payload.size());
    auto t1 = clock::steady_ns();
    PersistentState dec;
    for (std::uint64_t i=0;i<iters;++i) dec = Store::decode(payload);
    double nsp2 = ns_per_op(t1, iters);
    printf("  persistence_recover  %.3f us/op   %.0f ops/s\n", nsp2/1000.0, 1e9/nsp2);
    (void)dec;
  }

  // protocol encode/decode
  {
    Frame f; f.kind=ProtocolMessageKind::CONTRIBUTION; f.payload.assign(64,0x5a);
    const std::uint64_t iters = 50000;
    auto t0 = clock::steady_ns();
    std::vector<std::uint8_t> bytes;
    for (std::uint64_t i=0;i<iters;++i) bytes = Protocol::encode(f);
    double nsp = ns_per_op(t0, iters);
    printf("  protocol_encode  %.3f us/op   %.0f ops/s   %zu bytes/frame\n", nsp/1000.0, 1e9/nsp, bytes.size());
    auto t1 = clock::steady_ns();
    Frame out; std::size_t consumed; std::string reason;
    for (std::uint64_t i=0;i<iters;++i) Protocol::decode(std::span<const std::uint8_t>(bytes.data(), bytes.size()), consumed, out, reason);
    double nsp2 = ns_per_op(t1, iters);
    printf("  protocol_decode  %.3f us/op   %.0f ops/s\n", nsp2/1000.0, 1e9/nsp2);
    (void)out;
  }

  // concurrent planning
  {
    const std::uint64_t iters = 20000;
    auto t0 = clock::steady_ns();
    for (std::uint64_t i=0;i<iters;++i) { auto p = pl.plan(pin); (void)p; }
    double nsp = ns_per_op(t0, iters);
    printf("  concurrent_planning  %.3f us/op   %.0f ops/s\n", nsp/1000.0, 1e9/nsp);
  }

  // measurement ingestion
  {
    const std::uint64_t iters = 20000;
    auto t0 = clock::steady_ns();
    for (std::uint64_t i=0;i<iters;++i){ CollectiveMeasurement m; m.id=MeasurementId(i%1000); m.collective=CollectiveId(1); rt.ingest_measurement(std::move(m)); }
    double nsp = ns_per_op(t0, iters);
    printf("  measurement_ingest  %.3f us/op   %.0f ops/s\n", nsp/1000.0, 1e9/nsp);
  }

  printf("\nNote: host-staged CUDA/TCP collective performance is reported separately in the CUDA proof (test_cuda), not here.\n");
  return 0;
}
