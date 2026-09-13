///
/// @file win.cxx
/// @author BA7LYA (1042140025@qq.com)
/// @brief Windows implementation of the serial class, built on the Win32 comm API.
/// @version 0.2
/// @date 2026-09-14
/// @copyright Copyright (c) 2026
///

#include "win.hxx"

#include <system_error>

namespace ba7lya::serial {

namespace {

///
/// @brief Wraps the last Win32 error into an io_exception with context.
/// @param context Description of the operation that failed.
///
[[noreturn]] void throw_last_error(const std::string& context) {
    throw io_exception(
        std::error_code(static_cast<int>(::GetLastError()), std::system_category()),
        context
    );
}

///
/// @brief Converts a UTF-8 std::string to a UTF-16 wide string.
/// @param text UTF-8 input.
/// @return The converted string, empty on failure or empty input.
///
std::wstring to_wide(const std::string& text) {
    if (text.empty()) { return {}; }
    const int count
        = ::MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), nullptr, 0);
    std::wstring result(static_cast<size_t>(count), L'\0');
    ::MultiByteToWideChar(
        CP_UTF8,
        0,
        text.data(),
        static_cast<int>(text.size()),
        result.data(),
        count
    );
    return result;
}

///
/// @brief Adds the "\\\\.\\" namespace prefix unless the port already has it.
/// @note The prefix is required for COM10 and above and for named devices.
/// @param port Wide port name.
/// @return The prefixed port name.
///
std::wstring prefix_port(const std::wstring& port) {
    constexpr std::wstring_view prefix = L"\\\\.\\";
    if (port.starts_with(prefix)) { return port; }
    return std::wstring(prefix) + port;
}

} // namespace

///
/// @brief Stores the configuration and opens the port when a port name was given.
///
serial::impl::impl(
    const std::string& port,
    std::uint32_t baudrate,
    data_bits bytesize,
    parity parity,
    stop_bits stopbits,
    flow_ctrl flowcontrol
)
    : port_(port)
    , baudrate_(baudrate)
    , parity_(parity)
    , bytesize_(bytesize)
    , stopbits_(stopbits)
    , flowcontrol_(flowcontrol) {
    if (!port_.empty()) { open(); }
}

///
/// @brief Releases the port handle.
///
serial::impl::~impl() {
    try {
        close();
    } catch (...) {
    }
}

///
/// @brief Throws io_exception unless the port handle is open.
///
void serial::impl::require_open(std::string_view operation) const {
    if (!is_open_) { throw port_not_open_exception(std::string(operation)); }
}

///
/// @brief Opens the handle and applies all stored settings.
///
void serial::impl::open() {
    if (port_.empty()) { throw std::invalid_argument("Empty port is invalid."); }
    if (is_open_) { throw serial_exception("serial port already open."); }

    const std::wstring port = prefix_port(to_wide(port_));
    fd_ = ::CreateFileW(
        port.c_str(),
        GENERIC_READ | GENERIC_WRITE,
        0,
        nullptr,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        nullptr
    );

    if (fd_ == INVALID_HANDLE_VALUE) {
        if (::GetLastError() == ERROR_FILE_NOT_FOUND) {
            throw io_exception("Specified port, " + get_port() + ", does not exist.");
        }
        throw_last_error("Unknown error opening serial port");
    }

    try {
        reconfigure();
    } catch (...) {
        ::CloseHandle(fd_);
        fd_ = INVALID_HANDLE_VALUE;
        throw;
    }
    is_open_ = true;
}

