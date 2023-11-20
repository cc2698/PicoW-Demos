
# Blink
add_executable(blink)
pico_enable_stdio_usb(blink 0)
pico_enable_stdio_uart(blink 1)
target_sources(blink PRIVATE blink.c)
target_link_libraries(blink PRIVATE
		pico_stdlib
		pico_cyw43_arch_none)
pico_add_extra_outputs(blink) # Generate UF2 output

# Compiler optimization
add_compile_options(-O2)

