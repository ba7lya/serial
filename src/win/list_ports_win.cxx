///
/// @file list_ports_win.cxx
/// @author BA7LYA (1042140025@qq.com)
/// @brief Enumerates the serial ports present on the system via the SetupAPI.
/// @version 0.2
/// @date 2026-09-14
/// @copyright Copyright (c) 2026
///

#include <array>
#include <cwchar>
#include <string>
#include <vector>

// clang-format off
#include <windows.h> // must precede initguid/devguid/setupapi
#include <initguid.h> // must precede devguid.h to instantiate the GUIDs
#include <devguid.h>
#include <setupapi.h>
// clang-format on

#include "log.hxx"
#include "serial.hxx"

namespace ba7lya::serial {

namespace {

constexpr size_t registry_buffer_chars = 256;

///
/// @brief Converts a UTF-16 string view to a UTF-8 std::string.
/// @param text Wide input, may be empty.
/// @return The converted string, empty when conversion is not possible.
///
std::string utf8_encode(std::wstring_view text) {
    if (text.empty()) { return {}; }
    const int count = ::WideCharToMultiByte(
        CP_UTF8,
        0,
        text.data(),
        static_cast<int>(text.size()),
        nullptr,
        0,
        nullptr,
        nullptr
    );
    if (count <= 0) { return {}; }
    std::string result(static_cast<size_t>(count), '\0');
    ::WideCharToMultiByte(
        CP_UTF8,
        0,
        text.data(),
        static_cast<int>(text.size()),
        result.data(),
        count,
        nullptr,
        nullptr
    );
    return result;
}

///
/// @brief Reads the "PortName" value from a device's DEV registry key.
/// @param device_info_set The device information set being enumerated.
/// @param device_data The current device element.
/// @return The port name as UTF-8, empty when absent or unreadable.
///
std::string get_port_name(HDEVINFO device_info_set, SP_DEVINFO_DATA& device_data) {
    std::array<wchar_t, registry_buffer_chars> buffer {};
    DWORD size_bytes = sizeof(buffer);

    HKEY hkey = ::SetupDiOpenDevRegKey(
        device_info_set,
        &device_data,
        DICS_FLAG_GLOBAL,
        0,
        DIREG_DEV,
        KEY_READ
    );
    if (hkey == INVALID_HANDLE_VALUE) { return {}; }

    const bool found = ::RegQueryValueExW(
                           hkey,
                           L"PortName",
                           nullptr,
                           nullptr,
                           reinterpret_cast<LPBYTE>(buffer.data()),
                           &size_bytes
                       )
                    == ERROR_SUCCESS;
    ::RegCloseKey(hkey);

    if (!found || size_bytes < sizeof(wchar_t)) { return {}; }
    const size_t chars = (size_bytes / sizeof(wchar_t)) - 1; // drop the terminating NUL
    return utf8_encode({ buffer.data(), chars });
}

///
/// @brief Reads a device registry property as a string.
/// @param device_info_set The device information set being enumerated.
/// @param device_data The current device element.
/// @param property SPDRP_FRIENDLYNAME or SPDRP_HARDWAREID.
/// @return The property as UTF-8, empty when absent.
///
std::string get_device_property(
    HDEVINFO device_info_set,
    SP_DEVINFO_DATA& device_data,
    DWORD property
) {
    std::array<wchar_t, registry_buffer_chars> buffer {};
    DWORD type = 0;
    DWORD size_bytes = 0;

    const BOOL found = ::SetupDiGetDeviceRegistryPropertyW(
        device_info_set,
        &device_data,
        property,
        &type,
        reinterpret_cast<PBYTE>(buffer.data()),
        static_cast<DWORD>(sizeof(buffer)),
        &size_bytes
    );

    if (!found || size_bytes <= sizeof(wchar_t)) { return {}; }
    // SPDRP_HARDWAREID is REG_MULTI_SZ: keep only the first, NUL-terminated element.
    const size_t max_chars = (size_bytes / sizeof(wchar_t)) - 1;
    const size_t chars = wcsnlen(buffer.data(), max_chars);
    return utf8_encode({ buffer.data(), chars });
}

} // namespace

///
/// @brief Lists all present serial ports by walking the ports device class.
/// @return One serial_port_info per port, parallel (LPT) ports excluded.
///
std::vector<serial_port_info> list_ports() {
    std::vector<serial_port_info> ports;

    HDEVINFO device_info_set
        = ::SetupDiGetClassDevs(&GUID_DEVCLASS_PORTS, nullptr, nullptr, DIGCF_PRESENT);
    if (device_info_set == INVALID_HANDLE_VALUE) { return ports; }

    SP_DEVINFO_DATA device_data {};
    device_data.cbSize = sizeof(device_data);

    for (DWORD index = 0; ::SetupDiEnumDeviceInfo(device_info_set, index, &device_data); ++index) {
        const std::string name = get_port_name(device_info_set, device_data);

        // Ignore parallel ports and entries without a usable name.
        if (name.empty() || name.starts_with("LPT")) { continue; }

        serial_port_info port;
        port.name = name;
        port.description = get_device_property(device_info_set, device_data, SPDRP_FRIENDLYNAME);
        port.hardware_id = get_device_property(device_info_set, device_data, SPDRP_HARDWAREID);
        if (port.hardware_id.empty()) { port.hardware_id = "n/a"; }
        ports.push_back(std::move(port));
    }

    ::SetupDiDestroyDeviceInfoList(device_info_set);
    LOG_INFO("enumerated {} serial ports", ports.size());
    return ports;
}

} // namespace ba7lya::serial
