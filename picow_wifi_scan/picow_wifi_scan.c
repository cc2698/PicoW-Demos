/**
 * Copyright (c) 2022 Raspberry Pi (Trading) Ltd.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

// C libraries
#include <stdio.h>

// Pico
#include "pico/cyw43_arch.h"
#include "pico/stdlib.h"
#include "pico/types.h"

// Hardware
#include "hardware/clocks.h"
#include "hardware/vreg.h"

// Wifi scan callback function, prints cyw43_ev_scan_result_t as a string
static int scan_result(void* env, const cyw43_ev_scan_result_t* result)
{
    if (result) {
        // Compose MAC address
        char bssid_str[40];
        sprintf(bssid_str, "%02x:%02x:%02x:%02x:%02x:%02x", result->bssid[0],
                result->bssid[1], result->bssid[2], result->bssid[3],
                result->bssid[4], result->bssid[5]);

        // Print results
        printf("ssid: %-32s rssi: %4d chan: %3d mac: %s sec: %u\n",
               result->ssid, result->rssi, result->channel, bssid_str,
               result->auth_mode);
    }

    return 0;
}

int main()
{
    // Initialize all stdio types
    stdio_init_all();

    // Print header
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

    // Enable station mode
    cyw43_arch_enable_sta_mode();

    // Time of next scan
    absolute_time_t next_scan_time = nil_time;

    int scan_in_progress = false;

    while (true) {
        // If past the time of next scan
        if (absolute_time_diff_us(get_absolute_time(), next_scan_time) < 0) {
            // Start a scan if no scan is in progress, otherwise wait 10s
            if (!scan_in_progress) {
                // Scan options are currently ignored
                cyw43_wifi_scan_options_t scan_options = {0};

                // This function scans for nearby Wifi networks and runs the
                // callback function each time a network is found.
                int err = cyw43_wifi_scan(&cyw43_state, &scan_options, NULL,
                                          scan_result);

                if (err == 0) {
                    printf("\nPerforming wifi scan\n");
                    scan_in_progress = true;
                } else {
                    printf("Failed to start scan: %d\n", err);

                    // Wait 10s and scan again
                    next_scan_time = make_timeout_time_ms(10000);
                }
            } else if (!cyw43_wifi_scan_active(&cyw43_state)) {
                // This case triggers if you've just finished a scan, but
                // haven't reset the scan_in_progress flag yet

                // Wait 10s and scan again
                next_scan_time   = make_timeout_time_ms(10000);
                scan_in_progress = false;
            }
        }
    }

    cyw43_arch_deinit();
    return 0;
}
