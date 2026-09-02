#include "collectivefabric/reference/engine.hpp"
#include <cstdio>
#include <vector>
using namespace collectivefabric;
int main() {
  std::int64_t a[2]={5,6}, b[2]={7,8};
  std::vector<std::uint8_t> ba(reinterpret_cast<std::uint8_t*>(a), reinterpret_cast<std::uint8_t*>(a+2));
  std::vector<std::uint8_t> bb(reinterpret_cast<std::uint8_t*>(b), reinterpret_cast<std::uint8_t*>(b+2));
  std::vector<ReferenceEngine::Span> inputs{{ba.data(),ba.size()},{bb.data(),bb.size()}};
  std::vector<std::vector<std::uint8_t>> out;
  ReferenceEngine::execute(CollectiveKind::ALL_GATHER, ReductionOp::NONE, Datatype::INT64, 2, 0, 2, inputs, out);
  printf("all-gather rank0: [%lld, %lld, %lld, %lld]\n",
     (long long)((std::int64_t*)out[0].data())[0], (long long)((std::int64_t*)out[0].data())[1],
     (long long)((std::int64_t*)out[0].data())[2], (long long)((std::int64_t*)out[0].data())[3]);
  return 0;
}
