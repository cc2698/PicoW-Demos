
# Set PICO_BOARD environment variable to "pico_w"
set(PICO_BOARD pico_w)

add_executable(picow_wifi_scan_background
        picow_wifi_scan.c
)
pico_enable_stdio_usb(udp_ap 0)
pico_enable_stdio_uart(udp_ap 1)
target_include_directories(picow_wifi_scan_background PRIVATE
        ${CMAKE_CURRENT_LIST_DIR}
)
target_link_libraries(picow_wifi_scan_background
        pico_cyw43_arch_lwip_threadsafe_background
        pico_stdlib
)
pico_add_extra_outputs(picow_wifi_scan_background)

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
