
# CMake version
cmake_minimum_required(VERSION 3.12)

# Specify minimum C standard
set(CMAKE_C_STANDARD 11)

# Include the Pico sdk.cmake file
# include(pico_sdk_import.cmake)

# Project name
project(distance_vector_routing)

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
		main.c
		connect.c
		distance_vector.c
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

#
#	No layout
#

# Master
add_executable(dv_master)
pico_enable_stdio_usb(dv_master 0)
pico_enable_stdio_uart(dv_master 1)
target_compile_definitions(dv_master PRIVATE MASTER)
target_sources(dv_master PRIVATE ${SourceList})
pico_add_extra_outputs(dv_master) # Generate UF2 output

# Node
add_executable(dv_node_UART)
pico_enable_stdio_usb(dv_node_UART 0)
pico_enable_stdio_uart(dv_node_UART 1)
target_sources(dv_node_UART PRIVATE ${SourceList})
pico_add_extra_outputs(dv_node_UART) # Generate UF2 output

# USB Node
add_executable(dv_node_USB)
pico_enable_stdio_usb(dv_node_USB 1)
pico_enable_stdio_uart(dv_node_USB 0)
target_compile_definitions(dv_node_USB PRIVATE SERIAL_OVER_USB)
target_sources(dv_node_USB PRIVATE ${SourceList})
pico_add_extra_outputs(dv_node_USB) # Generate UF2 output

#
#	With layout defined
#

# Master
add_executable(dv_LAYOUT_master)
pico_enable_stdio_usb(dv_LAYOUT_master 0)
pico_enable_stdio_uart(dv_LAYOUT_master 1)
target_compile_definitions(dv_LAYOUT_master PRIVATE
		MASTER
		USE_LAYOUT
		)
target_sources(dv_LAYOUT_master PRIVATE ${SourceList})
pico_add_extra_outputs(dv_LAYOUT_master) # Generate UF2 output

# Node
add_executable(dv_LAYOUT_node_UART)
pico_enable_stdio_usb(dv_LAYOUT_node_UART 0)
pico_enable_stdio_uart(dv_LAYOUT_node_UART 1)
target_compile_definitions(dv_LAYOUT_node_UART PRIVATE
		USE_LAYOUT
		)
target_sources(dv_LAYOUT_node_UART PRIVATE ${SourceList})
pico_add_extra_outputs(dv_LAYOUT_node_UART) # Generate UF2 output

# USB Node
add_executable(dv_LAYOUT_node_USB)
pico_enable_stdio_usb(dv_LAYOUT_node_USB 1)
pico_enable_stdio_uart(dv_LAYOUT_node_USB 0)
target_compile_definitions(dv_LAYOUT_node_USB PRIVATE
		USE_LAYOUT
		SERIAL_OVER_USB
		)
target_sources(dv_LAYOUT_node_USB PRIVATE ${SourceList})
pico_add_extra_outputs(dv_LAYOUT_node_USB) # Generate UF2 output

# Compiler optimization
add_compile_options(-O2)