///
/// @brief Writes the stored baud rate, framing and timeout settings into the driver.
///
void serial::impl::reconfigure() {
    if (fd_ == INVALID_HANDLE_VALUE) {
        throw io_exception("Invalid file descriptor, is the serial port open?");
    }

    DCB dcb{};
    dcb.DCBlength = sizeof(dcb);
    if (!::GetCommState(fd_, &dcb)) {
        throw_last_error("Error getting the serial port state");
    }

    // The DCB BaudRate field takes any DWORD; the standard CBR_* values are plain numbers.
    dcb.BaudRate = baudrate_;

    switch (bytesize_) {
    case data_bits::five: dcb.ByteSize = 5; break;
    case data_bits::six: dcb.ByteSize = 6; break;
    case data_bits::seven: dcb.ByteSize = 7; break;
    case data_bits::eight: dcb.ByteSize = 8; break;
    default: throw std::invalid_argument("invalid data bits");
    }

    switch (parity_) {
    case parity::none: dcb.Parity = NOPARITY; break;
    case parity::odd: dcb.Parity = ODDPARITY; break;
    case parity::even: dcb.Parity = EVENPARITY; break;
    case parity::mark: dcb.Parity = MARKPARITY; break;
    case parity::space: dcb.Parity = SPACEPARITY; break;
    default: throw std::invalid_argument("invalid parity");
    }

    switch (stopbits_) {
    case stop_bits::one: dcb.StopBits = ONESTOPBIT; break;
    case stop_bits::one_point_five: dcb.StopBits = ONE5STOPBITS; break;
    case stop_bits::two: dcb.StopBits = TWOSTOPBITS; break;
    default: throw std::invalid_argument("invalid stop bit");
    }

    dcb.fOutxCtsFlow = flowcontrol_ == flow_ctrl::hardware;
    dcb.fRtsControl = flowcontrol_ == flow_ctrl::hardware ? RTS_CONTROL_HANDSHAKE
                                                          : RTS_CONTROL_DISABLE;
    dcb.fOutX = dcb.fInX = flowcontrol_ == flow_ctrl::software;

    if (!::SetCommState(fd_, &dcb)) { throw_last_error("Error setting serial port settings"); }

    COMMTIMEOUTS timeouts{
        .ReadIntervalTimeout = timeout_.inter_byte_timeout,
        .ReadTotalTimeoutMultiplier = timeout_.read_timeout_multiplier,
        .ReadTotalTimeoutConstant = timeout_.read_timeout_constant,
        .WriteTotalTimeoutMultiplier = timeout_.write_timeout_multiplier,
        .WriteTotalTimeoutConstant = timeout_.write_timeout_constant,
    };
    if (!::SetCommTimeouts(fd_, &timeouts)) { throw_last_error("Error setting timeouts"); }
}

///
/// @brief Closes the handle if the port is open.
///
void serial::impl::close() {
    if (!is_open_) { return; }
    if (fd_ != INVALID_HANDLE_VALUE && !::CloseHandle(fd_)) {
        fd_ = INVALID_HANDLE_VALUE;
        is_open_ = false;
        throw_last_error("Error closing serial port");
    }
    fd_ = INVALID_HANDLE_VALUE;
    is_open_ = false;
}

///
/// @brief Reports whether the port is open.
///
bool serial::impl::is_open() const { return is_open_; }

///
/// @brief Queries the driver receive queue size.
///
size_t serial::impl::available() {
    if (!is_open_) { return 0; }
    COMSTAT status{};
    if (!::ClearCommError(fd_, nullptr, &status)) {
        throw_last_error("Error while checking status of the serial port");
    }
    return static_cast<size_t>(status.cbInQue);
}

///
/// @brief Unsupported: Win32 comm API has no wait-for-readable primitive.
///
bool serial::impl::wait_readable(std::uint32_t) {
    throw io_exception("wait_readable is not implemented on Windows.");
}

///
/// @brief Unsupported on Windows; the caller can sleep itself.
///
void serial::impl::wait_byte_times(size_t) {
    throw io_exception("wait_byte_times is not implemented on Windows.");
}

///
/// @brief Reads bytes; ReadFile enforces the configured timeouts and returns short counts.
///
size_t serial::impl::read(std::span<std::uint8_t> buf) {
    require_open("serial::read");
    DWORD bytes_read = 0;
    if (!::ReadFile(fd_, buf.data(), static_cast<DWORD>(buf.size()), &bytes_read, nullptr)) {
        throw_last_error("Error while reading from the serial port");
    }
    return bytes_read;
}

///
/// @brief Writes bytes; WriteFile enforces the configured write timeout.
///
size_t serial::impl::write(std::span<const std::uint8_t> data) {
    require_open("serial::write");
    DWORD bytes_written = 0;
    if (!::WriteFile(fd_, data.data(), static_cast<DWORD>(data.size()), &bytes_written, nullptr)) {
        throw_last_error("Error while writing to the serial port");
    }
    return bytes_written;
}

///
/// @brief Waits for both buffers to drain.
///
void serial::impl::flush() {
    require_open("serial::flush");
    ::FlushFileBuffers(fd_);
}

///
/// @brief Purges the receive buffer.
///
void serial::impl::flush_rx_buffer() {
    require_open("serial::flush_rx_buffer");
    ::PurgeComm(fd_, PURGE_RXCLEAR);
}

///
/// @brief Purges the transmit buffer.
///
void serial::impl::flush_tx_buffer() {
    require_open("serial::flush_tx_buffer");
    ::PurgeComm(fd_, PURGE_TXCLEAR);
}

///
/// @brief Unsupported: Win32 has no timed break facility.
///
void serial::impl::send_break(int) {
    throw io_exception("send_break is not supported on Windows.");
}

