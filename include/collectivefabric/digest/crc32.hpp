#pragma once
// Collective Fabric - CRC-32 (IEEE 802.3 reflected) for frame and persistence
// integrity. Not used as a persistent identity; semantic digests use SHA-256.
#include <cstdint>
#include <cstddef>
#include <span>
#include <array>

namespace collectivefabric {

class Crc32 {
public:
  Crc32() = default;

  void update(std::span<const std::uint8_t> data) {
    const std::array<std::uint32_t, 256>& tbl = table();
    for (auto b : data) crc_ = tbl[(crc_ ^ b) & 0xffu] ^ (crc_ >> 8);
  }

  std::uint32_t value() const noexcept { return crc_ ^ 0xffffffffu; }
  void reset() noexcept { crc_ = 0xffffffffu; }

  static std::uint32_t compute(std::span<const std::uint8_t> data) {
    Crc32 c;
    c.update(data);
    return c.value();
  }

private:
  // Reference to a lazily-initialized, thread-safe CRC-32 lookup table.
  static const std::array<std::uint32_t, 256>& table() {
    static const std::array<std::uint32_t, 256> t = make_table();
    return t;
  }

  static std::array<std::uint32_t, 256> make_table() {
    std::array<std::uint32_t, 256> t{};
    for (std::uint32_t i = 0; i < 256; ++i) {
      std::uint32_t crc = i;
      for (int j = 0; j < 8; ++j) crc = (crc & 1) ? (0xEDB88320u ^ (crc >> 1)) : (crc >> 1);
      t[i] = crc;
    }
    return t;
  }

  std::uint32_t crc_ = 0xffffffffu;
};

} // namespace collectivefabric
