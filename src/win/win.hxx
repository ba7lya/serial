///
/// @file win.hxx
/// @author BA7LYA (1042140025@qq.com)
/// @brief Windows implementation of the serial pimpl, based on the Win32 comm API.
/// @version 0.2
/// @date 2026-09-14
/// @copyright Copyright (c) 2026
///

#pragma once

#include <windows.h>

#include <cstdint>
#include <span>
#include <string>
#include <string_view>

#include "serial.hxx"

namespace ba7lya::serial {

///
/// @brief Win32 serial port implementation behind serial::impl.
///
class serial::impl {
public:
    ///
    /// @brief Stores the requested port configuration and opens the port when one was given.
    /// @param port Port address, empty leaves the port closed.
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

    /// @brief Closes the port if still open.
    ~impl();

    impl(const impl&) = delete;
    impl& operator=(const impl&) = delete;

    /// @brief Opens the port and applies the stored configuration.
    void open();

    /// @brief Closes the port handle; a no-op when already closed.
    void close();

    /// @return True while the handle is valid.
    [[nodiscard]] bool is_open() const;

    /// @return Bytes waiting in the driver receive queue.
    size_t available();

    ///
    /// @brief Waits for readability; not supported by the Win32 comm API.
    /// @param timeout Ignored.
    /// @throws io_exception always.
    ///
    bool wait_readable(std::uint32_t timeout);

    ///
    /// @brief Waits for the transmission time of count characters; unsupported on Windows.
    /// @throws io_exception always.
    ///
    void wait_byte_times(size_t count);

    ///
    /// @brief Reads via ReadFile, which applies the configured timeouts itself.
    /// @param buf Destination span.
    /// @return Bytes read.
    ///
    size_t read(std::span<std::uint8_t> buf);

    ///
    /// @brief Writes via WriteFile, which applies the configured timeouts itself.
    /// @param data Bytes to send.
    /// @return Bytes written.
    ///
    size_t write(std::span<const std::uint8_t> data);

    /// @brief Flushes both buffers.
    void flush();

    /// @brief Drops received but unread data.
    void flush_rx_buffer();

    /// @brief Drops queued but unsent data.
    void flush_tx_buffer();

    ///
    /// @brief Sends a timed break; unsupported on Windows.
    /// @throws io_exception always.
    ///
    void send_break(int duration);

    /// @brief Drives the break condition.
    void set_break(bool level);

    /// @brief Drives the RTS line.
    void set_rts(bool level);

    /// @brief Drives the DTR line.
    void set_dtr(bool level);

    /// @brief Blocks on WaitCommEvent until a modem line changes.
    bool wait_for_change();

    /// @return State of the CTS line.
    bool get_cts();

    /// @return State of the DSR line.
    bool get_dsr();

    /// @return State of the ring indicator line.
    bool get_ri();

    /// @return State of the carrier detect line.
    bool get_cd();

    /// @brief Changes the port address, trimming any redundant "\\.\" prefix.
    void set_port(const std::string& port);

    /// @return The configured port address.
    [[nodiscard]] std::string get_port() const;

    /// @brief Stores the timeouts and re-applies them when the port is open.
    void set_timeout(const timeout& value);

    /// @return The stored timeout conditions.
    [[nodiscard]] timeout get_timeout() const;

    /// @brief Stores the baud rate and re-applies the configuration when open.
    void set_baudrate(std::uint32_t baudrate);

    /// @return The stored baud rate.
    [[nodiscard]] std::uint32_t get_baudrate() const;

    /// @brief Stores the data bits and re-applies the configuration when open.
    void set_data_bits(data_bits bytesize);

    /// @return The stored data bit count.
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
    /// @brief Throws io_exception unless the handle is valid.
    void require_open(std::string_view operation) const;

    /// @brief Writes the stored baud/framing/flow/timeout settings to the driver.
    void reconfigure();

    std::string port_; // Port address as UTF-8
    HANDLE fd_ = INVALID_HANDLE_VALUE;
    bool is_open_ = false;

    timeout timeout_{};
    std::uint32_t baudrate_{};
    parity parity_{};
    data_bits bytesize_{};
    stop_bits stopbits_{};
    flow_ctrl flowcontrol_{};
};

} // namespace ba7lya::serial
