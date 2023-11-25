
# Set PICO_BOARD environment variable to "pico_w"
set(PICO_BOARD pico_w)

# Access point
add_executable(picow_access_point)
pico_enable_stdio_usb(picow_access_point 1)
pico_enable_stdio_uart(picow_access_point 0)
target_sources(picow_access_point PRIVATE
        picow_access_point.c
        dhcpserver/dhcpserver.c
)
target_include_directories(picow_access_point PRIVATE
        ${CMAKE_CURRENT_LIST_DIR}
        ${CMAKE_CURRENT_LIST_DIR}/dhcpserver
)
target_link_libraries(picow_access_point
        pico_cyw43_arch_lwip_threadsafe_background
        pico_stdlib
)
pico_add_extra_outputs(picow_access_point)

# Compiler optimization
add_compile_options(-O2)
