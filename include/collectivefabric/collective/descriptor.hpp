#pragma once
// Collective Fabric - collective descriptor. A descriptor fully and
// deterministically describes a collective: which participants, at which
// generation, performing which operation on which datatype, with what buffer
// and in-place semantics, on which backend/transport, under which policy.
#include "collectivefabric/foundation/identifier.hpp"
#include "collectivefabric/foundation/generation.hpp"
#include "collectivefabric/foundation/enums.hpp"
#include "collectivefabric/foundation/errors.hpp"
#include "collectivefabric/foundation/checked.hpp"
#include "collectivefabric/foundation/source.hpp"
#include "collectivefabric/digest/canonical.hpp"
#include "collectivefabric/digest/sha256.hpp"
#include <cstdint>
#include <optional>
#include <string>
#include <vector>
#include <algorithm>

namespace collectivefabric {

enum class InPlace : std::uint8_t { OUT_OF_PLACE, IN_PLACE };

struct BufferRequirement {
  BufferPlacement placement = BufferPlacement::HOST;
  std::uint64_t element_count = 0;  // per-rank element count (all_gather: per rank contribution)
  std::uint64_t byte_count = 0;
  InPlace in_place = InPlace::OUT_OF_PLACE;
};

class CollectiveDescriptor {
public:
  CollectiveDescriptor() = default;

  // Mutators (fluent) ------------------------------------------------------
  CollectiveDescriptor& collective_id(CollectiveId id) { collective_id_ = id; return *this; }
  CollectiveDescriptor& group_id(CollectiveGroupId id) { group_id_ = id; return *this; }
  CollectiveDescriptor& group_generation(GroupGeneration g) { group_generation_ = g; return *this; }
  CollectiveDescriptor& collective_generation(CollectiveGeneration g) { collective_generation_ = g; return *this; }
  CollectiveDescriptor& kind(CollectiveKind k) { kind_ = k; return *this; }
  CollectiveDescriptor& reduction(ReductionOp r) { reduction_ = r; return *this; }
  CollectiveDescriptor& datatype(Datatype d) { datatype_ = d; return *this; }
  CollectiveDescriptor& element_count(std::uint64_t n) { element_count_ = n; return *this; }
  CollectiveDescriptor& root(RankId r) { root_ = r; has_root_ = true; return *this; }
  CollectiveDescriptor& requested_backend(BackendId b) { requested_backend_ = b; return *this; }
  CollectiveDescriptor& policy_generation(PolicyGeneration p) { policy_generation_ = p; return *this; }
  CollectiveDescriptor& source_info(Source s) { source_ = std::move(s); return *this; }
  CollectiveDescriptor& scheduling_intent(SchedulingIntent s) { intent_ = s; return *this; }
  CollectiveDescriptor& in_place(InPlace v) { in_place_ = v; return *this; }
  CollectiveDescriptor& add_constraint(TransportClass t) { constraints_.push_back(t); return *this; }
  CollectiveDescriptor& input_placement(BufferPlacement p) { input_placement_ = p; return *this; }
  CollectiveDescriptor& output_placement(BufferPlacement p) { output_placement_ = p; return *this; }

  // Accessors --------------------------------------------------------------
  CollectiveId collective_id() const noexcept { return collective_id_; }
  CollectiveGroupId group_id() const noexcept { return group_id_; }
  GroupGeneration group_generation() const noexcept { return group_generation_; }
  CollectiveGeneration collective_generation() const noexcept { return collective_generation_; }
  CollectiveKind kind() const noexcept { return kind_; }
  ReductionOp reduction() const noexcept { return reduction_; }
  Datatype datatype() const noexcept { return datatype_; }
  std::uint64_t element_count() const noexcept { return element_count_; }
  RankId root() const noexcept { return root_; }
  bool has_root() const noexcept { return has_root_; }
  BackendId requested_backend() const noexcept { return requested_backend_; }
  PolicyGeneration policy_generation() const noexcept { return policy_generation_; }
  const Source& source() const noexcept { return source_; }
  SchedulingIntent scheduling_intent() const noexcept { return intent_; }
  InPlace in_place() const noexcept { return in_place_; }
  const std::vector<TransportClass>& constraints() const noexcept { return constraints_; }
  BufferPlacement input_placement() const noexcept { return input_placement_; }
  BufferPlacement output_placement() const noexcept { return output_placement_; }

  // Derived size validation. Returns the byte count for one rank's element
  // payload (input or output), or throws on overflow.
  std::uint64_t element_byte_count() const {
    auto bytes = checked_mul(element_count_, datatype_size_bytes(datatype_));
    if (!bytes) throw Error(ErrorCode::ARITHMETIC_OVERFLOW, "element_count * datatype_size overflows uint64");
    return *bytes;
  }

