///
/// @file serial.hxx
/// @author BA7LYA (1042140025@qq.com)
/// @brief Provides a cross platform interface for interacting with serial ports.
/// @version 0.2
/// @date 2026-09-14
/// @copyright Copyright (c) 2026
///

#pragma once

#include <cstdint>
#include <memory>
#include <mutex>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "exception.hxx"

// <windows.h> defines min/max as macros; they break the declarations below.
#ifdef max
#undef max
#endif
#ifdef min
#undef min
#endif

namespace ba7lya::serial {

///
/// @brief Byte size of the serial frame.
///
enum class data_bits : std::uint8_t { five = 5, six = 6, seven = 7, eight = 8 };

///
/// @brief Parity checking method of the serial frame.
///
enum class parity : std::uint8_t { none = 0, odd = 1, even = 2, mark = 3, space = 4 };

///
/// @brief Number of stop bits of the serial frame.
/// @note one_point_five is only supported by some platforms and otherwise
/// handled as two stop bits.
///
enum class stop_bits : std::uint8_t { one = 1, two = 2, one_point_five = 3 };

///
/// @brief Number of stop-bit time units a stop_bits value represents.
/// @param value A stop_bits enumeration value.
/// @return 1.0, 1.5 or 2.0 stop bits.
///
constexpr double stop_bits_count(stop_bits value) {
    return value == stop_bits::one_point_five ? 1.5 : static_cast<double>(value);
}

///
/// @brief Flow control method of the serial port.
///
enum class flow_ctrl : std::uint8_t { none = 0, software, hardware };

///
/// @brief timeout description for read and write operations, all times in milliseconds.
/// @note Set inter_byte_timeout to timeout::max() to disable the inter-byte timeout.
///
struct timeout {
    /// @return The largest representable timeout, used to disable a timeout.
    static constexpr std::uint32_t max() { return UINT32_MAX; }

    ///
    /// @brief Builds a timeout with a single absolute read/write timeout.
    /// @param ms Milliseconds to wait after a call to read or write before timing out.
    /// @return A timeout representing this simple timeout.
    ///
    static constexpr timeout simple_timeout(std::uint32_t ms) {
        return {
            .inter_byte_timeout = max(),
            .read_timeout_constant = ms,
            .read_timeout_multiplier = 0,
            .write_timeout_constant = ms,
            .write_timeout_multiplier = 0,
        };
    }

    /// @brief Milliseconds between received bytes before a read times out (0 disables).
    std::uint32_t inter_byte_timeout {};

    /// @brief Constant milliseconds to wait after calling read.
    std::uint32_t read_timeout_constant {};

    /// @brief Milliseconds multiplied by the requested byte count, added to the read constant.
    std::uint32_t read_timeout_multiplier {};

    /// @brief Constant milliseconds to wait after calling write.
    std::uint32_t write_timeout_constant {};

    /// @brief Milliseconds multiplied by the written byte count, added to the write constant.
    std::uint32_t write_timeout_multiplier {};
};

///
/// @brief Describes one serial device enumerated by list_ports().
///
struct serial_port_info {
    /// @brief Port address, passable to the serial constructor ("COM1", "/dev/ttyUSB0", ...).
    std::string name;

    /// @brief Human readable description of the device, empty if unavailable.
    std::string description;

    /// @brief Hardware ID (e.g. USB VID:PID), "n/a" if unavailable.
    std::string hardware_id;
};

///
/// @brief Lists the serial ports available on the system.
/// @return A vector with one serial_port_info per detected port.
///
std::vector<serial_port_info> list_ports();

///
/// @brief Portable serial port interface, implemented per platform through a pimpl.
/// @note All operations are safe to call from multiple threads; concurrent calls are serialized.
///
class serial {
public:
    ///
    /// @brief Creates a serial object and opens the port if one is specified.
    /// @param port Port address, e.g. 'COM1' on Windows or '/dev/ttyS0' on Linux; empty leaves
    /// the port closed until open() is called.
    /// @param baudrate Transmission speed in bits per second.
    /// @param value Timeout conditions for read and write operations.
    /// @param bytesize Data bits per frame, default data_bits::eight.
    /// @param parity Parity method, default parity::none.
    /// @param stopbits Stop bits per frame, default stop_bits::one.
    /// @param flowcontrol Flow control method, default flow_ctrl::none.
    ///
    serial(
        std::string port = "",
        std::uint32_t baudrate = 9600,
        timeout value = {},
        data_bits bytesize = data_bits::eight,
        parity parity = parity::none,
        stop_bits stopbits = stop_bits::one,
        flow_ctrl flowcontrol = flow_ctrl::none
    );

