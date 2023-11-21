/*
 * Chris (cc2698@cornell.edu)
 *
 * This demonstration sets up one Pico-W as a Wifi access point, and another as
 * a Wifi station. Using a UDP connection between the two, the user is able to
 * type into the serial terminal on either Pico-W and have the input printed out
 * on the other Pico-W's serial terminal.
 *
 * To show that multicore is possible in this application, I use core 1 to turn
 * on the LED when a UDP packet is received. The application uses four
 * protothreads (send, recv, ack, and serial), all of which are executing on
 * core 0.
 *
 * The reason I create separate constructs for sending and acknowledging is to
 * avoid contention over the send_data buffer. If the threads were to execute in
 * the following order: recv -> serial -> send, the serial thread might
 * overwrite the ACK with the user input.
 *
 * Specifically for packets with text, the RTT is significantly faster from
 * (station -> ap -> station) as opposed to (ap -> station -> ap). Empty packets
 * mostly have a low RTT. Sometimes packets going from (ap -> station -> ap)
 * have a long RTT, likely because work is being done to keep the AP running.
 *
 * RTT is usually less than 100ms
 *
 * Compiles into the following binaries:
 *  - udp_ap.uf2
 *  - udp_station.uf2
 */

// C libraries
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Pico
#include "boards/pico_w.h"
#include "pico/cyw43_arch.h"
#include "pico/multicore.h"
#include "pico/stdlib.h"

// Hardware
#include "hardware/irq.h"
#include "hardware/timer.h"

// Protothreads
#include "pt_cornell_rp2040_v1_1_2.h"

// Lightweight IP
#include "lwip/udp.h" // Includes other necessary LwIP libraries

// DHCP
#include "dhcpserver/dhcpserver.h"

/*
 *  DEBUGGING
 */

#define PRINT_ON_RECV
#define PRINT_ON_SEND
#define TEST_PACKET

/*
 *  UDP
 */

// Properties
int access_point   = true;
int packet_counter = 0;
char my_addr[20]   = "255.255.255.255";

// UDP constants
#define UDP_PORT        4444 // Same port number on both devices
#define UDP_MSG_LEN_MAX 1400

// IP addresses
#define AP_ADDR      "192.168.4.1"
#define STATION_ADDR "192.168.4.10"
#define MASK_ADDR    "255.255.255.0"

// Wifi name and password
#define WIFI_SSID     "picow_test"
#define WIFI_PASSWORD "password"

// UDP send
static struct udp_pcb* udp_recv_pcb;
char recv_data[UDP_MSG_LEN_MAX];
struct pt_sem new_udp_recv_s;

// UDP recv
char dest_addr_str[20] = "255.255.255.255";
static ip_addr_t dest_addr;
static struct udp_pcb* udp_send_pcb;
char send_data[UDP_MSG_LEN_MAX];
struct pt_sem new_udp_send_s;

// UDP ack
char return_addr_str[20] = "255.255.255.255";
static ip_addr_t return_addr;
int return_ack_number;
char return_timestamp[50];
static struct udp_pcb* udp_ack_pcb;
struct pt_sem new_udp_ack_s;

// Bruce Land's TCP server structure. Stores metadata for an access point hosted
// by a Pico-W. This includes the IPv4 address.
typedef struct TCP_SERVER_T_ {
    struct tcp_pcb* server_pcb;
    bool complete;
    ip_addr_t gw;
    async_context_t* context;
} TCP_SERVER_T;

/*
 *  PACKET HANDLING
 */

// Packet types
#define NUM_PACKET_TYPES 2
const char* packet_types[NUM_PACKET_TYPES] = {"data", "ack"};

// Max length of header fields
#define TOK_LEN 40

// Check if a string is a valid packet type
int is_valid_packet_type(char* s)
{
    int valid = false;

    // Search packet_types for a match
    for (int i = 0; i < NUM_PACKET_TYPES; i++) {
        if (strcmp(packet_types[i], s) == 0) {
            valid = true;
        }
    }

    return valid;
}

// Copy a token into a header field.
void copy_field(char* field, char* token)
{
    // I use snprintf() here because it is extremely dangerous if a header field
    // gets written without a NULL terminator, which could happen if the token
    // exceeds the length of the buffer. The undefined behavior causes
    // non-reproducible bugs, usually ending with the Pico-W freezing.

    if (token == NULL) {
        snprintf(field, TOK_LEN, "n/a");
    } else {
        snprintf(field, TOK_LEN, "%s", token);
    }
}

