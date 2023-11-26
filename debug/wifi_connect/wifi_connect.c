
// C Libraries
#include <stdlib.h>
#include <string.h>

// Pico
#include "pico/cyw43_arch.h"
#include "pico/stdlib.h"

// Wifi name and password
#define WIFI_SSID     "picow_test"
#define WIFI_PASSWORD "password"

int main()
{
    stdio_init_all();

    printf("\n\n==================== %s ====================\n\n",
           "Wifi Scanner");

    // Initialize Wifi chip
    printf("Initializing cyw43...");
    if (cyw43_arch_init()) {
        printf("failed to initialise.\n");
        return 1;
    } else {
        printf("initialized!\n");
    }

    cyw43_arch_enable_sta_mode();

    printf("Connecting to Wi-Fi...\n");
    if (cyw43_arch_wifi_connect_timeout_ms(WIFI_SSID, WIFI_PASSWORD,
                                           CYW43_AUTH_WPA2_AES_PSK, 30000)) {
        printf("failed to connect.\n");
        return 1;
    } else {
        printf("Connected.\n");
    }

    cyw43_arch_deinit();

    return 0;
}
