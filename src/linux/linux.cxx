///
/// @file linux.cxx
/// @author BA7LYA (1042140025@qq.com)
/// @brief Linux implementation of the serial class, based on termios and pselect.
/// @version 0.2
/// @date 2026-09-14
/// @copyright Copyright (c) 2026
///

#include "linux.hxx"

#include <algorithm>
#include <array>
#include <cerrno>
#include <cstdint>
#include <cstring>
#include <fcntl.h>
#include <linux/serial.h>
#include <string>
#include <sys/ioctl.h>
#include <sys/select.h>
#include <termios.h>
#include <thread>
#include <unistd.h>
#include <utility>
#include <vector>

#include "log.hxx"

namespace ba7lya::serial {

namespace {

///
/// @brief Builds a timespec for a millisecond duration, for pselect.
/// @param millis Duration in milliseconds, negative values clamp to zero.
/// @return The equivalent timespec.
///
timespec to_timespec(std::int64_t millis) {
    millis = std::max<std::int64_t>(millis, 0);
    return timespec {
        .tv_sec = static_cast<time_t>(millis / 1000),
        .tv_nsec = static_cast<long>(millis % 1000) * 1000000L,
    };
}

///
/// @brief Maps a numeric baud rate onto its termios speed constant.
/// @param baudrate Speed in bits per second.
/// @param out Receives the matching speed_t.
/// @return True when the rate is one of the standard speeds.
///
bool standard_speed(std::uint32_t baudrate, speed_t& out) {
    static constexpr std::array<std::pair<std::uint32_t, speed_t>, 31> table {
        {
         { 0, B0 },
         { 50, B50 },
         { 75, B75 },
         { 110, B110 },
         { 134, B134 },
         { 150, B150 },
         { 200, B200 },
         { 300, B300 },
         { 600, B600 },
         { 1200, B1200 },
         { 1800, B1800 },
         { 2400, B2400 },
         { 4800, B4800 },
         { 9600, B9600 },
         { 19200, B19200 },
         { 38400, B38400 },
         { 57600, B57600 },
         { 115200, B115200 },
         { 230400, B230400 },
         { 460800, B460800 },
         { 500000, B500000 },
         { 576000, B576000 },
         { 921600, B921600 },
         { 1000000, B1000000 },
         { 1152000, B1152000 },
         { 1500000, B1500000 },
         { 2000000, B2000000 },
         { 2500000, B2500000 },
         { 3000000, B3000000 },
         { 3500000, B3500000 },
         { 4000000, B4000000 },
         }
    };

    for (const auto& [value, speed] : table) {
        if (baudrate == value) {
            out = speed;
            return true;
        }
    }
    return false;
}

} // namespace

///
/// @brief Stores the configuration and opens the port when a path was given.
///
serial::impl::impl(
    std::string port,
    std::uint32_t baudrate,
    data_bits bytesize,
    parity parity,
    stop_bits stopbits,
    flow_ctrl flowcontrol
)
    : port_(std::move(port))
    , baudrate_(baudrate)
    , parity_(parity)
    , bytesize_(bytesize)
    , stopbits_(stopbits)
    , flowcontrol_(flowcontrol) {
    update_byte_time();
    if (!port_.empty()) { open(); }
}

///
/// @brief Releases the file descriptor.
///
serial::impl::~impl() {
    try {
        close();
    }
    // NOLINTNEXTLINE(bugprone-empty-catch) -- destructors must not throw
    catch (...) {}
}

///
/// @brief Throws unless the port is open.
///
void serial::impl::require_open(std::string_view operation) const {
    if (!is_open_) { throw port_not_open_exception(std::string(operation)); }
}

///
/// @brief Throws an io_exception describing the current errno.
///
void serial::impl::throw_errno(std::string_view context) {
    const std::error_code code(errno, std::system_category());
    LOG_ERROR("{}: {}", context, code.message());
    throw io_exception(code, context.empty() ? code.message() : std::string(context));
}

///
/// @brief Opens the device read/write and non-blocking, then applies the configuration.
///
void serial::impl::open() {
    if (port_.empty()) { throw std::invalid_argument("Empty port is invalid."); }
    if (is_open_) { throw serial_exception("serial port already open."); }

    do { fd_ = ::open(port_.c_str(), O_RDWR | O_NOCTTY | O_NONBLOCK); }
    while (fd_ == -1 && errno == EINTR); // Recoverable, retry.
    if (fd_ == -1) {
        if (errno == ENFILE || errno == EMFILE) {
            LOG_ERROR("too many file handles to open {}", port_);
            throw io_exception("Too many file handles open.");
        }
        throw_errno("Error opening serial port");
    }

    try {
        reconfigure();
    } catch (...) {
        ::close(fd_);
        fd_ = -1;
        throw;
    }
    is_open_ = true;
    LOG_INFO("opened {}", port_);
}

///
/// @brief Writes all stored framing settings into the termios structure.
///
void serial::impl::reconfigure() {
    if (fd_ == -1) { throw io_exception("Invalid file descriptor, is the serial port open?"); }

    termios options {};
    if (::tcgetattr(fd_, &options) == -1) { throw_errno("tcgetattr"); }

    // Raw mode: local line, receiver enabled, no echo/canonical/signal/output processing.
    options.c_cflag |= static_cast<tcflag_t>(CLOCAL | CREAD);
    options.c_lflag
        &= static_cast<tcflag_t>(~(ICANON | ECHO | ECHOE | ECHOK | ECHONL | ISIG | IEXTEN));
    options.c_oflag &= static_cast<tcflag_t>(~OPOST);
    options.c_iflag &= static_cast<tcflag_t>(~(INLCR | IGNCR | ICRNL | IGNBRK | IUCLC | PARMRK));

    LOG_DEBUG("reconfigure {} to {} baud", port_, baudrate_);

    // Baud rate; custom rates fall through to the TIOCSSERIAL ioctl below.
    speed_t speed = 0;
    const bool standard = standard_speed(baudrate_, speed);
    if (standard && ::cfsetspeed(&options, speed) == -1) { throw_errno("cfsetspeed"); }

    // Data bits
    options.c_cflag &= static_cast<tcflag_t>(~CSIZE);
    switch (bytesize_) {
    case data_bits::five:  options.c_cflag |= CS5; break;
    case data_bits::six:   options.c_cflag |= CS6; break;
    case data_bits::seven: options.c_cflag |= CS7; break;
    case data_bits::eight: options.c_cflag |= CS8; break;
    }

    // Stop bits — the kernel has no 1.5, so it is handled as two.
    if (stopbits_ == stop_bits::one) { options.c_cflag &= static_cast<tcflag_t>(~CSTOPB); }
    else { options.c_cflag |= CSTOPB; }

    // Parity (Linux supports mark and space through CMSPAR)
    options.c_iflag &= static_cast<tcflag_t>(~(INPCK | ISTRIP));
    switch (parity_) {
    case parity::none: options.c_cflag &= static_cast<tcflag_t>(~(PARENB | PARODD | CMSPAR)); break;
    case parity::even:
        options.c_cflag = static_cast<tcflag_t>((options.c_cflag & ~(PARODD | CMSPAR)) | PARENB);
        break;
    case parity::odd:
        options.c_cflag = static_cast<tcflag_t>((options.c_cflag & ~CMSPAR) | (PARENB | PARODD));
        break;
    case parity::mark: options.c_cflag |= (PARENB | CMSPAR | PARODD); break;
    case parity::space:
        options.c_cflag = static_cast<tcflag_t>((options.c_cflag | (PARENB | CMSPAR)) & ~PARODD);
        break;
    }

    // Flow control: XON/XOFF in the iflag bits, RTS/CTS in the cflag bits.
    if (flowcontrol_ == flow_ctrl::software) { options.c_iflag |= (IXON | IXOFF); }
    else { options.c_iflag &= static_cast<tcflag_t>(~(IXON | IXOFF | IXANY)); }
    if (flowcontrol_ == flow_ctrl::hardware) { options.c_cflag |= CRTSCTS; }
    else { options.c_cflag &= static_cast<tcflag_t>(~CRTSCTS); }

    // VMIN/VTIME unused: pselect guarantees data before each read.
    options.c_cc[VMIN] = 0;
    options.c_cc[VTIME] = 0;

    if (::tcsetattr(fd_, TCSANOW, &options) == -1) { throw_errno("tcsetattr"); }

    // Non-standard baud rates through the async driver's custom divisor.
    if (!standard) {
        serial_struct ser {};
        if (::ioctl(fd_, TIOCGSERIAL, &ser) == -1) { throw_errno("TIOCGSERIAL"); }
        ser.custom_divisor = ser.baud_base / static_cast<int>(baudrate_);
        ser.flags = static_cast<int>((ser.flags & ~ASYNC_SPD_MASK) | ASYNC_SPD_CUST);
        if (::ioctl(fd_, TIOCSSERIAL, &ser) == -1) { throw_errno("TIOCSSERIAL"); }
    }

    update_byte_time();
}

///
/// @brief Recomputes the transmission time of one byte from the current framing.
///
void serial::impl::update_byte_time() {
    if (baudrate_ == 0) {
        byte_time_ns_ = 0;
        return;
    }
    // One start bit, the data bits, optional parity, plus the stop bits.
    const double bit_time_ns = 1e9 / baudrate_;
    byte_time_ns_ = static_cast<std::uint32_t>(
        bit_time_ns
        * (1.0 + static_cast<int>(bytesize_) + static_cast<double>(parity_ != parity::none)
           + stop_bits_count(stopbits_))
    );
}

///
/// @brief Closes the descriptor if the port is open.
///
void serial::impl::close() {
    if (!is_open_) { return; }
    if (fd_ != -1 && ::close(fd_) != 0) {
        fd_ = -1;
        is_open_ = false;
        throw_errno("Error closing serial port");
    }
    fd_ = -1;
    is_open_ = false;
    LOG_DEBUG("closed {}", port_);
}

///
/// @brief Reports whether the port is open.
///
bool serial::impl::is_open() const { return is_open_; }

///
/// @brief Asks the kernel how many bytes are queued for reading.
///
size_t serial::impl::available() const {
    if (!is_open_) { return 0; }
    int count = 0;
    if (::ioctl(fd_, TIOCINQ, &count) == -1) { throw_errno("ioctl(TIOCINQ)"); }
    return static_cast<size_t>(count);
}

///
/// @brief Blocks in pselect until the descriptor is readable or the timeout expires.
///
bool serial::impl::wait_readable(std::uint32_t timeout) {
    require_open("serial::wait_readable");

    fd_set readfds;
    FD_ZERO(&readfds);
    FD_SET(fd_, &readfds);

    const timespec deadline = to_timespec(timeout);
    const int ready = ::pselect(fd_ + 1, &readfds, nullptr, nullptr, &deadline, nullptr);

    if (ready < 0) {
        if (errno == EINTR) { return false; } // Interrupted, not an error.
        throw_errno("pselect");
    }
    return ready > 0 && FD_ISSET(fd_, &readfds);
}

///
/// @brief Sleeps for the time needed to transmit count bytes at the current settings.
///
void serial::impl::wait_byte_times(size_t count) const {
    std::this_thread::sleep_for(
        std::chrono::nanoseconds(static_cast<std::uint64_t>(byte_time_ns_) * count)
    );
}

///
/// @brief Reads until the buffer is full or the total / inter-byte timeouts expire.
///
size_t serial::impl::read(std::span<std::uint8_t> buf) {
    require_open("serial::read");

    // Total budget: constant + multiplier * requested bytes.
    const std::int64_t total_ms = static_cast<std::int64_t>(timeout_.read_timeout_constant)
                                + (static_cast<std::int64_t>(timeout_.read_timeout_multiplier)
                                   * static_cast<std::int64_t>(buf.size()));
    const millisecond_timer total_timeout(
        static_cast<std::uint32_t>(std::max<std::int64_t>(total_ms, 0))
    );

    size_t bytes_read = 0;
    while (bytes_read < buf.size()) {
        const std::int64_t remaining = total_timeout.remaining();
        if (remaining <= 0) {
            LOG_DEBUG("read timed out on {} after {} bytes", port_, bytes_read);
            break;
        }

        // Wait at most the smaller of the remaining total time and the inter-byte time.
        const auto step = std::min<std::uint32_t>(
            static_cast<std::uint32_t>(remaining),
            timeout_.inter_byte_timeout
        );
        if (!wait_readable(step)) { continue; }

        const ssize_t count = ::read(fd_, buf.data() + bytes_read, buf.size() - bytes_read);
        if (count < 0) {
            if (errno == EINTR || errno == EAGAIN) { continue; }
            throw_errno("Error while reading from the serial port");
        }
        if (count == 0) { break; }
        bytes_read += static_cast<size_t>(count);
    }
    LOG_TRACE("read {} bytes from {}", bytes_read, port_);
    return bytes_read;
}

///
/// @brief Writes until all bytes are sent or the write timeout expires.
///
size_t serial::impl::write(std::span<const std::uint8_t> data) {
    require_open("serial::write");

    const std::int64_t total_ms = static_cast<std::int64_t>(timeout_.write_timeout_constant)
                                + (static_cast<std::int64_t>(timeout_.write_timeout_multiplier)
                                   * static_cast<std::int64_t>(data.size()));
    const millisecond_timer total_timeout(
        static_cast<std::uint32_t>(std::max<std::int64_t>(total_ms, 0))
    );

    size_t bytes_written = 0;
    while (bytes_written < data.size()) {
        fd_set writefds;
        FD_ZERO(&writefds);
        FD_SET(fd_, &writefds);

        const timespec deadline = to_timespec(total_timeout.remaining());
        const int ready = ::pselect(fd_ + 1, nullptr, &writefds, nullptr, &deadline, nullptr);
        if (ready < 0) {
            if (errno == EINTR) { continue; }
            throw_errno("pselect");
        }
        if (ready == 0) {
            LOG_DEBUG("write timed out on {} after {} bytes", port_, bytes_written);
            break;
        }

        const ssize_t count
            = ::write(fd_, data.data() + bytes_written, data.size() - bytes_written);
        if (count < 0) {
            if (errno == EINTR || errno == EAGAIN) { continue; }
            throw_errno("Error while writing to the serial port");
        }
        bytes_written += static_cast<size_t>(count);
    }
    LOG_TRACE("wrote {} bytes to {}", bytes_written, port_);
    return bytes_written;
}

///
/// @brief Waits for the kernel output queue to drain.
///
void serial::impl::flush() {
    require_open("serial::flush");
    ::tcdrain(fd_);
}

///
/// @brief Discards unread input.
///
void serial::impl::flush_rx_buffer() {
    require_open("serial::flush_rx_buffer");
    ::tcflush(fd_, TCIFLUSH);
}

///
/// @brief Discards queued output.
///
void serial::impl::flush_tx_buffer() {
    require_open("serial::flush_tx_buffer");
    ::tcflush(fd_, TCOFLUSH);
}

///
/// @brief Sends a break signal; duration is converted to tcsendbreak's scale (0.25 s units).
///
void serial::impl::send_break(int duration) {
    require_open("serial::send_break");
    // NOLINTNEXTLINE(concurrency-mt-unsafe) -- port handle is mutex-serialized
    // by the facade; the underlying termios call is safe for this fd
    ::tcsendbreak(fd_, duration / 4);
}

///
/// @brief Drives the break condition.
///
void serial::impl::set_break(bool level) {
    require_open("serial::set_break");
    if (::ioctl(fd_, level ? TIOCSBRK : TIOCCBRK) == -1) { throw_errno("set_break"); }
}

///
/// @brief Drives the RTS line.
///
void serial::impl::set_rts(bool level) {
    require_open("serial::set_rts");
    int command = TIOCM_RTS;
    if (::ioctl(fd_, level ? TIOCMBIS : TIOCMBIC, &command) == -1) { throw_errno("set_rts"); }
}

///
/// @brief Drives the DTR line.
///
void serial::impl::set_dtr(bool level) {
    require_open("serial::set_dtr");
    int command = TIOCM_DTR;
    if (::ioctl(fd_, level ? TIOCMBIS : TIOCMBIC, &command) == -1) { throw_errno("set_dtr"); }
}

///
/// @brief Reads a modem status bit through TIOCMGET.
///
bool serial::impl::modem_line(int mask) const {
    int status = 0;
    if (::ioctl(fd_, TIOCMGET, &status) == -1) { throw_errno("ioctl(TIOCMGET)"); }
    return (status & mask) != 0;
}

///
/// @brief Blocks on the kernel's TIOCMIWAIT until a modem line changes state.
///
bool serial::impl::wait_for_change() {
    require_open("serial::wait_for_change");
    int command = TIOCM_CTS | TIOCM_DSR | TIOCM_RI | TIOCM_CD;
    if (::ioctl(fd_, TIOCMIWAIT, &command) == -1) { throw_errno("ioctl(TIOCMIWAIT)"); }
    return true;
}

/// @brief Returns the CTS line state.
bool serial::impl::get_cts() {
    require_open("serial::get_cts");
    return modem_line(TIOCM_CTS);
}

/// @brief Returns the DSR line state.
bool serial::impl::get_dsr() {
    require_open("serial::get_dsr");
    return modem_line(TIOCM_DSR);
}

/// @brief Returns the ring indicator line state.
bool serial::impl::get_ri() {
    require_open("serial::get_ri");
    return modem_line(TIOCM_RI);
}

/// @brief Returns the carrier detect line state.
bool serial::impl::get_cd() {
    require_open("serial::get_cd");
    return modem_line(TIOCM_CD);
}

/// @brief Stores the device path.
void serial::impl::set_port(const std::string& port) { port_ = port; }

/// @brief Returns the stored device path.
std::string serial::impl::get_port() const { return port_; }

/// @brief Stores the timeouts; enforced by read/write, so no driver call is needed.
void serial::impl::set_timeout(const timeout& value) { timeout_ = value; }

/// @brief Returns the stored timeouts.
timeout serial::impl::get_timeout() const { return timeout_; }

/// @brief Stores the baud rate and applies it when the port is open.
void serial::impl::set_baudrate(std::uint32_t baudrate) {
    baudrate_ = baudrate;
    if (is_open_) { reconfigure(); }
    else { update_byte_time(); }
}

/// @brief Returns the stored baud rate.
std::uint32_t serial::impl::get_baudrate() const { return baudrate_; }

/// @brief Stores the data bits and applies them when the port is open.
void serial::impl::set_data_bits(data_bits bytesize) {
    bytesize_ = bytesize;
    if (is_open_) { reconfigure(); }
    else { update_byte_time(); }
}

/// @brief Returns the stored data bits.
data_bits serial::impl::get_data_bits() const { return bytesize_; }

/// @brief Stores the parity and applies it when the port is open.
void serial::impl::set_parity(parity parity) {
    parity_ = parity;
    if (is_open_) { reconfigure(); }
    else { update_byte_time(); }
}

/// @brief Returns the stored parity.
parity serial::impl::get_parity() const { return parity_; }

/// @brief Stores the stop bits and applies them when the port is open.
void serial::impl::set_stop_bits(stop_bits stopbits) {
    stopbits_ = stopbits;
    if (is_open_) { reconfigure(); }
    else { update_byte_time(); }
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
