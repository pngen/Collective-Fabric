#pragma once
// Collective Fabric - collective group and membership. A group is immutable
// per GroupGeneration; membership changes always produce a new generation.
// Rank assignment is deterministic. Rank representation is dense (0..n-1).
#include "collectivefabric/foundation/identifier.hpp"
#include "collectivefabric/foundation/generation.hpp"
#include "collectivefabric/foundation/enums.hpp"
#include "collectivefabric/foundation/errors.hpp"
#include "collectivefabric/foundation/source.hpp"
#include "collectivefabric/digest/canonical.hpp"
#include "collectivefabric/digest/sha256.hpp"
#include <cstdint>
#include <optional>
#include <string>
#include <vector>
#include <algorithm>
#include <unordered_map>

namespace collectivefabric {

struct Participant {
  ParticipantId id;
  WorkerId worker;
  WorkerBootId boot;
  NodeId node;
  DeviceId device;
  RankId rank;
  BackendGeneration backend_generation;
  bool is_known = true;
};

class CollectiveGroup {
public:
  CollectiveGroup(CollectiveGroupId id, GroupGeneration generation, MembershipGeneration membership,
                  TopologyGeneration topology, HealthGeneration health, BackendGeneration backend,
                  GroupLifecycle lifecycle, Source provenance)
      : id_(id), generation_(generation), membership_(membership), topology_(topology),
        health_(health), backend_(backend), lifecycle_(lifecycle), provenance_(std::move(provenance)) {}

  const CollectiveGroupId& id() const noexcept { return id_; }
  const GroupGeneration& generation() const noexcept { return generation_; }
  const MembershipGeneration& membership_generation() const noexcept { return membership_; }
  const TopologyGeneration& topology_generation() const noexcept { return topology_; }
  const HealthGeneration& health_generation() const noexcept { return health_; }
  const BackendGeneration& backend_generation() const noexcept { return backend_; }
  GroupLifecycle lifecycle() const noexcept { return lifecycle_; }
  const Source& provenance() const noexcept { return provenance_; }

  std::size_t rank_count() const noexcept { return participants_.size(); }
  const std::vector<Participant>& participants() const noexcept { return participants_; }

  const Participant* participant_at(RankId r) const noexcept {
    for (const auto& p : participants_) if (p.rank == r) return &p;
    return nullptr;
  }
  const Participant* participant(ParticipantId id) const noexcept {
    for (const auto& p : participants_) if (p.id == id) return &p;
    return nullptr;
  }
  std::optional<RankId> rank_of(ParticipantId id) const noexcept {
    const auto* p = participant(id);
    if (!p) return std::nullopt;
    return p->rank;
  }
  bool is_live_authority(WorkerBootId boot) const noexcept {
    for (const auto& p : participants_) if (p.boot == boot) return true;
    return false;
  }

  void set_lifecycle(GroupLifecycle l) noexcept { lifecycle_ = l; }

  // Deterministic canonical digest of the group definition.
  Sha256::Digest digest() const {
    CanonicalWriter w;
    w.u64(id_.raw());
    w.u64(generation_.value());
    w.u64(membership_.value());
    w.u64(topology_.value());
    w.u64(health_.value());
    w.u64(backend_.value());
    w.u8(static_cast<std::uint8_t>(lifecycle_));
    w.u64(participants_.size());
    for (const auto& p : participants_) {
      w.u64(p.id.raw());
      w.u64(p.worker.raw());
      w.u64(p.boot.raw());
      w.u64(p.node.raw());
      w.u64(p.device.raw());
      w.u64(p.rank.raw());
      w.u64(p.backend_generation.value());
      w.boolean(p.is_known);
    }
    return Sha256::digest(w.data());
  }

private:
  CollectiveGroupId id_;
  GroupGeneration generation_;
  MembershipGeneration membership_;
  TopologyGeneration topology_;
  HealthGeneration health_;
  BackendGeneration backend_;
  GroupLifecycle lifecycle_;
  Source provenance_;
  std::vector<Participant> participants_;

