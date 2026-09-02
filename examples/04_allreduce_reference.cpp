#include "collectivefabric/reference/engine.hpp"
#include <cstdio>
#include <vector>
using namespace collectivefabric;
int main() {
  float a[4]={1,2,3,4}, b[4]={10,20,30,40};
  std::vector<std::uint8_t> ba(reinterpret_cast<std::uint8_t*>(a), reinterpret_cast<std::uint8_t*>(a+4));
  std::vector<std::uint8_t> bb(reinterpret_cast<std::uint8_t*>(b), reinterpret_cast<std::uint8_t*>(b+4));
  std::vector<ReferenceEngine::Span> inputs{{ba.data(),ba.size()},{bb.data(),bb.size()}};
  std::vector<std::vector<std::uint8_t>> out;
  ReferenceEngine::execute(CollectiveKind::ALL_REDUCE, ReductionOp::SUM, Datatype::FLOAT32, 4, 0, 2, inputs, out);
  printf("all-reduce rank0[0]=%f rank1[3]=%f\n", ((float*)out[0].data())[0], ((float*)out[1].data())[3]);
  return 0;
}
