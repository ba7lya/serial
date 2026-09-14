///
/// @file real_port_test.cxx
/// @author BA7LYA (1042140025@qq.com)
/// @brief Smoke tests that drive a real serial port end to end, skipping when none is present.
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
/// @brief Opens the first usable port found by list_ports() or skips the whole fixture.
///
class real_port_test : public ::testing::Test {
protected:
    /// @brief Probes each listed port until one opens with a short timeout.
    void SetUp() override {
        for (const auto& info : list_ports()) {
            port_ = std::make_unique<serial>();
            port_->set_port(info.name);
            port_->set_timeout(timeout::simple_timeout(100)); // keep reads fast
            try {
                port_->open();
            } catch (...) {
                port_.reset();
                continue; // busy or unsupported; try the next candidate
            }
            if (port_->is_open()) { return; }
            port_.reset();
        }
        GTEST_SKIP() << "No openable serial port on this machine.";
    }

    /// @brief Closes and destroys the port object.
    void TearDown() override {
        if (port_) { port_->close(); }
    }

    std::unique_ptr<serial> port_;
};

} // namespace

///
/// @brief A freshly opened port reports itself open and exposes its name.
///
TEST_F(real_port_test, is_open_and_name) {
    ASSERT_TRUE(port_->is_open());
    EXPECT_FALSE(port_->get_port().empty());
}

///
/// @brief available() is queryable on an open idle port.
///
TEST_F(real_port_test, available_queryable) { EXPECT_NO_THROW(port_->available()); }

///
/// @brief Reading an idle port times out and yields nothing rather than throwing.
///
TEST_F(real_port_test, read_times_out_empty) {
    std::string data;
    const size_t bytes_read = port_->read(data, 8); // 100 ms timeout set in SetUp
    EXPECT_EQ(bytes_read, 0U);
    EXPECT_TRUE(data.empty());
    EXPECT_EQ(port_->read(1), "");
}

///
/// @brief Writing a small buffer completes without error.
///
TEST_F(real_port_test, write_succeeds) {
    const size_t bytes_written = port_->write("ba7lya\r\n");
    EXPECT_GT(bytes_written, 0U);
}

///
/// @brief readline on an idle port returns empty after its timeout.
///
TEST_F(real_port_test, readline_times_out_empty) { EXPECT_EQ(port_->readline(32, "\n"), ""); }

///
/// @brief Changing framing on an open port is applied, not rejected.
///
TEST_F(real_port_test, reconfigure_while_open) {
    port_->set_baudrate(9600);
    port_->set_data_bits(data_bits::eight);
    port_->set_parity(parity::none);
    port_->set_stop_bits(stop_bits::one);
    port_->set_flow_ctrl(flow_ctrl::none);
    EXPECT_TRUE(port_->is_open());
}

///
/// @brief Buffer flushes and modem-line setters run without error on an open port.
///
TEST_F(real_port_test, flush_and_lines) {
    port_->flush_tx_buffer();
    port_->flush_rx_buffer();
    port_->set_rts(false);
    port_->set_dtr(false);
    EXPECT_NO_THROW(port_->flush_tx_buffer());
}

///
/// @brief close then re-open is a supported sequence.
///
TEST_F(real_port_test, reopen) {
    const std::string name = port_->get_port();
    port_->close();
    EXPECT_FALSE(port_->is_open());
    port_->open();
    EXPECT_TRUE(port_->is_open());
    EXPECT_EQ(port_->get_port(), name);
}
