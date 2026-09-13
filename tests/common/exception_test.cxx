///
/// @file exception_test.cxx
/// @author BA7LYA (1042140025@qq.com)
/// @brief Tests for the library exception hierarchy and messages.
/// @version 0.2
/// @date 2026-09-14
/// @copyright Copyright (c) 2026
///

#include <cstring>
#include <gtest/gtest.h>
#include <stdexcept>
#include <string>
#include <system_error>

#include "exception.hxx"

using namespace ba7lya::serial;

///
/// @brief serial_exception derives from runtime_error and frames its message.
///
TEST(exception_test, serial_exception_message) {
    const serial_exception e("open port");
    EXPECT_STREQ(e.what(), "serial_exception: open port failed.");
    // Throwable through the std::runtime_error interface.
    try {
        throw serial_exception("x");
    } catch (const std::runtime_error& r) {
        EXPECT_NE(std::strstr(r.what(), "serial_exception"), nullptr);
    }
}

///
/// @brief port_not_open_exception is a serial_exception naming the operation.
///
TEST(exception_test, port_not_open_is_serial_exception) {
    const port_not_open_exception e("serial::read");
    EXPECT_NE(std::strstr(e.what(), "serial::read"), nullptr);
    EXPECT_NE(std::strstr(e.what(), "not open"), nullptr);
    try {
        throw port_not_open_exception("serial::write");
    } catch (const serial_exception&) {
        SUCCEED();
        return;
    }
    FAIL() << "port_not_open_exception must catch as serial_exception";
}

///
/// @brief io_exception wrapping an error code keeps the code and context.
///
TEST(exception_test, io_exception_with_code) {
    const std::error_code ec = std::make_error_code(std::errc::permission_denied);
    const io_exception e(ec, "open failed");
    EXPECT_EQ(e.code(), ec);
    EXPECT_NE(std::strstr(e.what(), "open failed"), nullptr);
    // Catchable as std::system_error and std::runtime_error.
    try {
        throw io_exception(ec);
    } catch (const std::system_error& se) {
        EXPECT_EQ(se.code(), ec);
        SUCCEED();
        return;
    }
    FAIL() << "io_exception must catch as std::system_error";
}

///
/// @brief A message-only io_exception carries the generic io_error code.
///
TEST(exception_test, io_exception_message_only) {
    const io_exception e("pure logical failure");
    EXPECT_EQ(e.code(), std::make_error_code(std::errc::io_error));
    EXPECT_NE(std::strstr(e.what(), "pure logical failure"), nullptr);
}
