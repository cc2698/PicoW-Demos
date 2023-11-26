
// C libraries
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Pico
#include "boards/pico_w.h"
#include "pico/cyw43_arch.h"
#include "pico/multicore.h"
#include "pico/stdlib.h"
#include "pico/unique_id.h"

// Hardware
#include "hardware/irq.h"
#include "hardware/timer.h"

// Protothreads
#include "pt_cornell_rp2040_v1_1_2.h"

// Lightweight IP
#include "lwip/udp.h" // Includes other necessary LwIP libraries

// DHCP
#include "dhcpserver/dhcpserver.h"

// Local
#include "packet.h"
#include "wifi_scan.h"

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

// SSID of the target access point
char target_ssid[SSID_LEN];

// My SSID
char wifi_ssid[SSID_LEN];

// UDP constants
#define UDP_PORT 4444 // Same port number on both devices

// IP addresses
#define AP_ADDR      "192.168.4.1"
#define STATION_ADDR "192.168.4.10"
#define MASK_ADDR    "255.255.255.0"

// Wifi name and password
#define WIFI_PASSWORD "password"
// #define WIFI_PASSWORD NULL

// UDP recv
char recv_data[UDP_MSG_LEN_MAX];
static struct udp_pcb* udp_recv_pcb;
struct pt_sem new_udp_recv_s;

// UDP send
packet_t send_queue;
char dest_addr_str[20] = "255.255.255.255";
static ip_addr_t dest_addr;
static struct udp_pcb* udp_send_pcb;
struct pt_sem new_udp_send_s;

// UDP ack
packet_t ack_queue;
bool ack_pending         = false;
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

TCP_SERVER_T* state;
ip4_addr_t mask;
dhcp_server_t dhcp_server;

int boot_access_point();

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

    printf("You've got mail! (received a packet)\n");

    if (p != NULL) {
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

int connected_ID = 0;
int target_ID    = 0;

// Connect to a network and set a new IP address
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
        printf("connected!:\n");
        printf("\tSSID = %-25s\tPASS = %s\n", ssid, WIFI_PASSWORD);

        // Configure target IP address
        sprintf(my_addr, STATION_ADDR);
        sprintf(dest_addr_str, "%s", AP_ADDR);

        // Print address assigned by DCHP
        printf("\tConnected as: %s - ",
               ip4addr_ntoa(netif_ip4_addr(netif_list)));

        // Set local address, override the address assigned by DHCP
        ip_addr_t ip;
        ipaddr_aton(STATION_ADDR, &ip);
        netif_set_ipaddr(netif_default, &ip);

        // Print new local address
        printf("Modified to %s\n", ip4addr_ntoa(netif_ip4_addr(netif_list)));

        return 0;
    }
}

int my_id = 0;
struct pt_sem connect_sem;
int id_token_number;

int parent_ID;

int send_queue_id;

bool signal_connect_thread = false;

bool found_neighbors = false;

