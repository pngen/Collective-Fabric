#include "collectivefabric/protocol.hpp"
#include <cstdio>
#include <cstring>
using namespace collectivefabric;
int main() {
  int failures = 0;
  auto check = [&](bool c, const char* m){ if(!c){ printf("  FAIL %s\n", m); ++failures; } };
  Frame f; f.kind = ProtocolMessageKind::HELLO; f.payload = {1,2,3,4,5};
  auto bytes = Protocol::encode(f);
  Frame out; std::size_t consumed; std::string reason;
  auto r = Protocol::decode(std::span<const std::uint8_t>(bytes.data(), bytes.size()), consumed, out, reason);
  check(r == DecodeResult::FRAME, "roundtrip frame decode");
  check(out.kind == f.kind, "kind preserved");
  check(out.payload == f.payload, "payload preserved");
  check(consumed == bytes.size(), "consumed all bytes");
  // corruption: flip a payload byte
  bytes[bytes.size()-1] ^= 0xff;
  Frame out2; std::size_t c2; std::string r2;
  auto r3 = Protocol::decode(std::span<const std::uint8_t>(bytes.data(), bytes.size()), c2, out2, r2);
  check(r3 == DecodeResult::REJECT, "corrupt byte rejected as REJECT");
  // truncated
  Frame out3; std::size_t c3; std::string r3s;
  auto rt = Protocol::decode(std::span<const std::uint8_t>(bytes.data(), 10), c3, out3, r3s);
  check(rt == DecodeResult::NEED_MORE, "truncated -> NEED_MORE");
  // bad magic
  bytes[0] = 0x00;
  Frame out4; std::size_t c4; std::string r4;
  auto r5 = Protocol::decode(std::span<const std::uint8_t>(bytes.data(), bytes.size()), c4, out4, r4);
  check(r5 == DecodeResult::REJECT, "bad magic rejected");
  printf("protocol: %s\n", failures == 0 ? "ALL PASS" : "FAILURES");
  return failures == 0 ? 0 : 1;
}
