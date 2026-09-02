// Collective Fabric - real multiprocess closure driver. Spawns the coordinator
// and two real worker OS processes, kills worker A mid-collective, restarts it
// with a fresh boot, replays stale authority, and proves recovery.
#include <windows.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>
#include <fstream>
#include <thread>
#include <chrono>

struct Proc {
  PROCESS_INFORMATION pi{};
  bool ok = false;
  bool launch(const std::string& cmd) {
    STARTUPINFOA si{};
    si.cb = sizeof(si);
    char buf[4096];
    std::snprintf(buf, sizeof(buf), "%s", cmd.c_str());
    if (!CreateProcessA(nullptr, buf, nullptr, nullptr, FALSE, CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi)) {
      printf("spawn failed: %s\n", cmd.c_str());
      return false;
    }
    ok = true;
    return true;
  }
  void kill() { if (ok) { TerminateProcess(pi.hProcess, 0); WaitForSingleObject(pi.hProcess, 5000); } }
  ~Proc() { if (ok) { CloseHandle(pi.hThread); CloseHandle(pi.hProcess); } }
};

static int g_fail = 0;
void expect(bool cond, const char* what) {
  printf("  [%s] %s\n", cond ? "PASS" : "FAIL", what);
  if (!cond) ++g_fail;
}

static bool file_has(const std::string& path, const std::string& needle) {
  std::ifstream is(path);
  if (!is) return false;
  std::string line;
  while (std::getline(is, line)) if (line.find(needle) != std::string::npos) return true;
  return false;
}

static bool wait_for(const std::string& path, const std::string& needle, int seconds) {
  for (int i = 0; i < seconds * 20; ++i) {
    if (file_has(path, needle)) return true;
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
  }
  std::ifstream dbg(path);
  int n = 0;
  std::string dl;
  while (std::getline(dbg, dl)) ++n;
  printf("    [wait timeout] %s in %s (lines=%d)\n", needle.c_str(), path.c_str(), n);
  return false;
}

int main(int argc, char** argv) {
  std::string coord, worker, port = "6550", state, log;
  for (int i = 1; i < argc; ++i) {
    auto next = [&](const char* f) -> std::string { (void)f; if (i + 1 < argc) return std::string(argv[++i]); return ""; };
    std::string a = argv[i];
    if (a == "--coord") coord = next("--coord");
    else if (a == "--worker") worker = next("--worker");
    else if (a == "--port") port = next("--port");
    else if (a == "--state") state = next("--state");
    else if (a == "--log") log = next("--log");
  }
  DeleteFileA(log.c_str());
  DeleteFileA(state.c_str());

  printf("MULTIPROCESS closure driver log=[%s] state=[%s]\n", log.c_str(), state.c_str());
  // 1) coordinator (closure scenario)
  Proc coord1;
  coord1.launch("\"" + coord + "\" --port " + port + " --statefile \"" + state + "\" --logfile \"" + log + "\"");
  std::this_thread::sleep_for(std::chrono::milliseconds(400));

  Proc workerA, workerB;
  workerA.launch("\"" + worker + "\" --port " + port + " --name A --boot 100 --stall-after 1");
  workerB.launch("\"" + worker + "\" --port " + port + " --name B --boot 200 --stall-after 999");

  expect(wait_for(log, "PHASE_BARRIER_DONE", 8), "barrier completed over TCP");
  expect(wait_for(log, "PHASE_ALLREDUCE_DONE gen=1", 8), "all-reduce over TCP (gen 1)");
  expect(wait_for(log, "PHASE_SAVED", 8), "state persisted before failure");
  expect(wait_for(log, "PHASE_BEGIN_LIVE_COLLECTIVE", 8), "second collective with live authority begun");

  printf("  killing worker A (real OS process)\n");
  workerA.kill();
  expect(wait_for(log, "PHASE_LOSS_DETECTED", 10), "coordinator observed worker A loss");
  expect(wait_for(log, "PHASE_RECONFIGURE epoch=2", 10), "epoch advanced after loss");

  Proc workerA2;
  workerA2.launch("\"" + worker + "\" --port " + port + " --name A2 --boot 300 --stall-after 999");
  expect(wait_for(log, "PHASE_REGROUP gen=2", 10), "fresh group generation re-formed");
  expect(wait_for(log, "PHASE_STALE_REPLAY rejected=7 accepted=1", 8), "all stale-authority classes rejected");
  expect(wait_for(log, "PHASE_ALLREDUCE_DONE gen=2", 8), "fresh post-restart all-reduce (gen 2)");
  expect(wait_for(log, "PHASE_SAVED_FINAL", 8), "final authoritative state persisted");
  expect(wait_for(log, "PHASE_DONE", 8), "coordinator closure completed");

  workerA2.kill();
  workerB.kill();

  // 2) recovery: fresh coordinator process loads persisted state
  Proc coord2;
  coord2.launch("\"" + coord + "\" --port " + port + " --statefile \"" + state + "\" --logfile \"" + log + ".recover\"" + " --scenario recover");
  std::this_thread::sleep_for(std::chrono::milliseconds(400));
  Proc workerA3, workerB3;
  workerA3.launch("\"" + worker + "\" --port " + port + " --name A3 --boot 400 --stall-after 999");
  workerB3.launch("\"" + worker + "\" --port " + port + " --name B3 --boot 500 --stall-after 999");
  expect(wait_for(log + ".recover", "RECOVERY_MEASUREMENTS_REVALIDATION_REQUIRED", 10), "recovered measurements require revalidation");
  expect(wait_for(log + ".recover", "RECOVERY_AUTHORITY_CLEARED", 10), "recovered live authority cleared");
  expect(wait_for(log + ".recover", "RECOVERY_ALLREDUCE_DONE", 10), "post-recovery collective executed");
  expect(wait_for(log + ".recover", "RECOVERY_DONE", 10), "recovery complete");
  workerA3.kill();
  workerB3.kill();
  coord1.kill();
  coord2.kill();

  printf("MULTIPROCESS closure: %s\n", g_fail == 0 ? "ALL PASS" : "FAILURES");
  return g_fail == 0 ? 0 : 1;
}
