#pragma once
// Collective Fabric - exact resource and message accounting. Counters never go
// negative; duplicate release/completion is detected and rejected; teardown
// must close active and reserved accounting to zero.
#include "collectivefabric/foundation/identifier.hpp"
#include "collectivefabric/foundation/errors.hpp"
#include <cstdint>
#include <map>
#include <mutex>

namespace collectivefabric {

class Accounting {
public:
  // --- active collectives -------------------------------------------------
  void collective_started() { std::lock_guard<std::mutex> l(mu_); ++active_collectives_; ++total_submitted_; }
  void collective_completed() {
    std::lock_guard<std::mutex> l(mu_);
    if (!active_collectives_) throw Error(ErrorCode::VALIDATION, "accounting: collective_completed with no active collective");
    --active_collectives_;
  }
  void collective_aborted() {
    std::lock_guard<std::mutex> l(mu_);
    if (!active_collectives_) throw Error(ErrorCode::VALIDATION, "accounting: collective_aborted with no active collective");
    --active_collectives_;
    ++aborted_collectives_;
  }
  void active_rank_contribution() { std::lock_guard<std::mutex> l(mu_); ++active_rank_contributions_; }
  void rank_contribution_complete() {
    std::lock_guard<std::mutex> l(mu_);
    if (!active_rank_contributions_) throw Error(ErrorCode::VALIDATION, "accounting: rank_contribution_complete underflow");
    --active_rank_contributions_;
  }

  // --- bytes ---------------------------------------------------------------
  // Reserve bytes under a reservation id. Duplicate reservation id rejected.
  void reserve(ReservationId res, std::uint64_t bytes) {
    std::lock_guard<std::mutex> l(mu_);
    if (reservations_.count(res)) throw Error(ErrorCode::ALREADY_EXISTS, "accounting: duplicate reservation id");
    reservations_.emplace(res, bytes);
    reserved_bytes_ += bytes;
  }
  // Release a reservation. Unknown/duplicate release rejected; never negative.
  void release(ReservationId res) {
    std::lock_guard<std::mutex> l(mu_);
    auto it = reservations_.find(res);
    if (it == reservations_.end()) throw Error(ErrorCode::NOT_FOUND, "accounting: release of unknown reservation");
    if (it->second > reserved_bytes_) throw Error(ErrorCode::VALIDATION, "accounting: reserved bytes underflow");
    reserved_bytes_ -= it->second;
    reservations_.erase(it);
  }
  void transmitted_bytes(std::uint64_t n) { std::lock_guard<std::mutex> l(mu_); transmitted_ref_bytes_ += n; }
  void completed_bytes(std::uint64_t n) { std::lock_guard<std::mutex> l(mu_); completed_bytes_ += n; }
  void failed_bytes(std::uint64_t n) { std::lock_guard<std::mutex> l(mu_); failed_bytes_ += n; }
  void retry_recorded() { std::lock_guard<std::mutex> l(mu_); ++retries_; }
  void stale_message_rejected() { std::lock_guard<std::mutex> l(mu_); ++stale_rejections_; }
  void duplicate_message_rejected() { std::lock_guard<std::mutex> l(mu_); ++duplicate_rejections_; }
  void participant_restart() { std::lock_guard<std::mutex> l(mu_); ++participant_restarts_; }
  void reconfiguration_recorded() { std::lock_guard<std::mutex> l(mu_); ++reconfigurations_; }

  // --- read-only snapshot --------------------------------------------------
  struct Snapshot {
    std::uint64_t active_collectives = 0;
    std::uint64_t active_rank_contributions = 0;
    std::uint64_t reserved_bytes = 0;
    std::uint64_t total_submitted = 0;
    std::uint64_t completed_bytes = 0;
    std::uint64_t failed_bytes = 0;
    std::uint64_t transmitted_ref_bytes = 0;
    std::uint64_t retries = 0;
    std::uint64_t aborted_collectives = 0;
    std::uint64_t stale_rejections = 0;
    std::uint64_t duplicate_rejections = 0;
    std::uint64_t participant_restarts = 0;
    std::uint64_t reconfigurations = 0;
  };
  Snapshot snapshot() const {
    std::lock_guard<std::mutex> l(mu_);
    return Snapshot{active_collectives_, active_rank_contributions_, reserved_bytes_, total_submitted_,
                    completed_bytes_, failed_bytes_, transmitted_ref_bytes_, retries_, aborted_collectives_,
                    stale_rejections_, duplicate_rejections_, participant_restarts_, reconfigurations_};
  }

private:
  mutable std::mutex mu_;
  std::uint64_t active_collectives_ = 0;
  std::uint64_t active_rank_contributions_ = 0;
  std::uint64_t reserved_bytes_ = 0;
  std::uint64_t total_submitted_ = 0;
  std::uint64_t completed_bytes_ = 0;
  std::uint64_t failed_bytes_ = 0;
  std::uint64_t transmitted_ref_bytes_ = 0;
  std::uint64_t retries_ = 0;
  std::uint64_t aborted_collectives_ = 0;
  std::uint64_t stale_rejections_ = 0;
  std::uint64_t duplicate_rejections_ = 0;
  std::uint64_t participant_restarts_ = 0;
  std::uint64_t reconfigurations_ = 0;
  std::map<ReservationId, std::uint64_t> reservations_;
};

} // namespace collectivefabric