    serial(const serial&) = delete;
    serial& operator=(const serial&) = delete;

    ~serial();

    ///
    /// @brief Opens the serial port, requires a non-empty port and a closed handle.
    /// @throws std::invalid_argument if no port is set.
    /// @throws io_exception if the port does not exist or cannot be opened.
    /// @throws serial_exception if the port is already open.
    ///
    void open();

    ///
    /// @brief Closes the serial port; a no-op when already closed.
    ///
    void close();

    /// @return True while the port handle is open.
    [[nodiscard]]
    bool is_open() const;

    /// @return Number of bytes readable from the input buffer right now.
    size_t available();

    ///
    /// @brief Blocks until data is readable or read_timeout_constant milliseconds elapsed.
    /// @return True if the port became readable, false on timeout or interruption.
    ///
    bool wait_readable();

    ///
    /// @brief Sleeps for the transmission time of count characters at the current settings.
    /// @param count Number of characters to wait for.
    ///
    void wait_byte_times(size_t count);

    ///
    /// @brief Reads up to buffer.size() bytes from the port into the buffer.
    /// @note Returns early (fewer bytes than requested) when a timeout expires; it never
    /// throws for timeouts, only for real I/O errors. Requires the port to be open.
    /// @param buffer Destination span of at least the requested size.
    /// @return Number of bytes actually read.
    ///
    size_t read(std::span<std::uint8_t> buffer);

    ///
    /// @brief Reads size bytes, appending them to the back of the vector.
    /// @param buffer Destination vector, extended by the bytes read.
    /// @param size Number of bytes to read.
    /// @return Number of bytes actually read.
    ///
    size_t read(std::vector<std::uint8_t>& buffer, size_t size = 1);

    ///
    /// @brief Reads size bytes, appending them to the end of the string.
    /// @param buffer Destination string, extended by the bytes read.
    /// @param size Number of bytes to read.
    /// @return Number of bytes actually read.
    ///
    size_t read(std::string& buffer, size_t size = 1);

    ///
    /// @brief Reads size bytes and returns them as a string.
    /// @param size Number of bytes to read.
    /// @return The data read from the port.
    ///
    std::string read(size_t size = 1);

    ///
    /// @brief Reads byte-per-byte until eol is seen, a timeout occurs or size bytes were read.
    /// @param buffer Destination string, the line (including eol) is appended to it.
    /// @param size Maximum number of bytes to read in total.
    /// @param eol End-of-line sequence to search for.
    /// @return Number of bytes read.
    ///
    size_t readline(std::string& buffer, size_t size = 65536, std::string_view eol = "\n");

    ///
    /// @brief Reads one line and returns it.
    /// @param size Maximum number of bytes to read in total.
    /// @param eol End-of-line sequence to search for.
    /// @return The line read, including the eol sequence if one was seen.
    ///
    std::string readline(size_t size = 65536, std::string_view eol = "\n");

    ///
    /// @brief Reads multiple lines until the read timeout expires.
    /// @note Requires a non-zero read timeout. Partial trailing data without eol is returned
    /// as a final line.
    /// @param size Maximum number of bytes to read in total.
    /// @param eol End-of-line sequence separating the lines.
    /// @return The lines read, each including its eol sequence.
    ///
    std::vector<std::string> readlines(size_t size = 65536, std::string_view eol = "\n");

    ///
    /// @brief Writes data to the serial port, honouring the write timeout.
    /// @param data Bytes to transmit.
    /// @return Number of bytes actually written (may be less on timeout).
    ///
    size_t write(std::span<const std::uint8_t> data);

    ///
    /// @brief Writes a vector's bytes to the serial port.
    /// @param data Bytes to transmit.
    /// @return Number of bytes actually written.
    ///
    size_t write(const std::vector<std::uint8_t>& data);

    ///
    /// @brief Writes a string's bytes to the serial port.
    /// @param data Bytes to transmit.
    /// @return Number of bytes actually written.
    ///
    size_t write(const std::string& data);

    ///
    /// @brief Changes the port address, re-opening the port when it was open.
    /// @param port Port address, e.g. 'COM1' or '/dev/ttyUSB0'.
    ///
    void set_port(const std::string& port);

