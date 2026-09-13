///
/// @file serial_example.cxx
/// @author BA7LYA (1042140025@qq.com)
/// @brief This example expects the serial port to have a loopback on it.
/// @version 0.2
/// @date 2026-09-14
/// @copyright Copyright (c) 2026
///

#include <charconv>
#include <cstdint>
#include <iostream>
#include <string>

#include "serial.hxx"

using namespace ba7lya::serial;

///
/// @brief Prints the command line usage to stderr.
///
void print_usage() {
    std::cerr << "Usage: serial_example {<serial port address>|-e} <baudrate> [test string]\n";
}

///
/// @brief Writes the test string and reads it back, reporting the byte counts each round.
/// @param serial Open port with a loopback.
/// @param test_string Data to send.
/// @param request Extra bytes to ask for beyond the string length (0, +1 or -1).
/// @param iterations Number of rounds to run.
///
void loop_rounds(Serial& serial, const std::string& test_string, int request, int iterations) {
    for (int count = 0; count < iterations; ++count) {
        const size_t bytes_written = serial.write(test_string);
        const std::string result
            = serial.read(static_cast<size_t>(static_cast<int>(test_string.size()) + request));

        std::cout << "Iteration: " << count << ", Bytes written: " << bytes_written
                  << ", Bytes read: " << result.size() << ", String read: " << result << "\n";
    }
}

///
/// @brief Parses the baud rate argument.
/// @param text Command line argument.
/// @param baudrate Receives the parsed value.
/// @return True when the whole argument was a valid number.
///
bool parse_baudrate(std::string_view text, std::uint32_t& baudrate) {
    const char* first = text.data();
    const char* last = text.data() + text.size();
    return std::from_chars(first, last, baudrate).ptr == last;
}

///
/// @brief Runs the loopback timing demonstration.
/// @param argc Argument count.
/// @param argv Argument values: port (or -e), baudrate, optional test string.
/// @return Process exit code.
///
int run(int argc, char** argv) {
    if (argc < 2) {
        print_usage();
        return EXIT_FAILURE;
    }

    const std::string port(argv[1]);
    if (port == "-e") {
        for (const auto& device : list_ports()) {
            std::cout << "(" << device.name << ", " << device.description << ", "
                      << device.hardware_id << ")\n";
        }
        return EXIT_SUCCESS;
    }
    if (argc < 3) {
        print_usage();
        return EXIT_FAILURE;
    }

    std::uint32_t baudrate = 0;
    if (!parse_baudrate(argv[2], baudrate)) {
        std::cerr << "Invalid baudrate: " << argv[2] << "\n";
        return EXIT_FAILURE;
    }

    const std::string test_string = argc == 4 ? argv[3] : "Testing.";

    // port, baudrate, timeout in milliseconds
    Serial serial(port, baudrate, Timeout::simple_timeout(1000));
    std::cout << "Is the serial port open? " << (serial.is_open() ? "Yes." : "No.") << "\n";

    std::cout << "Timeout == 1000ms, asking for 1 more byte than written.\n";
    loop_rounds(serial, test_string, +1, 10);

    serial.set_timeout(Timeout::max(), 250, 0, 250, 0);
    std::cout << "Timeout == 250ms, asking for 1 more byte than written.\n";
    loop_rounds(serial, test_string, +1, 10);

    std::cout << "Timeout == 250ms, asking for exactly what was written.\n";
    loop_rounds(serial, test_string, 0, 10);

    std::cout << "Timeout == 250ms, asking for 1 less than was written.\n";
    loop_rounds(serial, test_string, -1, 10);

    return EXIT_SUCCESS;
}

///
/// @brief Entry point; reports any escaping exception.
///
int main(int argc, char** argv) {
    try {
        return run(argc, argv);
    } catch (const std::exception& e) {
        std::cerr << "Unhandled exception: " << e.what() << std::endl;
        return EXIT_FAILURE;
    }
}
