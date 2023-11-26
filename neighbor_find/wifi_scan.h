#ifndef WIFI_SCAN_H
#define WIFI_SCAN_H

// Maximum acceptable SSID length
#define SSID_LEN 50

// Maximum number of nodes that can join the network
#define MAX_NODES 5

// Whether pidogs were found during a scan
extern bool pidogs_found;

// The SSID of the most recent pidog_<hex ID> wifi network
extern char pidog_target_ssid[SSID_LEN];

// (Placeholder) track neighbors by ID
extern int is_nbr[MAX_NODES];

// Initiate a wifi scan
int scan_wifi();

#endif