// =================================================
// Connection managing thread
// =================================================
static PT_THREAD(protothread_connect(struct pt* pt))
{
    PT_BEGIN(pt);

    static int err;

    static char id_token[TOK_LEN];
    static packet_t token_packet;
    static char msg_buf[TOK_LEN];

    while (true) {
        PT_YIELD_UNTIL(pt, signal_connect_thread && !ack_pending);

        // Reset the signal flag
        signal_connect_thread = false;

        printf("\n\nBegin reconnect:\n");
        printf("access_point = %d\n", access_point);
        printf("My ID number = %d\n", my_id);
        printf("My wifi name = %s\n", wifi_ssid);
        printf("connected_id = %d; target_id = %d; ack_pending = %d\n",
               connected_ID, target_ID, ack_pending);

        if (access_point) {
            // Switch to station mode
            printf("Switch to station mode!\n");
            cyw43_arch_disable_ap_mode();

            // Free the TCP_SERVER state
            free(state);

            // Disable the DHCP server
            dhcp_server_deinit(&dhcp_server);

            // Re-initialize Wifi chip
            cyw43_arch_deinit();
            printf("Initializing cyw43...");
            if (cyw43_arch_init()) {
                printf("failed to initialise.\n");
                return 1;
            } else {
                printf("initialized!\n");
            }

            cyw43_arch_enable_sta_mode();
            access_point = false;

            // Set dest addr to the access point
            sprintf(dest_addr_str, "%s", AP_ADDR);

            if (target_ID == -1) {
                printf("Giving time for nearby APs to boot");
                for (int i = 0; i < 10; i++) {
                    printf(".");
                    sleep_ms(150);
                }
                printf("\n");

                // Scan for targets
                scan_wifi();

                if (pidogs_found) {
                    // If pidogs found, copy the result into target_ssid
                    snprintf(target_ssid, SSID_LEN, "%s", scan_result);

                    if (connect_to_network(target_ssid) == 0) {

                        // TODO: Mark as child node

                        // Compose token to send
                        sprintf(msg_buf, "%d", id_token_number);
                        token_packet =
                            new_packet("token", -1, my_id, my_addr,
                                       packet_counter, time_us_64(), msg_buf);

                        // Enqueue packet (or is this done by recv thread?)
                        send_queue = token_packet;

                        // Signal send thread
                        PT_SEM_SAFE_SIGNAL(pt, &new_udp_send_s);
                    }
                } else {
                    printf("No pidogs found\n");
                    found_neighbors = true;

                    if (my_id == 1) {
                        // Master node has received token back, and has no
                        // uninitialized neighbors.

                        signal_connect_thread = true;
                        target_ID             = 0;
                    } else {
                        // If no pidogs (uninitialized nodes) were found, hand
                        // the token back to the parent node
                        printf("Changing target ID: %d --> ", target_ID);
                        target_ID = parent_ID;
                        printf("%d\n", target_ID);

                        printf("\nNO UNINITIALIZED NEIGHBORS, HAND TOKEN "
                               "BACKWARDS\n\n");
                    }
                }
            }

            if (target_ID > 0) {
                // Connect to someone else's network
                snprintf(target_ssid, SSID_LEN, "picow_%d", target_ID);

                if (connect_to_network(target_ssid) == 0) {
                    // If successful, change the connected_id number
                    connected_ID = target_ID;

                    printf("connected id: %d\n", connected_ID);
                    printf("dest_addr: %s\n", dest_addr_str);

                    // Compose token to send
                    sprintf(msg_buf, "%d", id_token_number);
                    token_packet =
                        new_packet("token", target_ID, my_id, my_addr,
                                   packet_counter, time_us_64(), msg_buf);

                    // Enqueue packet (or is this done by recv thread?)
                    send_queue = token_packet;

                    // Signal send thread
                    PT_SEM_SAFE_SIGNAL(pt, &new_udp_send_s);
                }
            }
        } else {
            if (target_ID == 0) {

                printf("Switch to AP mode!\n");
                cyw43_arch_disable_sta_mode();

                snprintf(wifi_ssid, SSID_LEN, "picow_%d", my_id);

                // Re-initialize Wifi chip
                cyw43_arch_deinit();
                printf("Initializing cyw43...");
                if (cyw43_arch_init()) {
                    printf("failed to initialise.\n");
                    return 1;
                } else {
                    printf("initialized!\n");
                }

                boot_access_point();

                connected_ID = 0;
            }
        }

        // Re-initialize UDP recv callback function
        udp_remove(udp_recv_pcb);
        printf("Initializing recv callback...");
        if (udp_recv_callback_init()) {
            printf("receive callback failed to initialize.");
        } else {
            printf("success!\n");
        }

        printf("\nEnd reconnect:\n");
        printf("access_point = %d\n", access_point);
        printf("My ID number = %d\n", my_id);
        printf("My wifi name = %s\n", wifi_ssid);
        printf("connected_id = %d; target_id = %d; ack_pending = %d\n\n\n",
               connected_ID, target_ID, ack_pending);

        // Print list of neighbors
        if (found_neighbors && access_point) {
            printf("\x1b[32m");
            printf("NEIGHBOR SEARCH RESULTS:\n");
            printf("My ID number: %d\n", my_id);
            printf("My neighbors: ");
            for (int i = 0; i < MAX_NODES; i++) {
                if (is_nbr[i]) {
                    printf("%d ", i);
                }
            }
            printf("\n");
            printf("\x1b[0m");
        }

        PT_YIELD(pt);
    }

    PT_END(pt);
}

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
        send_buf = send_queue;

        // Convert to string
        packet_to_str(buffer, send_buf);

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
        print_packet(buffer, send_buf);
        printf("\n");
