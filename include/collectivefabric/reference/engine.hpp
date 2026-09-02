#pragma once
// Collective Fabric - deterministic in-process reference collective engine.
// Serves as the oracle for correct reference semantics of all supported
// operations. Uses a fixed, documented reduction order: left fold in rank
// order (acc = op(acc, input[r]) for r = 1..n-1 starting from rank 0). This is
// deterministic but is NOT claimed to be mathematically associative for
// floating-point types.
#include "collectivefabric/foundation/identifier.hpp"
#include "collectivefabric/foundation/enums.hpp"
#include "collectivefabric/foundation/errors.hpp"
#include "collectivefabric/foundation/checked.hpp"
#include <cstdint>
#include <cstring>
#include <vector>
#include <span>
#include <algorithm>
#include <limits>

namespace collectivefabric {

class ReferenceEngine {
public:
  struct Span {
    const std::uint8_t* data = nullptr;
    std::size_t bytes = 0;
  };

  // Execute a reference collective. 'inputs' is indexed by rank order
  // (inputs[r] is the contribution from rank r). 'root_index' is the rank
  // index owning the root for BROADCAST/REDUCE. 'outputs' is resized to
  // rank_count and filled with each rank's authoritative result.
  static void execute(CollectiveKind kind, ReductionOp op, Datatype dt,
                      std::uint64_t element_count, std::uint64_t root_index,
                      std::uint64_t rank_count,
                      const std::vector<Span>& inputs,
                      std::vector<std::vector<std::uint8_t>>& outputs) {
    const std::size_t dt_size = datatype_size_bytes(dt);
    if (dt_size == 0) throw Error(ErrorCode::VALIDATION, "reference engine: unknown datatype size");
    auto per = checked_mul(element_count, dt_size);
    if (!per) throw Error(ErrorCode::ARITHMETIC_OVERFLOW, "reference engine: element_count*datatype_size overflow");
    const std::size_t per_bytes = static_cast<std::size_t>(*per);

    auto need_total = checked_mul(per_bytes, rank_count);
    if (!need_total) throw Error(ErrorCode::ARITHMETIC_OVERFLOW, "reference engine: rank_count*per_bytes overflow");
    const std::size_t total_bytes = static_cast<std::size_t>(*need_total);

    outputs.assign(rank_count, std::vector<std::uint8_t>());
    switch (kind) {
      case CollectiveKind::BARRIER: barrier(outputs, rank_count); break;
      case CollectiveKind::BROADCAST: broadcast(inputs, dt, op, per_bytes, root_index, rank_count, outputs); break;
      case CollectiveKind::REDUCE: reduce(inputs, dt, op, per_bytes, root_index, rank_count, outputs); break;
      case CollectiveKind::ALL_REDUCE: all_reduce(inputs, dt, op, per_bytes, rank_count, outputs); break;
      case CollectiveKind::ALL_GATHER: all_gather(inputs, per_bytes, rank_count, total_bytes, outputs); break;
      case CollectiveKind::REDUCE_SCATTER: reduce_scatter(inputs, dt, op, per_bytes, element_count, rank_count, outputs); break;
    }
  }

private:
  static void barrier(std::vector<std::vector<std::uint8_t>>& outputs, std::uint64_t rank_count) {
    for (std::uint64_t i = 0; i < rank_count; ++i) outputs[i].clear();
  }

  static void broadcast(const std::vector<Span>& inputs, Datatype, ReductionOp,
                        std::size_t per_bytes, std::uint64_t root_index,
                        std::uint64_t rank_count,
                        std::vector<std::vector<std::uint8_t>>& outputs) {
    if (root_index >= inputs.size() || inputs[root_index].bytes < per_bytes) {
      throw Error(ErrorCode::VALIDATION, "broadcast: root input too small");
    }
    for (std::uint64_t r = 0; r < rank_count; ++r) {
      outputs[r].assign(per_bytes, 0);
      std::memcpy(outputs[r].data(), inputs[root_index].data, per_bytes);
    }
  }

