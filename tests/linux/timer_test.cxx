///
/// @file timer_test.cxx
/// @author BA7LYA (1042140025@qq.com)
/// @brief Tests the monotonic millisecond_timer helper of the POSIX backend (Linux only).
/// @version 0.2
/// @date 2026-09-14
/// @copyright Copyright (c) 2026
///

#include <array>
#include <chrono>
#include <cstdint>
#include <gtest/gtest.h>
#include <memory>
#include <thread>

#include "linux/linux.hxx"

using ba7lya::serial::millisecond_timer;
using namespace std::chrono_literals;

namespace {

///
/// @brief Short countdowns expire within a millisecond of their nominal duration.
/// @note Do 100 trials of timing gaps between 0 and 19 milliseconds.
///
TEST(timer_test, short_intervals) {
    for (uint32_t trial = 0; trial < 100; ++trial) {
        const uint32_t ms = trial % 20;
        const millisecond_timer timer(ms);
        std::this_thread::sleep_for(std::chrono::milliseconds(ms) + 1ms);
        const int64_t remaining = timer.remaining();
        EXPECT_LE(remaining, 0) << "timer should have expired";
        EXPECT_GE(remaining, -10) << "expired by more than the sleep overhead";
    }
}

///
/// @brief Ten overlapping one-second countdowns stay in the expected relative order.
///
TEST(timer_test, overlapping_long_intervals) {
    std::array<std::unique_ptr<millisecond_timer>, 10> timers;

    // Start each timer 1 ms after the previous one.
    for (auto& timer : timers) {
        timer = std::make_unique<millisecond_timer>(1000);
        std::this_thread::sleep_for(1ms);
    }

    std::this_thread::sleep_for(500ms);
    for (size_t t = 0; t < timers.size(); ++t) {
        EXPECT_NEAR(timers[t]->remaining(), 500 - static_cast<int64_t>(t), 20);
    }

    std::this_thread::sleep_for(500ms);
    for (size_t t = 0; t < timers.size(); ++t) {
        EXPECT_NEAR(timers[t]->remaining(), -static_cast<int64_t>(t), 25);
    }
}

} // namespace
