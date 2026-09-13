///
/// @file serial_test.cxx
/// @author BA7LYA (1042140025@qq.com)
/// @brief Portable unit tests for the Serial facade: parameter handling, errors and listing.
/// @version 0.2
/// @date 2026-09-14
/// @copyright Copyright (c) 2026
///

#include <gtest/gtest.h>

#include "ba7lya/serial/serial.hxx"

using namespace ba7lya::serial;

///
/// @brief Exercises construction without opening a port.
///
TEST(SerialTest, constructs_closed) {
    Serial serial;
    EXPECT_FALSE(serial.is_open());
    EXPECT_EQ(serial.get_port(), "");
    EXPECT_EQ(serial.get_baudrate(), 9600u);
    EXPECT_EQ(serial.get_data_bits(), data_bits::EIGHT);
    EXPECT_EQ(serial.get_parity(), parity::NONE);
    EXPECT_EQ(serial.get_stop_bits(), stop_bits::ONE);
    EXPECT_EQ(serial.get_flow_ctrl(), flow_ctrl::NONE);
}

///
/// @brief An empty port name makes open() fail with invalid_argument.
///
TEST(SerialTest, open_without_port_throws) {
    Serial serial;
    EXPECT_THROW(serial.open(), std::invalid_argument);
}

///
/// @brief Operations on a closed port throw port_not_open_exception.
///
TEST(SerialTest, operations_on_closed_port_throw) {
    Serial serial;
    EXPECT_THROW(serial.read(1), port_not_open_exception);
    EXPECT_THROW(serial.write(std::string("x")), port_not_open_exception);
    EXPECT_THROW(serial.flush(), port_not_open_exception);
    EXPECT_THROW(serial.set_rts(true), port_not_open_exception);
    EXPECT_THROW(serial.get_cts(), port_not_open_exception);
}

/// @brief Opening a port that is already open is rejected.
///
TEST(SerialTest, double_open_throws) {
    // Use the first real port we can open; skip when the machine has none.
    for (const auto& info : list_ports()) {
        Serial serial;
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
TEST(SerialTest, open_missing_port_throws) {
    Serial serial;
    serial.set_port("definitely-not-a-serial-port");
    EXPECT_THROW(serial.open(), io_exception);
    EXPECT_FALSE(serial.is_open());
}

///
/// @brief Configuration setters round-trip through the getters.
///
TEST(SerialTest, settings_roundtrip) {
    Serial serial;
    serial.set_baudrate(115200);
    serial.set_data_bits(data_bits::SEVEN);
    serial.set_parity(parity::EVEN);
    serial.set_stop_bits(stop_bits::TWO);
    serial.set_flow_ctrl(flow_ctrl::HARDWARE);
    serial.set_timeout(Timeout::simple_timeout(500));

    EXPECT_EQ(serial.get_baudrate(), 115200u);
    EXPECT_EQ(serial.get_data_bits(), data_bits::SEVEN);
    EXPECT_EQ(serial.get_parity(), parity::EVEN);
    EXPECT_EQ(serial.get_stop_bits(), stop_bits::TWO);
    EXPECT_EQ(serial.get_flow_ctrl(), flow_ctrl::HARDWARE);
    EXPECT_EQ(serial.get_timeout().read_timeout_constant, 500u);
}

///
/// @brief Timeout helpers produce the documented field layout.
///
TEST(TimeoutTest, simple_timeout_fields) {
    const Timeout t = Timeout::simple_timeout(250);
    EXPECT_EQ(t.inter_byte_timeout, Timeout::max());
    EXPECT_EQ(t.read_timeout_constant, 250u);
    EXPECT_EQ(t.read_timeout_multiplier, 0u);
    EXPECT_EQ(t.write_timeout_constant, 250u);
    EXPECT_EQ(t.write_timeout_multiplier, 0u);
}

///
/// @brief stop_bits_count maps the enum to frame time units.
///
TEST(StopBitsTest, counts) {
    EXPECT_DOUBLE_EQ(stop_bits_count(stop_bits::ONE), 1.0);
    EXPECT_DOUBLE_EQ(stop_bits_count(stop_bits::ONE_POINT_FIVE), 1.5);
    EXPECT_DOUBLE_EQ(stop_bits_count(stop_bits::TWO), 2.0);
}

///
/// @brief list_ports returns well-formed entries or none; it must never throw.
///
TEST(ListPortsTest, never_throws) {
    const auto ports = list_ports();
    for (const auto& port : ports) { EXPECT_FALSE(port.name.empty()); }
}