  friend class GroupBuilder;
};

// Deterministic, validated group construction. Assigns dense ranks by sorting
// participants on ParticipantId when ranks are not explicitly assigned.
class GroupBuilder {
public:
  GroupBuilder(CollectiveGroupId id, GroupGeneration generation) : id_(id), generation_(generation) {}

  GroupBuilder& membership_generation(MembershipGeneration m) { membership_ = m; return *this; }
  GroupBuilder& topology_generation(TopologyGeneration t) { topology_ = t; return *this; }
  GroupBuilder& health_generation(HealthGeneration h) { health_ = h; return *this; }
  GroupBuilder& backend_generation(BackendGeneration b) { backend_ = b; return *this; }
  GroupBuilder& lifecycle(GroupLifecycle l) { lifecycle_ = l; return *this; }
  GroupBuilder& provenance(Source s) { provenance_ = std::move(s); return *this; }

  // Explicit rank; otherwise ranks assigned deterministically at build time.
  GroupBuilder& add_participant(ParticipantId id, WorkerId worker, WorkerBootId boot, NodeId node,
                                DeviceId device, BackendGeneration backend_gen, RankId rank = RankId{}) {
    participants_.push_back(Participant{id, worker, boot, node, device, rank, backend_gen, true});
    return *this;
  }

  // Build and validate. Throws Error(VALIDATION) on any invalid structure.
  CollectiveGroup build() {
    // Deterministic dense rank assignment.
    if (std::any_of(participants_.begin(), participants_.end(), [](const Participant& p) { return p.rank.is_null(); })) {
      // not all explicitly ranked: assign by sorted participant id
      std::vector<Participant> sorted = participants_;
      std::sort(sorted.begin(), sorted.end(), [](const Participant& a, const Participant& b) { return a.id < b.id; });
      for (std::size_t i = 0; i < sorted.size(); ++i) sorted[i].rank = RankId(i);
      participants_ = std::move(sorted);
    } else {
      // explicit ranks: sort by rank, then validate density
      std::sort(participants_.begin(), participants_.end(), [](const Participant& a, const Participant& b) { return a.rank < b.rank; });
    }

    validate();
    CollectiveGroup g(id_, generation_, membership_, topology_, health_, backend_, lifecycle_, provenance_);
    g.participants_ = participants_;
    return g;
  }

private:
  void validate() const {
    if (participants_.empty()) throw Error(ErrorCode::VALIDATION, "group has zero participants");
    if (membership_.is_null()) throw Error(ErrorCode::VALIDATION, "group requires a membership generation");
    if (topology_.is_null()) throw Error(ErrorCode::VALIDATION, "group requires a topology generation");
    // no duplicate participant id
    for (std::size_t i = 0; i < participants_.size(); ++i)
      for (std::size_t j = i + 1; j < participants_.size(); ++j) {
        if (participants_[i].id == participants_[j].id) throw Error(ErrorCode::VALIDATION, "duplicate ParticipantId");
        if (participants_[i].rank == participants_[j].rank) throw Error(ErrorCode::VALIDATION, "duplicate RankId");
        if (participants_[i].worker == participants_[j].worker && participants_[i].boot == participants_[j].boot)
          throw Error(ErrorCode::VALIDATION, "duplicate boot identity");
      }
    // dense ranks 0..n-1
    for (std::size_t i = 0; i < participants_.size(); ++i)
      if (participants_[i].rank.raw() != i) throw Error(ErrorCode::VALIDATION, "rank hole or non-dense rank representation");
    // conflicting boot identities within a participant
    for (const auto& p : participants_) {
      if (p.worker.is_null() || p.boot.is_null()) throw Error(ErrorCode::VALIDATION, "participant requires worker and boot identity");
    }
  }

  CollectiveGroupId id_;
  GroupGeneration generation_;
  MembershipGeneration membership_{1};
  TopologyGeneration topology_{1};
  HealthGeneration health_{0};
  BackendGeneration backend_{0};
  GroupLifecycle lifecycle_ = GroupLifecycle::CREATING;
  Source provenance_;
  std::vector<Participant> participants_;
};

} // namespace collectivefabric
