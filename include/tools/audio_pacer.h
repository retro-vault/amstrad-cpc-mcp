//
// Optional wall-clock pacing for deterministic PCM capture.
//
// Audio samples remain derived solely from emulated T-states. This helper
// delays their producer in small batches when a caller explicitly requests a
// percentage of real CPC speed, without altering the sample stream.
//
// GPL 3.0 License (see: LICENSE)
// Copyright (C) 2026 tomaz stih
//

#ifndef TOOLS_AUDIO_PACER_H
#define TOOLS_AUDIO_PACER_H

#include <chrono>
#include <cstdint>

namespace tools {

class audio_pacer final {
public:
    // Start a fresh timing epoch. A zero speed disables wall-clock pacing.
    void configure(int sample_rate, int speed_percent);

    // Account for one produced sample and wait when the current batch is
    // ahead of its wall-clock deadline.
    void wait();

private:
    int sample_rate_ = 1;
    int speed_percent_ = 0;
    std::uint64_t samples_ = 0;
    std::uint64_t remainder_ = 0;
    std::uint64_t block_samples_ = 1;
    std::chrono::steady_clock::time_point deadline_;
    std::chrono::steady_clock::time_point last_sample_;
};

} // namespace tools

#endif // TOOLS_AUDIO_PACER_H
