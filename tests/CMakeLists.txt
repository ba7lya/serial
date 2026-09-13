# --- unit tests (GoogleTest) ---
find_package(GTest CONFIG REQUIRED)
include(GoogleTest)

add_executable(serial_unit_tests
    serial_test.cxx
    exception_test.cxx
)
target_link_libraries(serial_unit_tests PRIVATE ba7lya::serial GTest::gtest GTest::gtest_main)
target_compile_features(serial_unit_tests PRIVATE cxx_std_23)

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    # pty-based and timing tests, POSIX only.
    target_sources(serial_unit_tests PRIVATE
        linux_serial_test.cxx
        linux_timer_test.cxx
    )
    target_include_directories(serial_unit_tests PRIVATE ${PROJECT_SOURCE_DIR}/src)
    target_link_libraries(serial_unit_tests PRIVATE util)
endif()

gtest_discover_tests(serial_unit_tests)
