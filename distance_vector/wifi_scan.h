#ifndef WIFI_SCAN_H
#define WIFI_SCAN_H

// Local
#include "network_opts.h"
#include "node.h"

// Scan result when no pidogs are found
#define NO_UNINITIALIZED_NBRS "none"

// Whether pidogs were found during a scan
extern bool pidogs_found;

// The SSID of the most recent pidog_<hex ID> wifi network
extern char nbr_find_scan_result[SSID_LEN];

// The neighbor which most needs my distance vector
extern nbr_t* routing_scan_result;

// Scan types
typedef enum scan_type {
    NBR_FIND_SCAN,
    DV_ROUTE_SCAN
} scan_type_t;

// Non-ID scan targets
enum {
    DV_SCAN       = -3,
    NF_SCAN       = -2,
    ENABLE_AP     = -1,
    CONNECT_TO_AP = 0
};

// Scan for picow_<ID> and pidog_<hex ID> networks, returns 0 on success. Places
// the result of the scan in the [scan_result] variable.
int scan_wifi(scan_type_t t);

#endif