#endif

        // Print new local address
        printf("What i think is the dest addr: %s\n", ip4addr_ntoa(&dest_addr));

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

    static char return_msg[10];

    static bool is_data, is_ack, is_token;

    while (true) {
        // Wait until the buffer is written
        PT_SEM_SAFE_WAIT(pt, &new_udp_recv_s);

        // Convert the contents of the received packet to a packet_t
        recv_buf = str_to_packet(recv_data);

        is_data = is_ack = is_token = false;
        if (strcmp(recv_buf.packet_type, "data") == 0) {
            is_data = true;
        } else if (strcmp(recv_buf.packet_type, "ack") == 0) {
            is_ack = true;
        } else if (strcmp(recv_buf.packet_type, "token") == 0) {
            printf("is a token\n");
            is_token = true;
        }

        printf("Recv type: data = %d, ack = %d, token = %d\n", is_data, is_ack,
               is_token);

#ifndef PRINT_ON_RECV
        if (strcmp(recv_buf.packet_type, "ack") == 0) {
            printf("%3d", recv_buf.ack_num);
        }
#else
        // Print formatted packet contents
        printf("| Incoming...\n");
        print_packet(recv_data, recv_buf);
        if (strcmp(recv_buf.packet_type, "ack") == 0) {
            rtt_ms = (time_us_64() - recv_buf.timestamp) / 1000.0f;
            printf("|\tRTT:     %.2f ms\n", rtt_ms);
        }
        printf("\n");
#endif

        // If data or token was received, respond with ACK
        if (is_data || is_token) {
            // Assign return address and ACK number
            strcpy(return_addr_str, recv_buf.ip_addr);

            // Write to the ack queue
            printf("is token: %d\n", is_token);
            ack_queue = new_packet("ack", recv_buf.src_id, my_id, my_addr,
                                   recv_buf.ack_num, recv_buf.timestamp,
                                   recv_buf.packet_type);

            // Signal ACK thread
            PT_SEM_SAFE_SIGNAL(pt, &new_udp_ack_s);
            ack_pending = true;
        }

        // Determine if ack'ing token
        if (is_ack) {
            if (strcmp(recv_buf.msg, "token") == 0) {
                printf("token ack'ed\n");
                // Signal connect thread to re-enable AP mode
                target_ID             = 0;
                signal_connect_thread = true;

                printf("connected_id = %d; target_id = %d; ack_pending = %d\n",
                       connected_ID, target_ID, ack_pending);
            }
        }

        // Assign ID number
        if (is_token) {
            printf("is a token\n");

            // Convert the token number into an integer
            id_token_number = atoi(recv_buf.msg);

            // If I don't have an ID yet, give myself one
            if (my_id == 0) {
                printf("Parent ID before = %d\n", parent_ID);
                printf("My ID before = %d\n", my_id);

                // Parent is whoever gave you the token
                parent_ID = recv_buf.src_id;
                // Increment the token number
                my_id = ++id_token_number;

                printf("Parent ID after = %d\n", parent_ID);
                printf("My ID after = %d\n", my_id);

                printf("connected_id = %d; target_id = %d; ack_pending = %d\n",
                       connected_ID, target_ID, ack_pending);
            }

            // Signal connect thread to scan for neighbors
            target_ID             = -1;
            signal_connect_thread = true;
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
        ack_buf = ack_queue;

        // Convert to string
        packet_to_str(buffer, ack_buf);

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
        print_packet(buffer, ack_buf);
        printf("\n");
#endif

        // Send packet
        // cyw43_arch_lwip_begin();
        er = udp_sendto(udp_ack_pcb, p, &return_addr, UDP_PORT);
        // cyw43_arch_lwip_end();

        ack_pending = false;

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
        // Yielding here is not strictly necessary but it gives a little bit
        // of slack for the async processes so that the output is in the
        // correct order (most of the time).
        //      - Bruce Land
        PT_YIELD_usec(1e5);

        // Spawn thread for non-blocking read
        serial_read;

        printf("dest_addr: %s\n", dest_addr_str);

        if (strcmp(pt_serial_in_buffer, "token") == 0) {
            send_queue = new_packet("token", target_ID, my_id, my_addr,
                                    packet_counter, time_us_64(), "1");
        } else if (strcmp(pt_serial_in_buffer, "nbr") == 0) {
            // Print list of neighbors
            printf("My neighbors (by ID): ");
            for (int i = 0; i < MAX_NODES; i++) {
                if (is_nbr[i]) {
                    printf("%d ", i);
                }
            }
            printf("\n");
        } else {
            send_queue =
                new_packet("data", target_ID, my_id, my_addr, packet_counter,
                           time_us_64(), pt_serial_in_buffer);
        }

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
}

/*
 *  CORE 0 MAIN
 */

// Boot up the access point, returns 0 on success.
int boot_access_point()
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

    // Turn on access point
    cyw43_arch_enable_ap_mode(wifi_ssid, WIFI_PASSWORD,
                              CYW43_AUTH_WPA2_AES_PSK);
    printf("Access point mode enabled!\n");
    printf("\tSSID = %s\n", wifi_ssid);

    // The variable 'state' is a pointer to a TCP_SERVER_T struct
    // Set up the access point IP address and mask
    ipaddr_aton(AP_ADDR, ip_2_ip4(&state->gw));
    ipaddr_aton(MASK_ADDR, ip_2_ip4(&mask));

    // Configure target IP address
    sprintf(my_addr, AP_ADDR);
    sprintf(dest_addr_str, "%s", STATION_ADDR);

    // Start the Dynamic Host Configuration Protocol (DHCP) server. Even
    // though in the program DHCP is not required, LwIP seems to need
    // it!
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
    printf("My IPv4 addr: %s\n", ip4addr_ntoa(netif_ip4_addr(netif_list)));

    access_point = true;

    return 0;
}

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
    my_id        = 1;
