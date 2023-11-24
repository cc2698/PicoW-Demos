
// C libraries
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Pico
#include "boards/pico_w.h"
#include "pico/cyw43_arch.h"
#include "pico/multicore.h"
#include "pico/mutex.h"
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
// #define WIFI_SSID     "picow_test"
#define WIFI_PASSWORD "password"

// UDP recv
char recv_data[UDP_MSG_LEN_MAX];
static struct udp_pcb* udp_recv_pcb;
struct pt_sem new_udp_recv_s;

// UDP send
char dest_addr_str[20] = "255.255.255.255";
static ip_addr_t dest_addr;
static struct udp_pcb* udp_send_pcb;
struct pt_sem new_udp_send_s;

// UDP ack
char return_addr_str[20] = "255.255.255.255";
static ip_addr_t return_addr;
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
 *  WIFI SCANNING
 */

// Maximum acceptable SSID length
#define SSID_LEN 50

// SSID of the access point
char target_ssid[SSID_LEN];

// If the result of the scan is a network starting with "picow"
static int scan_callback(void* env, const cyw43_ev_scan_result_t* result)
{
    if (result) {
        char header[10] = "";

        // Get first 5 characters of the SSID
        *header = '\0';
        snprintf(header, 6, "%s", result->ssid);

        if (strcmp(header, "picow") == 0) {
            // Compose MAC address
            char bssid_str[40];
            sprintf(bssid_str, "%02x:%02x:%02x:%02x:%02x:%02x",
                    result->bssid[0], result->bssid[1], result->bssid[2],
                    result->bssid[3], result->bssid[4], result->bssid[5]);

            printf("ssid: %-32s rssi: %4d chan: %3d mac: %s sec: %u\n",
                   result->ssid, result->rssi, result->channel, bssid_str,
                   result->auth_mode);

            snprintf(target_ssid, SSID_LEN, "%s", result->ssid);
        }
    }

    return 0;
}

// Initiate a wifi scan
int find_target()
{
    // Scan options don't matter
    cyw43_wifi_scan_options_t scan_options = {0};

    printf("Starting Wifi scan...");

    // This function scans for nearby Wifi networks and runs the
    // callback function each time a network is found.
    int err = cyw43_wifi_scan(&cyw43_state, &scan_options, NULL, scan_callback);

    if (err == 0) {
        printf("scan started successfully!\n");
    } else {
        printf("failed to start scan. err = %d\n", err);
        return 1;
    }

    sleep_ms(5000);

    return 0;
}

/*
 *  PACKET HANDLING
 */

// Packet types
#define NUM_PACKET_TYPES 2
const char* packet_types[NUM_PACKET_TYPES] = {"data", "ack"};

// Max length of header fields
#define TOK_LEN 40

// Structure that stores an outgoing packet
typedef struct outgoing_packet {
    char packet_type[TOK_LEN];
    char ip_addr[TOK_LEN];
    int ack_num;
    uint64_t timestamp;
    char msg[UDP_MSG_LEN_MAX];
} packet_t;

// Mutexes
struct mutex send_mutex, ack_mutex;

// Packet queues
packet_t send_queue, ack_queue;

packet_t compose_packet(char* type, char* addr, int ack, uint64_t t, char* m)
{
    packet_t op;

    snprintf(op.packet_type, TOK_LEN, "%s", type);
    snprintf(op.ip_addr, TOK_LEN, "%s", addr);
    op.ack_num   = ack;
    op.timestamp = t;
    snprintf(op.msg, UDP_MSG_LEN_MAX, "%s", m);

    return op;
}

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

// Convert a string to a packet
packet_t string_to_packet(char* s)
{
    // Not strictly necessary, but the strtok() function is destructive of
    // its inputs therefore we copy the recv buffer into a temporary
    // variable to avoid messing with it directly.
    char tbuf[UDP_MSG_LEN_MAX];
    sprintf(tbuf, s);

    packet_t op;

    char* token;

    // Data or ACK
    token = strtok(tbuf, ";");
    copy_field(op.packet_type, token);

    // Source IP address
    token = strtok(NULL, ";");
    copy_field(op.ip_addr, token);

    // ACK number
    char ack_num_str[TOK_LEN];
    token = strtok(NULL, ";");
    copy_field(ack_num_str, token);
    if (strcmp(ack_num_str, "n/a") != 0) {
        op.ack_num = atoi(ack_num_str);
    }

    // Timestamp
    char timestamp_str[TOK_LEN];
    token = strtok(NULL, ";");
    copy_field(timestamp_str, token);
    if (strcmp(timestamp_str, "n/a") != 0) {
        op.timestamp = strtoull(timestamp_str, NULL, 10);
    }

    // Contents
    token = strtok(NULL, ";");
    copy_field(op.msg, token);

    return op;
}

