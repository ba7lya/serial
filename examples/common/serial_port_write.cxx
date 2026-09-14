///
/// @file serial_port_write.cxx
/// @author BA7LYA (1042140025@qq.com)
/// @brief Example: send the contents of a file over a serial port.
/// @version 0.2
/// @date 2026-09-14
/// @copyright Copyright (c) 2026
///

#include <cstdlib>
#include <fstream>
#include <iostream>
#include <sstream>
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
    handle.set_timeout(timeout::simple_timeout(1000)); // 1 s write timeout

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
/// @brief Reads a file and writes it to the serial port, then drains the transmit buffer.
/// @param argc Argument count: <filename> [port].
/// @param argv Argument values.
/// @return EXIT_SUCCESS when every byte was accepted by the port.
///
int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <filename> [port]" << std::endl;
        return EXIT_FAILURE;
    }

    std::ifstream input_file(argv[1], std::ios::binary);
    if (!input_file) {
        std::cerr << "Error: Could not open file " << argv[1] << " for reading." << std::endl;
        return EXIT_FAILURE;
    }
    std::ostringstream contents;
    contents << input_file.rdbuf();

    serial serial_port;
    try {
        open_first_port(serial_port, argc > 2 ? argv[2] : "");
    } catch (const std::exception& e) {
        std::cerr << "The serial port did not open: " << e.what() << std::endl;
        return EXIT_FAILURE;
    }
    std::cout << "Writing " << contents.str().size() << " bytes to " << serial_port.get_port()
              << "...\n";

    const std::string data = contents.str();
    const size_t bytes_written = serial_port.write(data);
    serial_port.flush(); // Wait until the data has actually been transmitted.

    std::cout << "Wrote " << bytes_written << " of " << data.size() << " bytes." << std::endl;
    return bytes_written == data.size() ? EXIT_SUCCESS : EXIT_FAILURE;
}
