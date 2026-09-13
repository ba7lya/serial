# --- cmake version ---
cmake_minimum_required(VERSION 3.21)

# --- project ---
project(ba7lya.serial LANGUAGES CXX)

# --- build options ---
option(${PROJECT_NAME}_BUILD_EXAMPLE "Build examples" ON )
option(${PROJECT_NAME}_BUILD_TEST    "Build tests"    ON )

# --- target ---
add_library(${PROJECT_NAME})
add_library(ba7lya::serial ALIAS ${PROJECT_NAME})

target_include_directories(${PROJECT_NAME} PUBLIC ${PROJECT_SOURCE_DIR}/include)
target_compile_features(${PROJECT_NAME} PRIVATE cxx_std_23)
target_compile_options(${PROJECT_NAME} PRIVATE "$<$<CXX_COMPILER_ID:MSVC>:/utf-8>")

if(WIN32)
    target_compile_definitions(${PROJECT_NAME} PRIVATE
        WINVER=0x0A00       # Windows 10
        _WIN32_WINNT=0x0A00 # Windows 10
        NOMINMAX            # keep std::min/max usable
    )
    target_sources(${PROJECT_NAME} PRIVATE
        src/serial.cxx
        src/win/win.cxx
        src/win/list_ports_win.cxx
    )
    target_link_libraries(${PROJECT_NAME} PRIVATE SetupAPI advapi32)
elseif(UNIX) # Linux
    target_sources(${PROJECT_NAME} PRIVATE
        src/serial.cxx
        src/linux/linux.cxx
        src/linux/list_ports_linux.cxx
    )
else()
    message(FATAL_ERROR "Unsupported platform: only Windows and Linux are supported.")
endif()

# --- install ---
include(GNUInstallDirs)
install(TARGETS ${PROJECT_NAME}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
# Headers live flat in include/ during development; as an installed dependency
# they are namespaced under include/ba7lya/serial/.
install(DIRECTORY include/
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/ba7lya/serial
    FILES_MATCHING PATTERN "*.hxx"
)

# --- examples ---
if(${PROJECT_NAME}_BUILD_EXAMPLE)
    add_subdirectory(examples)
endif()

# --- unit tests ---
if(${PROJECT_NAME}_BUILD_TEST)
    include(CTest)
    add_subdirectory(tests)
endif()
