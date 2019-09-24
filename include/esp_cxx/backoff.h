#ifndef BACKOFF_H_
#define BACKOFF_H_

#include <random>

namespace esp_cxx {

// 14 steps of backoff on a 100ms delay is near 30-mins. Seems solid default
// for low power usage and low chance of prolongued disconnect.
template <int kBaseMs = 100, int kMaxBackoffMs = 30*60*1000>
class BackoffCalculator {
 public:
  // TODO(awong): static_assert that there's not overflow in bounds.
  int MsToNextTry() {
    int backoff_ms = kBaseMs * (1 << backoff_attempt_);

    if (backoff_ms >= kMaxBackoffMs) {
      backoff_ms = kMaxBackoffMs;
    } else {
      backoff_attempt_++;
    }

    return backoff_ms + jitter_dist_(e_);
  }

  void Reset() { backoff_attempt_ = 0; }

 private:
  int backoff_attempt_ = 0;
  std::random_device rd_;
  std::mt19937 e_{rd_()};
  // Per https://cloud.google.com/iot/docs/how-tos/exponential-backoff, use 1000ms of jitter.
  std::uniform_int_distribution<int> jitter_dist_{0, 1000};
};

}  // namespace esp_cxx

#endif  // BACKOFF_H_
