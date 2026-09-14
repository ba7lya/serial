///
/// @file exception.hxx
/// @author BA7LYA (1042140025@qq.com)
/// @brief Exception types thrown by the serial port library.
/// @version 0.2
/// @date 2026-09-14
/// @copyright Copyright (c) 2026 BA7LYA
/// @license SPDX-License-Identifier: MIT
///

#pragma once

#include <stdexcept>
#include <string>
#include <system_error>

namespace ba7lya::serial {

///
/// @brief General library error that does not originate from an OS error code.
///
class serial_exception : public std::runtime_error {
public:
    ///
    /// @brief Constructs the exception with the library prefix and a description.
    /// @param description What operation failed and why.
    ///
    explicit serial_exception(const std::string& description)
        : std::runtime_error("serial_exception: " + description + " failed.") {}
};

///
/// @brief I/O failure, either mapped from an OS error code or purely logical.
///
class io_exception : public std::system_error {
public:
    ///
    /// @brief Wraps an OS error code with the default context.
    /// @param code The underlying error code (Windows GetLastError or errno based).
    ///
    explicit io_exception(std::error_code code)
        : std::system_error(code, "I/O operation failed") {}

    ///
    /// @brief Wraps an OS error code with additional context.
    /// @param code The underlying error code.
    /// @param context Human readable description of the failed operation.
    ///
    io_exception(std::error_code code, const std::string& context)
        : std::system_error(code, context) {}

    ///
    /// @brief Builds a pure logical I/O error without an OS error code.
    /// @param message Human readable description of the failure.
    ///
    explicit io_exception(const std::string& message)
        : std::system_error(std::make_error_code(std::errc::io_error), message) {}
};

///
/// @brief Thrown when an operation requires the port to be open but it is not.
///
class port_not_open_exception : public serial_exception {
public:
    ///
    /// @brief Constructs the exception naming the operation that needs an open port.
    /// @param operation Name of the failing operation, e.g. "serial::read".
    ///
    explicit port_not_open_exception(const std::string& operation)
        : serial_exception(operation + ": port is not open") {}
};

} // namespace ba7lya::serial
