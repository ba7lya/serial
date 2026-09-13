///
/// @file serial_test.cxx
/// @author BA7LYA (1042140025@qq.com)
/// @brief Portable unit tests for the serial facade: parameter handling, errors and listing.
/// @version 0.2
/// @date 2026-09-14
/// @copyright Copyright (c) 2026
///

#include <gtest/gtest.h>

#include "serial.hxx"

using namespace ba7lya::serial;

///
/// @brief Exercises construction without opening a port.
///
TEST(serial_test, constructs_closed) {
    serial serial;
    EXPECT_FALSE(serial.is_open());
    EXPECT_EQ(serial.get_port(), "");
    EXPECT_EQ(serial.get_baudrate(), 9600u);
    EXPECT_EQ(serial.get_data_bits(), data_bits::eight);
    EXPECT_EQ(serial.get_parity(), parity::none);
    EXPECT_EQ(serial.get_stop_bits(), stop_bits::one);
    EXPECT_EQ(serial.get_flow_ctrl(), flow_ctrl::none);
}

///
/// @brief An empty port name makes open() fail with invalid_argument.
///
TEST(serial_test, open_without_port_throws) {
    serial serial;
    EXPECT_THROW(serial.open(), std::invalid_argument);
}

///
/// @brief Operations on a closed port throw port_not_open_exception.
///
TEST(serial_test, operations_on_closed_port_throw) {
    serial serial;
    EXPECT_THROW(serial.read(1), port_not_open_exception);
    EXPECT_THROW(serial.write(std::string("x")), port_not_open_exception);
    EXPECT_THROW(serial.flush(), port_not_open_exception);
    EXPECT_THROW(serial.set_rts(true), port_not_open_exception);
    EXPECT_THROW(serial.get_cts(), port_not_open_exception);
}

/// @brief Opening a port that is already open is rejected.
///
TEST(serial_test, double_open_throws) {
    // Use the first real port we can open; skip when the machine has none.
    for (const auto& info : list_ports()) {
        serial serial;
        serial.set_port(info.name);
        try {
            serial.open();
        } catch (...) {
            continue;
        }
        EXPECT_THROW(serial.open(), serial_exception);
        serial.close();
        EXPECT_FALSE(serial.is_open());
        return;
    }
    GTEST_SKIP() << "No usable serial port on this machine.";
}

///
/// @brief Setting a nonexistent port fails cleanly and leaves the object closed.
///
TEST(serial_test, open_missing_port_throws) {
    serial serial;
    serial.set_port("definitely-not-a-serial-port");
    EXPECT_THROW(serial.open(), io_exception);
    EXPECT_FALSE(serial.is_open());
}

///
/// @brief Configuration setters round-trip through the getters.
///
TEST(serial_test, settings_roundtrip) {
    serial serial;
    serial.set_baudrate(115200);
    serial.set_data_bits(data_bits::seven);
    serial.set_parity(parity::even);
    serial.set_stop_bits(stop_bits::two);
    serial.set_flow_ctrl(flow_ctrl::hardware);
    serial.set_timeout(timeout::simple_timeout(500));

    EXPECT_EQ(serial.get_baudrate(), 115200u);
    EXPECT_EQ(serial.get_data_bits(), data_bits::seven);
    EXPECT_EQ(serial.get_parity(), parity::even);
    EXPECT_EQ(serial.get_stop_bits(), stop_bits::two);
    EXPECT_EQ(serial.get_flow_ctrl(), flow_ctrl::hardware);
    EXPECT_EQ(serial.get_timeout().read_timeout_constant, 500u);
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