  static void reduce(const std::vector<Span>& inputs, Datatype dt, ReductionOp op,
                     std::size_t per_bytes, std::uint64_t root_index,
                     std::uint64_t rank_count,
                     std::vector<std::vector<std::uint8_t>>& outputs) {
    checksize(inputs, per_bytes, rank_count);
    outputs[root_index].assign(per_bytes, 0);
    reduce_into(inputs, dt, op, per_bytes, rank_count, outputs[root_index].data());
  }

  static void all_reduce(const std::vector<Span>& inputs, Datatype dt, ReductionOp op,
                         std::size_t per_bytes, std::uint64_t rank_count,
                         std::vector<std::vector<std::uint8_t>>& outputs) {
    checksize(inputs, per_bytes, rank_count);
    for (std::uint64_t r = 0; r < rank_count; ++r) outputs[r].assign(per_bytes, 0);
    reduce_into(inputs, dt, op, per_bytes, rank_count, outputs[0].data());
    for (std::uint64_t r = 1; r < rank_count; ++r) std::memcpy(outputs[r].data(), outputs[0].data(), per_bytes);
  }

  static void all_gather(const std::vector<Span>& inputs, std::size_t per_bytes,
                         std::uint64_t rank_count, std::size_t total_bytes,
                         std::vector<std::vector<std::uint8_t>>& outputs) {
    for (std::uint64_t r = 0; r < rank_count; ++r) {
      if (inputs[r].bytes < per_bytes) throw Error(ErrorCode::VALIDATION, "all_gather: rank input too small");
      outputs[r].assign(total_bytes, 0);
      for (std::uint64_t i = 0; i < rank_count; ++i)
        std::memcpy(outputs[r].data() + i * per_bytes, inputs[i].data, per_bytes);
    }
  }

  static void reduce_scatter(const std::vector<Span>& inputs, Datatype dt, ReductionOp op,
                             std::size_t per_bytes, std::uint64_t element_count,
                             std::uint64_t rank_count,
                             std::vector<std::vector<std::uint8_t>>& outputs) {
    auto total = checked_mul(per_bytes, rank_count);
    if (!total) throw Error(ErrorCode::ARITHMETIC_OVERFLOW, "reduce_scatter: overflow");
    const std::size_t total_bytes = static_cast<std::size_t>(*total);
    for (std::uint64_t r = 0; r < rank_count; ++r)
      if (inputs[r].bytes < total_bytes) throw Error(ErrorCode::VALIDATION, "reduce_scatter: rank input too small");
    for (std::uint64_t r = 0; r < rank_count; ++r) outputs[r].assign(per_bytes, 0);
    // For each block b, reduce across all ranks' block b; block b belongs to rank b.
    for (std::uint64_t b = 0; b < rank_count; ++b) {
      std::vector<Span> block_inputs;
      block_inputs.reserve(rank_count);
      for (std::uint64_t r = 0; r < rank_count; ++r)
        block_inputs.push_back(Span{inputs[r].data + b * per_bytes, per_bytes});
      reduce_into(block_inputs, dt, op, per_bytes, rank_count, outputs[b].data());
    }
    (void)element_count;
  }

  static void checksize(const std::vector<Span>& inputs, std::size_t per_bytes, std::uint64_t rank_count) {
    if (inputs.size() < rank_count) throw Error(ErrorCode::VALIDATION, "reference engine: missing rank contribution");
    for (std::uint64_t r = 0; r < rank_count; ++r)
      if (inputs[r].bytes < per_bytes) throw Error(ErrorCode::VALIDATION, "reference engine: rank contribution too small");
  }

  // Reduction left fold in rank order. Result written to out (per_bytes).
  static void reduce_into(const std::vector<Span>& inputs, Datatype dt, ReductionOp op,
                          std::size_t per_bytes, std::uint64_t rank_count, std::uint8_t* out) {
    switch (dt) {
      case Datatype::INT32: do_reduce<std::int32_t>(inputs, op, per_bytes, rank_count, out, reduce_i32); break;
      case Datatype::UINT32: do_reduce<std::uint32_t>(inputs, op, per_bytes, rank_count, out, reduce_u32); break;
      case Datatype::INT64: do_reduce<std::int64_t>(inputs, op, per_bytes, rank_count, out, reduce_i64); break;
      case Datatype::FLOAT32: do_reduce<float>(inputs, op, per_bytes, rank_count, out, reduce_f32); break;
      case Datatype::FLOAT64: do_reduce<double>(inputs, op, per_bytes, rank_count, out, reduce_f64); break;
      case Datatype::BYTE:
        throw Error(ErrorCode::VALIDATION, "reference engine: BYTE datatype not valid for reduction");
    }
  }

