
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

int num_unique_results = 0;
uint64_t unique_results[MAX_NODES];

bool id_is_not_a_repeat(uint64_t id)
{
    for (int i = 0; i < num_unique_results; i++) {
        if (unique_results[i] == id) {
            // Match found
            return false;
        }
    }

    return true;
}

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

        if (result_is_pidog || result_is_picow) {
            // Create a token buffer (strtok is destructive)
            char tbuf[SSID_LEN];
            sprintf(tbuf, "%s", result->ssid);

            // Separate the ID number from the SSID: picow_<ID>
            char* token;
            token = strtok(tbuf, "_");
            token = strtok(NULL, "_");

            if (result_is_pidog) {
                // Convert the hexadecimal ID to a uint64_t
                uint64_t id = strtoull(token, NULL, 16);

                if (id_is_not_a_repeat(id)) {
                    // Log unique result
                    unique_results[num_unique_results] = id;
                    num_unique_results++;

                    // Write SSID to scan_result
                    snprintf(scan_result, SSID_LEN, "%s", result->ssid);

                    printf("\tssid: %-25s rssi: %4d\t<-- Uninitialized node\n",
                           result->ssid, result->rssi);
                }

            } else if (result_is_picow) {
                // Convert the decimal ID to an integer
                int id = atoi(token);

                if (id_is_not_a_repeat(id)) {
                    // Log unique result
                    unique_results[num_unique_results] = id;
                    num_unique_results++;

                    printf("\tssid: %-25s rssi: %4d\t<-- ID = %d\n",
                           result->ssid, result->rssi, id);
                }

                // Mark as neighbor
                is_nbr[id] = true;
            }
        }
    }

    return 0;
}

int scan_wifi()
{
    // Scan options don't matter
    cyw43_wifi_scan_options_t scan_options = {0};

    // Clear last scan result
    snprintf(scan_result, SSID_LEN, "%s", NO_UNINITIALIZED_NBRS);

    // Reset list of seen IDs
    num_unique_results = 0;
    for (int i = 0; i < MAX_NODES; i++) {
        unique_results[i] = 0;
    }

    printf("Starting scan...");

    // This function scans for nearby Wifi networks and runs the
    // callback function each time a network is found.
    int err = cyw43_wifi_scan(&cyw43_state, &scan_options, NULL, scan_callback);

    if (err == 0) {
        printf("success!\n");
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

    printf("\t%*c...\n", 4, ' ');
    printf("\tScan result: %-25s\n", scan_result);

    return 0;
}
