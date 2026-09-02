#include "collectivefabric/reference/engine.hpp"
#include <cstdio>
#include <vector>
using namespace collectivefabric;
int main() {
  float root[3] = {7.0f, 8.0f, 9.0f};
  std::vector<std::uint8_t> rb(reinterpret_cast<std::uint8_t*>(root), reinterpret_cast<std::uint8_t*>(root+3));
  std::vector<ReferenceEngine::Span> inputs{{rb.data(), rb.size()}, {rb.data(), rb.size()}};
  std::vector<std::vector<std::uint8_t>> out;
  ReferenceEngine::execute(CollectiveKind::BROADCAST, ReductionOp::NONE, Datatype::FLOAT32, 3, 0, 2, inputs, out);
  printf("broadcast rank0[0]=%f rank1[2]=%f\n", ((float*)out[0].data())[0], ((float*)out[1].data())[2]);
  return 0;
}
