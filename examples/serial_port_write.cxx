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
/// @brief Reads a whole file and writes it to the serial port with a 1 s write timeout.
/// @param argc Argument count; expects one file name.
/// @param argv Argument values: argv[1] = file to transmit.
/// @return EXIT_SUCCESS on success.
///
int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <filename>" << std::endl;
        return EXIT_FAILURE;
    }

    std::ifstream input_file(argv[1], std::ios::binary);
    if (!input_file) {
        std::cerr << "Error: Could not open file " << argv[1] << " for reading." << std::endl;
        return EXIT_FAILURE;
    }
    std::ostringstream contents;
    contents << input_file.rdbuf();

    Serial serial_port("/dev/ttyUSB1", 115200, Timeout::simple_timeout(1000));
    if (!serial_port.is_open()) {
        std::cerr << "The serial port did not open." << std::endl;
        return EXIT_FAILURE;
    }

    const std::string data = contents.str();
    std::cout << "Writing " << data.size() << " bytes to the serial port." << std::endl;
    const size_t bytes_written = serial_port.write(data);
    serial_port.flush(); // Wait until the data has actually been transmitted.

    std::cout << "Wrote " << bytes_written << " of " << data.size() << " bytes." << std::endl;
    return bytes_written == data.size() ? EXIT_SUCCESS : EXIT_FAILURE;
}
