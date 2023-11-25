
# Set PICO_BOARD environment variable to "pico_w"
set(PICO_BOARD pico_w)

# Wifi scan
add_executable(picow_wifi_scan_background
        picow_wifi_scan.c
)
pico_enable_stdio_usb(picow_wifi_scan_background 0)
pico_enable_stdio_uart(picow_wifi_scan_background 1)
target_include_directories(picow_wifi_scan_background PRIVATE
        ${CMAKE_CURRENT_LIST_DIR}
)
target_link_libraries(picow_wifi_scan_background
        pico_cyw43_arch_lwip_threadsafe_background
        pico_stdlib
)
pico_add_extra_outputs(picow_wifi_scan_background)

# Compiler optimization
add_compile_options(-O2)
