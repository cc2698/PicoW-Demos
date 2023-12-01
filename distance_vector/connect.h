#ifndef CONNECT_H
#define CONNECT_H

// Local
#include "network_opts.h"

// Wifi password
#define WIFI_PASSWORD "password"

// IP addresses
#define AP_ADDR      "192.168.4.1"
#define STATION_ADDR "192.168.4.10"
#define MASK_ADDR    "255.255.255.0"

// Flag for whether the pico is an access point
extern int access_point;

// My IPv4 address
extern char my_addr[IP_ADDR_LEN];

// (Placeholder) Destination IPv4 address
extern char dest_addr_str[IP_ADDR_LEN];

// ID to which this pico is currently connected
extern int connected_ID;

// Generate a picow SSID
void generate_picow_ssid(char* buf, int picow_ID);

// Re-initialize the cyw43
int re_init_cyw43();

// Boot up the access point, returns 0 on success.
int boot_ap();

// Shutdown the access point, de-init DHCP, and free the TCP_SERVER state
void shutdown_ap();

// Boot up the station
void boot_station();

// Shutdown the station
void shutdown_station();

// Connect to a network and set a new IP address
int connect_to_network(char* ssid);

#endif