  // Full reduction payload for ALL_GATHER / REDUCE_SCATTER where the total per
  // rank is rank_count * element_count. 'rank_count' is supplied by the group.
  std::uint64_t aggregate_byte_count(std::uint64_t rank_count) const {
    auto per = checked_mul(element_count_, datatype_size_bytes(datatype_));
    if (!per) throw Error(ErrorCode::ARITHMETIC_OVERFLOW, "element_count * datatype_size overflows uint64");
    auto total = checked_mul(*per, rank_count);
    if (!total) throw Error(ErrorCode::ARITHMETIC_OVERFLOW, "per-rank * rank_count overflows uint64");
    return *total;
  }

  // Validate operation/datatype compatibility and structural size properties.
  // Throws Error(VALIDATION) on any invalid combination.
  void validate() const {
    if (kind_ == CollectiveKind::REDUCE || kind_ == CollectiveKind::ALL_REDUCE ||
        kind_ == CollectiveKind::REDUCE_SCATTER) {
      if (reduction_ == ReductionOp::NONE) {
        throw Error(ErrorCode::VALIDATION, "reduction collective requires a reduction operation");
      }
    } else if (kind_ == CollectiveKind::BARRIER || kind_ == CollectiveKind::BROADCAST) {
      if (reduction_ != ReductionOp::NONE) {
        throw Error(ErrorCode::VALIDATION, "barrier/broadcast must not carry a reduction operation");
      }
    }
    if (datatype_ == static_cast<Datatype>(0xff)) {
      throw Error(ErrorCode::VALIDATION, "invalid datatype");
    }
    if (datatype_size_bytes(datatype_) == 0) {
      throw Error(ErrorCode::VALIDATION, "datatype has no known byte size");
    }
    if (kind_ == CollectiveKind::BARRIER) {
      // barrier carries no payload
      return;
    }
    (void)element_byte_count();  // overflow check
    if ((kind_ == CollectiveKind::BROADCAST || kind_ == CollectiveKind::REDUCE) && !has_root_) {
      throw Error(ErrorCode::VALIDATION, "broadcast/reduce requires a root rank");
    }
    if (kind_ == CollectiveKind::BROADCAST || kind_ == CollectiveKind::REDUCE ||
        kind_ == CollectiveKind::ALL_REDUCE || kind_ == CollectiveKind::ALL_GATHER ||
        kind_ == CollectiveKind::REDUCE_SCATTER) {
      if (element_count_ == 0) throw Error(ErrorCode::VALIDATION, "payload collectives require a nonzero element count");
    }
  }

  // Deterministic canonical identity digest (semantic identity).
  Sha256::Digest canonical_digest() const {
    CanonicalWriter w;
    w.u64(collective_id_.raw());
    w.u64(group_id_.raw());
    w.u64(group_generation_.value());
    w.u64(collective_generation_.value());
    w.u8(static_cast<std::uint8_t>(kind_));
    w.u8(static_cast<std::uint8_t>(reduction_));
    w.u8(static_cast<std::uint8_t>(datatype_));
    w.u64(element_count_);
    w.u64(root_.raw());
    w.boolean(has_root_);
    w.u8(static_cast<std::uint8_t>(in_place_));
    w.u64(requested_backend_.raw());
    w.u64(policy_generation_.value());
    w.u8(static_cast<std::uint8_t>(input_placement_));
    w.u8(static_cast<std::uint8_t>(output_placement_));
    w.u8(static_cast<std::uint8_t>(intent_));
    w.u64(constraints_.size());
    for (auto t : constraints_) w.u8(static_cast<std::uint8_t>(t));
    return Sha256::digest(w.data());
  }

private:
  CollectiveId collective_id_;
  CollectiveGroupId group_id_;
  GroupGeneration group_generation_;
  CollectiveGeneration collective_generation_;
  CollectiveKind kind_ = CollectiveKind::BARRIER;
  ReductionOp reduction_ = ReductionOp::NONE;
  Datatype datatype_ = Datatype::BYTE;
  std::uint64_t element_count_ = 0;
  RankId root_;
  bool has_root_ = false;
  BackendId requested_backend_;
  PolicyGeneration policy_generation_;
  Source source_;
  SchedulingIntent intent_ = SchedulingIntent::DEFAULT;
  InPlace in_place_ = InPlace::OUT_OF_PLACE;
  std::vector<TransportClass> constraints_;
  BufferPlacement input_placement_ = BufferPlacement::HOST;
  BufferPlacement output_placement_ = BufferPlacement::HOST;
};

} // namespace collectivefabric
