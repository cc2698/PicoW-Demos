#ifndef WIFI_SCAN_H
#define WIFI_SCAN_H

// Local
#include "network_opts.h"

// Scan result when no pidogs are found
#define NO_UNINITIALIZED_NBRS "none"

// Whether pidogs were found during a scan
extern bool pidogs_found;

// The SSID of the most recent pidog_<hex ID> wifi network
extern char scan_result[SSID_LEN];

// Non-ID scan targets
enum {
    RUN_SCAN  = -2,
    ENABLE_AP = -1
};

// Scan for picow_<ID> and pidog_<hex ID> networks, returns 0 on success. Places
// the result of the scan in the [scan_result] variable.
int scan_wifi();

#endif