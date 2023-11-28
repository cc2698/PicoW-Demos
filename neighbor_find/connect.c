
// C libraries
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Pico
#include "boards/pico_w.h"
#include "pico/cyw43_arch.h"

// Lightweight IP
#include "lwip/ip_addr.h"
#include "lwip/netif.h"

// DHCP
#include "dhcpserver/dhcpserver.h"

// Local
#include "connect.h"
#include "layout.h"
#include "node.h"

int access_point = true;

char dest_addr_str[IP_ADDR_LEN] = "255.255.255.255";

int connected_ID = 0;

// Bruce Land's TCP server structure. Stores metadata for an access point
// hosted by a Pico-W. This includes the IPv4 address.
typedef struct TCP_SERVER_T_ {
    struct tcp_pcb* server_pcb;
    bool complete;
    ip_addr_t gw;
    async_context_t* context;
} TCP_SERVER_T;

// Access point metadata
TCP_SERVER_T* state;
ip4_addr_t mask;
dhcp_server_t dhcp_server;

#ifdef USE_LAYOUT

void generate_picow_ssid(char* buf, int picow_ID)
{
    // SSID = picow_<physical_ID>_<ID>
    if (picow_ID == self.ID) {
        snprintf(buf, SSID_LEN, "picow_%d_%d", self.physical_ID, self.ID);
    } else {
        // Get the neighbor's physical ID (stored from the scan)
        snprintf(buf, SSID_LEN, "picow_%d_%d", ID_to_phys_ID[picow_ID],
                 picow_ID);
    }
}

#else

void generate_picow_ssid(char* buf, int picow_ID)
{
    // SSID = picow_<ID>
    snprintf(buf, SSID_LEN, "picow_%d", picow_ID);
}

#endif

int boot_ap()
{
    // Allocate TCP server state
    state = calloc(1, sizeof(TCP_SERVER_T));
    printf("Allocating TCP server state...");
    if (!state) {
        printf("failed to allocate state.\n");
        return 1;
    } else {
        printf("allocated!\n");
    }

    // Enable access point
    cyw43_arch_enable_ap_mode(self.wifi_ssid, WIFI_PASSWORD,
                              CYW43_AUTH_WPA2_AES_PSK);
    printf("Access point mode enabled!\n");
    printf("\tssid         = %s\n", self.wifi_ssid);

    // The variable 'state' is a pointer to a TCP_SERVER_T struct
    // Set up the access point IP address and mask
    ipaddr_aton(AP_ADDR, ip_2_ip4(&state->gw));
    ipaddr_aton(MASK_ADDR, ip_2_ip4(&mask));

    // Configure target IP address
    snprintf(self.ip_addr, IP_ADDR_LEN, "%s", AP_ADDR);
    snprintf(dest_addr_str, IP_ADDR_LEN, "%s", STATION_ADDR);

    // Start the Dynamic Host Configuration Protocol (DHCP) server. Even though
    // in the program DHCP is not required, LwIP seems to need it!
    //      - Bruce Land
    //
    // I believe that DHCP is what assigns the Pico-W client an initial
    // IP address, which can be changed manually later.
    //      - Chris
    //
    // Set the Pico-W IP address from the 'state' structure
    // Set 'mask' as defined above
    dhcp_server_init(&dhcp_server, &state->gw, &mask);

    // // Print IP address (old method)
    // printf("My IPv4 addr = %s\n", ip4addr_ntoa(&state->gw));

    // Print IP address (potentially better method)
    printf("\tMy IPv4 addr = %s\n", ip4addr_ntoa(netif_ip4_addr(netif_list)));

    access_point = true;

    return 0;
}

void shutdown_ap()
{
    printf("Shutting down access point...");

    // Disable access point
    cyw43_arch_disable_ap_mode();
    access_point = false;

    // Disable the DHCP server
    dhcp_server_deinit(&dhcp_server);

    // Free the TCP_SERVER state
    free(state);

    printf("success!\n");
}

// Boot up the station
void boot_station()
{
    printf("Booting station mode...");

    // Enable station
    cyw43_arch_enable_sta_mode();
    access_point = false;

    printf("success!\n");
}

// Shutdown the station
void shutdown_station()
{
    printf("Shutting down station...");

    // Disable station
    cyw43_arch_disable_sta_mode();

    printf("success!\n");
}

int connect_to_network(char* ssid)
{
    if (access_point) {
        printf("ERROR: Can't connect to a network while in AP mode.\n");
        return 1;
    }

    // Connect to the access point
    printf("Connecting to %s...", ssid);
    if (cyw43_arch_wifi_connect_timeout_ms(ssid, WIFI_PASSWORD,
                                           CYW43_AUTH_WPA2_AES_PSK, 30000)) {
        printf("failed to connect.\n");
        return 1;
    } else {
        printf("connected!\n");
        printf("\tssid         = %s\n", ssid);
        printf("\tpassword     = %s\n", WIFI_PASSWORD);

        // Configure target IP address
        snprintf(self.ip_addr, IP_ADDR_LEN, "%s", STATION_ADDR);
        snprintf(dest_addr_str, IP_ADDR_LEN, "%s", AP_ADDR);

        // Print address assigned by DHCP
        printf("\tMy IPv4 addr = %s (DHCP) --> ",
               ip4addr_ntoa(netif_ip4_addr(netif_list)));

        // Set local address, override the address assigned by DHCP
        ip_addr_t ip;
        ipaddr_aton(STATION_ADDR, &ip);
        netif_set_ipaddr(netif_default, &ip);

        // Print new local address
        printf("%s (new)\n", ip4addr_ntoa(netif_ip4_addr(netif_list)));

        return 0;
    }
}
