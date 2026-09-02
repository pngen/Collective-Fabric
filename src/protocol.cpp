#include "collectivefabric/protocol.hpp"
#include "collectivefabric/digest/crc32.hpp"

namespace collectivefabric {

namespace {
void put_u16(std::vector<std::uint8_t>& v, std::uint16_t x) { v.push_back((x >> 8) & 0xff); v.push_back(x & 0xff); }
void put_u32(std::vector<std::uint8_t>& v, std::uint32_t x) {
  v.push_back((x >> 24) & 0xff); v.push_back((x >> 16) & 0xff); v.push_back((x >> 8) & 0xff); v.push_back(x & 0xff);
}
void put_u64(std::vector<std::uint8_t>& v, std::uint64_t x) {
  for (int i = 7; i >= 0; --i) v.push_back((x >> (i * 8)) & 0xff);
}
std::uint16_t get_u16(const std::uint8_t* p) { return std::uint16_t((p[0] << 8) | p[1]); }
std::uint32_t get_u32(const std::uint8_t* p) {
  return (std::uint32_t(p[0]) << 24) | (std::uint32_t(p[1]) << 16) | (std::uint32_t(p[2]) << 8) | p[3];
}
std::uint64_t get_u64(const std::uint8_t* p) {
  std::uint64_t v = 0;
  for (int i = 0; i < 8; ++i) v = (v << 8) | p[i];
  return v;
}
} // namespace

std::vector<std::uint8_t> Protocol::encode(const Frame& f) {
  std::vector<std::uint8_t> out;
  out.reserve(COLLECTIVEFABRIC_PROTOCOL_HEADER + f.payload.size());
  put_u32(out, COLLECTIVEFABRIC_PROTOCOL_MAGIC);
  put_u16(out, f.version);
  put_u16(out, static_cast<std::uint16_t>(f.kind));
  put_u64(out, f.payload.size());
  // CRC-32 over header (magic/version/kind/length) + payload. The stored crc
  // field itself is excluded from the checksum.
  Crc32 crc;
  crc.update(std::span<const std::uint8_t>(out.data(), out.size()));
  crc.update(std::span<const std::uint8_t>(f.payload.data(), f.payload.size()));
  put_u32(out, crc.value());
  out.insert(out.end(), f.payload.begin(), f.payload.end());
  return out;
}

DecodeResult Protocol::decode(std::span<const std::uint8_t> bytes, std::size_t& consumed, Frame& out, std::string& reason) {
  consumed = 0;
  if (bytes.size() < 16) return DecodeResult::NEED_MORE;  // need magic/version/kind/length
  const std::uint32_t magic = get_u32(bytes.data());
  if (magic != COLLECTIVEFABRIC_PROTOCOL_MAGIC) { reason = "bad magic"; return DecodeResult::REJECT; }
  const std::uint16_t version = get_u16(bytes.data() + 4);
  if (version != COLLECTIVEFABRIC_PROTOCOL_VERSION) { reason = "unsupported protocol version"; return DecodeResult::REJECT; }
  const std::uint16_t kind_raw = get_u16(bytes.data() + 6);
  if (kind_raw == 0 || kind_raw > static_cast<std::uint16_t>(ProtocolMessageKind::ERROR_MSG)) {
    reason = "invalid message kind enum"; return DecodeResult::REJECT;
  }
  const std::uint64_t length = get_u64(bytes.data() + 8);
  if (length > COLLECTIVEFABRIC_PROTOCOL_MAX_PAYLOAD) { reason = "oversized payload"; return DecodeResult::REJECT; }
  if (bytes.size() < 20) return DecodeResult::NEED_MORE;  // need checksum field
  const std::uint64_t total = 20 + length;
  if (bytes.size() < total) return DecodeResult::NEED_MORE;  // truncated payload
  const std::uint32_t stored_crc = get_u32(bytes.data() + 16);
  Crc32 crc;
  crc.update(bytes.subspan(0, 16));
  crc.update(bytes.subspan(20, static_cast<std::size_t>(length)));
  if (crc.value() != stored_crc) { reason = "checksum mismatch"; return DecodeResult::REJECT; }

  out.version = version;
  out.kind = static_cast<ProtocolMessageKind>(kind_raw);
  out.payload.assign(bytes.data() + 20, bytes.data() + 20 + length);
  consumed = static_cast<std::size_t>(total);
  return DecodeResult::FRAME;
}

void FrameDecoder::feed(std::span<const std::uint8_t> data) {
  buffer_.insert(buffer_.end(), data.begin(), data.end());
}

DecodeResult FrameDecoder::next(Frame& frame, std::string& reason) {
  std::size_t consumed = 0;
  DecodeResult r = Protocol::decode(std::span<const std::uint8_t>(buffer_.data(), buffer_.size()), consumed, frame, reason);
  if (r == DecodeResult::FRAME) {
    buffer_.erase(buffer_.begin(), buffer_.begin() + static_cast<std::ptrdiff_t>(consumed));
  }
  return r;
}

} // namespace collectivefabric
