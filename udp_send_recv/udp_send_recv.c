/*
 * Compiles into the following binaries:
 *  - udp_ap.uf2
 *  - udp_station.uf2
 */

// C libraries
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Pico
#include "pico/cyw43_arch.h"
#include "pico/multicore.h"
#include "pico/stdlib.h"

// Protothreads
#include "pt_cornell_rp2040_v1_1_2.h"

// Lightweight IP
#include "lwip/netif.h" // Included by lwip/udp.h
#include "lwip/opt.h"   // Included by lwip/udp.h
#include "lwip/pbuf.h"  // Included by lwip/udp.h
#include "lwip/udp.h"

// DHCP
#include "dhcpserver/dhcpserver.h"

// Is the Pico-W an access point?
int access_point = true;

// UDP constants
#define UDP_PORT        4444 // Same port number on both devices
#define UDP_MSG_LEN_MAX 1400

// IP addresses
#define AP_ADDR      "192.168.4.1"
#define STATION_ADDR "192.168.4.10"
char udp_target_pico[20] = "255.255.255.255";

// Wifi name and password
#define WIFI_SSID     "picow_test"
#define WIFI_PASSWORD "password"

// UDP send/recv constructs
char recv_data[UDP_MSG_LEN_MAX];
char send_data[UDP_MSG_LEN_MAX];
struct pt_sem new_udp_send_s, new_udp_recv_s;

// Bruce Land's TCP server structure. Stores metadata for an access point hosted
// by a Pico-W. This includes the IPv4 address.
typedef struct TCP_SERVER_T_ {
    struct tcp_pcb* server_pcb;
    bool complete;
    ip_addr_t gw;
    async_context_t* context;
} TCP_SERVER_T;

/*
 *	UDP CALLBACK SETUP
 */

// UDP recv function
void udpecho_raw_recv(void* arg, struct udp_pcb* upcb, struct pbuf* p,
                      const ip_addr_t* addr, u16_t port)
{
    // Prevent "unused argument" compiler warning
    LWIP_UNUSED_ARG(arg);

    if (p != NULL) {
        // Copy the payload into the recv buffer
        memcpy(recv_data, p->payload, UDP_MSG_LEN_MAX);

        // Free the packet buffer
        pbuf_free(p);

        // Signal that the recv buffer has been written
        PT_SEM_SIGNAL(pt, &new_udp_recv_s);
    } else {
        printf("ERROR: NULL pt in callback\n");
    }
}

static struct udp_pcb* udpecho_raw_pcb;

// Define the recv callback function
int udpecho_raw_init(void)
{
    struct pbuf* p; // OMVED
    udpecho_raw_pcb = udp_new_ip_type(IPADDR_TYPE_ANY);
    p = pbuf_alloc(PBUF_TRANSPORT, UDP_MSG_LEN_MAX + 1, PBUF_POOL);

    if (udpecho_raw_pcb != NULL) {
        err_t err;

        // Bind the UDP PCB to the socket
        // - netif_ip4_addr returns the picow ip address
        err = udp_bind(udpecho_raw_pcb, netif_ip4_addr(netif_list),
                       UDP_PORT); // DHCP addr

        if (err == ERR_OK) {
            // This function assigns the callback function for when a UDP packet
            // is received
            udp_recv(udpecho_raw_pcb, udpecho_raw_recv, NULL);
        } else {
            printf("UDP bind error\n");
            return 1;
        }
    } else {
        printf("ERROR: udpecho_raw_pcb was NULL\n");
        return 1;
    }

    // Success
    return 0;
}

/*
 *	THREADS
 */

// ==================================================
// UDP send thread
// ==================================================
static ip_addr_t addr;
static PT_THREAD(protothread_udp_send(struct pt* pt))
{
    PT_BEGIN(pt);

    // Initialize a PCB for UDP-send
    static struct udp_pcb* pcb;
    pcb = udp_new();

    pcb->remote_port = UDP_PORT;
    pcb->local_port  = UDP_PORT;

    static int counter = 0;

    while (true) {
        // Wait until the send buffer is written
        PT_SEM_WAIT(pt, &new_udp_send_s);

        // Assign target pico IP addr
        ipaddr_aton(udp_target_pico, &addr);

        // Allocate pbuf
        static int udp_send_length;
        udp_send_length = strlen(send_data);
        struct pbuf* p =
            pbuf_alloc(PBUF_TRANSPORT, udp_send_length + 1, PBUF_RAM);

        // Clear the payload and write to it
        char* req = (char*) p->payload; // Cast from void* to char*
        memset(req, 0, udp_send_length + 1);
        memcpy(req, send_data, udp_send_length);

        // Send packet
        // cyw43_arch_lwip_begin();
        printf("\n| Send:\n|\tnum:  %d\n|\tdest: %s\n|\tmsg:  \"%s\"\n\n",
               counter, udp_target_pico, send_data);
        err_t er = udp_sendto(pcb, p, &addr, UDP_PORT); // port
        // cyw43_arch_lwip_end();

        // Free the packet buffer
        pbuf_free(p);
        if (er == ERR_OK) {
            counter++;
        } else {
            printf("Failed to send UDP packet! error=%d\n", er);
        }

        PT_YIELD(pt);
    }

    PT_END(pt);
}

// ==================================================
// udp recv thread
// ==================================================
static PT_THREAD(protothread_udp_recv(struct pt* pt))
{
    PT_BEGIN(pt);

    while (1) {
        // Wait until the recv buffer is written
        PT_SEM_WAIT(pt, &new_udp_recv_s);

        // Print the contents of the recv buffer
        printf("\n| Recv:\n|\tmsg:  \"%s\"\n\n", recv_data);

        PT_YIELD(pt);
    }

    PT_END(pt);
}

