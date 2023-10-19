add_executable(blink)

target_sources(blink PRIVATE blink.c)

# Pull in our pico_stdlib which pulls in commonly used features
target_link_libraries(blink pico_stdlib pico_cyw43_arch_none)

# create map/bin/hex file etc.
pico_add_extra_outputs(blink)

# Use serial
pico_enable_stdio_usb(blink 0)
pico_enable_stdio_uart(blink 1)
