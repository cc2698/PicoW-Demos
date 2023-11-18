add_executable(picow_access_point_background
        main.c
        picow_access_point.c
        mpu6050.c
        dhcpserver/dhcpserver.c
        dnsserver/dnsserver.c
        )
pico_enable_stdio_usb(picow_access_point_background 0)
pico_enable_stdio_uart(picow_access_point_background 1)
target_include_directories(picow_access_point_background PRIVATE
        ${CMAKE_CURRENT_LIST_DIR}
        ${CMAKE_CURRENT_LIST_DIR}/.. # for our common lwipopts
        ${CMAKE_CURRENT_LIST_DIR}/dhcpserver
        ${CMAKE_CURRENT_LIST_DIR}/dnsserver
        )
target_link_libraries(picow_access_point_background
        pico_cyw43_arch_lwip_threadsafe_background
        pico_stdlib
        hardware_i2c
        )
pico_add_extra_outputs(picow_access_point_background)

add_executable(picow_access_point_poll
        main.c
        picow_access_point.c
        mpu6050.c
        dhcpserver/dhcpserver.c
        dnsserver/dnsserver.c
        )
pico_enable_stdio_usb(picow_access_point_poll 0)
pico_enable_stdio_uart(picow_access_point_poll 1)
target_include_directories(picow_access_point_poll PRIVATE
        ${CMAKE_CURRENT_LIST_DIR}
        ${CMAKE_CURRENT_LIST_DIR}/.. # for our common lwipopts
        ${CMAKE_CURRENT_LIST_DIR}/dhcpserver
        ${CMAKE_CURRENT_LIST_DIR}/dnsserver
        )
target_link_libraries(picow_access_point_poll
        pico_cyw43_arch_lwip_poll
        pico_stdlib
        hardware_i2c
        )
pico_add_extra_outputs(picow_access_point_poll)
