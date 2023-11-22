
# CMake version
cmake_minimum_required(VERSION 3.12)

# Project name (anything you want)
project(lwip_testing)

# Specify minimum C/C++ standard
# set(CMAKE_C_STANDARD 11)
# set(CMAKE_CXX_STANDARD 17)

# Include the Pico sdk.cmake file
# include(pico_sdk_import.cmake)

# Initialize the Pico C/C++ SDK
if (PICO_SDK_VERSION_STRING VERSION_LESS "1.3.0")
    message(FATAL_ERROR "Raspberry Pi Pico SDK version 1.3.0 (or later) required. Your version is ${PICO_SDK_VERSION_STRING}")
endif()

# Set PICO_BOARD environment variable to "pico_w"
set(PICO_BOARD pico_w)

pico_sdk_init()

#######

# Access point
add_executable(udp_ap)
pico_enable_stdio_usb(udp_ap 0)
pico_enable_stdio_uart(udp_ap 1)
target_sources(udp_ap PRIVATE
		udp_send_recv.c
		dhcpserver/dhcpserver.c
)
target_compile_definitions(udp_ap PRIVATE AP)
target_include_directories(udp_ap PRIVATE
        ${CMAKE_CURRENT_LIST_DIR}
		${CMAKE_CURRENT_LIST_DIR}/dhcpserver
)
target_link_libraries(udp_ap PRIVATE
		pico_cyw43_arch
		pico_cyw43_arch_lwip_threadsafe_background 
		pico_stdlib
		pico_multicore
)
pico_add_extra_outputs(udp_ap) # Generate UF2 output

# Station
add_executable(udp_station)
pico_enable_stdio_usb(udp_station 0)
pico_enable_stdio_uart(udp_station 1)
target_sources(udp_station PRIVATE
		udp_send_recv.c
		dhcpserver/dhcpserver.c
)
target_include_directories(udp_station PRIVATE
        ${CMAKE_CURRENT_LIST_DIR}
		${CMAKE_CURRENT_LIST_DIR}/dhcpserver
)
target_link_libraries(udp_station PRIVATE
		pico_cyw43_arch
		pico_cyw43_arch_lwip_threadsafe_background
		pico_stdlib
		pico_multicore
)
pico_add_extra_outputs(udp_station) # Generate UF2 output

# Compiler optimization
add_compile_options(-O2)
