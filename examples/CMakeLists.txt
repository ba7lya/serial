# Platform-independent examples plus per-platform ones.
set(examples
    common/enumerator_example.cxx
    common/serial_example.cxx
)
if(WIN32)
    list(APPEND examples win/example_vk162.cxx)
elseif(UNIX)
    list(APPEND examples
        linux/serial_port_read.cxx
        linux/serial_port_write.cxx
    )
endif()

foreach(source IN LISTS examples)
    get_filename_component(example ${source} NAME_WE)
    add_executable(${example} ${source})
    target_link_libraries(${example} PRIVATE ba7lya::serial)
    target_compile_features(${example} PRIVATE cxx_std_23)
endforeach()
