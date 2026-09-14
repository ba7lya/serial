///
/// @file example_vk162.cxx
/// @author BA7LYA (1042140025@qq.com)
/// @brief Example: stream NMEA sentences from a VK-162 GPS receiver to the console.
/// @version 0.2
/// @date 2026-09-14
/// @copyright Copyright (c) 2026 BA7LYA
/// @license SPDX-License-Identifier: MIT
///

#include <iostream>

#include "serial.hxx"

///
/// @brief Opens the GPS port and prints one NMEA line at a time until interrupted.
/// @return -1 when the port cannot be opened.
///
int main() {
    using namespace ba7lya::serial;

    // The VK-162 speaks NMEA at 9600 baud, 8N1.
    serial serial("COM8", 9600);
    if (!serial.is_open()) {
        std::cerr << "Failed to open the GPS port." << std::endl;
        return -1;
    }

    std::string line;
    for (;;) {
        line.clear();
        if (serial.readline(line, 256, "\n") > 0) { std::cout << line << std::flush; }
    }
}
