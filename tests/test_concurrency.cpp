#include "test_util.hpp"
#include "collectivefabric/runtime.hpp"
#include "collectivefabric/backend/reference_backend.hpp"
#include "collectivefabric/planner/planner.hpp"
#include <thread>
#include <vector>
#include <atomic>
using namespace collectivefabric;

static void make_runtime(Runtime& rt) {
  auto backend = std::make_shared<ReferenceBackend>(BackendId(1), BackendGeneration(1));
  rt.register_backend(backend);
  Topology topo(TopologyGeneration(1)); topo.set_provenance(TopologyProvenance::SYNTHETIC);
  auto nd = topo.add_node("n");
  auto d0 = topo.add_device(nd, "d0", false, 0);
  auto d1 = topo.add_device(nd, "d1", false, 0);
  rt.adopt_topology(topo);
  GroupBuilder gb(CollectiveGroupId(1), GroupGeneration(1));
  gb.membership_generation(MembershipGeneration(1)).topology_generation(TopologyGeneration(1));
  gb.add_participant(ParticipantId(1), WorkerId(1), WorkerBootId(100), nd, d0, BackendGeneration(1), RankId(0));
  gb.add_participant(ParticipantId(2), WorkerId(2), WorkerBootId(200), nd, d1, BackendGeneration(1), RankId(1));
  rt.register_group(gb.build());
}

int main() {
  Runtime rt;
  make_runtime(rt);
  const auto* grp = rt.find_group(CollectiveGroupId(1), GroupGeneration(1));
  const auto* topo = rt.topology();
  const auto* cap = &rt.find_backend(BackendId(1))->capabilities();

  // Concurrent collective submissions: each thread uses its own collective id.
  const int T = 8;
  std::atomic<int> success{0};
  std::atomic<bool> failed{false};
  std::vector<std::thread> threads;
  for (int t = 0; t < T; ++t) {
    threads.emplace_back([&, t]() {
      try {
        CollectiveDescriptor d;
        d.group_id(CollectiveGroupId(1)).group_generation(GroupGeneration(1)).collective_generation(CollectiveGeneration(t + 2));
        d.kind(CollectiveKind::ALL_REDUCE).reduction(ReductionOp::SUM).datatype(Datatype::FLOAT32).element_count(4);
        d.collective_id(CollectiveId(1000 + t)).policy_generation(PolicyGeneration(1));
        auto eid = rt.create_collective(d);
        PlannerInput pin; pin.descriptor=&d; pin.group=grp; pin.topology=topo; pin.backend=cap;
        rt.plan_collective(eid, pin);
        rt.reserve_collective(eid);
        rt.dispatch_collective(eid);
        std::vector<float> a{1,2,3,4}, b{10,20,30,40};
        std::vector<std::vector<std::uint8_t>> inputs;
        inputs.push_back(std::vector<std::uint8_t>(reinterpret_cast<std::uint8_t*>(a.data()), reinterpret_cast<std::uint8_t*>(a.data()+4)));
        inputs.push_back(std::vector<std::uint8_t>(reinterpret_cast<std::uint8_t*>(b.data()), reinterpret_cast<std::uint8_t*>(b.data()+4)));
        auto mid = rt.execute_collective(eid, inputs);
        auto outs = rt.outputs(eid);
        if (outs.size() == 2 && ((const float*)outs[0].data())[0] == 11.0f) ++success;
        else failed = true;
      } catch (...) { failed = true; }
    });
  }
  for (auto& th : threads) th.join();
  CF_CHECK(!failed.load());
  CF_CHECK(success.load() == T);

  // Concurrent group lookups.
  std::atomic<int> lookups{0};
  std::vector<std::thread> lt;
  for (int t = 0; t < 8; ++t) lt.emplace_back([&]() {
    for (int i = 0; i < 100; ++i) if (rt.find_group(CollectiveGroupId(1), GroupGeneration(1))) ++lookups;
  });
  for (auto& th : lt) th.join();
  CF_CHECK(lookups.load() == 800);

  // Concurrent measurement ingest + lookups.
  std::vector<std::thread> mt;
  for (int t = 0; t < 8; ++t) mt.emplace_back([&, t]() {
    CollectiveMeasurement m; m.id = MeasurementId(500 + t); m.success = true; m.provenance = MeasurementProvenance::MEASURED;
    rt.ingest_measurement(m);
  });
  for (auto& th : mt) th.join();
  CF_CHECK(rt.measurement_count() >= 8);

  // Duplicate completion race: exactly one of two threads succeeds.
  {
    CollectiveDescriptor d;
    d.group_id(CollectiveGroupId(1)).group_generation(GroupGeneration(1)).collective_generation(CollectiveGeneration(50));
    d.kind(CollectiveKind::ALL_REDUCE).reduction(ReductionOp::SUM).datatype(Datatype::FLOAT32).element_count(4);
    d.collective_id(CollectiveId(999)).policy_generation(PolicyGeneration(1));
    auto eid = rt.create_collective(d);
    auto* g = rt.find_group(CollectiveGroupId(1), GroupGeneration(1));
    const auto* c2 = &rt.find_backend(BackendId(1))->capabilities();
    PlannerInput pin; pin.descriptor=&d; pin.group=g; pin.topology=rt.topology(); pin.backend=c2;
    rt.plan_collective(eid, pin);
    rt.reserve_collective(eid);
    rt.dispatch_collective(eid);
    rt.accounting().active_rank_contribution();
    std::atomic<int> okc{0};
    std::thread t1([&](){ try { rt.complete_rank(eid, RankId(0)); ++okc; } catch (...) {} });
    std::thread t2([&](){ try { rt.complete_rank(eid, RankId(0)); ++okc; } catch (...) {} });
    t1.join(); t2.join();
    CF_CHECK(okc.load() == 1);
  }

  CF_FINISH("test_concurrency");
}
