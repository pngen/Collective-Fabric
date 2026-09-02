#pragma once
// Collective Fabric - deterministic canonical binary encoding.
// Integers are big-endian; variable-length fields are length-prefixed with a
// u64 count. Decoding is strictly bounded and reports truncation/overflow
// explicitly; a decoded field never silently exceeds the remaining input.
#include <cstdint>
#include <cstddef>
#include <span>
#include <vector>
#include <string>
#include <string_view>
#include <optional>
#include <cstring>

namespace collectivefabric {

class CanonicalWriter {
public:
  void u8(std::uint8_t v) { buf_.push_back(v); }
  void u16(std::uint16_t v) { push_be(v, 2); }
  void u32(std::uint32_t v) { push_be(v, 4); }
  void u64(std::uint64_t v) { push_be(v, 8); }
  void i32(std::int32_t v) { u32(static_cast<std::uint32_t>(v)); }
  void i64(std::int64_t v) { u64(static_cast<std::uint64_t>(v)); }
  void f32(float v) {
    std::uint32_t bits;
    static_assert(sizeof(bits) == sizeof(v));
    std::memcpy(&bits, &v, sizeof(bits));
    u32(bits);
  }
  void f64(double v) {
    std::uint64_t bits;
    static_assert(sizeof(bits) == sizeof(v));
    std::memcpy(&bits, &v, sizeof(bits));
    u64(bits);
  }
  void boolean(bool v) { u8(v ? 1 : 0); }

  // Variable-length blob: u64 length prefix followed by the bytes.
  void bytes(std::span<const std::uint8_t> data) {
    u64(static_cast<std::uint64_t>(data.size()));
    buf_.insert(buf_.end(), data.begin(), data.end());
  }
  void string(std::string_view s) {
    bytes(std::span<const std::uint8_t>(reinterpret_cast<const std::uint8_t*>(s.data()), s.size()));
  }

  // A typed enum is encoded as its underlying integer value via the caller.
  template <class UInt>
  void scalar(UInt v) { u64(static_cast<std::uint64_t>(v)); }

  std::span<const std::uint8_t> data() const noexcept { return {buf_.data(), buf_.size()}; }
  std::size_t size() const noexcept { return buf_.size(); }

private:
  void push_be(std::uint64_t v, int n) {
    for (int i = n - 1; i >= 0; --i) buf_.push_back(static_cast<std::uint8_t>((v >> (i * 8)) & 0xff));
  }
  std::vector<std::uint8_t> buf_;
};

class CanonicalReader {
public:
  explicit CanonicalReader(std::span<const std::uint8_t> data) : data_(data) {}

  std::size_t remaining() const noexcept { return data_.size() - pos_; }
  bool at_end() const noexcept { return pos_ == data_.size(); }
  std::size_t position() const noexcept { return pos_; }

  bool u8(std::uint8_t& out) {
    if (remaining() < 1) return false;
    out = data_[pos_++];
    return true;
  }
  bool u16(std::uint16_t& out) { return read_be(out, 2); }
  bool u32(std::uint32_t& out) { return read_be(out, 4); }
  bool u64(std::uint64_t& out) { return read_be(out, 8); }
  bool i32(std::int32_t& out) { std::uint32_t u; if (!u32(u)) return false; out = static_cast<std::int32_t>(u); return true; }
  bool i64(std::int64_t& out) { std::uint64_t u; if (!u64(u)) return false; out = static_cast<std::int64_t>(u); return true; }
  bool f32(float& out) { std::uint32_t u; if (!u32(u)) return false; std::memcpy(&out, &u, sizeof(out)); return true; }
  bool f64(double& out) { std::uint64_t u; if (!u64(u)) return false; std::memcpy(&out, &u, sizeof(out)); return true; }
  bool boolean(bool& out) { std::uint8_t b; if (!u8(b)) return false; out = (b != 0); return true; }

  // Read a length-prefixed blob as a zero-copy view into the underlying data.
  // Fails if the length prefix is malformed/oversized or exceeds remaining.
  bool bytes(std::span<const std::uint8_t>& out) {
    std::uint64_t len;
    if (!u64(len)) return false;
    if (len > remaining()) return false;
    out = data_.subspan(pos_, static_cast<std::size_t>(len));
    pos_ += static_cast<std::size_t>(len);
    return true;
  }
  bool string(std::string& out) {
    std::span<const std::uint8_t> b;
    if (!bytes(b)) return false;
    out.assign(reinterpret_cast<const char*>(b.data()), b.size());
    return true;
  }

private:
  template <class UInt>
  bool read_be(UInt& out, int n) {
    if (remaining() < static_cast<std::size_t>(n)) return false;
    UInt v = 0;
    for (int i = 0; i < n; ++i) v = static_cast<UInt>((v << 8) | data_[pos_++]);
    out = v;
    return true;
  }
  std::span<const std::uint8_t> data_;
  std::size_t pos_ = 0;
};

} // namespace collectivefabric
