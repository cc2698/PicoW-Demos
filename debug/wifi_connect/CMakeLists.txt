
# Set PICO_BOARD environment variable to "pico_w"
set(PICO_BOARD pico_w)

# Wifi connect
add_executable(wifi_connect
        wifi_connect.c
)
pico_enable_stdio_usb(blink 0)
pico_enable_stdio_uart(blink 1)
target_include_directories(wifi_connect PRIVATE
        ${CMAKE_CURRENT_LIST_DIR}
)
target_link_libraries(wifi_connect
        pico_cyw43_arch_lwip_threadsafe_background
        pico_stdlib
)
pico_add_extra_outputs(wifi_connect)

# Compiler optimization
add_compile_options(-O2)
