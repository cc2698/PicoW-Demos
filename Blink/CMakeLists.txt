
# Set PICO_BOARD environment variable to "pico_w"
set(PICO_BOARD pico_w)

# Blink (serial UART)
add_executable(blink)
pico_enable_stdio_usb(blink 0)
pico_enable_stdio_uart(blink 1)
target_sources(blink PRIVATE blink.c)
target_link_libraries(blink PRIVATE
		pico_stdlib
		pico_cyw43_arch_none)
pico_add_extra_outputs(blink) # Generate UF2 output

# Blink (serial USB)
add_executable(blink_usb)
pico_enable_stdio_usb(blink_usb 1)
pico_enable_stdio_uart(blink_usb 0)
target_sources(blink_usb PRIVATE blink.c)
target_link_libraries(blink_usb PRIVATE
		pico_stdlib
		pico_cyw43_arch_none)
pico_add_extra_outputs(blink_usb) # Generate UF2 output

# Compiler optimization
add_compile_options(-O2)