///
/// @brief Drives the break condition with EscapeCommFunction.
///
void serial::impl::set_break(bool level) {
    require_open("serial::set_break");
    ::EscapeCommFunction(fd_, level ? SETBREAK : CLRBREAK);
}

///
/// @brief Drives the RTS line.
///
void serial::impl::set_rts(bool level) {
    require_open("serial::set_rts");
    ::EscapeCommFunction(fd_, level ? SETRTS : CLRRTS);
}

///
/// @brief Drives the DTR line.
///
void serial::impl::set_dtr(bool level) {
    require_open("serial::set_dtr");
    ::EscapeCommFunction(fd_, level ? SETDTR : CLRDTR);
}

///
/// @brief Blocks until one of EV_CTS / EV_DSR / EV_RING / EV_RLSD fires.
///
bool serial::impl::wait_for_change() {
    require_open("serial::wait_for_change");
    DWORD event = 0;
    return ::SetCommMask(fd_, EV_CTS | EV_DSR | EV_RING | EV_RLSD)
        && ::WaitCommEvent(fd_, &event, nullptr);
}

///
/// @brief Reads the modem status word and tests one bit.
/// @param mask Bit to test, e.g. MS_CTS_ON.
/// @return True when the line is asserted.
///
static bool modem_status(HANDLE fd, DWORD mask, std::string_view line) {
    DWORD status = 0;
    if (!::GetCommModemStatus(fd, &status)) {
        throw io_exception("Error getting the status of the " + std::string(line) + " line.");
    }
    return (status & mask) != 0;
}

/// @brief Returns the CTS line state.
bool serial::impl::get_cts() {
    require_open("serial::get_cts");
    return modem_status(fd_, MS_CTS_ON, "CTS");
}

/// @brief Returns the DSR line state.
bool serial::impl::get_dsr() {
    require_open("serial::get_dsr");
    return modem_status(fd_, MS_DSR_ON, "DSR");
}

/// @brief Returns the ring indicator line state.
bool serial::impl::get_ri() {
    require_open("serial::get_ri");
    return modem_status(fd_, MS_RING_ON, "RI");
}

/// @brief Returns the carrier detect line state.
bool serial::impl::get_cd() {
    require_open("serial::get_cd");
    return modem_status(fd_, MS_RLSD_ON, "CD");
}

///
/// @brief Stores the port address, trimming a redundant "\\.\" prefix if present.
///
void serial::impl::set_port(const std::string& port) {
    constexpr std::string_view prefix = "\\\\.\\\\";
    if (port.starts_with(prefix)) {
        port_ = port.substr(prefix.size());
    }
    else {
        port_ = port;
    }
}

///
/// @brief Returns the stored port address.
///
std::string serial::impl::get_port() const { return port_; }

///
/// @brief Stores the timeouts and applies them when the port is open.
///
void serial::impl::set_timeout(const timeout& value) {
    timeout_ = value;
    if (is_open_) { reconfigure(); }
}

/// @brief Returns the stored timeouts.
timeout serial::impl::get_timeout() const { return timeout_; }

/// @brief Stores the baud rate and applies it when the port is open.
void serial::impl::set_baudrate(std::uint32_t baudrate) {
    baudrate_ = baudrate;
    if (is_open_) { reconfigure(); }
}

/// @brief Returns the stored baud rate.
std::uint32_t serial::impl::get_baudrate() const { return baudrate_; }

/// @brief Stores the data bits and applies them when the port is open.
void serial::impl::set_data_bits(data_bits bytesize) {
    bytesize_ = bytesize;
    if (is_open_) { reconfigure(); }
}

/// @brief Returns the stored data bits.
data_bits serial::impl::get_data_bits() const { return bytesize_; }

/// @brief Stores the parity and applies it when the port is open.
void serial::impl::set_parity(parity parity) {
    parity_ = parity;
    if (is_open_) { reconfigure(); }
}

/// @brief Returns the stored parity.
parity serial::impl::get_parity() const { return parity_; }

/// @brief Stores the stop bits and applies them when the port is open.
void serial::impl::set_stop_bits(stop_bits stopbits) {
    stopbits_ = stopbits;
    if (is_open_) { reconfigure(); }
}

/// @brief Returns the stored stop bits.
stop_bits serial::impl::get_stop_bits() const { return stopbits_; }

/// @brief Stores the flow control and applies it when the port is open.
void serial::impl::set_flow_ctrl(flow_ctrl flowcontrol) {
    flowcontrol_ = flowcontrol;
    if (is_open_) { reconfigure(); }
}

/// @brief Returns the stored flow control.
flow_ctrl serial::impl::get_flow_ctrl() const { return flowcontrol_; }

} // namespace ba7lya::serial
