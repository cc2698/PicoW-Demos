
# Set PICO_BOARD environment variable to "pico_w"
set(PICO_BOARD pico_w)

# unique_id (serial UART)
add_executable(unique_id)
pico_enable_stdio_usb(unique_id 0)
pico_enable_stdio_uart(unique_id 1)
target_sources(unique_id PRIVATE unique_id.c)
target_link_libraries(unique_id PRIVATE
		pico_stdlib
		pico_cyw43_arch_none
)
pico_add_extra_outputs(unique_id) # Generate UF2 output

# unique_id (serial USB)
add_executable(unique_id_usb)
pico_enable_stdio_usb(unique_id_usb 1)
pico_enable_stdio_uart(unique_id_usb 0)
target_sources(unique_id_usb PRIVATE unique_id.c)
target_link_libraries(unique_id_usb PRIVATE
		pico_stdlib
		pico_cyw43_arch_none
)
pico_add_extra_outputs(unique_id_usb) # Generate UF2 output

# Compiler optimization
add_compile_options(-O2)