/*
 *  ALARM
 */

// Alarm ID and duration
alarm_id_t led_alarm;
#define ALARM_MS 750

// Signal core 1 to turn the LED on
volatile int led_flag = false;

#define led_on()                                                               \
    do {                                                                       \
        cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, 1);                         \
    } while (0);

#define led_off()                                                              \
    do {                                                                       \
        cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, 0);                         \
    } while (0);

// Alarm callback function
int64_t alarm_callback(alarm_id_t id, void* user_data)
{
    led_off();
    return 0; // Returns 0 to not reschedule the alarm
}

/*
 *	UDP CALLBACK SETUP
 */

// UDP recv function
void udp_recv_callback(void* arg, struct udp_pcb* upcb, struct pbuf* p,
                       const ip_addr_t* addr, u16_t port)
{
    // Prevent "unused argument" compiler warning
    LWIP_UNUSED_ARG(arg);

    if (p != NULL) {
        // Copy the payload into the recv buffer
        memcpy(recv_data, p->payload, UDP_MSG_LEN_MAX);

        // Free the packet buffer
        pbuf_free(p);

        // Signal waiting threads
        PT_SEM_SIGNAL(pt, &new_udp_recv_s);
    } else {
        printf("ERROR: NULL pt in callback\n");
    }
}

