///
/// @file linux_serial_test.cxx
/// @author BA7LYA (1042140025@qq.com)
/// @brief Tests the POSIX serial implementation against a pseudo-terminal pair (Linux only).
/// @version 0.2
/// @date 2026-09-14
/// @copyright Copyright (c) 2026
///

#include <gtest/gtest.h>
#include <pty.h>
#include <unistd.h>

#include <array>
#include <cerrno>
#include <cstring>
#include <memory>
#include <string>
#include <string_view>

#include "serial.hxx"

using namespace ba7lya::serial;

namespace {

///
/// @brief Fixture that connects the serial object under test to a fresh pty slave.
///
class serial_tests : public ::testing::Test {
protected:
    /// @brief Creates a pty pair and opens the slave side through the library.
    void SetUp() override {
        ASSERT_EQ(::openpty(&master_fd_, &slave_fd_, name_, nullptr, nullptr), 0)
            << std::strerror(errno);
        port_ = std::make_unique<serial>(std::string(name_), 115200, timeout::simple_timeout(250));
        ASSERT_TRUE(port_->is_open());
    }

    /// @brief Closes the library port and the raw pty descriptors.
    void TearDown() override {
        port_.reset();
        if (master_fd_ != -1) { ::close(master_fd_); }
        if (slave_fd_ != -1) { ::close(slave_fd_); }
    }

    /// @brief Writes to the master end so that the library can read it.
    /// @param text Data to feed into the port.
    void feed_master(std::string_view text) {
        ASSERT_EQ(
            ::write(master_fd_, text.data(), text.size()),
            static_cast<ssize_t>(text.size())
        );
    }

    std::unique_ptr<serial> port_;
    int master_fd_ = -1;
    int slave_fd_ = -1;
    char name_[100]{};
};

///
/// @brief Data written to the master end is read back through the library.
///
TEST_F(serial_tests, readWorks) {
    feed_master("abc\n");
    EXPECT_EQ(port_->read(4), "abc\n");
}

///
/// @brief Data written through the library arrives at the master end.
///
TEST_F(serial_tests, writeWorks) {
    ASSERT_EQ(port_->write("abc\n"), 4u);
    std::array<char, 5> buf{};
    const ssize_t n = ::read(master_fd_, buf.data(), 4);
    ASSERT_EQ(n, 4);
    EXPECT_EQ(std::string(buf.data(), static_cast<size_t>(n)), "abc\n");
}

///
/// @brief A read with nothing to deliver times out and yields an empty string.
///
TEST_F(serial_tests, timeoutWorks) {
    EXPECT_EQ(port_->read(1), "");
    feed_master("abc\n"); // Still usable after a timeout.
    EXPECT_EQ(port_->read(4), "abc\n");
}

///
/// @brief A timed-out read still returns the partial data that had arrived.
///
TEST_F(serial_tests, partialRead) {
    feed_master("abc\n");
    EXPECT_EQ(port_->read(10), "abc\n");
    feed_master("abc\n"); // Still usable afterwards.
    EXPECT_EQ(port_->read(4), "abc\n");
}

///
/// @brief readline stops at the newline; further calls return the following lines.
///
TEST_F(serial_tests, readlineWorks) {
    feed_master("line1\nline2\n");
    EXPECT_EQ(port_->readline(64), "line1\n");
    EXPECT_EQ(port_->readline(64), "line2\n");
}

///
/// @brief wait_readable reports readability before a read of queued data.
///
TEST_F(serial_tests, waitReadableWorks) {
    feed_master("xyz");
    EXPECT_TRUE(port_->wait_readable());
    EXPECT_EQ(port_->read(3), "xyz");
}

} // namespace
