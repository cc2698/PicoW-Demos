#ifndef CONNECT_H
#define CONNECT_H

// Wifi password
#define WIFI_PASSWORD "password"
// #define WIFI_PASSWORD NULL

// IP addresses
#define AP_ADDR      "192.168.4.1"
#define STATION_ADDR "192.168.4.10"
#define MASK_ADDR    "255.255.255.0"

// Flag for whether the pico is an access point
extern int access_point;

// My IPv4 address
extern char my_addr[20];

// (Placeholder) Destination IPv4 address
extern char dest_addr_str[20];

// Boot up the access point, returns 0 on success.
int boot_access_point();

// Shutdown the access point, DHCP, and free the TCP_SERVER state
void shutdown_ap();

// Connect to a network and set a new IP address
int connect_to_network(char* ssid);

#endif