// Define the recv callback function
int udp_recv_callback_init(void)
{
    // Allocate packet buffer
    struct pbuf* p;
    p = pbuf_alloc(PBUF_TRANSPORT, UDP_MSG_LEN_MAX + 1, PBUF_POOL);

    // Create a new UDP PCB
    udp_recv_pcb = udp_new_ip_type(IPADDR_TYPE_ANY);

    if (udp_recv_pcb != NULL) {
        err_t err;

        // Bind the UDP PCB to the socket
        // - netif_ip4_addr returns the picow ip address
        err = udp_bind(udp_recv_pcb, netif_ip4_addr(netif_list),
                       UDP_PORT); // DHCP addr

        if (err == ERR_OK) {
            // This function assigns the callback function for when a UDP packet
            // is received
            udp_recv(udp_recv_pcb, udp_recv_callback, NULL);
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
static PT_THREAD(protothread_udp_send(struct pt* pt))
{
    PT_BEGIN(pt);

    // Create a new UDP PCB
    udp_send_pcb = udp_new();

    // Assign port numbers
    udp_send_pcb->remote_port = UDP_PORT;
    udp_send_pcb->local_port  = UDP_PORT;

    // Payload
    static char buffer[UDP_MSG_LEN_MAX];
    static int timestamp;
    static int udp_send_length;

    // Stores the address of the pbuf payload
    static char* req;

    // Error code
    static err_t er;

    while (true) {
        // Wait until the buffer is written
        PT_SEM_WAIT(pt, &new_udp_send_s);

        // Assign target pico IP address
        ipaddr_aton(dest_addr_str, &dest_addr);

        // Timestamp the packet
        timestamp = time_us_64();

        // Append header to the payload
        sprintf(buffer, "%s;%s;%d;%lu;%s", "data", my_addr, packet_counter,
                timestamp, send_data);

#ifdef TEST_PACKET
        // Send a test packet with no header, lets you test how the system
        // responds to an unrecognizable packet type
        if (strcmp(send_data, "???") == 0) {
            strcpy(buffer, "Incredibly long test packet with an unrecognizable "
                           "header that cannot be parsed by the recv thread.");
        }
#endif

        // Allocate pbuf
        udp_send_length = strlen(buffer);
        struct pbuf* p =
            pbuf_alloc(PBUF_TRANSPORT, udp_send_length + 1, PBUF_RAM);

        // Clear the payload and write to it
        req = (char*) p->payload; // Cast from void* to char*
        memset(req, 0, udp_send_length + 1);
        memcpy(req, buffer, udp_send_length);

#ifdef PRINT_ON_SEND
        // Print formatted packet contents
        printf("| Outgoing...\n");
        printf("|\tpayload: { %s }\n", buffer);
        printf("|\tdest:    %s\n", dest_addr_str);
        printf("|\tnum:     %d\n", packet_counter);
        printf("|\tmsg:     %s\n", send_data);
        printf("\n");
#endif

        // Send packet
        // cyw43_arch_lwip_begin();
        er = udp_sendto(udp_send_pcb, p, &dest_addr, UDP_PORT);
        // cyw43_arch_lwip_end();

        if (er == ERR_OK) {
            packet_counter++;
        } else {
            printf("Failed to send UDP packet! error=%d\n", er);
        }

        // Free the packet buffer
        pbuf_free(p);

        PT_YIELD(pt);
    }

    PT_END(pt);
}

// ==================================================
// UDP recv thread
// ==================================================
static PT_THREAD(protothread_udp_recv(struct pt* pt))
{
    PT_BEGIN(pt);

    // Not strictly necessary, but the strtok() function is destructive of
    // its inputs therefore we copy the recv buffer into a temporary
    // variable to avoid messing with it directly.
    static char tbuf[UDP_MSG_LEN_MAX];

    // For tokenizing the packet
    static char packet_type[TOK_LEN];
    static char src_addr[TOK_LEN];
    static char packet_num[TOK_LEN];
    static char timestamp_str[TOK_LEN];
    static char msg[UDP_MSG_LEN_MAX];
    static char* token;

    // Timestamp when the packet was sent
    static uint64_t timestamp;

    static float rtt_ms;

    while (true) {
        // Wait until the buffer is written
        PT_SEM_WAIT(pt, &new_udp_recv_s);

        sprintf(tbuf, recv_data);

        // Data or ACK
        token = strtok(tbuf, ";");
        copy_field(packet_type, token);

        // Source IP address
        token = strtok(NULL, ";");
        copy_field(src_addr, token);

        // ACK number
        token = strtok(NULL, ";");
        copy_field(packet_num, token);

        // Timestamp
        token = strtok(NULL, ";");
        copy_field(timestamp_str, token);

        if (strcmp(timestamp_str, "n/a") != 0) {
            timestamp = strtoull(timestamp_str, NULL, 10);

            // If this packet is an ack, calculate the RTT
            if (strcmp(packet_type, "ack") == 0) {
                rtt_ms = (time_us_64() - timestamp) / 1000.0f;
            }
        }

        // Contents
        token = strtok(NULL, ";");
        copy_field(msg, token);

#ifdef PRINT_ON_RECV
        // Print formatted packet contents
        printf("| Incoming...\n");
        printf("|\tPayload: { %s }\n", recv_data);
        printf("|\ttype:    %s\n", packet_type);
        printf("|\tfrom:    %s\n", src_addr);
        printf("|\tack:     %s\n", packet_num);
        if (strcmp(packet_type, "data") == 0) {
            printf("|\tmsg:     %s\n", msg);
        } else if (strcmp(packet_type, "ack") == 0) {
            printf("|\tRTT:     %.2f ms\n", rtt_ms);
        } else {
            printf("|\tmsg:     %s\n", msg);
        }
        printf("\n");
#endif

        // If data was received, respond with ACK
        if (strcmp(packet_type, "data") == 0) {
            // Assign return address and ACK number
            strcpy(return_addr_str, src_addr);
            strcpy(return_timestamp, timestamp_str);
            return_ack_number = atoi(packet_num);

            // Signal ACK thread
            PT_SEM_SIGNAL(pt, &new_udp_ack_s);

            // Flag core 1 to turn on the LED
            led_flag = true;
        }

        PT_YIELD(pt);
    }

    PT_END(pt);
}

// ==================================================
// UDP ack thread
// ==================================================
static PT_THREAD(protothread_udp_ack(struct pt* pt))
{
    PT_BEGIN(pt);

    // Create a new UDP PCB
    udp_ack_pcb = udp_new();

    // Assign port numbers
    udp_ack_pcb->remote_port = UDP_PORT;
    udp_ack_pcb->local_port  = UDP_PORT;

    // Payload
    static char buffer[UDP_MSG_LEN_MAX];
    static int udp_ack_length;

    // Stores the address of the pbuf payload
    static char* req;

    // Error code
    static err_t er;

    while (true) {
        // Wait until the buffer is written
        PT_SEM_WAIT(pt, &new_udp_ack_s);

        // Assign target pico IP address
        ipaddr_aton(return_addr_str, &return_addr);

        // Append header to the payload
        sprintf(buffer, "%s;%s;%d;%s", "ack", my_addr, return_ack_number,
                return_timestamp);

        // Allocate pbuf
        udp_ack_length = strlen(buffer);
        struct pbuf* p =
            pbuf_alloc(PBUF_TRANSPORT, udp_ack_length + 1, PBUF_RAM);

        // Clear the payload and write to it
        req = (char*) p->payload; // Cast from void* to char*
        memset(req, 0, udp_ack_length + 1);
        memcpy(req, buffer, udp_ack_length);

#ifdef PRINT_ON_SEND
        // Print formatted packet contents
        printf("| Outgoing...\n");
        printf("|\tPayload: { %s }\n", buffer);
        printf("|\tdest:    %s\n", return_addr_str);
        printf("|\tnum:     %d\n", return_ack_number);
        printf("\n");
#endif
        // Send packet
        // cyw43_arch_lwip_begin();
        er = udp_sendto(udp_ack_pcb, p, &return_addr, UDP_PORT);
        // cyw43_arch_lwip_end();

        if (er != ERR_OK) {
            printf("Failed to send UDP ack! error=%d\n", er);
        }

        // Free the packet buffer
        pbuf_free(p);

        PT_YIELD(pt);
    }

    PT_END(pt);
}

// =================================================
// Serial input thread
// =================================================
static PT_THREAD(protothread_serial(struct pt* pt))
{
    PT_BEGIN(pt);

    while (true) {
        // Yielding here is not strictly necessary but it gives a little bit of
        // slack for the async processes so that the output is in the correct
        // order (most of the time).
        //      - Bruce Land
        PT_YIELD_usec(1e5);

        // Spawn threads for non-blocking read/write
        serial_write;
        serial_read;

        // Write message to send buffer
        memset(send_data, 0, UDP_MSG_LEN_MAX);
        sprintf(send_data, "%s", pt_serial_in_buffer);

        // Signal waiting threads
        PT_SEM_SIGNAL(pt, &new_udp_send_s);
    }

    PT_END(pt);
}

/*
 *  CORE 1 MAIN
 */

// Core 1 main function
void core_1_main()
{
    printf("Core 1 launched!\n");

    while (true) {
        if (led_flag) {
            led_on();

            // Restart alarm
            cancel_alarm(led_alarm);
            led_alarm = add_alarm_in_ms(ALARM_MS, alarm_callback, NULL, false);

            // Reset flag
            led_flag = false;
        }
    }
}

/*
 *  CORE 0 MAIN
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

    // Print out whether you're an AP or a station
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
        ipaddr_aton(AP_ADDR, ip_2_ip4(&state->gw));
        ipaddr_aton(MASK_ADDR, ip_2_ip4(&mask));

        // Configure target IP address
        sprintf(my_addr, AP_ADDR);
        sprintf(dest_addr_str, "%s", STATION_ADDR);

        // Start the Dynamic Host Configuration Protocol (DHCP) server. Even
        // though in the program DHCP is not required, LwIP seems to need it!
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
            sprintf(my_addr, STATION_ADDR);
            sprintf(dest_addr_str, "%s", AP_ADDR);

            // Set local address, override the address assigned by DHCP
            ip_addr_t ip;
            ipaddr_aton(STATION_ADDR, &ip);

            netif_set_ipaddr(netif_default, &ip);

            // Print new local address
            printf("Modified: new Pico-W IP addr: %s\n",
                   ip4addr_ntoa(netif_ip4_addr(netif_list)));
        }
    }

    // Initialize UDP recv callback function
    printf("Initializing recv callback...");
    if (udp_recv_callback_init()) {
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
    PT_SEM_INIT(&new_udp_ack_s, 0);

    // Launch multicore
    multicore_reset_core1();
    multicore_launch_core1(&core_1_main);

    // Start protothreads
    printf("Starting Protothreads on Core 0!\n");
    pt_add_thread(protothread_udp_send);
    pt_add_thread(protothread_udp_recv);
    pt_add_thread(protothread_udp_ack);
    pt_add_thread(protothread_serial);
    pt_schedule_start;

    // De-initialize the cyw43 architecture.
    cyw43_arch_deinit();

    return 0;
}
