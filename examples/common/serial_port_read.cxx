///
/// @file serial_port_read.cxx
/// @author BA7LYA (1042140025@qq.com)
/// @brief Example: configure a serial port and read incoming data.
/// @version 0.2
/// @date 2026-09-14
/// @copyright Copyright (c) 2026
///

#include <cstdlib>
#include <iostream>
#include <string>

#include "serial.hxx"

using namespace ba7lya::serial;

///
/// @brief Configures and opens the port named by the user, or the first openable one found.
/// @param handle Closed serial object to configure and open.
/// @param port User-provided port address; empty triggers automatic selection.
/// @throws whatever open() throws when no port can be opened.
///
void open_first_port(serial& handle, const std::string& port) {
    handle.set_baudrate(115200);
    handle.set_data_bits(data_bits::eight);
    handle.set_parity(parity::none);
    handle.set_stop_bits(stop_bits::one);
    handle.set_flow_ctrl(flow_ctrl::none);
    handle.set_timeout(timeout::simple_timeout(250)); // 250 ms read/write timeout

    if (!port.empty()) {
        handle.set_port(port);
        handle.open();
        return;
    }
    for (const auto& info : list_ports()) {
        handle.set_port(info.name);
        try {
            handle.open();
            return;
        } catch (...) {
            continue; // busy or unsupported; try the next candidate
        }
    }
    throw io_exception("No openable serial port found.");
}

///
/// @brief Reads and prints incoming data: first one byte, then everything queued.
/// @param argc Argument count; argv[1] optionally names the port.
/// @param argv Argument values.
/// @return EXIT_SUCCESS once data has been read.
///
int main(int argc, char** argv) {
    const std::string port = argc > 1 ? argv[1] : "";

    serial serial_port;
    try {
        open_first_port(serial_port, port);
    } catch (const std::exception& e) {
        std::cerr << "The serial port did not open: " << e.what() << std::endl;
        return EXIT_FAILURE;
    }
    std::cout << "Reading from " << serial_port.get_port() << "...\n";

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
