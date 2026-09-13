///
/// @file serial_port_read.cxx
/// @author BA7LYA (1042140025@qq.com)
/// @brief Example: configure a serial port and read incoming data byte-wise.
/// @version 0.2
/// @date 2026-09-14
/// @copyright Copyright (c) 2026
///

#include <cstdlib>
#include <iostream>

#include "ba7lya/serial/serial.hxx"

using namespace ba7lya::serial;

///
/// @brief Opens a port, configures framing and a timeout, then prints received data.
/// @return EXIT_SUCCESS when at least one byte was read.
///
int main() {
    Serial serial_port;
    serial_port.set_port("/dev/ttyUSB0");
    serial_port.set_baudrate(115200);
    serial_port.set_data_bits(data_bits::EIGHT);
    serial_port.set_parity(parity::NONE);
    serial_port.set_stop_bits(stop_bits::ONE);
    serial_port.set_flow_ctrl(flow_ctrl::NONE);
    serial_port.set_timeout(Timeout::simple_timeout(250)); // 250 ms read/write timeout

    try {
        serial_port.open();
    } catch (const std::exception& e) {
        std::cerr << "The serial port did not open: " << e.what() << std::endl;
        return EXIT_FAILURE;
    }

    // Block (up to the read timeout) until at least one byte is available.
    std::string buffer;
    while (buffer.empty()) { serial_port.read(buffer, 1); }
    std::cout << "First byte: " << buffer << std::flush;

    // Read whatever else has arrived, tolerating a timeout with fewer bytes.
    buffer.clear();
    serial_port.read(buffer, serial_port.available());
    std::cout << "Rest of data: " << buffer << std::flush;

    std::cout << "\nThe example program successfully completed!" << std::endl;
    return EXIT_SUCCESS;
}
