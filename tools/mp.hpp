#pragma once
// Collective Fabric - real multiprocess coordinator/worker scenario driver.
// Consumes the core Runtime, protocol framing, reference collective engine, and
// persistence to prove distributed authority across real OS processes.
#include "collectivefabric/runtime.hpp"
#include "collectivefabric/backend/reference_backend.hpp"
#include "collectivefabric/planner/planner.hpp"
#include "collectivefabric/reference/engine.hpp"
#include "collectivefabric/store.hpp"
#include "collectivefabric/protocol.hpp"
#include "collectivefabric/digest/canonical.hpp"
#include "collectivefabric/foundation/enums.hpp"
#include "collectivefabric/foundation/clock.hpp"
#include "collectivefabric/authority.hpp"
#include "tcp.hpp"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

namespace collectivefabric_mp {

using namespace collectivefabric;

struct Args {
  unsigned short port = 0;
  std::string statefile;
  std::string phases;
  std::string logfile;
  std::string name;
};

struct AuthClaim {
  std::uint64_t epoch = 0;
  std::uint64_t boot = 0;
  std::uint64_t group_gen = 0;
  std::uint64_t collective_gen = 0;
  std::uint64_t attempt_gen = 0;
  std::uint64_t dispatch_gen = 0;
};

// A blocking frame channel over a socket. Serializes writes; reads frames
// frame-by-frame and reports disconnects / corruption.
class FrameChannel {
public:
  explicit FrameChannel(SOCKET s) : conn_(s) {}

  bool send(const Frame& f) {
    auto bytes = Protocol::encode(f);
    return conn_.send_all(bytes.data(), bytes.size());
  }
  bool recv(Frame& out, std::string& reason) {
    std::uint8_t buf[8192];
    while (true) {
      Frame f;
      std::string why;
      auto r = decoder_.next(f, why);
      if (r == DecodeResult::FRAME) { out = std::move(f); return true; }
      if (r == DecodeResult::REJECT) { reason = why; return false; }
      int n = conn_.recv_some(buf, sizeof(buf));
      if (n <= 0) { reason = "connection closed"; return false; }
      decoder_.feed(std::span<const std::uint8_t>(buf, static_cast<std::size_t>(n)));
    }
  }
  bool valid() const noexcept { return conn_.valid(); }
  void close() { conn_.close(); }
  SOCKET get() const noexcept { return conn_.get(); }

private:
  collectivefabric_tools::TcpConnection conn_;
  FrameDecoder decoder_;
};

inline std::vector<std::uint8_t> enc_boot_name(std::uint64_t boot, const std::string& name) {
  CanonicalWriter w; w.u64(boot); w.string(name);
  return std::vector<std::uint8_t>(w.data().begin(), w.data().end());
}
inline std::vector<std::uint8_t> enc_auth(const AuthClaim& a) {
  CanonicalWriter w; w.u64(a.epoch); w.u64(a.boot); w.u64(a.group_gen); w.u64(a.collective_gen);
  w.u64(a.attempt_gen); w.u64(a.dispatch_gen);
  return std::vector<std::uint8_t>(w.data().begin(), w.data().end());
}
inline bool dec_auth(std::span<const std::uint8_t> p, AuthClaim& a) {
  CanonicalReader r(p);
  return r.u64(a.epoch) && r.u64(a.boot) && r.u64(a.group_gen) && r.u64(a.collective_gen) &&
         r.u64(a.attempt_gen) && r.u64(a.dispatch_gen);
}
inline std::vector<std::uint8_t> enc_simple(std::uint64_t a, std::uint64_t b) {
  CanonicalWriter w; w.u64(a); w.u64(b);
  return std::vector<std::uint8_t>(w.data().begin(), w.data().end());
}
inline std::vector<std::uint8_t> enc_rank_accept(std::uint64_t rank, std::uint64_t rank_count, std::uint64_t group_id, std::uint64_t group_gen) {
  CanonicalWriter w; w.u64(rank); w.u64(rank_count); w.u64(group_id); w.u64(group_gen);
  return std::vector<std::uint8_t>(w.data().begin(), w.data().end());
}
inline std::vector<std::uint8_t> enc_blob(const AuthClaim& a, std::span<const std::uint8_t> bytes) {
  CanonicalWriter w; w.u64(a.epoch); w.u64(a.boot); w.u64(a.group_gen); w.u64(a.collective_gen);
  w.u64(a.attempt_gen); w.u64(a.dispatch_gen); w.bytes(bytes);
  return std::vector<std::uint8_t>(w.data().begin(), w.data().end());
}

struct Desc {
  std::uint8_t kind = 0;
  std::uint8_t datatype = 0;
  std::uint8_t reduction = 0;
  std::uint64_t element_count = 0;
  std::uint64_t root = 0;
  std::uint64_t rank_count = 0;
  std::uint64_t group_id = 0;
  std::uint64_t group_gen = 0;
  std::uint64_t collective_gen = 0;
  std::uint64_t epoch = 0;
  std::uint64_t attempt_gen = 0;
  std::uint64_t dispatch_gen = 0;
};
inline std::vector<std::uint8_t> enc_desc(const Desc& d) {
  CanonicalWriter w; w.u8(d.kind); w.u8(d.datatype); w.u8(d.reduction); w.u64(d.element_count);
  w.u64(d.root); w.u64(d.rank_count); w.u64(d.group_id); w.u64(d.group_gen); w.u64(d.collective_gen);
  w.u64(d.epoch); w.u64(d.attempt_gen); w.u64(d.dispatch_gen);
  return std::vector<std::uint8_t>(w.data().begin(), w.data().end());
}
inline bool dec_desc(std::span<const std::uint8_t> p, Desc& d) {
  CanonicalReader r(p);
  return r.u8(d.kind) && r.u8(d.datatype) && r.u8(d.reduction) && r.u64(d.element_count) &&
         r.u64(d.root) && r.u64(d.rank_count) && r.u64(d.group_id) && r.u64(d.group_gen) &&
         r.u64(d.collective_gen) && r.u64(d.epoch) && r.u64(d.attempt_gen) && r.u64(d.dispatch_gen);
}

} // namespace collectivefabric_mp
