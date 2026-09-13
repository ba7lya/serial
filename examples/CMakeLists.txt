# Every example is a small standalone program linking the library.
foreach(example enumerator_example example_vk162 serial_example serial_port_read serial_port_write)
    add_executable(${example} ${example}.cxx)
    target_link_libraries(${example} PRIVATE ba7lya::serial)
    target_compile_features(${example} PRIVATE cxx_std_23)
endforeach()
