
#include "picow_access_point.h"
#include "mpu6050.h"

// Standard C libraries
#include <stdio.h>
#include <string.h>

// Standard Pico-W libraries
#include "pico/cyw43_arch.h"
#include "pico/stdlib.h"
#include "hardware/i2c.h"

// DHCP and DNS
#include "dhcpserver.h"
#include "dnsserver.h"

float acceleration[3], gyro[3];

int main() {
    stdio_init_all();

    TCP_SERVER_T *state = calloc(1, sizeof(TCP_SERVER_T));
    if (!state) {
        DEBUG_printf("failed to allocate state\n");
        return 1;
    }
    printf("TCP Server state allocated!\n");

    if (cyw43_arch_init()) {
        DEBUG_printf("failed to initialise\n");
        return 1;
    }
    printf("CYW43 initialized!\n");

    // Reset the IMU
    i2c_init(I2C_CHAN, I2C_BAUD_RATE) ;
    gpio_set_function(SDA_PIN, GPIO_FUNC_I2C) ;
    gpio_set_function(SCL_PIN, GPIO_FUNC_I2C) ;
    gpio_pull_up(SDA_PIN) ;
    gpio_pull_up(SCL_PIN) ;

    mpu6050_reset();
    mpu6050_read_raw(acceleration, gyro);

    const char *ap_name = "picow_test";
#if 1
    const char *password = "password";
#else
    const char *password = NULL;
#endif

    cyw43_arch_enable_ap_mode(ap_name, password, CYW43_AUTH_WPA2_AES_PSK);

    ip4_addr_t mask;
    IP4_ADDR(ip_2_ip4(&state->gw), 192, 168, 4, 1);
    IP4_ADDR(ip_2_ip4(&mask), 255, 255, 255, 0);

    // Start the dhcp server
    dhcp_server_t dhcp_server;
    dhcp_server_init(&dhcp_server, &state->gw, &mask);

    // Start the dns server
    dns_server_t dns_server;
    dns_server_init(&dns_server, &state->gw);

    if (!tcp_server_open(state)) {
        DEBUG_printf("failed to open server\n");
        return 1;
    }
    printf("TCP server opened!\n");

    while(!state->complete) {
        // the following #ifdef is only here so this same example can be used in multiple modes;
        // you do not need it in your code
#if PICO_CYW43_ARCH_POLL
        // if you are using pico_cyw43_arch_poll, then you must poll periodically from your
        // main loop (not from a timer interrupt) to check for Wi-Fi driver or lwIP work that needs to be done.
        cyw43_arch_poll();
        // you can poll as often as you like, however if you have nothing else to do you can
        // choose to sleep until either a specified time, or cyw43_arch_poll() has work to do:
        cyw43_arch_wait_for_work_until(make_timeout_time_ms(1000));
#else
        // // if you are not using pico_cyw43_arch_poll, then Wi-FI driver and lwIP work
        // // is done via interrupt in the background. This sleep is just an example of some (blocking)
        // // work you might be doing.
        // sleep_ms(1000);

        // Read the IMU
        mpu6050_read_raw(acceleration, gyro);
        DEBUG_printf("Ax = %f ; Ay = %f ; Az = %f\n", acceleration[0], acceleration[1], acceleration[2]);
        sleep_ms(1000);
#endif
    }
    dns_server_deinit(&dns_server);
    dhcp_server_deinit(&dhcp_server);
    cyw43_arch_deinit();
    return 0;
}
