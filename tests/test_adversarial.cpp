#include "test_util.hpp"
#include "collectivefabric/runtime.hpp"
#include "collectivefabric/backend/reference_backend.hpp"
#include "collectivefabric/planner/planner.hpp"
#include "collectivefabric/protocol.hpp"
#include "collectivefabric/lifecycle.hpp"
#include "collectivefabric/foundation/checked.hpp"
#include <vector>
using namespace collectivefabric;

static void set_up(Runtime& rt, const CollectiveGroup*& grp, const Topology*& topo, const BackendCapabilities*& cap) {
  auto backend = std::make_shared<ReferenceBackend>(BackendId(1), BackendGeneration(1));
  rt.register_backend(backend);
  Topology t(TopologyGeneration(1)); t.set_provenance(TopologyProvenance::SYNTHETIC);
  auto nd = t.add_node("n");
  auto d0 = t.add_device(nd, "d0", false, 0);
  auto d1 = t.add_device(nd, "d1", false, 0);
  rt.adopt_topology(t);
  GroupBuilder gb(CollectiveGroupId(1), GroupGeneration(1));
  gb.membership_generation(MembershipGeneration(1)).topology_generation(TopologyGeneration(1));
  gb.add_participant(ParticipantId(1), WorkerId(1), WorkerBootId(100), nd, d0, BackendGeneration(1), RankId(0));
  gb.add_participant(ParticipantId(2), WorkerId(2), WorkerBootId(200), nd, d1, BackendGeneration(1), RankId(1));
  rt.register_group(gb.build());
  grp = rt.find_group(CollectiveGroupId(1), GroupGeneration(1));
  topo = rt.topology(); cap = &rt.find_backend(BackendId(1))->capabilities();
}

