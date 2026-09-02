// Collective Fabric - real multiprocess worker participant.
#include "mp.hpp"
#include <chrono>
#include <thread>

namespace collectivefabric_mp {

int run_worker(const Args& a, std::uint64_t boot, std::uint64_t stall_after) {
  collectivefabric_tools::winsock_init();
  std::string err;
  auto conn = collectivefabric_tools::TcpConnection::connect("127.0.0.1", a.port, err);
  if (!conn.valid()) { printf("WORKER connect failed %s\n", err.c_str()); return 1; }
  FrameChannel ch(conn.get());
  Frame f;
  f.kind = ProtocolMessageKind::HELLO;
  f.payload = enc_boot_name(boot, a.name);
  ch.send(f);
  f.kind = ProtocolMessageKind::JOIN_GROUP;
  f.payload = enc_auth(AuthClaim{1, boot, 1, 0, 0, 0});
  ch.send(f);
  std::string reason;
  Frame acc;
  if (!ch.recv(acc, reason)) { printf("WORKER group_accept failed %s\n", reason.c_str()); return 1; }
  CanonicalReader r(acc.payload);
  std::uint64_t rank, rank_count, group_id, group_gen;
  if (!(r.u64(rank) && r.u64(rank_count) && r.u64(group_id) && r.u64(group_gen))) { printf("WORKER accept decode failed\n"); return 1; }
  printf("WORKER boot=%llu rank=%llu rank_count=%llu gen=%llu\n", (unsigned long long)boot, (unsigned long long)rank,
         (unsigned long long)rank_count, (unsigned long long)group_gen);
  fflush(stdout);

  // barrier participation
  f.kind = ProtocolMessageKind::BARRIER;
  f.payload = enc_auth(AuthClaim{1, boot, group_gen, 0, 0, 0});
  ch.send(f);
  Frame barack;
  if (ch.recv(barack, reason) && barack.kind == ProtocolMessageKind::COMPLETE) {
    printf("WORKER barrier done\n"); fflush(stdout);
  }

  int collective_index = 0;
  while (true) {
    Frame in;
    if (!ch.recv(in, reason)) {
      if (reason == "connection closed" || reason.find("closed") != std::string::npos) {
        printf("WORKER coordinator closed\n"); fflush(stdout); return 0;
      }
      printf("WORKER recv failed %s\n", reason.c_str()); return 1;
    }
    if (in.kind == ProtocolMessageKind::REQUEST_COLLECTIVE) {
      Desc d;
      bool dec = dec_desc(in.payload, d);
      if (!dec) { printf("WORKER desc decode failed\n"); return 1; }
      if ((std::uint64_t)collective_index >= stall_after) {
        printf("WORKER STALL boot=%llu index=%d\n", (unsigned long long)boot, collective_index);
        fflush(stdout);
        std::this_thread::sleep_for(std::chrono::hours(24));
        return 0;
      }
      static const float v0[4] = {1,2,3,4};
      static const float v1[4] = {10,20,30,40};
      const float* base = (rank == 0) ? v0 : v1;
      std::vector<std::uint8_t> bytes(reinterpret_cast<const std::uint8_t*>(base), reinterpret_cast<const std::uint8_t*>(base + d.element_count));
      AuthClaim auth; auth.epoch = 1; auth.boot = boot; auth.group_gen = d.group_gen; auth.collective_gen = d.collective_gen;
      auth.attempt_gen = d.attempt_gen; auth.dispatch_gen = d.dispatch_gen;
      Frame con; con.kind = ProtocolMessageKind::CONTRIBUTION;
      con.payload = enc_blob(auth, bytes);
      ch.send(con);
      printf("WORKER contributed rank=%llu bytes=%zu\n", (unsigned long long)rank, bytes.size());
      fflush(stdout);
      ++collective_index;
      Frame res;
      if (!ch.recv(res, reason)) { printf("WORKER result recv failed %s\n", reason.c_str()); return 1; }
      if (res.kind != ProtocolMessageKind::RESULT) { printf("WORKER expected RESULT\n"); return 1; }
      CanonicalReader rr(res.payload);
      AuthClaim ra; std::span<const std::uint8_t> blob;
      if (!(rr.u64(ra.epoch) && rr.u64(ra.boot) && rr.u64(ra.group_gen) && rr.u64(ra.collective_gen) &&
            rr.u64(ra.attempt_gen) && rr.u64(ra.dispatch_gen) && rr.bytes(blob))) { printf("WORKER result decode failed\n"); return 1; }
      const float* rv = reinterpret_cast<const float*>(blob.data());
      printf("WORKER result[0]=%f\n", rv[0]); fflush(stdout);
      Frame comp; comp.kind = ProtocolMessageKind::COMPLETE; comp.payload = enc_simple(1, 1);
      ch.send(comp);
    } else if (in.kind == ProtocolMessageKind::COMPLETE) {
      printf("WORKER complete ack\n"); fflush(stdout);
    } else if (in.kind == ProtocolMessageKind::ERROR_MSG) {
      printf("WORKER error\n"); return 1;
    }
  }
}

} // namespace collectivefabric_mp

int main(int argc, char** argv) {
  collectivefabric_mp::Args a;
  std::uint64_t boot = 0, stall_after = 999;
  for (int i = 1; i < argc; ++i) {
    std::string arg = argv[i];
    auto next = [&](const char* flag) -> std::string {
      if (arg == flag && i + 1 < argc) return argv[++i];
      return "";
    };
    if (arg == "--port") a.port = (unsigned short)atoi(next("--port").c_str());
    else if (arg == "--name") a.name = next("--name");
    else if (arg == "--boot") boot = strtoull(next("--boot").c_str(), nullptr, 10);
    else if (arg == "--stall-after") stall_after = strtoull(next("--stall-after").c_str(), nullptr, 10);
  }
  if (boot == 0) {
    std::uint64_t h = 1469598103934665603ull;
    for (char ch : a.name) { h ^= static_cast<std::uint8_t>(ch); h *= 1099511628211ull; }
    using namespace std::chrono;
    boot = static_cast<std::uint64_t>(high_resolution_clock::now().time_since_epoch().count()) ^ h;
  }
  a.phases = a.name;
  return collectivefabric_mp::run_worker(a, boot, stall_after);
}
