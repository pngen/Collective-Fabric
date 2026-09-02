#include "collectivefabric/reference/engine.hpp"
#include <cstdio>
#include <vector>
using namespace collectivefabric;
int main() {
  std::int64_t s0[4]={1,2,10,20}, s1[4]={100,200,1000,2000};
  std::vector<std::uint8_t> b0(reinterpret_cast<std::uint8_t*>(s0), reinterpret_cast<std::uint8_t*>(s0+4));
  std::vector<std::uint8_t> b1(reinterpret_cast<std::uint8_t*>(s1), reinterpret_cast<std::uint8_t*>(s1+4));
  std::vector<ReferenceEngine::Span> inputs{{b0.data(),b0.size()},{b1.data(),b1.size()}};
  std::vector<std::vector<std::uint8_t>> out;
  ReferenceEngine::execute(CollectiveKind::REDUCE_SCATTER, ReductionOp::SUM, Datatype::INT64, 2, 0, 2, inputs, out);
  printf("reduce-scatter rank0=[%lld,%lld] rank1=[%lld,%lld]\n",
     (long long)((std::int64_t*)out[0].data())[0], (long long)((std::int64_t*)out[0].data())[1],
     (long long)((std::int64_t*)out[1].data())[0], (long long)((std::int64_t*)out[1].data())[1]);
  return 0;
}
