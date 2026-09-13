///
/// @file serial_test.cxx
/// @author BA7LYA (1042140025@qq.com)
/// @brief Portable unit tests for the serial facade: parameter handling, errors and listing.
/// @version 0.2
/// @date 2026-09-14
/// @copyright Copyright (c) 2026
///

#include <cstdint>
#include <gtest/gtest.h>
#include <span>
#include <string>
#include <thread>
#include <vector>

#include "serial.hxx"

using namespace ba7lya::serial;

///
/// @brief Exercises construction without opening a port.
///
TEST(serial_test, constructs_closed) {
    serial port;
    EXPECT_FALSE(port.is_open());
    EXPECT_EQ(port.get_port(), "");
    EXPECT_EQ(port.get_baudrate(), 9600u);
    EXPECT_EQ(port.get_data_bits(), data_bits::eight);
    EXPECT_EQ(port.get_parity(), parity::none);
    EXPECT_EQ(port.get_stop_bits(), stop_bits::one);
    EXPECT_EQ(port.get_flow_ctrl(), flow_ctrl::none);
    EXPECT_EQ(port.available(), 0u);
}

///
/// @brief An empty port name makes open() fail with invalid_argument.
///
TEST(serial_test, open_without_port_throws) {
    serial port;
    EXPECT_THROW(port.open(), std::invalid_argument);
}

///
/// @brief Every operation that needs a handle rejects a closed port.
///
TEST(serial_test, operations_on_closed_port_throw) {
    serial port;
    std::vector<std::uint8_t> bytes(4);
    std::string text;

    EXPECT_THROW(port.read(bytes), port_not_open_exception);
    EXPECT_THROW(port.read(text), port_not_open_exception);
    EXPECT_THROW(port.read(std::span(bytes)), port_not_open_exception);
    EXPECT_THROW(port.readline(text), port_not_open_exception);
    EXPECT_THROW(port.readlines(), port_not_open_exception);
    EXPECT_THROW(port.write(std::span(bytes)), port_not_open_exception);
    EXPECT_THROW(port.write(text), port_not_open_exception);
    EXPECT_THROW(port.flush(), port_not_open_exception);
    EXPECT_THROW(port.flush_rx_buffer(), port_not_open_exception);
    EXPECT_THROW(port.flush_tx_buffer(), port_not_open_exception);
    EXPECT_THROW(port.send_break(100), port_not_open_exception);
    EXPECT_THROW(port.set_break(), port_not_open_exception);
    EXPECT_THROW(port.set_rts(), port_not_open_exception);
    EXPECT_THROW(port.set_dtr(), port_not_open_exception);
    EXPECT_THROW(port.wait_for_change(), port_not_open_exception);
    EXPECT_THROW(port.get_cts(), port_not_open_exception);
    EXPECT_THROW(port.get_dsr(), port_not_open_exception);
    EXPECT_THROW(port.get_ri(), port_not_open_exception);
    EXPECT_THROW(port.get_cd(), port_not_open_exception);
}

///
/// @brief close() on an already closed port is a no-op.
///
TEST(serial_test, close_is_idempotent) {
    serial port;
    EXPECT_NO_THROW(port.close());
    port.close();
    EXPECT_FALSE(port.is_open());
}

///
/// @brief Opening a port that is already open is rejected.
///
TEST(serial_test, double_open_throws) {
    // Use the first real port we can open; skip when the machine has none.
    for (const auto& info : list_ports()) {
        serial port;
        port.set_port(info.name);
        try {
            port.open();
        } catch (...) { continue; }
        EXPECT_THROW(port.open(), serial_exception);
        port.close();
        EXPECT_FALSE(port.is_open());
        return;
    }
    GTEST_SKIP() << "No usable serial port on this machine.";
}

///
/// @brief A missing port fails cleanly and leaves the object closed.
///
TEST(serial_test, open_missing_port_throws) {
    serial port;
    port.set_port("definitely-not-a-serial-port");
    EXPECT_THROW(port.open(), io_exception);
    EXPECT_FALSE(port.is_open());
}

