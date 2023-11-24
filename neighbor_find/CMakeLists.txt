
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
add_executable(nbr_find_master)
pico_enable_stdio_usb(nbr_find_master 0)
pico_enable_stdio_uart(nbr_find_master 1)
target_sources(nbr_find_master PRIVATE
		neighbor_find.c
		dhcpserver/dhcpserver.c
)
target_include_directories(nbr_find_master PRIVATE
        ${CMAKE_CURRENT_LIST_DIR}
		${CMAKE_CURRENT_LIST_DIR}/dhcpserver
)
target_link_libraries(nbr_find_master PRIVATE
		pico_cyw43_arch
		pico_cyw43_arch_lwip_threadsafe_background 
		pico_stdlib
		pico_multicore
)
pico_add_extra_outputs(nbr_find_master) # Generate UF2 output

# Station
add_executable(nbr_find)
pico_enable_stdio_usb(nbr_find 0)
pico_enable_stdio_uart(nbr_find 1)
target_sources(nbr_find PRIVATE
		neighbor_find.c
		dhcpserver/dhcpserver.c
)
target_compile_definitions(nbr_find PRIVATE AP)
target_include_directories(nbr_find PRIVATE
        ${CMAKE_CURRENT_LIST_DIR}
		${CMAKE_CURRENT_LIST_DIR}/dhcpserver
)
target_link_libraries(nbr_find PRIVATE
		pico_cyw43_arch
		pico_cyw43_arch_lwip_threadsafe_background
		pico_stdlib
		pico_multicore
)
pico_add_extra_outputs(nbr_find) # Generate UF2 output

# Compiler optimization
add_compile_options(-O2)
