#ifndef WIFI_SCAN_H
#define WIFI_SCAN_H

// Maximum acceptable SSID length
#define SSID_LEN 50

// Scan result when no pidogs are found
#define NO_UNINITIALIZED_NBRS "none"

// Maximum number of nodes that can join the network
#define MAX_NODES 5

// Whether pidogs were found during a scan
extern bool pidogs_found;

// The SSID of the most recent pidog_<hex ID> wifi network
extern char scan_result[SSID_LEN];

// (Placeholder) track neighbors by ID
extern int is_nbr[MAX_NODES];

// Non-ID scan targets
enum {
    RUN_SCAN  = -1,
    ENABLE_AP = 0
};

// Scan for picow_<ID> and pidog_<hex ID> networks, returns 0 on success. Places
// the result of the scan in the [scan_result] variable.
int scan_wifi();

#endif