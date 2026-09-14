///
/// @file serial.cxx
/// @author BA7LYA (1042140025@qq.com)
/// @brief Platform-independent facade for the serial class, forwarding to the per-platform impl.
/// @version 0.2
/// @date 2026-09-14
/// @copyright Copyright (c) 2026
///

#include "serial.hxx"

#include <cstddef>
#include <utility>
#include <vector>

#ifdef _WIN32
#include "win/win.hxx"
#else
#include "linux/linux.hxx"
#endif

namespace ba7lya::serial {

///
/// @brief Constructs the object and its platform implementation, opening the port when given.
///
serial::serial(
    std::string port,
    std::uint32_t baudrate,
    timeout value,
    data_bits bytesize,
    parity parity,
    stop_bits stopbits,
    flow_ctrl flowcontrol
)
    : pimpl_(
          std::make_unique<impl>(std::move(port), baudrate, bytesize, parity, stopbits, flowcontrol)
      ) {
    pimpl_->set_timeout(value);
}

serial::~serial() = default;

///
/// @brief Opens the serial port through the platform implementation.
///
void serial::open() {
    const std::scoped_lock guard(mutex_);
    pimpl_->open();
}

///
/// @brief Closes the serial port; safe to call when already closed.
///
void serial::close() {
    const std::scoped_lock guard(mutex_);
    pimpl_->close();
}

///
/// @brief Reports whether the port is currently open.
///
bool serial::is_open() const {
    const std::scoped_lock guard(mutex_);
    return pimpl_->is_open();
}

///
/// @brief Returns the number of bytes immediately readable from the input buffer.
///
size_t serial::available() {
    const std::scoped_lock guard(mutex_);
    return pimpl_->available();
}

///
/// @brief Blocks until data is readable or the read timeout constant elapses.
///
bool serial::wait_readable() {
    const std::scoped_lock guard(mutex_);
    return pimpl_->wait_readable(pimpl_->get_timeout().read_timeout_constant);
}

///
/// @brief Sleeps for the transmission time of count characters.
///
void serial::wait_byte_times(size_t count) {
    const std::scoped_lock guard(mutex_);
    pimpl_->wait_byte_times(count);
}

///
/// @brief Reads raw bytes into the span, honouring the read timeout.
///
size_t serial::read(std::span<std::uint8_t> buffer) {
    const std::scoped_lock guard(mutex_);
    return pimpl_->read(buffer);
}

///
/// @brief Reads bytes and appends them to a vector, resizing it to the data actually read.
///
size_t serial::read(std::vector<std::uint8_t>& buffer, size_t size) {
    const std::scoped_lock guard(mutex_);
    const size_t old_size = buffer.size();
    buffer.resize(old_size + size);
    try {
        const size_t bytes_read = pimpl_->read(std::span(buffer).subspan(old_size));
        buffer.resize(old_size + bytes_read);
        return bytes_read;
    } catch (...) {
        buffer.resize(old_size);
        throw;
    }
}

///
/// @brief Reads bytes and appends them to a string.
///
size_t serial::read(std::string& buffer, size_t size) {
    const std::scoped_lock guard(mutex_);
    const size_t old_size = buffer.size();
    buffer.resize(old_size + size);
    try {
        auto* bytes = reinterpret_cast<std::uint8_t*>(buffer.data() + old_size);
        const size_t bytes_read = pimpl_->read(std::span(bytes, size));
        buffer.resize(old_size + bytes_read);
        return bytes_read;
    } catch (...) {
        buffer.resize(old_size);
        throw;
    }
}

///
/// @brief Reads bytes and returns them as a new string.
/// @note Delegates to the locking overload; must not lock the mutex itself.
///
std::string serial::read(size_t size) {
    std::string buffer;
    read(buffer, size);
    return buffer;
}

///
/// @brief Reads byte-per-byte until the eol sequence, a read timeout or the size limit.
///
size_t serial::readline(std::string& buffer, size_t size, std::string_view eol) {
    const std::scoped_lock guard(mutex_);
    std::vector<std::uint8_t> tmp(size);
    size_t read_so_far = 0;
    while (read_so_far < size) {
        const size_t bytes_read = pimpl_->read(std::span(tmp).subspan(read_so_far, 1));
        read_so_far += bytes_read;
        if (bytes_read == 0) { break; } // Read timeout
        if (read_so_far >= eol.size()
            && std::string_view(
                   reinterpret_cast<const char*>(tmp.data() + read_so_far - eol.size()),
                   eol.size()
               ) == eol) {
            break; // End of line found
        }
    }
    buffer.append(reinterpret_cast<const char*>(tmp.data()), read_so_far);
    return read_so_far;
}

///
/// @brief Reads one line and returns it.
/// @note Delegates to the locking overload; must not lock the mutex itself.
///
std::string serial::readline(size_t size, std::string_view eol) {
    std::string buffer;
    readline(buffer, size, eol);
    return buffer;
}

///
/// @brief Reads lines until the read timeout expires or the size limit is reached.
///
std::vector<std::string> serial::readlines(size_t size, std::string_view eol) {
    const std::scoped_lock guard(mutex_);
    std::vector<std::string> lines;
    std::vector<std::uint8_t> tmp(size);
    size_t read_so_far = 0;
    size_t start_of_line = 0;

    // Turns the bytes in [start_of_line, read_so_far) into a line.
    const auto flush_line = [&]
    {
        if (start_of_line != read_so_far) {
            lines.emplace_back(
                reinterpret_cast<const char*>(tmp.data() + start_of_line),
                read_so_far - start_of_line
            );
        }
        start_of_line = read_so_far;
    };

    while (read_so_far < size) {
        const size_t bytes_read = pimpl_->read(std::span(tmp).subspan(read_so_far, 1));
        read_so_far += bytes_read;
        if (bytes_read == 0) { break; } // Read timeout, keep any partial line
        if (read_so_far >= eol.size()
            && std::string_view(
                   reinterpret_cast<const char*>(tmp.data() + read_so_far - eol.size()),
                   eol.size()
               ) == eol) {
            flush_line();
        }
    }
    flush_line(); // Trailing data without eol becomes the last line
    return lines;
}

///
/// @brief Writes raw bytes to the port, honouring the write timeout.
///
size_t serial::write(std::span<const std::uint8_t> data) {
    const std::scoped_lock guard(mutex_);
    return pimpl_->write(data);
}

///
/// @brief Writes a vector's bytes to the port.
/// @note Delegates to the locking overload; must not lock the mutex itself.
///
size_t serial::write(const std::vector<std::uint8_t>& data) {
    return write(std::span(data.data(), data.size()));
}

///
/// @brief Writes a string's bytes to the port.
/// @note Delegates to the locking overload; must not lock the mutex itself.
///
size_t serial::write(const std::string& data) {
    return write(std::span(reinterpret_cast<const std::uint8_t*>(data.data()), data.size()));
}

///
/// @brief Changes the port address, keeping the port open when it was open before.
///
void serial::set_port(const std::string& port) {
    const std::scoped_lock guard(mutex_);
    const bool was_open = pimpl_->is_open();
    pimpl_->close();
    pimpl_->set_port(port);
    if (was_open) { pimpl_->open(); }
}

///
/// @brief Returns the configured port address.
///
std::string serial::get_port() const {
    const std::scoped_lock guard(mutex_);
    return pimpl_->get_port();
}

///
/// @brief Sets the read and write timeouts.
///
void serial::set_timeout(const timeout& value) {
    const std::scoped_lock guard(mutex_);
    pimpl_->set_timeout(value);
}

///
/// @brief Returns the current timeout conditions.
///
timeout serial::get_timeout() const {
    const std::scoped_lock guard(mutex_);
    return pimpl_->get_timeout();
}

///
/// @brief Sets the baud rate, applying it to the hardware when the port is open.
///
void serial::set_baudrate(std::uint32_t baudrate) {
    const std::scoped_lock guard(mutex_);
    pimpl_->set_baudrate(baudrate);
}

///
/// @brief Returns the current baud rate.
///
std::uint32_t serial::get_baudrate() const {
    const std::scoped_lock guard(mutex_);
    return pimpl_->get_baudrate();
}

///
/// @brief Sets the data bit count, applying it to the hardware when the port is open.
///
void serial::set_data_bits(data_bits bytesize) {
    const std::scoped_lock guard(mutex_);
    pimpl_->set_data_bits(bytesize);
}

///
/// @brief Returns the current data bit count.
///
data_bits serial::get_data_bits() const {
    const std::scoped_lock guard(mutex_);
    return pimpl_->get_data_bits();
}

///
/// @brief Sets the parity method, applying it to the hardware when the port is open.
///
void serial::set_parity(parity parity) {
    const std::scoped_lock guard(mutex_);
    pimpl_->set_parity(parity);
}

///
/// @brief Returns the current parity method.
///
parity serial::get_parity() const {
    const std::scoped_lock guard(mutex_);
    return pimpl_->get_parity();
}

///
/// @brief Sets the stop bits, applying them to the hardware when the port is open.
///
void serial::set_stop_bits(stop_bits stopbits) {
    const std::scoped_lock guard(mutex_);
    pimpl_->set_stop_bits(stopbits);
}

///
/// @brief Returns the current stop bit setting.
///
stop_bits serial::get_stop_bits() const {
    const std::scoped_lock guard(mutex_);
    return pimpl_->get_stop_bits();
}

///
/// @brief Sets the flow control method, applying it when the port is open.
///
void serial::set_flow_ctrl(flow_ctrl flowcontrol) {
    const std::scoped_lock guard(mutex_);
    pimpl_->set_flow_ctrl(flowcontrol);
}

///
/// @brief Returns the current flow control method.
///
flow_ctrl serial::get_flow_ctrl() const {
    const std::scoped_lock guard(mutex_);
    return pimpl_->get_flow_ctrl();
}

///
/// @brief Waits until both transmit and receive buffers are empty.
///
void serial::flush() {
    const std::scoped_lock guard(mutex_);
    pimpl_->flush();
}

///
/// @brief Discards received but unread data.
///
void serial::flush_rx_buffer() {
    const std::scoped_lock guard(mutex_);
    pimpl_->flush_rx_buffer();
}

///
/// @brief Discards data queued for transmission but not yet sent.
///
void serial::flush_tx_buffer() {
    const std::scoped_lock guard(mutex_);
    pimpl_->flush_tx_buffer();
}

///
/// @brief Sends an RS-232 break signal for duration milliseconds.
///
void serial::send_break(int duration) {
    const std::scoped_lock guard(mutex_);
    pimpl_->send_break(duration);
}

///
/// @brief Drives the break condition to the given level.
///
void serial::set_break(bool level) {
    const std::scoped_lock guard(mutex_);
    pimpl_->set_break(level);
}

///
/// @brief Drives the RTS line to the given level.
///
void serial::set_rts(bool level) {
    const std::scoped_lock guard(mutex_);
    pimpl_->set_rts(level);
}

///
/// @brief Drives the DTR line to the given level.
///
void serial::set_dtr(bool level) {
    const std::scoped_lock guard(mutex_);
    pimpl_->set_dtr(level);
}

///
/// @brief Blocks until one of the CTS, DSR, RI, CD modem lines changes state.
///
bool serial::wait_for_change() {
    const std::scoped_lock guard(mutex_);
    return pimpl_->wait_for_change();
}

///
/// @brief Returns the current state of the CTS modem line.
///
bool serial::get_cts() {
    const std::scoped_lock guard(mutex_);
    return pimpl_->get_cts();
}

///
/// @brief Returns the current state of the DSR modem line.
///
bool serial::get_dsr() {
    const std::scoped_lock guard(mutex_);
    return pimpl_->get_dsr();
}

///
/// @brief Returns the current state of the RI modem line.
///
bool serial::get_ri() {
    const std::scoped_lock guard(mutex_);
    return pimpl_->get_ri();
}

///
/// @brief Returns the current state of the CD modem line.
///
bool serial::get_cd() {
    const std::scoped_lock guard(mutex_);
    return pimpl_->get_cd();
}

} // namespace ba7lya::serial
