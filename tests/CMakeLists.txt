# --- unit tests (GoogleTest) ---
# vcpkg ships a GTest config package; Debian/Ubuntu's libgtest-dev only has
# the find-module, so try config mode first and fall back.
find_package(GTest CONFIG QUIET)
if(NOT GTest_FOUND)
    find_package(GTest REQUIRED)
endif()
include(GoogleTest)

set(test_sources
    common/serial_test.cxx
    common/exception_test.cxx
    common/real_port_test.cxx
)
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    # pty-based and timing tests, POSIX only.
    list(APPEND test_sources
        linux/pty_serial_test.cxx
        linux/timer_test.cxx
    )
elseif(WIN32)
    # Windows behavioral contracts (unsupported wait_*/send_break, modem lines).
    list(APPEND test_sources win/behavior_test.cxx)
endif()

add_executable(serial_unit_tests ${test_sources})
target_link_libraries(serial_unit_tests PRIVATE ba7lya::serial GTest::gtest GTest::gtest_main)
target_compile_features(serial_unit_tests PRIVATE cxx_std_23)

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    target_include_directories(serial_unit_tests PRIVATE ${PROJECT_SOURCE_DIR}/src)
    target_link_libraries(serial_unit_tests PRIVATE util)
endif()

# gtest_discover_tests executes the test binary to collect cases; vcpkg's
# shared gtest would not be found without this copy (applocal is off by
# default).
if(WIN32)
    add_custom_command(TARGET serial_unit_tests POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
                $<TARGET_FILE:GTest::gtest> $<TARGET_FILE_DIR:serial_unit_tests>
    )
endif()

gtest_discover_tests(serial_unit_tests)