// =================================================
// input thread
// =================================================
static PT_THREAD(protothread_serial(struct pt* pt))
{
    PT_BEGIN(pt);

    while (1) {
        // Yielding here is not strictly necessary but it gives a little bit of
        // slack for the async processes so that the output is in the correct
        // order (most of the time)
        PT_YIELD_usec(100000);

        // Spawn threads for non-blocking read/write
        serial_write;
        serial_read;

        // If the input buffer is non-empty
        if (strcmp(pt_serial_in_buffer, "") != 0) {
            // Write message to send buffer
            memset(send_data, 0, UDP_MSG_LEN_MAX);
            sprintf(send_data, "%s", pt_serial_in_buffer);
            PT_SEM_SIGNAL(pt, &new_udp_send_s);
        }
    }

    PT_END(pt);
}

/*
 *  MAIN
 */

int main()
{
    // Initialize all stdio types
    stdio_init_all();

// If this pico is hosting an AP, set access_point to true. The macro is defined
// at compile-time by CMake allowing for the same file to be compiled into two
// separate binaries, one for the access point and one for the station.
#ifdef AP
    access_point = true;
#else
    access_point = false;
#endif

    // Printout whether you're an AP or a station
    printf("\n\n==================== %s ====================\n\n",
           (access_point ? "Access Point" : "Station"));

    // Initialize Wifi chip
    printf("Initializing cyw43...");
    if (cyw43_arch_init()) {
        printf("failed to initialise.\n");
        return 1;
    } else {
        printf("initialized!\n");
    }

    if (access_point) {
        // Allocate TCP server state
        TCP_SERVER_T* state = calloc(1, sizeof(TCP_SERVER_T));
        printf("Allocating TCP server state...");
        if (!state) {
            printf("failed to allocate state.\n");
            return 1;
        } else {
            printf("allocated!\n");
        }

        // Turn on access point
        cyw43_arch_enable_ap_mode(WIFI_SSID, WIFI_PASSWORD,
                                  CYW43_AUTH_WPA2_AES_PSK);
        printf("Access point mode enabled!\n");

        // The variable 'state' is a pointer to a TCP_SERVER_T struct
        // Set up the access point IP address and mask
        ip4_addr_t mask;
        IP4_ADDR(ip_2_ip4(&state->gw), 192, 168, 4, 1);
        IP4_ADDR(ip_2_ip4(&mask), 255, 255, 255, 0);

        // Configure target IP address
        sprintf(udp_target_pico, "%s", STATION_ADDR);

        // Start the Dynamic Host Configuration Protocol (DHCP) server. Even
        // though in the program DHCP is not required, LWIP seems to need it!
        //      - Bruce Land
        //
        // I believe that DHCP is what assigns the Pico-W client an initial IP
        // address, which can be changed manually later.
        //      - Chris
        //
        // Set the Pico-W IP address from the 'state' structure
        // Set 'mask' as defined above
        dhcp_server_t dhcp_server;
        dhcp_server_init(&dhcp_server, &state->gw, &mask);

        // Print IP address
        printf("My IPv4 addr = %s\n", ip4addr_ntoa(&state->gw));

    } else {
        // If all Pico-Ws boot at the same time, this delay gives the access
        // point time to setup before the client tries to connect.
        sleep_ms(1000);

        // Enable station mode
        cyw43_arch_enable_sta_mode();
        printf("Station mode enabled!\n");

        // Connect to the access point
        printf("Connecting to Wi-Fi...");
        if (cyw43_arch_wifi_connect_timeout_ms(
                WIFI_SSID, WIFI_PASSWORD, CYW43_AUTH_WPA2_AES_PSK, 30000)) {
            printf("failed to connect.\n");
            return 1;
        } else {
            printf("connected to Wi-Fi:\n\tSSID = %s\n\tPASS = %s\n", WIFI_SSID,
                   WIFI_PASSWORD);

            // Print address assigned by DCHP
            printf("Connected as: Pico-W IP addr: %s\n",
                   ip4addr_ntoa(netif_ip4_addr(netif_list)));

            // Configure target IP address
            sprintf(udp_target_pico, "%s", AP_ADDR);

            // Set local address, override the address assigned by DHCP
            ip_addr_t ip;
            IP4_ADDR(&ip, 192, 168, 4, 10);
            netif_set_ipaddr(netif_default, &ip);

            // Print new local address
            printf("Modified: new Pico-W IP addr: %s\n",
                   ip4addr_ntoa(netif_ip4_addr(netif_list)));
        }
    }

    // Initialize UDP recv callback function
    printf("Initializing recv callback...");
    if (udpecho_raw_init()) {
        printf("receive callback failed to initialize.");
    } else {
        printf("callback initialized!\n");
    }

    // The threads use semaphores to signal each other when buffers are written.
    // If a thread tries to aquire a semaphore that is unavailable, it yields to
    // the next thread in the scheduler.
    printf("Initializing send/recv semaphores...\n");
    PT_SEM_INIT(&new_udp_send_s, 0);
    PT_SEM_INIT(&new_udp_recv_s, 0);

    // =====================================
    // core 1
    // start core 1 threads
    // multicore_reset_core1();
    // multicore_launch_core1(&core1_main);

    // Start protothreads
    printf("Starting Protothreads!");
    pt_add_thread(protothread_udp_recv);
    pt_add_thread(protothread_udp_send);
    pt_add_thread(protothread_serial);
    pt_schedule_start;

    // De-initialize the cyw43 architecture.
    cyw43_arch_deinit();

    return 0;
}
