#include "test_util.hpp"
#include "collectivefabric/reference/engine.hpp"
#include "collectivefabric/collective/descriptor.hpp"
using namespace collectivefabric;

std::vector<std::vector<std::uint8_t>> mk_inputs(const std::vector<std::vector<float>>& xs) {
  std::vector<std::vector<std::uint8_t>> out;
  for (const auto& x : xs) out.push_back(std::vector<std::uint8_t>(reinterpret_cast<const std::uint8_t*>(x.data()), reinterpret_cast<const std::uint8_t*>(x.data()+x.size())));
  return out;
}
std::vector<ReferenceEngine::Span> spans_of(const std::vector<std::vector<std::uint8_t>>& in) {
  std::vector<ReferenceEngine::Span> s;
  for (const auto& x : in) s.push_back(ReferenceEngine::Span{x.data(), x.size()});
  return s;
}

int main() {
  std::vector<std::vector<std::uint8_t>> outputs;
  auto a = mk_inputs({{1,2,3,4},{10,20,30,40}});
  auto spans = spans_of(a);

  // ALL_REDUCE SUM
  ReferenceEngine::execute(CollectiveKind::ALL_REDUCE, ReductionOp::SUM, Datatype::FLOAT32, 4, 0, 2, spans, outputs);
  CF_CHECK(outputs.size() == 2);
  CF_CHECK(((const float*)outputs[0].data())[0] == 11.0f && ((const float*)outputs[0].data())[3] == 44.0f);
  CF_CHECK(((const float*)outputs[1].data())[2] == 33.0f);

  // ALL_REDUCE PRODUCT
  ReferenceEngine::execute(CollectiveKind::ALL_REDUCE, ReductionOp::PRODUCT, Datatype::FLOAT32, 4, 0, 2, spans, outputs);
  CF_CHECK(((const float*)outputs[0].data())[0] == 10.0f);

  // ALL_REDUCE MIN / MAX
  ReferenceEngine::execute(CollectiveKind::ALL_REDUCE, ReductionOp::MIN, Datatype::FLOAT32, 4, 0, 2, spans, outputs);
  CF_CHECK(((const float*)outputs[0].data())[0] == 1.0f);
  ReferenceEngine::execute(CollectiveKind::ALL_REDUCE, ReductionOp::MAX, Datatype::FLOAT32, 4, 0, 2, spans, outputs);
  CF_CHECK(((const float*)outputs[0].data())[0] == 10.0f);

  // int64 reduction exact
  std::int64_t x0[2] = {5, 1000000000};
  std::int64_t x1[2] = {7, 5};
  std::vector<std::uint8_t> b0(reinterpret_cast<const std::uint8_t*>(x0), reinterpret_cast<const std::uint8_t*>(x0+2));
  std::vector<std::uint8_t> b1(reinterpret_cast<const std::uint8_t*>(x1), reinterpret_cast<const std::uint8_t*>(x1+2));
  std::vector<ReferenceEngine::Span> is{{b0.data(), b0.size()}, {b1.data(), b1.size()}};
  ReferenceEngine::execute(CollectiveKind::ALL_REDUCE, ReductionOp::SUM, Datatype::INT64, 2, 0, 2, is, outputs);
  CF_CHECK(((const std::int64_t*)outputs[0].data())[0] == 12);
  CF_CHECK(((const std::int64_t*)outputs[0].data())[1] == 1000000005LL);

  // BROADCAST root value to all ranks
  std::vector<std::uint8_t> root(reinterpret_cast<const std::uint8_t*>(x1), reinterpret_cast<const std::uint8_t*>(x1+2));
  std::vector<ReferenceEngine::Span> br{{root.data(), root.size()}, {root.data(), root.size()}};
  ReferenceEngine::execute(CollectiveKind::BROADCAST, ReductionOp::NONE, Datatype::INT64, 2, 0, 2, br, outputs);
  CF_CHECK(((const std::int64_t*)outputs[1].data())[0] == 7);
  CF_CHECK(((const std::int64_t*)outputs[0].data())[1] == 5);

  // REDUCE -> root rank gets result
  ReferenceEngine::execute(CollectiveKind::REDUCE, ReductionOp::SUM, Datatype::INT64, 2, 0, 2, is, outputs);
  CF_CHECK(outputs[0].size() == 16);
  CF_CHECK(((const std::int64_t*)outputs[0].data())[0] == 12);

  // ALL_GATHER: each output has all contributions in rank order
  ReferenceEngine::execute(CollectiveKind::ALL_GATHER, ReductionOp::NONE, Datatype::INT64, 2, 0, 2, is, outputs);
  CF_CHECK(outputs[0].size() == 32);  // 2 ranks * 2 int64
  CF_CHECK(((const std::int64_t*)outputs[0].data())[0] == 5);   // rank0 element0
  CF_CHECK(((const std::int64_t*)outputs[0].data())[2] == 7);   // rank1 element0
  CF_CHECK(((const std::int64_t*)outputs[1].data())[0] == 5);

  // REDUCE_SCATTER: each rank gets its reduced block. Inputs: each rank
  // provides rank_count*element_count elements. Use 2 ranks, element_count=2.
  std::int64_t s0[4] = {1,2,10,20};   // rank0 block0={1,2}, block1={10,20}
  std::int64_t s1[4] = {100,200,1000,2000};
  std::vector<std::uint8_t> c0(reinterpret_cast<const std::uint8_t*>(s0), reinterpret_cast<const std::uint8_t*>(s0+4));
  std::vector<std::uint8_t> c1(reinterpret_cast<const std::uint8_t*>(s1), reinterpret_cast<const std::uint8_t*>(s1+4));
  std::vector<ReferenceEngine::Span> rs{{c0.data(), c0.size()}, {c1.data(), c1.size()}};
  ReferenceEngine::execute(CollectiveKind::REDUCE_SCATTER, ReductionOp::SUM, Datatype::INT64, 2, 0, 2, rs, outputs);
  // block0: reduce over both ranks' block0 -> {1+100, 2+200}={101,202}; rank0 gets it
  CF_CHECK(((const std::int64_t*)outputs[0].data())[0] == 101);
  CF_CHECK(((const std::int64_t*)outputs[0].data())[1] == 202);
  // block1: {10+1000, 20+2000}={1010,2020}; rank1 gets it
  CF_CHECK(((const std::int64_t*)outputs[1].data())[0] == 1010);

  // Descriptor validation.
  CollectiveDescriptor d;
  d.kind(CollectiveKind::ALL_REDUCE).reduction(ReductionOp::NONE).datatype(Datatype::FLOAT32).element_count(4);
  CF_CHECK_THROWS(d.validate());
  d.reduction(ReductionOp::SUM);
  CF_CHECK_NO_THROW(d.validate());
  d.element_count(1ULL << 60);  // huge * 4 bytes overflows? 2^60*4 = 2^62 fits uint64
  d.element_count(UINT64_MAX);
  CF_CHECK_THROWS(d.element_byte_count());  // overflow

  CF_FINISH("test_collective_semantics");
}
