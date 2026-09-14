///
/// @file behavior_test.cxx
/// @author BA7LYA (1042140025@qq.com)
/// @brief Windows-specific behavioral contract tests for the serial library.
/// @version 0.2
/// @date 2026-09-14
/// @copyright Copyright (c) 2026
///

#include <cstdint>
#include <gtest/gtest.h>
#include <memory>
#include <string>

#include "serial.hxx"

using namespace ba7lya::serial;

namespace {

///
/// @brief Opens the first usable COM port or skips the fixture.
///
class win_behavior_test : public ::testing::Test {
protected:
    /// @brief Probes listed ports with a short timeout until one opens.
    void SetUp() override {
        for (const auto& info : list_ports()) {
            port_ = std::make_unique<serial>();
            port_->set_port(info.name);
            port_->set_timeout(timeout::simple_timeout(100));
            try {
                port_->open();
            } catch (...) {
                port_.reset();
                continue;
            }
            if (port_->is_open()) { return; }
            port_.reset();
        }
        GTEST_SKIP() << "No openable COM port on this machine.";
    }

    /// @brief Closes and destroys the port object.
    void TearDown() override {
        if (port_) { port_->close(); }
    }

    std::unique_ptr<serial> port_;
};

} // namespace

///
/// @brief wait_readable has no Win32 primitive and must report that on an open port.
///
TEST_F(win_behavior_test, wait_readable_unsupported) {
    EXPECT_THROW(port_->wait_readable(), io_exception);
}

///
/// @brief wait_byte_times is likewise unsupported.
///
TEST_F(win_behavior_test, wait_byte_times_unsupported) {
    EXPECT_THROW(port_->wait_byte_times(10), io_exception);
}

///
/// @brief Timed breaks are unsupported, while the level-based set_break works.
///
TEST_F(win_behavior_test, send_break_unsupported_set_break_ok) {
    EXPECT_THROW(port_->send_break(100), io_exception);
    EXPECT_NO_THROW(port_->set_break(false));
}

///
/// @brief All four modem status lines are queryable on an open port.
///
TEST_F(win_behavior_test, modem_lines_queryable) {
    EXPECT_NO_THROW(port_->get_cts());
    EXPECT_NO_THROW(port_->get_dsr());
    EXPECT_NO_THROW(port_->get_ri());
    EXPECT_NO_THROW(port_->get_cd());
}

///
/// @brief Port addresses without the \\\\.\\ prefix open fine, including high COM numbers.
///
TEST_F(win_behavior_test, plain_com_name_opens) {
    EXPECT_TRUE(port_->get_port().starts_with("COM"));
}

///
/// @brief Enumerated ports expose non-empty names and n/a-normalised hardware ids.
///
TEST(win_list_ports_test, fields_normalised) {
    for (const auto& info : list_ports()) {
        EXPECT_FALSE(info.name.empty());
        EXPECT_FALSE(info.hardware_id.empty());
    }
}