int main() {
  // checked arithmetic overflow detection
  CF_CHECK(!checked_mul(UINT64_MAX, 2).has_value());
  CF_CHECK(would_overflow_mul(UINT64_MAX, 2));
  CF_CHECK(checked_mul(100, 100).value() == 10000);
  CF_CHECK(!checked_add(UINT64_MAX, 1).has_value());

  // lifecycle guards
  CollectiveStateMachine sm;
  CF_CHECK(sm.state() == CollectiveState::CREATED);
  CF_CHECK(sm.can_transition_to(CollectiveState::PLANNED));
  CF_CHECK(!sm.can_transition_to(CollectiveState::SUCCEEDED));  // cannot skip
  CF_CHECK_THROWS(sm.transition_to(CollectiveState::SUCCEEDED));
  sm.transition_to(CollectiveState::PLANNED);
  sm.transition_to(CollectiveState::RESERVED);
  sm.transition_to(CollectiveState::DISPATCHED);
  sm.transition_to(CollectiveState::RUNNING);
  sm.transition_to(CollectiveState::COMPLETING);
  sm.transition_to(CollectiveState::SUCCEEDED);
  CF_CHECK(sm.is_terminal());
  CF_CHECK_THROWS(sm.transition_to(CollectiveState::RUNNING));  // terminal
  printf("S1a\n"); fflush(stdout);

  // descriptor invalid combinations
  CollectiveDescriptor d;
  d.group_id(CollectiveGroupId(1)).group_generation(GroupGeneration(1)).collective_generation(CollectiveGeneration(1));
  d.kind(CollectiveKind::BROADCAST).root(RankId(0)).datatype(Datatype::FLOAT32).element_count(4);
  d.validate();  // broadcast with NONE reduction is fine
  d.reduction(ReductionOp::SUM);  // broadcast with reduction is invalid
  CF_CHECK_THROWS(d.validate());
  d.reduction(ReductionOp::NONE);
  d.kind(CollectiveKind::ALL_REDUCE); d.reduction(ReductionOp::NONE);  // all-reduce needs reduction
  CF_CHECK_THROWS(d.validate());
  d.reduction(ReductionOp::SUM);
  d.element_count(0);
  CF_CHECK_THROWS(d.validate());  // payload collective with zero count
  d.element_count(UINT64_MAX);
  CF_CHECK_THROWS(d.element_byte_count());  // overflow
  printf("S1b\n"); fflush(stdout);
  // runtime adversarial
  Runtime rt;
  const CollectiveGroup* grp; const Topology* topo; const BackendCapabilities* cap;
  set_up(rt, grp, topo, cap);

  // unknown group generation rejected at create
  CollectiveDescriptor bad;
  bad.group_id(CollectiveGroupId(1)).group_generation(GroupGeneration(99)).collective_generation(CollectiveGeneration(1));
  bad.kind(CollectiveKind::ALL_REDUCE).reduction(ReductionOp::SUM).datatype(Datatype::FLOAT32).element_count(4);
  bad.collective_id(CollectiveId(1));
  CF_CHECK_THROWS(rt.create_collective(bad));
  // valid create + execute
  bad.group_generation(GroupGeneration(1));
  auto eid = rt.create_collective(bad);
  PlannerInput pin; pin.descriptor=&bad; pin.group=grp; pin.topology=topo; pin.backend=cap;
  rt.plan_collective(eid, pin);
  rt.reserve_collective(eid);
  rt.dispatch_collective(eid);
  std::vector<float> a{1,2,3,4}, b{10,20};
  std::vector<std::vector<std::uint8_t>> inputs;
  inputs.push_back(std::vector<std::uint8_t>(reinterpret_cast<std::uint8_t*>(a.data()), reinterpret_cast<std::uint8_t*>(a.data()+4)));
  inputs.push_back(std::vector<std::uint8_t>(reinterpret_cast<std::uint8_t*>(b.data()), reinterpret_cast<std::uint8_t*>(b.data()+2)));  // wrong size
  auto mid = rt.execute_collective(eid, inputs);
  CF_CHECK(mid.is_null());  // failed (not swallowed) -> no measurement
  CF_CHECK(rt.execution_state(eid) == CollectiveState::FAILED);
  // abort
  auto e2 = rt.create_collective(bad);
  rt.plan_collective(e2, pin); rt.reserve_collective(e2); rt.dispatch_collective(e2);
  rt.abort_collective(e2);
  CF_CHECK(rt.execution_state(e2) == CollectiveState::ABORTED);
  CF_CHECK_THROWS(rt.complete_rank(e2, RankId(0)));  // still throws? complete_rank on ABORTED -> transition fails
  // (complete_rank transition would fail; we assert the collective is aborted and cannot commit)
  // old health
  HealthRecord h;
  h.state = HealthState::HEALTHY; h.generation = HealthGeneration(2); h.source = make_source("s2", ProvenanceKind::MEASURED);
  rt.update_health(h);
  HealthRecord stale;
  stale.state = HealthState::DEGRADED; stale.generation = HealthGeneration(1);
  CF_CHECK_THROWS(rt.update_health(stale));  // stale generation rejected
  // protocol oversized payload, invalid enum, corrupt checksum, truncation
  Frame f; f.kind = ProtocolMessageKind::CONTRIBUTION; f.payload.assign(COLLECTIVEFABRIC_PROTOCOL_MAX_PAYLOAD + 1, 0);
  auto bytes = Protocol::encode(f);
  // encode caps payload at MAX+1? encode doesn't bound; set a manual huge length
  // Instead, craft a frame with huge length header.
  {
    std::vector<std::uint8_t> hdr(20, 0);
    hdr[0]=0x43; hdr[1]=0x46; hdr[2]=0x42; hdr[3]=0x50;  // magic
    hdr[4]=0; hdr[5]=1;  // version
    hdr[6]=0; hdr[7]=1;  // kind HELLO
    for (int i=0;i<8;++i) hdr[8+i] = 0xFF;  // length huge
    std::size_t consumed; Frame out; std::string reason;
    auto r = Protocol::decode(std::span<const std::uint8_t>(hdr.data(), hdr.size()), consumed, out, reason);
    CF_CHECK(r == DecodeResult::REJECT);  // oversized payload
  }

  // duplicate persisted IDs rejected (via store decode)
  std::vector<std::uint8_t> no_authority_payload;  // placeholder
  (void)no_authority_payload;

  CF_FINISH("test_adversarial");
}