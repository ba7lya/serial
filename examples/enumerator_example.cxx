///
/// @file enumerator_example.cxx
/// @author BA7LYA (1042140025@qq.com)
/// @brief Example: print every serial port detected on the system.
/// @version 0.2
/// @date 2026-09-14
/// @copyright Copyright (c) 2026
///

#include <iostream>

#include "serial.hxx"

using namespace ba7lya::serial;

///
/// @brief Lists each port's name, description and hardware id.
/// @return Always 0.
///
int main() {
    for (const auto& port : list_ports()) {
        std::cout << "name: " << port.name << '\n'
                  << "desc: " << port.description << '\n'
                  << "hwid: " << port.hardware_id << std::endl;
    }
    return 0;
}
