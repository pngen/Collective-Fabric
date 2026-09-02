#pragma once
// Collective Fabric - error model. Failures are explicit and never silently
// swallowed. Public functions that can fail report an Error carrying a code
// and a human-readable reason.
#include <stdexcept>
#include <string>

namespace collectivefabric {

enum class ErrorCode {
  VALIDATION,
  DECODE,
  ENCODE,
  ARITHMETIC_OVERFLOW,
  STALE_AUTHORITY,
  CONFLICT,
  NOT_FOUND,
  ALREADY_EXISTS,
  INVALID_ARGUMENT,
  INCOMPATIBLE,
  IO,
  UNSUPPORTED,
  LIFECYCLE,
  UNKNOWN,
};

class Error : public std::runtime_error {
public:
  Error(ErrorCode code, std::string message)
      : std::runtime_error(std::move(message)), code_(code) {}
  ErrorCode code() const noexcept { return code_; }
private:
  ErrorCode code_;
};

} // namespace collectivefabric