#endif

    // Print out whether you're an AP or a station
    printf("\n\n==================== %s ====================\n\n",
           (access_point ? "NF Node v2" : "NF Master v2"));

    // Initialize Wifi chip
    printf("Initializing cyw43...");
    if (cyw43_arch_init()) {
        printf("failed to initialise.\n");
        return 1;
    } else {
        printf("initialized!\n");
    }

    if (access_point) {
        // Initialize as pidog_<hex ID>
        char unique_board_id[20];
        pico_get_unique_board_id_string(
            unique_board_id, 2 * PICO_UNIQUE_BOARD_ID_SIZE_BYTES + 2);
        snprintf(wifi_ssid, SSID_LEN, "pidog_%s", unique_board_id);

        boot_access_point();

    } else {
        // If all Pico-Ws boot at the same time, this delay gives the access
        // point time to setup before the client tries to connect.
        sleep_ms(1000);

        // Enable station mode
        cyw43_arch_enable_sta_mode();
        printf("Station mode enabled!\n");

        // Perform a wifi scan
        printf("Current target SSID = %s\n", target_ssid);

        scan_wifi();

        snprintf(target_ssid, SSID_LEN, "%s", scan_result);

        printf("New target SSID = %s\n", target_ssid);

        // Connect to the access point
        connect_to_network(target_ssid);
    }

    // Initialize UDP recv callback function
    printf("Initializing recv callback...");
    if (udp_recv_callback_init()) {
        printf("receive callback failed to initialize.");
    } else {
        printf("success!\n");
    }

    // The threads use semaphores to signal each other when buffers are
    // written. If a thread tries to aquire a semaphore that is unavailable,
    // it yields to the next thread in the scheduler.
    PT_SEM_SAFE_INIT(&new_udp_send_s, 0);
    PT_SEM_SAFE_INIT(&new_udp_recv_s, 0);
    PT_SEM_SAFE_INIT(&new_udp_ack_s, 0);

    // Launch multicore
    multicore_reset_core1();
    multicore_launch_core1(&core_1_main);

    // Start protothreads
    printf("Starting Protothreads on Core 0!\n\n");
    pt_add_thread(protothread_udp_send);
    pt_add_thread(protothread_udp_recv);
    pt_add_thread(protothread_udp_ack);
    pt_add_thread(protothread_serial);
    pt_add_thread(protothread_connect);
    pt_schedule_start;

    // De-initialize the cyw43 architecture.
    cyw43_arch_deinit();

    return 0;
}
