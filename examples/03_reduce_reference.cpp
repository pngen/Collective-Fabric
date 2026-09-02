#include "collectivefabric/reference/engine.hpp"
#include <cstdio>
#include <vector>
using namespace collectivefabric;
int main() {
  float a[2]={1.0f,2.0f}, b[2]={3.0f,4.0f};
  std::vector<std::uint8_t> ba(reinterpret_cast<std::uint8_t*>(a), reinterpret_cast<std::uint8_t*>(a+2));
  std::vector<std::uint8_t> bb(reinterpret_cast<std::uint8_t*>(b), reinterpret_cast<std::uint8_t*>(b+2));
  std::vector<ReferenceEngine::Span> inputs{{ba.data(),ba.size()},{bb.data(),bb.size()}};
  std::vector<std::vector<std::uint8_t>> out;
  ReferenceEngine::execute(CollectiveKind::REDUCE, ReductionOp::SUM, Datatype::FLOAT32, 2, 0, 2, inputs, out);
  printf("reduce root[0]=%f root[1]=%f\n", ((float*)out[0].data())[0], ((float*)out[0].data())[1]);
  return 0;
}
