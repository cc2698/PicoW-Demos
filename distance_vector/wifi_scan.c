
// C libraries
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Pico
#include "pico/cyw43_arch.h"
#include "pico/stdlib.h"

// Local
#include "layout.h"
#include "node.h"
#include "wifi_scan.h"

bool pidogs_found;

char scan_result[SSID_LEN];

// Track unique SSIDs during a scan
int num_unique_results = 0;
uint64_t unique_results[MAX_NODES];

// Returns true if the specified ID has not been encountered during this scan
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
        // Get first 5 characters of the SSID
        char header[10] = "";
        snprintf(header, 6, "%s", result->ssid);

        // Set flags
        bool result_is_pidog = (strcmp(header, "pidog") == 0);
        bool result_is_picow = (strcmp(header, "picow") == 0);
        bool is_visible      = true;

        if (result_is_pidog || result_is_picow) {
            // Create a token buffer (strtok is destructive)
            char tbuf[SSID_LEN];
            snprintf(tbuf, SSID_LEN, "%s", result->ssid);

            // Separate the header
            char* token;
            token = strtok(tbuf, "_");

#ifdef USE_LAYOUT
            // Parse the extra physical ID out of the middle
            token              = strtok(NULL, "_");
            int result_phys_ID = atoi(token);

            // Use the network connections list to determine if this node is
            // actually visible
            is_visible = conn_array[self.physical_ID][result_phys_ID];
#endif

            // Get the ID - <ID> if picow_<ID> / <hex ID> if pidog_<hex ID>
            token = strtok(NULL, "_");

            if (result_is_pidog && is_visible) {
                // Convert the hexadecimal ID to a uint64_t
                uint64_t id = strtoull(token, NULL, 16);

                if (id_is_not_a_repeat(id)) {
                    // Log unique result
                    unique_results[num_unique_results] = id;
                    num_unique_results++;

                    // Write SSID to scan_result
                    snprintf(scan_result, SSID_LEN, "%s", result->ssid);

                    printf("\tssid: %-*s rssi: %4d dB  <-- New node\n",
                           SSID_LEN, result->ssid, result->rssi);
                }

            } else if (result_is_picow && is_visible) {
                // Convert the decimal ID to an integer
                int id = atoi(token);

                if (id_is_not_a_repeat(id)) {
                    // Log unique result
                    unique_results[num_unique_results] = id;
                    num_unique_results++;

                    printf("\tssid: %-*s rssi: %4d dB  <-- ID = %d\n", SSID_LEN,
                           result->ssid, result->rssi, id);
                }

                // Mark as neighbor
                self.ID_is_nbr[id] = true;

#ifdef USE_LAYOUT
                // Store the physical ID corresponding to the ID assigned by the
                // neighbor finding process
                ID_to_phys_ID[id] = result_phys_ID;
#endif
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

    // Mark whether any pidogs (uninitialized nodes) were found
    if (strcmp(scan_result, NO_UNINITIALIZED_NBRS) == 0) {
        pidogs_found = false;
    } else {
        pidogs_found = true;
    };

    // Print the last pidog found
    printf("\t%*c...\n", 4, ' ');
    printf("\tscan result: %-30s\n", scan_result);

    return 0;
}