    /// @return The currently configured port address.
    [[nodiscard]]
    std::string get_port() const;

    ///
    /// @brief Sets the read and write timeouts.
    /// @note inter_byte_timeout == 0 disables it; a total timeout of 0 enables non-blocking mode.
    /// @param timeout New timeout conditions.
    ///
    void set_timeout(const timeout& value);

    ///
    /// @brief Sets the read and write timeouts from individual fields.
    /// @param inter_byte_timeout Milliseconds between bytes before timeout (0 disables).
    /// @param read_timeout_constant Constant read timeout in milliseconds.
    /// @param read_timeout_multiplier Per-byte read timeout in milliseconds.
    /// @param write_timeout_constant Constant write timeout in milliseconds.
    /// @param write_timeout_multiplier Per-byte write timeout in milliseconds.
    ///
    void set_timeout(
        std::uint32_t inter_byte_timeout,
        std::uint32_t read_timeout_constant,
        std::uint32_t read_timeout_multiplier,
        std::uint32_t write_timeout_constant,
        std::uint32_t write_timeout_multiplier
    ) {
        set_timeout(
            timeout {
                .inter_byte_timeout = inter_byte_timeout,
                .read_timeout_constant = read_timeout_constant,
                .read_timeout_multiplier = read_timeout_multiplier,
                .write_timeout_constant = write_timeout_constant,
                .write_timeout_multiplier = write_timeout_multiplier,
            }
        );
    }

    /// @return The current read and write timeout conditions.
    [[nodiscard]]
    timeout get_timeout() const;

    ///
    /// @brief Sets the transmission speed.
    /// @param baudrate Speed in bits per second.
    ///
    void set_baudrate(std::uint32_t baudrate);

    /// @return The current baud rate in bits per second.
    [[nodiscard]]
    std::uint32_t get_baudrate() const;

    ///
    /// @brief Sets the number of data bits per frame.
    /// @param bytesize Data bits, 5 to 8.
    ///
    void set_data_bits(data_bits bytesize);

    /// @return The current data bit count.
    [[nodiscard]]
    data_bits get_data_bits() const;

    ///
    /// @brief Sets the parity method.
    /// @param parity Parity to use.
    ///
    void set_parity(parity parity);

    /// @return The current parity method.
    [[nodiscard]]
    parity get_parity() const;

    ///
    /// @brief Sets the number of stop bits.
    /// @param stopbits Stop bits to use.
    ///
    void set_stop_bits(stop_bits stopbits);

    /// @return The current stop bit setting.
    [[nodiscard]]
    stop_bits get_stop_bits() const;

    ///
    /// @brief Sets the flow control method.
    /// @param flowcontrol Flow control to use.
    ///
    void set_flow_ctrl(flow_ctrl flowcontrol);

    /// @return The current flow control method.
    [[nodiscard]]
    flow_ctrl get_flow_ctrl() const;

    /// @brief Blocks until both input and output buffers are flushed.
    void flush();

    /// @brief Discards data received but not yet read.
    void flush_rx_buffer();

    /// @brief Discards data written but not yet transmitted.
    void flush_tx_buffer();

    ///
    /// @brief Sends an RS-232 break signal for the given duration.
    /// @param duration Break duration in milliseconds.
    ///
    void send_break(int duration);

    ///
    /// @brief Holds the break condition at a level.
    /// @param level Level to drive the break condition to.
    ///
    void set_break(bool level = true);

    ///
    /// @brief Drives the RTS handshaking line.
    /// @param level Level to set RTS to.
    ///
    void set_rts(bool level = true);

    ///
    /// @brief Drives the DTR handshaking line.
    /// @param level Level to set DTR to.
    ///
    void set_dtr(bool level = true);

    ///
    /// @brief Blocks until CTS, DSR, RI or CD changes or an interrupt occurs.
    /// @note Uses a platform event when available, otherwise polls with ~1ms resolution.
    /// @return True when a line changed, false when something else ended the wait.
    ///
    bool wait_for_change();

    /// @return Current state of the CTS line.
    bool get_cts();

    /// @return Current state of the DSR line.
    bool get_dsr();

    /// @return Current state of the RI line.
    bool get_ri();

    /// @return Current state of the CD line.
    bool get_cd();

private:
    mutable std::mutex mutex_; // Serializes all port access through the facade
    class impl;
    std::unique_ptr<impl> pimpl_;
};

} // namespace ba7lya::serial
