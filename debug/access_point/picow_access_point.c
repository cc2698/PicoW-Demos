
// C libraries
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Pico
#include "boards/pico_w.h"
#include "pico/cyw43_arch.h"
#include "pico/stdlib.h"

// DHCP
#include "dhcpserver/dhcpserver.h"

/*
 *  DEBUGGING
 */

#define PRINT_ON_RECV
#define PRINT_ON_SEND
#define TEST_PACKET

/*
 *  UDP
 */

// Properties
int access_point   = true;
int packet_counter = 0;
char my_addr[20]   = "255.255.255.255";

// UDP constants
#define UDP_PORT        4444 // Same port number on both devices
#define UDP_MSG_LEN_MAX 1400

// IP addresses
#define AP_ADDR   "192.168.4.1"
#define MASK_ADDR "255.255.255.0"

// Wifi name and password
#define WIFI_SSID     "picow_always_on"
#define WIFI_PASSWORD "password"

// Startup time
uint64_t start_time;
int last_print;
#define INTERVAL_MS 1e5

// Bruce Land's TCP server structure. Stores metadata for an access point
// hosted by a Pico-W. This includes the IPv4 address.
typedef struct TCP_SERVER_T_ {
    struct tcp_pcb* server_pcb;
    bool complete;
    ip_addr_t gw;
    async_context_t* context;
} TCP_SERVER_T;

#define HIGH 1
#define LOW  0
int led_state = LOW;

#define led_on()                                                               \
    do {                                                                       \
        led_state = HIGH;                                                      \
        cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, led_state);                 \
    } while (0);

#define led_off()                                                              \
    do {                                                                       \
        led_state = LOW;                                                       \
        cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, led_state);                 \
    } while (0);

#define led_toggle()                                                           \
    do {                                                                       \
        led_state = !led_state;                                                \
        cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, led_state);                 \
    } while (0);

bool repeating_timer_callback(struct repeating_timer* t)
{
    led_toggle();
    return true;
}

int main()
{
    // Initialize all stdio types
    stdio_init_all();

    printf("\n\n==================== %s ====================\n\n",
           "Access Point");

    // Initialize Wifi chip
    printf("Initializing cyw43...");
    if (cyw43_arch_init()) {
        printf("failed to initialise.\n");
        return 1;
    } else {
        printf("initialized!\n");
    }
    // Allocate TCP server state
    TCP_SERVER_T* state = calloc(1, sizeof(TCP_SERVER_T));
    printf("Allocating TCP server state...");
    if (!state) {
        printf("failed to allocate state.\n");
        return 1;
    } else {
        printf("allocated!\n");
    }

    // Turn on access point
    cyw43_arch_enable_ap_mode(WIFI_SSID, WIFI_PASSWORD,
                              CYW43_AUTH_WPA2_AES_PSK);
    printf("Access point mode enabled!\n");

    start_time = time_us_64();

    // The variable 'state' is a pointer to a TCP_SERVER_T struct
    // Set up the access point IP address and mask
    ip4_addr_t mask;
    ipaddr_aton(AP_ADDR, ip_2_ip4(&state->gw));
    ipaddr_aton(MASK_ADDR, ip_2_ip4(&mask));

    // Start the Dynamic Host Configuration Protocol (DHCP) server. Even
    // though in the program DHCP is not required, LwIP seems to need it!
    //      - Bruce Land
    //
    // I believe that DHCP is what assigns the Pico-W client an initial IP
    // address, which can be changed manually later.
    //      - Chris
    //
    // Set the Pico-W IP address from the 'state' structure
    // Set 'mask' as defined above
    dhcp_server_t dhcp_server;
    dhcp_server_init(&dhcp_server, &state->gw, &mask);

    // Print IP address
    printf("My IPv4 addr = %s\n", ip4addr_ntoa(&state->gw));

    struct repeating_timer timer;
    add_repeating_timer_ms(-750, repeating_timer_callback, NULL, &timer);

    while (true) {
        if ((time_us_64() - last_print) > 15 * 1e6) {
            int sec = (time_us_64() - start_time) / 1e6;
            int min = sec / 60;
            sec     = sec % 60;

            printf("Access point has been active for: %4dm %2ds\n", min, sec);

            last_print = time_us_64();
        }
    }

    // De-initialize the cyw43 architecture.
    cyw43_arch_deinit();

    return 0;
}
