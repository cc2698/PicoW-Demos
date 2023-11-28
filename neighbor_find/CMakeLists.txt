
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

# Source (*.c) files
set(SourceList
		neighbor_find.c
		connect.c
		layout.c
		node.c
		packet.c
		utils.c
		wifi_scan.c
		dhcpserver/dhcpserver.c
)

# Header (*.h) files
include_directories(
		${CMAKE_CURRENT_LIST_DIR}
		${CMAKE_CURRENT_LIST_DIR}/dhcpserver
		${CMAKE_CURRENT_LIST_DIR}/lwipopts
		${CMAKE_CURRENT_LIST_DIR}/protothreads
)

# Linked libraries
link_libraries(
		pico_cyw43_arch
		pico_cyw43_arch_lwip_threadsafe_background 
		pico_stdlib
		pico_multicore
)

# Master
add_executable(nbr_find_master)
pico_enable_stdio_usb(nbr_find_master 0)
pico_enable_stdio_uart(nbr_find_master 1)
target_compile_definitions(nbr_find_master PRIVATE MASTER)
target_sources(nbr_find_master PRIVATE ${SourceList})
pico_add_extra_outputs(nbr_find_master) # Generate UF2 output

# Node
add_executable(nbr_find)
pico_enable_stdio_usb(nbr_find 0)
pico_enable_stdio_uart(nbr_find 1)
target_sources(nbr_find PRIVATE ${SourceList})
pico_add_extra_outputs(nbr_find) # Generate UF2 output

# USB Node
add_executable(nbr_find_usb)
pico_enable_stdio_usb(nbr_find_usb 1)
pico_enable_stdio_uart(nbr_find_usb 0)
target_compile_definitions(nbr_find_usb PRIVATE SERIAL_OVER_USB)
target_sources(nbr_find_usb PRIVATE ${SourceList})
pico_add_extra_outputs(nbr_find_usb) # Generate UF2 output

# Compiler optimization
add_compile_options(-O2)