/*
 *  LED
 */

#define HIGH 1
#define LOW  0

volatile int led_state = LOW;

#define led_on()                                                               \
    do {                                                                       \
        led_state = HIGH;                                                      \
        cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, led_state);                 \
    } while (0);

#define led_off()                                                              \
    do {                                                                       \
        led_state = LOW;                                                       \
        cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, led_state);                 \
    } while (0);

#define led_toggle()                                                           \
    do {                                                                       \
        led_state = !led_state;                                                \
        cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, led_state);                 \
    } while (0);

/*
 *  ALARM
 */

// Alarm ID and duration
alarm_id_t led_alarm;
#define ALARM_MS 750

// Signal core 1 to turn the LED on
volatile int led_flag = false;

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
        // Blink the LED
        led_on();
        cancel_alarm(led_alarm);
        led_alarm = add_alarm_in_ms(ALARM_MS, alarm_callback, NULL, false);

        // Copy the payload into the recv buffer
        memcpy(recv_data, p->payload, UDP_MSG_LEN_MAX);

        // Free the packet buffer
        pbuf_free(p);

        // Signal waiting threads
        PT_SEM_SAFE_SIGNAL(pt, &new_udp_recv_s);
    } else {
        printf("ERROR: NULL pt in callback\n");
    }
}

