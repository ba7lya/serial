///
/// @file linux.hxx
/// @author BA7LYA (1042140025@qq.com)
/// @brief Linux implementation of the Serial pimpl, based on termios and pselect.
/// @version 0.2
/// @date 2026-09-14
/// @copyright Copyright (c) 2026
///

#pragma once

#include <termios.h>

#include <chrono>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>

#include "ba7lya/serial/serial.hxx"

namespace ba7lya::serial {

///
/// @brief Counts down a millisecond deadline against the monotonic clock.
///
class MillisecondTimer {
public:
    ///
    /// @brief Starts a countdown from now.
    /// @param millis Duration until the deadline expires.
    ///
    explicit MillisecondTimer(std::uint32_t millis)
        : expiry_(std::chrono::steady_clock::now() + std::chrono::milliseconds(millis)) {}

    ///
    /// @brief Returns the time left until the deadline.
    /// @return Milliseconds remaining; negative once the deadline has passed.
    ///
    std::int64_t remaining() const {
        using namespace std::chrono;
        return duration_cast<milliseconds>(expiry_ - steady_clock::now()).count();
    }

private:
    std::chrono::steady_clock::time_point expiry_;
};

///
/// @brief Linux serial port implementation behind Serial::impl.
///
class Serial::impl {
public:
    ///
    /// @brief Stores the requested port configuration and opens the port when one was given.
    /// @param port Device path, empty leaves the port closed.
    /// @param baudrate Speed in bits per second.
    /// @param bytesize Data bits per frame.
    /// @param parity Parity method.
    /// @param stopbits Stop bits per frame.
    /// @param flowcontrol Flow control method.
    ///
    impl(
        const std::string& port,
        std::uint32_t baudrate,
        data_bits bytesize,
        parity parity,
        stop_bits stopbits,
        flow_ctrl flowcontrol
    );

    /// @brief Closes the descriptor if still open.
    ~impl();

    impl(const impl&) = delete;
    impl& operator=(const impl&) = delete;

    /// @brief Opens the device non-blocking and applies the stored configuration.
    void open();

    /// @brief Closes the descriptor; a no-op when already closed.
    void close();

    /// @return True while the descriptor is valid.
    [[nodiscard]] bool is_open() const;

    /// @return Bytes waiting in the kernel input queue.
    size_t available();

    ///
    /// @brief Blocks via pselect until readable or the timeout expires.
    /// @param timeout Milliseconds to wait.
    /// @return True when data became readable.
    ///
    bool wait_readable(std::uint32_t timeout);

    /// @brief Sleeps for the transmission time of count characters.
    void wait_byte_times(size_t count);

    ///
    /// @brief Reads until buf is full or the configured timeouts expire.
    /// @param buf Destination span.
    /// @return Bytes read.
    ///
    size_t read(std::span<std::uint8_t> buf);

    ///
    /// @brief Writes all bytes or stops when the write timeout expires.
    /// @param data Bytes to send.
    /// @return Bytes written.
    ///
    size_t write(std::span<const std::uint8_t> data);

    /// @brief Waits for the output queue to drain.
    void flush();

    /// @brief Discards unread input.
    void flush_rx_buffer();

    /// @brief Discards untransmitted output.
    void flush_tx_buffer();

    /// @brief Sends a break signal for duration milliseconds.
    void send_break(int duration);

    /// @brief Drives the break condition via TIOCSBRK / TIOCCBRK.
    void set_break(bool level);

    /// @brief Drives the RTS line via TIOCMBIS / TIOCMBIC.
    void set_rts(bool level);

    /// @brief Drives the DTR line via TIOCMBIS / TIOCMBIC.
    void set_dtr(bool level);

    /// @brief Blocks on TIOCMIWAIT until a modem line changes.
    bool wait_for_change();

    /// @return State of the CTS line.
    bool get_cts();

    /// @return State of the DSR line.
    bool get_dsr();

    /// @return State of the ring indicator line.
    bool get_ri();

    /// @return State of the carrier detect line.
    bool get_cd();

    /// @brief Changes the stored device path.
    void set_port(const std::string& port);

    /// @return The stored device path.
    [[nodiscard]] std::string get_port() const;

    /// @brief Stores the timeouts (enforced in read/write, not by the driver).
    void set_timeout(const Timeout& timeout);

    /// @return The stored timeout conditions.
    [[nodiscard]] Timeout get_timeout() const;

    /// @brief Stores the baud rate and re-applies the configuration when open.
    void set_baudrate(std::uint32_t baudrate);

    /// @return The stored baud rate.
    [[nodiscard]] std::uint32_t get_baudrate() const;

    /// @brief Stores the data bits and re-applies the configuration when open.
    void set_data_bits(data_bits bytesize);

    /// @return The stored data bits.
    [[nodiscard]] data_bits get_data_bits() const;

    /// @brief Stores the parity and re-applies the configuration when open.
    void set_parity(parity parity);

    /// @return The stored parity.
    [[nodiscard]] parity get_parity() const;

    /// @brief Stores the stop bits and re-applies the configuration when open.
    void set_stop_bits(stop_bits stopbits);

    /// @return The stored stop bits.
    [[nodiscard]] stop_bits get_stop_bits() const;

    /// @brief Stores the flow control and re-applies the configuration when open.
    void set_flow_ctrl(flow_ctrl flowcontrol);

    /// @return The stored flow control.
    [[nodiscard]] flow_ctrl get_flow_ctrl() const;

private:
    /// @brief Throws io_exception unless the descriptor is open.
    void require_open(std::string_view operation) const;

    /// @brief Throws io_exception carrying the current errno, with context.
    [[noreturn]] static void throw_errno(std::string_view context);

    /// @brief Reads one modem status bit via TIOCMGET.
    bool modem_line(int mask);

    /// @brief Writes the stored framing settings into the termios structure.
    void reconfigure();

    /// @brief Recomputes byte_time_ns_ from the current framing.
    void update_byte_time();

    std::string port_; // Device path
    int fd_ = -1;

    bool is_open_ = false;

    Timeout timeout_{};
    std::uint32_t baudrate_{};
    std::uint32_t byte_time_ns_{}; // Nanoseconds to transmit one byte

    parity parity_{};
    data_bits bytesize_{};
    stop_bits stopbits_{};
    flow_ctrl flowcontrol_{};
};

} // namespace ba7lya::serial
