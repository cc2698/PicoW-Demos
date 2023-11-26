
// C libraries
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Pico
#include "pico/cyw43_arch.h"
#include "pico/stdlib.h"

// Local
#include "wifi_scan.h"

bool pidogs_found;
char scan_result[SSID_LEN];

int is_nbr[MAX_NODES];

// Scan callback function
static int scan_callback(void* env, const cyw43_ev_scan_result_t* result)
{
    if (result) {
        char header[10] = "";

        // Get first 5 characters of the SSID
        // *header = '\0';
        snprintf(header, 6, "%s", result->ssid);

        bool result_is_pidog = (strcmp(header, "pidog") == 0);
        bool result_is_picow = (strcmp(header, "picow") == 0);

        if (result_is_pidog) {
            printf("\tssid: %-32s rssi: %4d\t<-- Uninitialized node\n",
                   result->ssid, result->rssi);
            snprintf(scan_result, SSID_LEN, "%s", result->ssid);
        }

        if (result_is_picow) {
            // Separate the ID number from the SSID: picow_<ID>
            char tbuf[SSID_LEN];
            sprintf(tbuf, "%s", result->ssid);

            char* token;

            // Get the ID
            token = strtok(tbuf, "_");
            token = strtok(NULL, "_");

            // Convert the ID to an integer
            int id = atoi(token);

            // Mark as neighbor
            is_nbr[id] = true;

            printf("\tssid: %-32s rssi: %4d\t<-- ID = %d\n", result->ssid,
                   result->rssi, id);
        }
    }

    return 0;
}

int scan_wifi()
{
    // Scan options don't matter
    cyw43_wifi_scan_options_t scan_options = {0};

    // Clear last pidog found
    snprintf(scan_result, SSID_LEN, "%s", NO_UNINITIALIZED_NBRS);

    printf("Starting Wifi scan...");

    // This function scans for nearby Wifi networks and runs the
    // callback function each time a network is found.
    int err = cyw43_wifi_scan(&cyw43_state, &scan_options, NULL, scan_callback);

    if (err == 0) {
        printf("scan started successfully!\n");
    } else {
        printf("failed to start scan. err = %d\n", err);
        return 1;
    }

    // Give time for the scan to start
    sleep_ms(500);

    while (cyw43_wifi_scan_active(&cyw43_state)) {
        // Block until scan is complete
    }

    // Mark whether any uninitialized nodes were found during the scan
    if (strcmp(scan_result, NO_UNINITIALIZED_NBRS) == 0) {
        pidogs_found = false;
    } else {
        pidogs_found = true;
    };

    return 0;
}
