///
/// @file list_ports_linux.cxx
/// @author BA7LYA (1042140025@qq.com)
/// @brief Enumerates the serial ports on Linux by globbing /dev and reading sysfs metadata.
/// @version 0.2
/// @date 2026-09-14
/// @copyright Copyright (c) 2026
///

#include <glob.h>

#include <cstdlib>
#include <fstream>
#include <array>
#include <memory>
#include <string>
#include <string_view>
#include <sys/stat.h>
#include <unistd.h>
#include <utility>
#include <vector>

#include "serial.hxx"

namespace ba7lya::serial {

namespace {

///
/// @brief Expands a set of shell glob patterns into the matching paths.
/// @param patterns Glob patterns, e.g. "/dev/ttyUSB*".
/// @return All matched paths, in glob order.
///
std::vector<std::string> glob(const std::vector<std::string>& patterns) {
    std::vector<std::string> found;
    if (patterns.empty()) { return found; }

    glob_t results{};
    int retval = ::glob(patterns[0].c_str(), 0, nullptr, &results);
    for (auto it = patterns.begin() + 1; it != patterns.end(); ++it) {
        retval = ::glob(it->c_str(), GLOB_APPEND, nullptr, &results);
    }
    if (retval == 0 || retval == GLOB_NOMATCH) {
        found.assign(results.gl_pathv, results.gl_pathv + results.gl_pathc);
    }
    ::globfree(&results);
    return found;
}

///
/// @brief Returns the last path component.
/// @param path A filesystem path.
/// @return The file or directory name after the final slash.
///
std::string basename(std::string_view path) {
    const size_t pos = path.rfind('/');
    return pos == std::string_view::npos ? std::string(path) : std::string(path.substr(pos + 1));
}

///
/// @brief Returns the parent directory path.
/// @param path A filesystem path.
/// @return Everything before the final slash.
///
std::string dirname(std::string_view path) {
    const size_t pos = path.rfind('/');
    if (pos == std::string_view::npos) { return std::string(path); }
    if (pos == 0) { return "/"; }
    return std::string(path.substr(0, pos));
}

///
/// @brief Tests whether a filesystem path exists.
/// @param path The path to stat.
/// @return True when the path resolves.
///
bool path_exists(const std::string& path) {
    struct stat sb;
    return ::stat(path.c_str(), &sb) == 0;
}

///
/// @brief Resolves a path through any symlinks.
/// @param path The path to resolve.
/// @return The canonical path, or an empty string on failure.
///
std::string realpath(const std::string& path) {
    std::unique_ptr<char, decltype(&std::free)> resolved(
        ::realpath(path.c_str(), nullptr), &std::free
    );
    return resolved ? std::string(resolved.get()) : std::string();
}

///
/// @brief Reads the first line of a text file.
/// @param file The file path.
/// @return The line, or an empty string when the file is unreadable.
///
std::string read_line(const std::string& file) {
    std::ifstream ifs(file);
    std::string line;
    if (ifs) { std::getline(ifs, line); }
    return line;
}

///
/// @brief Builds a USB friendly name from the manufacturer, product and serial attributes.
/// @param sys_usb_path The sysfs directory of the USB device.
/// @return The composed name, empty when no attribute is present.
///
std::string usb_sysfs_friendly_name(const std::string& sys_usb_path) {
    const std::string manufacturer = read_line(sys_usb_path + "/manufacturer");
    const std::string product = read_line(sys_usb_path + "/product");
    const std::string serial = read_line(sys_usb_path + "/serial");
    if (manufacturer.empty() && product.empty() && serial.empty()) { return {}; }
    return manufacturer + " " + product + " " + serial;
}

///
/// @brief Builds the "USB VID:PID=... SNR=..." hardware identifier.
/// @param sysfs_path The sysfs directory carrying idVendor/idProduct/serial.
/// @return The hardware id string.
///
std::string usb_sysfs_hw_string(const std::string& sysfs_path) {
    const std::string serial = read_line(sysfs_path + "/serial");
    const std::string vid = read_line(sysfs_path + "/idVendor");
    const std::string pid = read_line(sysfs_path + "/idProduct");
    std::string hw = "USB VID:PID=" + vid + ":" + pid;
    if (!serial.empty()) { hw += " SNR=" + serial; }
    return hw;
}

///
/// @brief Resolves the friendly name and hardware id for one device node via sysfs.
/// @param device_path The /dev path, e.g. /dev/ttyUSB0.
/// @return A {friendly_name, hardware_id} pair.
///
std::pair<std::string, std::string> get_sysfs_info(const std::string& device_path) {
    const std::string device_name = basename(device_path);
    std::string friendly_name;
    std::string hardware_id;
    const std::string sys_device_path = "/sys/class/tty/" + device_name + "/device";

    if (device_name.starts_with("ttyUSB")) {
        const std::string dir = dirname(dirname(realpath(sys_device_path)));
        if (path_exists(dir)) {
            friendly_name = usb_sysfs_friendly_name(dir);
            hardware_id = usb_sysfs_hw_string(dir);
        }
    }
    else if (device_name.starts_with("ttyACM")) {
        const std::string dir = dirname(realpath(sys_device_path));
        if (path_exists(dir)) {
            friendly_name = usb_sysfs_friendly_name(dir);
            hardware_id = usb_sysfs_hw_string(dir);
        }
    }
    else {
        // PCI and on-board UARTs expose a plain id file.
        const std::string sys_id_path = sys_device_path + "/id";
        if (path_exists(sys_id_path)) { hardware_id = read_line(sys_id_path); }
    }

    if (friendly_name.empty()) { friendly_name = device_name; }
    if (hardware_id.empty()) { hardware_id = "n/a"; }
    return {friendly_name, hardware_id};
}

} // namespace

///
/// @brief Lists the serial ports present on the system.
/// @return One serial_port_info per matching /dev node.
///
std::vector<serial_port_info> list_ports() {
    static constexpr std::array<std::string_view, 6> patterns{{
        "/dev/ttyACM*", "/dev/ttyS*", "/dev/ttyUSB*",
        "/dev/tty.*",   "/dev/cu.*", "/dev/rfcomm*",
    }};

    std::vector<serial_port_info> results;
    for (const std::string& device : glob({patterns.begin(), patterns.end()})) {
        const auto [friendly_name, hardware_id] = get_sysfs_info(device);
        results.push_back({device, friendly_name, hardware_id});
    }
    return results;
}

} // namespace ba7lya::serial