///
/// @brief Configuration setters round-trip through the getters.
///
TEST(serial_test, settings_roundtrip) {
    serial port;
    port.set_baudrate(115200);
    port.set_data_bits(data_bits::seven);
    port.set_parity(parity::even);
    port.set_stop_bits(stop_bits::two);
    port.set_flow_ctrl(flow_ctrl::hardware);
    port.set_timeout(timeout::simple_timeout(500));

    EXPECT_EQ(port.get_baudrate(), 115200u);
    EXPECT_EQ(port.get_data_bits(), data_bits::seven);
    EXPECT_EQ(port.get_parity(), parity::even);
    EXPECT_EQ(port.get_stop_bits(), stop_bits::two);
    EXPECT_EQ(port.get_flow_ctrl(), flow_ctrl::hardware);
    EXPECT_EQ(port.get_timeout().read_timeout_constant, 500u);
}

///
/// @brief The five-argument set_timeout overload fills every field.
///
TEST(serial_test, set_timeout_fields) {
    serial port;
    port.set_timeout(1, 2, 3, 4, 5);
    const timeout t = port.get_timeout();
    EXPECT_EQ(t.inter_byte_timeout, 1u);
    EXPECT_EQ(t.read_timeout_constant, 2u);
    EXPECT_EQ(t.read_timeout_multiplier, 3u);
    EXPECT_EQ(t.write_timeout_constant, 4u);
    EXPECT_EQ(t.write_timeout_multiplier, 5u);
}

///
/// @brief get_port reflects set_port even while the port is closed.
///
TEST(serial_test, port_name_roundtrip) {
    serial port;
    port.set_port("COM42");
    EXPECT_EQ(port.get_port(), "COM42");
    port.set_port("/dev/ttyUSB7");
    EXPECT_EQ(port.get_port(), "/dev/ttyUSB7");
}

///
/// @brief Concurrent access is serialized instead of corrupting state.
///
TEST(serial_test, concurrent_calls_are_safe) {
    serial port;
    std::vector<std::thread> threads;
    for (int i = 0; i < 4; ++i) {
        threads.emplace_back(
            [&port, i]
            {
                for (int n = 0; n < 200; ++n) {
                    port.set_baudrate(9600 + i);
                    port.set_data_bits(data_bits::eight);
                    port.set_timeout(timeout::simple_timeout(100));
                    (void)port.get_baudrate();
                    (void)port.is_open();
                    (void)port.available();
                    EXPECT_THROW(port.read(1), port_not_open_exception);
                }
            }
        );
    }
    for (auto& t : threads) { t.join(); }
    EXPECT_FALSE(port.is_open());
}

///
/// @brief timeout helpers produce the documented field layout.
///
TEST(timeout_test, simple_timeout_fields) {
    const timeout t = timeout::simple_timeout(250);
    EXPECT_EQ(t.inter_byte_timeout, timeout::max());
    EXPECT_EQ(t.read_timeout_constant, 250u);
    EXPECT_EQ(t.read_timeout_multiplier, 0u);
    EXPECT_EQ(t.write_timeout_constant, 250u);
    EXPECT_EQ(t.write_timeout_multiplier, 0u);
}

///
/// @brief A default-constructed timeout is all zeros, i.e. non-blocking.
///
TEST(timeout_test, default_is_zero) {
    const timeout t;
    EXPECT_EQ(t.inter_byte_timeout, 0u);
    EXPECT_EQ(t.read_timeout_constant, 0u);
    EXPECT_EQ(t.write_timeout_constant, 0u);
}

///
/// @brief stop_bits_count maps the enum to frame time units.
///
TEST(stop_bits_test, counts) {
    EXPECT_DOUBLE_EQ(stop_bits_count(stop_bits::one), 1.0);
    EXPECT_DOUBLE_EQ(stop_bits_count(stop_bits::one_point_five), 1.5);
    EXPECT_DOUBLE_EQ(stop_bits_count(stop_bits::two), 2.0);
}

///
/// @brief list_ports returns well-formed entries or none; it must never throw.
///
TEST(list_ports_test, never_throws) {
    const auto ports = list_ports();
    for (const auto& port : ports) { EXPECT_FALSE(port.name.empty()); }
}
