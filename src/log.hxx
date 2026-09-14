///
/// @file log.hxx
/// @author BA7LYA (1042140025@qq.com)
/// @brief Internal logging facade: LOG_* maps to the spdlog macro family when
/// compiled with BA7LYA_SERIAL_HAVE_SPDLOG, otherwise expands to nothing.
/// @version 0.2
/// @date 2026-09-14
/// @copyright Copyright (c) 2026 BA7LYA
/// @license SPDX-License-Identifier: MIT
///

#pragma once

///
/// @note Define BA7LYA_SERIAL_HAVE_SPDLOG (the CMake option
/// ba7lya.serial_ENABLE_LOG does this) and link spdlog to compile the log
/// calls in; the fmt-style arguments are then evaluated lazily by spdlog,
/// subject to its own runtime level filter. When the option is off the macros
/// discard their arguments entirely, so arguments must not carry side
/// effects. Swap spdlog for another backend by editing only this header.
///

#ifdef BA7LYA_SERIAL_HAVE_SPDLOG

// Keep the trace/debug macros alive; spdlog prunes calls below this level at
// compile time, so raise or lower here to trade verbosity for code size.
#define SPDLOG_ACTIVE_LEVEL SPDLOG_LEVEL_TRACE
#include <spdlog/spdlog.h>

// NOLINTBEGIN(cppcoreguidelines-macro-usage) -- must wrap the spdlog macro
// family to keep lazy formatting and compile-time level pruning.
#define LOG_TRACE(...)    SPDLOG_TRACE(__VA_ARGS__)
#define LOG_DEBUG(...)    SPDLOG_DEBUG(__VA_ARGS__)
#define LOG_INFO(...)     SPDLOG_INFO(__VA_ARGS__)
#define LOG_WARN(...)     SPDLOG_WARN(__VA_ARGS__)
#define LOG_ERROR(...)    SPDLOG_ERROR(__VA_ARGS__)
#define LOG_CRITICAL(...) SPDLOG_CRITICAL(__VA_ARGS__)
// NOLINTEND(cppcoreguidelines-macro-usage)

#else

// static_cast<void>(0) evaluates nothing (arguments must be side-effect free,
// see above), parses in every statement position and is the no-op form the
// C++ core guidelines endorse.
// NOLINTBEGIN(cppcoreguidelines-macro-usage) -- a logging facade must be
// switchable at compile time; constexpr templates cannot vanish together
// with the backend dependency.
#define LOG_TRACE(...)    static_cast<void>(0)
#define LOG_DEBUG(...)    static_cast<void>(0)
#define LOG_INFO(...)     static_cast<void>(0)
#define LOG_WARN(...)     static_cast<void>(0)
#define LOG_ERROR(...)    static_cast<void>(0)
#define LOG_CRITICAL(...) static_cast<void>(0)
// NOLINTEND(cppcoreguidelines-macro-usage)

#endif
