#pragma once
// Collective Fabric - minimal deterministic test harness. Each test is a
// focused executable that returns nonzero on any failed check.
#include <cstdio>
#include <string>
#include <vector>

namespace cftest {
inline int g_failures = 0;
inline int g_checks = 0;
inline void check(bool cond, const char* what) {
  ++g_checks;
  if (!cond) { printf("  [FAIL] %s\n", what); ++g_failures; }
  fflush(stdout);
}
inline void check_true(bool cond, const std::string& what) {
  ++g_checks;
  if (!cond) { printf("  [FAIL] %s\n", what.c_str()); ++g_failures; }
  fflush(stdout);
}
template <class Fn>
inline void check_throws(Fn&& fn, const char* what) {
  ++g_checks;
  bool threw = false;
  try { fn(); } catch (...) { threw = true; }
  if (!threw) { printf("  [FAIL] expected exception: %s\n", what); ++g_failures; }
}
template <class Fn>
inline void check_no_throw(Fn&& fn, const char* what) {
  ++g_checks;
  try { fn(); } catch (...) { printf("  [FAIL] unexpected exception: %s\n", what); ++g_failures; }
}
inline int finish(const char* name) {
  if (g_failures == 0) { printf("%s: ALL PASS (%d checks)\n", name, g_checks); fflush(stdout); return 0; }
  printf("%s: FAILURES=%d/%d\n", name, g_failures, g_checks); fflush(stdout);
  return 1;
}
} // namespace cftest

#define CF_CHECK(x) ::cftest::check((x), #x)
#define CF_CHECK_MSG(x, m) ::cftest::check_true((x), m)
#define CF_CHECK_THROWS(x) ::cftest::check_throws([&]() { x; }, #x)
#define CF_CHECK_NO_THROW(x) ::cftest::check_no_throw([&]() { x; }, #x)
#define CF_FINISH(name) return ::cftest::finish(name)