  static std::int32_t op32(std::int32_t a, std::int32_t b, ReductionOp op) {
    switch (op) { case ReductionOp::SUM: return a + b; case ReductionOp::PRODUCT: return a * b;
      case ReductionOp::MIN: return a < b ? a : b; case ReductionOp::MAX: return a > b ? a : b; default: return b; }
  }
  static std::uint32_t op32(std::uint32_t a, std::uint32_t b, ReductionOp op) {
    switch (op) { case ReductionOp::SUM: return a + b; case ReductionOp::PRODUCT: return a * b;
      case ReductionOp::MIN: return a < b ? a : b; case ReductionOp::MAX: return a > b ? a : b; default: return b; }
  }
  static std::uint64_t op64(std::int64_t a, std::int64_t b, ReductionOp op) {
    switch (op) { case ReductionOp::SUM: return static_cast<std::uint64_t>(a + b); case ReductionOp::PRODUCT: return static_cast<std::uint64_t>(a * b);
      case ReductionOp::MIN: return static_cast<std::uint64_t>(a < b ? a : b); case ReductionOp::MAX: return static_cast<std::uint64_t>(a > b ? a : b); default: return static_cast<std::uint64_t>(b); }
  }
  static std::uint64_t op64(std::uint64_t a, std::uint64_t b, ReductionOp op) {
    switch (op) { case ReductionOp::SUM: return a + b; case ReductionOp::PRODUCT: return a * b;
      case ReductionOp::MIN: return a < b ? a : b; case ReductionOp::MAX: return a > b ? a : b; default: return b; }
  }
  static float opf(float a, float b, ReductionOp op) {
    switch (op) { case ReductionOp::SUM: return a + b; case ReductionOp::PRODUCT: return a * b;
      case ReductionOp::MIN: return a < b ? a : b; case ReductionOp::MAX: return a > b ? a : b; default: return b; }
  }
  static double opf(double a, double b, ReductionOp op) {
    switch (op) { case ReductionOp::SUM: return a + b; case ReductionOp::PRODUCT: return a * b;
      case ReductionOp::MIN: return a < b ? a : b; case ReductionOp::MAX: return a > b ? a : b; default: return b; }
  }

  static std::int32_t reduce_i32(std::int32_t a, std::int32_t b, ReductionOp op) { return static_cast<std::int32_t>(op32(a, b, op)); }
  static std::uint32_t reduce_u32(std::uint32_t a, std::uint32_t b, ReductionOp op) { return op32(a, b, op); }
  static std::int64_t reduce_i64(std::int64_t a, std::int64_t b, ReductionOp op) { return static_cast<std::int64_t>(op64(a, b, op)); }
  static std::uint64_t reduce_u64(std::uint64_t a, std::uint64_t b, ReductionOp op) { return op64(a, b, op); }
  static float reduce_f32(float a, float b, ReductionOp op) { return opf(a, b, op); }
  static double reduce_f64(double a, double b, ReductionOp op) { return opf(a, b, op); }

  template <class T, class Op>
  static void do_reduce(const std::vector<Span>& inputs, ReductionOp op, std::size_t per_bytes,
                        std::uint64_t rank_count, std::uint8_t* out, Op opfn) {
    const std::size_t n = per_bytes / sizeof(T);
    const T* first = reinterpret_cast<const T*>(inputs[0].data);
    T* dst = reinterpret_cast<T*>(out);
    for (std::size_t i = 0; i < n; ++i) dst[i] = first[i];
    for (std::uint64_t r = 1; r < rank_count; ++r) {
      const T* src = reinterpret_cast<const T*>(inputs[r].data);
      for (std::size_t i = 0; i < n; ++i) dst[i] = opfn(dst[i], src[i], op);
    }
  }
};

} // namespace collectivefabric
