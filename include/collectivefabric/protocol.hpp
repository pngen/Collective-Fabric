#pragma once
// Collective Fabric - bounded, versioned, typed, corruption-resistant frame
// protocol for multiprocess coordination over a stream transport (TCP).
// Frame format (big-endian): magic(4) version(2) kind(2) length(8) crc32(4)
// payload(...). Payload lengths are bounded before allocation; bad magic,
// unsupported version, oversized payload, truncation, checksum mismatch, and
// invalid enum values are all rejected explicitly.
#include <cstdint>
#include <cstddef>
#include <span>
#include <vector>
#include <string>

namespace collectivefabric {

enum class ProtocolMessageKind : std::uint16_t {
  HELLO = 1,        // worker -> coordinator: register WorkerBootId
  JOIN_GROUP = 2,   // worker -> coordinator: join group generation
  GROUP_ACCEPT = 3, // coordinator -> worker: membership accepted (rank assigned)
  CONTRIBUTION = 4, // worker -> coordinator: contribution bytes for a collective
  RESULT = 5,       // coordinator -> worker: authoritative result bytes
  BARRIER = 6,      // barrier request/ack
  COMPLETE = 7,     // completion ack
  EPOCH = 8,        // coordinator -> worker: current epoch / generations
  REQUEST_COLLECTIVE = 9,  // coordinator -> worker: request to participate in a collective
  ERROR_MSG = 10,   // error notification
};

inline std::string_view protocol_kind_to_string(ProtocolMessageKind k) {
  switch (k) {
    case ProtocolMessageKind::HELLO: return "HELLO";
    case ProtocolMessageKind::JOIN_GROUP: return "JOIN_GROUP";
    case ProtocolMessageKind::GROUP_ACCEPT: return "GROUP_ACCEPT";
    case ProtocolMessageKind::CONTRIBUTION: return "CONTRIBUTION";
    case ProtocolMessageKind::RESULT: return "RESULT";
    case ProtocolMessageKind::BARRIER: return "BARRIER";
    case ProtocolMessageKind::COMPLETE: return "COMPLETE";
    case ProtocolMessageKind::EPOCH: return "EPOCH";
    case ProtocolMessageKind::REQUEST_COLLECTIVE: return "REQUEST_COLLECTIVE";
    case ProtocolMessageKind::ERROR_MSG: return "ERROR_MSG";
  }
  return "UNKNOWN";
}

struct Frame {
  std::uint16_t version = 1;
  ProtocolMessageKind kind = ProtocolMessageKind::ERROR_MSG;
  std::vector<std::uint8_t> payload;
};

enum class DecodeResult { NEED_MORE, FRAME, REJECT };

#define COLLECTIVEFABRIC_PROTOCOL_MAGIC 0x43464250u   // "CFBP"
#define COLLECTIVEFABRIC_PROTOCOL_VERSION 1
#define COLLECTIVEFABRIC_PROTOCOL_HEADER 20
#define COLLECTIVEFABRIC_PROTOCOL_MAX_PAYLOAD (64u * 1024u * 1024u)

class Protocol {
public:
  // Encode a frame to bytes.
  static std::vector<std::uint8_t> encode(const Frame& f);

  // Try to decode exactly one frame from the front of 'bytes'. On NEED_MORE,
  // no bytes were consumed. On REJECT, the reason is set and the caller should
  // close the connection (corruption is unrecoverable). On FRAME, out is filled
  // and consumed bytes are reported.
  static DecodeResult decode(std::span<const std::uint8_t> bytes, std::size_t& consumed, Frame& out, std::string& reason);
};

// Stateful streaming decoder that accumulates bytes and yields frames.
class FrameDecoder {
public:
  void feed(std::span<const std::uint8_t> data);
  // Returns true and fills 'frame' if a full valid frame is available.
  DecodeResult next(Frame& frame, std::string& reason);
  std::size_t buffered() const noexcept { return buffer_.size(); }

private:
  std::vector<std::uint8_t> buffer_;
};

} // namespace collectivefabric
