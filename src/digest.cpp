#include "collectivefabric/digest/sha256.hpp"

namespace collectivefabric {

namespace {
int hex_val(char c) {
  if (c >= '0' && c <= '9') return c - '0';
  if (c >= 'a' && c <= 'f') return c - 'a' + 10;
  if (c >= 'A' && c <= 'F') return c - 'A' + 10;
  return -1;
}
} // namespace

Sha256::Digest Sha256::from_hex(std::string_view hex) {
  Digest out{};
  std::size_t n = hex.size() < 64 ? hex.size() : 64;
  for (std::size_t i = 0, j = 0; i + 1 < n && j < 32; i += 2, ++j) {
    int hi = hex_val(hex[i]);
    int lo = hex_val(hex[i + 1]);
    if (hi < 0 || lo < 0) break;
    out[j] = static_cast<std::uint8_t>((hi << 4) | lo);
  }
  return out;
}

} // namespace collectivefabric