// Define the recv callback function
int udp_recv_callback_init(void)
{
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

    // Outgoing packet
    static packet_t send_buf;

    // Payload
    static char buffer[UDP_MSG_LEN_MAX];
    static int udp_send_length;

    // The address of the pbuf payload
    static char* req;

    // Error code
    static err_t er;

    while (true) {
        // Wait until the buffer is written
        PT_SEM_SAFE_WAIT(pt, &new_udp_send_s);

        // Assign target pico IP address, string -> ip_addr_t
        ipaddr_aton(dest_addr_str, &dest_addr);

        // Pop the head of the queue
        mutex_enter_blocking(&send_mutex);
        send_buf = send_queue;
        mutex_exit(&send_mutex);

        // Append header to the payload
        sprintf(buffer, "%s;%s;%d;%llu;%s", send_buf.packet_type,
                send_buf.ip_addr, send_buf.ack_num, send_buf.timestamp,
                send_buf.msg);

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
        printf("|\tdest:    %s\n", send_buf.ip_addr);
        printf("|\tnum:     %d\n", send_buf.ack_num);
        printf("|\tmsg:     %s\n", send_buf.msg);
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

    // Timestamp when the packet was sent
    static uint64_t timestamp;

    // Round trip time
    static float rtt_ms;

    // Incoming packet
    static packet_t recv_buf;

    while (true) {
        // Wait until the buffer is written
        PT_SEM_SAFE_WAIT(pt, &new_udp_recv_s);

        // Convert the contents of the received packet to a packet_t
        recv_buf = string_to_packet(recv_data);

#ifndef PRINT_ON_RECV
        if (strcmp(recv_buf.packet_type, "ack") == 0) {
            printf("%3d", recv_buf.ack_num);
        }
#else
        // Print formatted packet contents
        printf("| Incoming...\n");
        printf("|\tPayload: { %s }\n", recv_data);
        printf("|\ttype:    %s\n", recv_buf.packet_type);
        printf("|\tfrom:    %s\n", recv_buf.ip_addr);
        printf("|\tack:     %d\n", recv_buf.ack_num);
        if (strcmp(recv_buf.packet_type, "data") == 0) {
            printf("|\tmsg:     %s\n", recv_buf.msg);
        } else if (strcmp(recv_buf.packet_type, "ack") == 0) {
            rtt_ms = (time_us_64() - recv_buf.timestamp) / 1000.0f;
            printf("|\tRTT:     %.2f ms\n", rtt_ms);
        } else {
            printf("|\tmsg:     %s\n", recv_buf.msg);
        }
        printf("\n");
#endif

        // If data was received, respond with ACK
        if (strcmp(recv_buf.packet_type, "data") == 0) {
            // Assign return address and ACK number
            strcpy(return_addr_str, recv_buf.ip_addr);

            // Write to the ack queue
            mutex_enter_blocking(&ack_mutex);
            ack_queue = compose_packet("ack", my_addr, recv_buf.ack_num,
                                       recv_buf.timestamp, "");
            mutex_exit(&ack_mutex);

            // Signal ACK thread
            PT_SEM_SAFE_SIGNAL(pt, &new_udp_ack_s);
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

    // Outgoing packet
    static packet_t ack_buf;

    // Payload
    static char buffer[UDP_MSG_LEN_MAX];
    static int udp_ack_length;

    // The address of the pbuf payload
    static char* req;

    // Error code
    static err_t er;

    while (true) {
        // Wait until the buffer is written
        PT_SEM_SAFE_WAIT(pt, &new_udp_ack_s);

        // Assign target pico IP address
        ipaddr_aton(return_addr_str, &return_addr);

        // Pop the head of the queue
        mutex_enter_blocking(&ack_mutex);
        ack_buf = ack_queue;
        mutex_exit(&ack_mutex);

        // Append header to the payload
        sprintf(buffer, "%s;%s;%d;%llu", ack_buf.packet_type, ack_buf.ip_addr,
                ack_buf.ack_num, ack_buf.timestamp);

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
        printf("|\tnum:     %d\n", ack_buf.ack_num);
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

        // Spawn thread for non-blocking read
        serial_read;

        mutex_enter_blocking(&send_mutex);
        send_queue = compose_packet("data", my_addr, packet_counter,
                                    time_us_64(), pt_serial_in_buffer);
        mutex_exit(&send_mutex);

        // Signal waiting threads
        PT_SEM_SAFE_SIGNAL(pt, &new_udp_send_s);
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

    pt_add_thread(protothread_udp_send);

    printf("Starting Protothreads on Core 1!\n");
    pt_schedule_start;
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
           (access_point ? "Multicore Access Point" : "Multicore Station"));

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

        // I define this locally to guarantee that station nodes don't have
        // access to it.
        const char wifi_ssid[30] = "picow_test_auto_connect";

        // Turn on access point
        cyw43_arch_enable_ap_mode(wifi_ssid, WIFI_PASSWORD,
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

        // Perform a wifi scan
        find_target();
        printf("New target SSID = %s\n", target_ssid);

        // Connect to the access point
        printf("Connecting to Wi-Fi...");
        if (cyw43_arch_wifi_connect_timeout_ms(
                target_ssid, WIFI_PASSWORD, CYW43_AUTH_WPA2_AES_PSK, 30000)) {
            printf("failed to connect.\n");
            return 1;
        } else {
            printf("connected!:\n\tSSID = %s\n\tPASS = %s\n", target_ssid,
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

    // Initialize mutexes
    mutex_init(&send_mutex);
    mutex_init(&ack_mutex);

    // The threads use semaphores to signal each other when buffers are
    // written. If a thread tries to aquire a semaphore that is unavailable,
    // it yields to the next thread in the scheduler.
    printf("Initializing send/recv semaphores...\n");
    PT_SEM_SAFE_INIT(&new_udp_send_s, 0);
    PT_SEM_SAFE_INIT(&new_udp_recv_s, 0);
    PT_SEM_SAFE_INIT(&new_udp_ack_s, 0);

    // Launch multicore
    multicore_reset_core1();
    multicore_launch_core1(&core_1_main);

    // Start protothreads
    printf("Starting Protothreads on Core 0!\n");
    pt_add_thread(protothread_udp_recv);
    pt_add_thread(protothread_udp_ack);
    pt_add_thread(protothread_serial);
    pt_schedule_start;

    // De-initialize the cyw43 architecture.
    cyw43_arch_deinit();

    return 0;
}
