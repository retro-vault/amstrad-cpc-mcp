//
// Optional wall-clock pacing for deterministic PCM capture.
//
// Short monotonic-clock batches keep syscall overhead low. A long gap starts
// a new epoch so resuming an MCP run does not race to catch up with time during
// which the emulated machine was stopped.
//
// GPL 3.0 License (see: LICENSE)
// Copyright (C) 2026 tomaz stih
//

#include "tools/audio_pacer.h"

#include <chrono>
#include <thread>

namespace tools {

namespace {

constexpr int blocks_per_second = 200;
constexpr auto idle_limit = std::chrono::milliseconds(250);
constexpr std::uint64_t nanoseconds_per_second = 1000000000ull;

} // namespace

void audio_pacer::configure(int sample_rate, int speed_percent)
{
    sample_rate_ = sample_rate;
    speed_percent_ = speed_percent;
    samples_ = 0;
    remainder_ = 0;
    block_samples_ = static_cast<std::uint64_t>(sample_rate_) /
                     blocks_per_second;
    if (block_samples_ == 0)
        block_samples_ = 1;

    const auto now = std::chrono::steady_clock::now();
    deadline_ = now;
    last_sample_ = now;
}

void audio_pacer::wait()
{
    if (speed_percent_ == 0)
        return;

    auto now = std::chrono::steady_clock::now();
    if (now - last_sample_ > idle_limit) {
        deadline_ = now;
        samples_ = 0;
        remainder_ = 0;
    }
    last_sample_ = now;

    ++samples_;
    if (samples_ < block_samples_)
        return;

    const std::uint64_t denominator =
        static_cast<std::uint64_t>(sample_rate_) * speed_percent_;
    const std::uint64_t numerator =
        samples_ * nanoseconds_per_second * 100u + remainder_;
    const std::uint64_t nanoseconds = numerator / denominator;
    remainder_ = numerator % denominator;
    samples_ = 0;
    deadline_ += std::chrono::nanoseconds(nanoseconds);

    now = std::chrono::steady_clock::now();
    if (now < deadline_) {
        std::this_thread::sleep_until(deadline_);
    } else if (now - deadline_ > idle_limit) {
        deadline_ = now;
        remainder_ = 0;
    }
    last_sample_ = std::chrono::steady_clock::now();
}

} // namespace tools
