
// C libraries
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Pico
#include "boards/pico_w.h"
#include "pico/cyw43_arch.h"
#include "pico/multicore.h"
#include "pico/rand.h"
#include "pico/stdlib.h"

// Hardware
#include "hardware/irq.h"
#include "hardware/timer.h"

// Protothreads
#include "protothreads/pt_cornell_rp2040_v1_1_2.h"

// Lightweight IP
#include "lwip/udp.h" // Includes other necessary LwIP libraries

// DHCP
#include "dhcpserver/dhcpserver.h"

// Local
#include "connect.h"
#include "distance_vector.h"
#include "node.h"
#include "packet.h"
#include "utils.h"
#include "wifi_scan.h"

/********************************
 *  DEBUGGING
 ********************************/

#define PRINT_ON_RECV
#define PRINT_ON_SEND

/********************************
 *  UDP
 ********************************/

// UDP constants
#define UDP_PORT 4444 // Same port number on both devices

// UDP recv
char recv_data[UDP_MSG_LEN_MAX];
static struct udp_pcb* udp_recv_pcb;
struct pt_sem new_udp_recv_s;

// UDP send
packet_t send_queue;
static ip_addr_t dest_addr;
static struct udp_pcb* udp_send_pcb;
bool signal_send_thread = false;

// UDP ack
packet_t ack_queue;
bool ack_queue_empty              = true;
char return_addr_str[IP_ADDR_LEN] = "255.255.255.255";
static ip_addr_t return_addr;
static struct udp_pcb* udp_ack_pcb;
struct pt_sem new_udp_ack_s;

// UDP recv callback function
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
        // - netif_ip4_addr returns the pico-w ip address
        err = udp_bind(udp_recv_pcb, netif_ip4_addr(netif_list),
                       UDP_PORT); // DHCP addr

        if (err == ERR_OK) {
            // This function assigns the callback function for when a UDP
            // packet is received
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

/************************************************
 *  WIFI CONNECT / DISCONNECT
 ************************************************/

// Time allowed for AP to boot back up
#define AP_BOOT_TIME 1000

// Signal protothread_connect that a new connection needs to be made
bool signal_connect_thread = false;

// ID to target during connection. Can take special values
int connected_ID = ENABLE_AP;
int target_ID    = ENABLE_AP;

// SSID of the target access point
char target_ssid[SSID_LEN];

/************************************************
 *  STATE
 ************************************************/

// Modes of the network
typedef enum phase {
    DO_NOTHING,
    NB_FINDING,
    DV_ROUTING
} phase_t;

phase_t phase = DO_NOTHING;

/************************************************
 *  ROUTING
 ************************************************/

// Cooldown min and max durations in microseconds (us)
#define COOLDOWN_MIN (15 * 1e6)
#define COOLDOWN_MAX (30 * 1e6)

// Scan scheduling macros
#define NO_SCAN   UINT64_MAX
#define SCAN_ASAP 0

// Time of next routing scan
uint64_t next_dv_scan = NO_SCAN;

// Generate a random uint64_t in the range [min, max]
uint64_t rand_uint64(uint64_t min, uint64_t max)
{
    float rand = (float) (get_rand_32()) / UINT32_MAX;
    return (uint64_t) (min + (max - min) * rand);
}

/************************************************
 *	THREADS
 ************************************************/

// Re-initialize the Wifi chip when switching between AP and station modes.
// Limited amounts of testing suggests that this isn't necessary but I'm leaving
// the option here in just in case you need to turn it back on.
#define RE_INIT_CYW43_BTW_MODES false

// =================================================
// Connection managing thread
// =================================================
static PT_THREAD(protothread_connect(struct pt* pt))
{
    PT_BEGIN(pt);

    // Error code for connect_to_network()
    static int connect_err;

    // Destination ID
    int dest_ID;

    // Buffer for composing messages
    static char msg_buf[TOK_LEN];

    // Amount of time to stay in AP mode after scanning and not seeing anyone
    uint64_t cooldown_usec;

    while (true) {

        // Wait until signalled AND there are no pending ACKs
        PT_YIELD_UNTIL(pt,
                       (signal_connect_thread || time_us_64() > next_dv_scan)
                           && ack_queue_empty);

        printf("\n========== CONNECT THREAD ==========\n");
        // printf("target_ID: %d\n", target_ID);

        // If the thread was triggered by a scan, set target to scan
        if (!signal_connect_thread && time_us_64() > next_dv_scan) {
            printf("Scan scheduled:\n");
            printf("\tcurr time    = %.1f sec\n", (float) time_us_64() / 1e6);
            printf("\tnext_dv_scan = %.1f sec\n", (float) next_dv_scan / 1e6);

            target_ID = DV_SCAN;
        }

        printf("target_ID: %d\n", target_ID);

        signal_connect_thread = false;

        // Reset error code
        connect_err = 0;

        /************************************************
         *  Toggle connection state
         ************************************************/

        if (target_ID != ENABLE_AP) {
            // Set mode to station mode
            if (access_point) {
                shutdown_ap();
#if RE_INIT_CYW43_BTW_MODES
                re_init_cyw43();
#endif
                boot_station();

                // Set dest addr to the access point
                snprintf(dest_addr_str, IP_ADDR_LEN, "%s", AP_ADDR);
            } else {
                shutdown_station();
                boot_station();
            }
        } else if (target_ID == ENABLE_AP) {
            if (!access_point) {
                shutdown_station();
#if RE_INIT_CYW43_BTW_MODES
                re_init_cyw43();
#endif
                // Turn on the access point
                generate_picow_ssid(self.wifi_ssid, self.ID);

                // Only set connected_ID if the AP successfully booted
                if (boot_ap() == 0) {
                    connected_ID = target_ID;
                }
            }
        }

        /************************************************
         *  Perform scan or connect behavior
         ************************************************/

        if (target_ID == DV_SCAN) {

            if (num_unupdated_nbrs(&self) == 0 && phase == DV_ROUTING) {
                print_dist_vector(&self, self.ID);
                print_routing_table(&self);

                next_dv_scan = NO_SCAN;

                // Signal for AP mode
                target_ID             = ENABLE_AP;
                signal_connect_thread = true;

                // Don't scan anymore until your DV updates
                phase = DO_NOTHING;

            } else {
                scan_wifi(DV_ROUTE_SCAN);

                if (routing_scan_result != NULL) {
                    dest_ID = routing_scan_result->ID;

                    // Load my distance vector into the send queue
                    dv_to_str(msg_buf, &self, dest_ID, self.dist_vector, true);
                    send_queue =
                        new_packet("dv", dest_ID, self.ID, self.ip_addr,
                                   self.counter, time_us_64(), msg_buf);
                    signal_send_thread = true;

                    // Signal for a reconnection
                    printf("Changing target_ID: %d", target_ID);
                    target_ID = dest_ID;
                    printf(" --> %d\n", target_ID);

                    signal_connect_thread = true;

                    next_dv_scan = time_us_64() + COOLDOWN_MIN;

                } else {
                    // Stay in AP mode for a random number of microseconds
                    cooldown_usec = rand_uint64(COOLDOWN_MIN, COOLDOWN_MAX);
                    next_dv_scan  = time_us_64() + cooldown_usec;
                    printf("Waiting %.1f sec before scanning again (random)\n",
                           (float) cooldown_usec / 1e6);

                    // Signal for AP mode
                    target_ID             = ENABLE_AP;
                    signal_connect_thread = true;
                }
            }

        } else if (target_ID == NF_SCAN) {

            // Give time for whoever sent you the token to boot back up
            printf("Waiting for nearby APs to boot:\n\t");
            sleep_ms_progress_bar(AP_BOOT_TIME, 30);

            // Scan for targets
            scan_wifi(NBR_FIND_SCAN);

            if (pidogs_found) {
                // Copy the result into target_ssid
                snprintf(target_ssid, SSID_LEN, "%s", nbr_find_scan_result);

                // Try to connect to wifi
                connect_err = connect_to_network(target_ssid);
            } else {
                // Flag that neighbors have been recorded
                if (!self.knows_nbrs) {
                    self.knows_nbrs = true;

                    init_dist_vector_routing(&self);
                }

                if (self.ID == MASTER_ID) {
                    // Master node has received token back, and has no
                    // uninitialized neighbors.
                    target_ID             = ENABLE_AP;
                    signal_connect_thread = true;

                    // (Placeholder) Dequeue the token from the send thread
                    signal_send_thread = false;
                } else {
                    // If node is not the master node, hand the token
                    // back to the parent node
                    target_ID             = self.parent_ID;
                    signal_connect_thread = true;

                    print_green;
                    printf("\nNO UNINITIALIZED NEIGHBORS, SEND TOKEN BACK TO "
                           "PARENT NODE\n\n");
                    print_reset;
                }
            }

        } else if (target_ID >= CONNECT_TO_AP) {

            // Connect to someone else's network
            generate_picow_ssid(target_ssid, target_ID);

            // Try to connect to wifi
            connect_err = connect_to_network(target_ssid);

            // Update time of last contact
            if (phase == DV_ROUTING && self.nbrs[target_ID] != NULL) {
                self.nbrs[target_ID]->last_contact = time_us_64();
            }

            if (connect_err == 0) {
                // If successful, change the connected_id number
                connected_ID = target_ID;
            } else {
                // If failed, go back to AP mode
                target_ID             = ENABLE_AP;
                signal_connect_thread = true;

                if (phase == DV_ROUTING) {
                    next_dv_scan = time_us_64() + COOLDOWN_MIN;
                }
            }
        } else if (target_ID != ENABLE_AP) {
            // Invalid target error
            print_red;
            printf("ERROR: ");
            print_reset;
            printf("target_ID = %d\n", target_ID);
        }

        /************************************************
         *  Re-initialize UDP
         ************************************************/

        // Re-initialize UDP recv callback function
        udp_remove(udp_recv_pcb);
        printf("Initializing recv callback...");
        if (udp_recv_callback_init()) {
            printf("receive callback failed to initialize.");
        } else {
            printf("success!\n");
        }

        // Print the results of neighbor finding
        if (phase == NB_FINDING && self.knows_nbrs && access_point) {
            print_neighbors();

            // Print distance vector and routing table
            for (int i = 0; i < MAX_NODES; i++) {
                print_dist_vector(&self, i);
            }

            print_routing_table(&self);

            phase = DO_NOTHING;
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
    udp_send_pcb              = udp_new();
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

        PT_YIELD_UNTIL(pt, signal_send_thread && !signal_connect_thread
                               && !access_point);
        signal_send_thread = false;

        printf("\n========== SEND THREAD ==========\n");

        // Assign target pico IP address, string -> ip_addr_t
        ipaddr_aton(dest_addr_str, &dest_addr);

        // Pop the head of the queue
        send_buf = send_queue;

        // Set the return IP address of the packet
        snprintf(send_buf.ip_addr, IP_ADDR_LEN, "%s", self.ip_addr);

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
        print_orange;
        printf("| Outgoing...\n");
        print_packet(buffer, send_buf);
        print_reset;
#endif

        if (!access_point) { // Print destination addr
            printf("Destination IPv4 addr: %s\n", ip4addr_ntoa(&dest_addr));
        } else {
            print_red;
            printf("ERROR: ");
            print_reset;
            printf("An access point is using the send thread!\n");
        }

        // Send packet
        cyw43_arch_lwip_begin();
        er = udp_sendto(udp_send_pcb, p, &dest_addr, UDP_PORT);
        cyw43_arch_lwip_end();

        if (er == ERR_OK) {
            self.counter++;
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

    // Datatype of the received packet
    static bool is_data, is_ack, is_token, is_dv;
    static bool ack_is_data, ack_is_token, ack_is_dv;

    // The ID number specified by the token
    int token_id_number;

    // Buffer for composing messages
    static char msg_buf[UDP_MSG_LEN_MAX];

    static bool dv_updated = false;

    while (true) {
        // Wait until the buffer is written
        PT_SEM_SAFE_WAIT(pt, &new_udp_recv_s);

        printf("\n========== RECEIVE THREAD ==========\n");

        // Convert the contents of the received packet to a packet_t
        recv_buf = str_to_packet(recv_data);

        // Determine the packet type
        is_data  = (strcmp(recv_buf.packet_type, "data") == 0);
        is_ack   = (strcmp(recv_buf.packet_type, "ack") == 0);
        is_token = (strcmp(recv_buf.packet_type, "token") == 0);
        is_dv    = (strcmp(recv_buf.packet_type, "dv") == 0);

#ifndef PRINT_ON_RECV
        // Print ack
        if (is_ack) {
            printf("Received ack for %3d", recv_buf.ack_num);
        }
#else
        // Print formatted packet contents
        if (recv_buf.dest_id == self.ID && !is_ack) {
            print_green;
        } else {
            print_cyan;
        }
        printf("| Incoming...\n");
        print_packet(recv_data, recv_buf);
        if (is_ack) {
            rtt_ms = (time_us_64() - recv_buf.timestamp) / 1000.0f;
            printf("|\tRTT:       %.2f ms\n", rtt_ms);
        }
        print_reset;
#endif

        // Forward the packet to the next hop router
        if (is_data && recv_buf.dest_id != self.ID) {
            send_queue         = recv_buf;
            send_queue.src_id  = self.ID;
            signal_send_thread = true;

            // Request reconnection
            target_ID             = self.routing_table[recv_buf.dest_id];
            signal_connect_thread = true;
        }

        /************************************************
         *  Type-specific behavior
         ************************************************/

        // If data or token was received, respond with ACK
        if (!is_ack) {
            // Assign return address
            strcpy(return_addr_str, recv_buf.ip_addr);

            // Write to the ack queue
            ack_queue = new_packet("ack", recv_buf.src_id, self.ID,
                                   self.ip_addr, recv_buf.ack_num,
                                   recv_buf.timestamp, recv_buf.packet_type);

            // Signal ACK thread
            PT_SEM_SAFE_SIGNAL(pt, &new_udp_ack_s);
            ack_queue_empty = false;
        }

        if (is_ack) {
            ack_is_data  = (strcmp(recv_buf.msg, "data") == 0);
            ack_is_token = (strcmp(recv_buf.msg, "token") == 0);
            ack_is_dv    = (strcmp(recv_buf.msg, "dv") == 0);

            if (ack_is_data) {
                printf("Data has been ack'ed\n");

                // Signal connect thread to re-enable AP mode
                target_ID             = ENABLE_AP;
                signal_connect_thread = true;
            } else if (ack_is_token) {
                printf("Token has been ack'ed\n");

                led_off();

                // Signal connect thread to re-enable AP mode
                target_ID             = ENABLE_AP;
                signal_connect_thread = true;
            } else if (ack_is_dv) {
                printf("DV has been ack'ed\n");

                self.nbrs[recv_buf.src_id]->up_to_date   = true;
                self.nbrs[recv_buf.src_id]->last_contact = time_us_64();

                // If you successfully sent a DV, try sending another one out
                // immediately.
                //
                // The normal DV algorithm broadcasts to all neighbors
                // simultaneously. The Picos cannot acheive this so this is the
                // closest I can get.
                target_ID    = DV_SCAN;
                next_dv_scan = SCAN_ASAP;
            }
        }

        if (is_token) {
            printf("Received the token\n");

            led_on();

            phase = NB_FINDING;

            token_id_number = atoi(recv_buf.msg);

            if (self.ID == DEFAULT_ID) {
                // Give myself an ID, increment the token
                printf("Assigning myself an ID number:\n");
                printf("\tMy ID:     %3d --> ", self.ID);
                self.ID = token_id_number++;
                printf("%3d\n", self.ID);

#ifdef USE_LAYOUT
                // Register my physical ID
                ID_to_phys_ID[self.ID] = self.physical_ID;
#endif

                // Parent node is whoever gave you the token
                printf("\tParent ID: %3d --> ", self.parent_ID);
                self.parent_ID = recv_buf.src_id;
                printf("%3d\n", self.parent_ID);
            }

            // Place incremented token in the send queue
            snprintf(msg_buf, TOK_LEN, "%d", token_id_number);
            send_queue = new_packet("token", target_ID, self.ID, STATION_ADDR,
                                    self.counter, time_us_64(), msg_buf);
            signal_send_thread = true;

            // Signal connect thread to scan for neighbors
            target_ID             = NF_SCAN;
            signal_connect_thread = true;

        } else if (is_dv) {
            phase = DV_ROUTING;

            // Store the distance vector
            str_to_dv(&self, recv_buf.src_id, recv_buf.msg);

            dv_updated = update_dist_vector_by_nbr_id(&self, recv_buf.src_id);

            // Delay sending your DV out in case more people try to send you DVs
            printf("Delaying next scan: (old) %.1f sec ",
                   (float) next_dv_scan / 1e6);
            next_dv_scan = time_us_64() + (10 * 1e6);
            printf("--> %.1f sec (new)\n", (float) next_dv_scan / 1e6);
            printf("\tcurrent time = %.1f sec\n", (float) time_us_64() / 1e6);

            // Print DV and neighbor's DV
            print_dist_vector(&self, recv_buf.src_id);
            print_dist_vector(&self, self.ID);

            print_routing_table(&self);
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
    udp_ack_pcb              = udp_new();
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

        printf("\n========== ACK THREAD ==========\n");

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
        print_cyan;
        printf("| Outgoing...\n");
        print_packet(buffer, ack_buf);
        print_reset;
#endif

        // Send packet
        cyw43_arch_lwip_begin();
        er = udp_sendto(udp_ack_pcb, p, &return_addr, UDP_PORT);
        cyw43_arch_lwip_end();

        // The thread protothread_connect waits on this flag
        ack_queue_empty = true;

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

    // Tokenizing
    char tbuf[UDP_MSG_LEN_MAX];
    char* token;

    // Destination ID
    int dest_ID;

    // Buffer for composing messages
    char msg_buffer[UDP_MSG_LEN_MAX];

    while (true) {
        // Yielding here is not strictly necessary but it gives a little bit
        // of slack for the async processes so that the output is in the
        // correct order (most of the time).
        //      - Bruce Land
        PT_YIELD_usec(1e5);

        // Spawn thread for non-blocking read
        serial_read;

        printf("\n========== SERIAL THREAD ==========\n");
        print_bold;
        printf("USER INPUT >>> %s\n", pt_serial_in_buffer);
        print_reset;

        if (strcmp(pt_serial_in_buffer, "token") == 0) {
            led_on();

            // Load the token into the send queue
            send_queue = new_packet("token", target_ID, self.ID, self.ip_addr,
                                    self.counter, time_us_64(), "1");
            signal_send_thread = true;

        } else if (strcmp(pt_serial_in_buffer, "dv") == 0) {
            next_dv_scan = SCAN_ASAP;
        } else {
            snprintf(tbuf, UDP_MSG_LEN_MAX, "%s", pt_serial_in_buffer);

            // Parse input for a dest. ID and a msg
            token   = strtok(tbuf, "-");
            dest_ID = atoi(token);
            token   = strtok(NULL, "-");
            snprintf(msg_buffer, UDP_MSG_LEN_MAX, "%s", token);

            print_bold;
            printf("\tdest ID = %d\n", dest_ID);
            printf("\tmessage = %s\n", msg_buffer);
            print_reset;

            send_queue = new_packet("data", dest_ID, self.ID, self.ip_addr,
                                    self.counter, time_us_64(), msg_buffer);
            signal_send_thread = true;

            // Signal for a reconnection
            target_ID             = self.routing_table[dest_ID];
            signal_connect_thread = true;
        }
    }

    PT_END(pt);
}

/********************************
 *  CORE 1 MAIN
 ********************************/

void core_1_main()
{
    printf("Core 1 launched!\n");
}

/********************************
 *  CORE 0 MAIN
 ********************************/

int main()
{
// If this pico is the master node, set is_master to true. The macro is defined
// at compile-time by CMake allowing for the same file to be compiled into two
// separate binaries, one for the access point and one for the station.
#ifdef MASTER
    bool is_master = true;
#else
    bool is_master = false;
#endif

    // Initialize all stdio types
    stdio_init_all();

#ifdef SERIAL_OVER_USB
    // Press ENTER to start if using serial over USB. This gives you time to
    // restart the PuTTY terminal before initialization starts.
    getchar();
#endif

    // Print anything output during main() in bold
    print_reset;
    print_bold;

    // Print out whether you're an AP or a station
    printf("\n\n==================== DV Routing %s v2 ====================\n\n",
           (is_master ? "Master" : "Node"));

#ifdef USE_LAYOUT
    print_yellow;

    // Initialize the connectivity array
    init_layout();

    if (is_master) {
        // Print out the network adjacency list
        print_adj_list(DEFAULT_ID, true);
    }

    print_white;
#endif

    // Initialize this node
    self = new_node(is_master);

    // Initialize Wifi chip
    printf("Initializing cyw43...");
    if (cyw43_arch_init()) {
        print_red;
        printf("failed.\n");
        print_reset;

        return 1;
    } else {
        printf("initialized!\n");
    }

    if (is_master) {
        // If all Pico-Ws boot at the same time, this delay gives the other
        // nodes time to setup before the master tries to scan.
        printf("Waiting for nearby APs to boot:\n\t");
        sleep_ms_progress_bar(2 * AP_BOOT_TIME, 30);

        boot_station();

        // Perform a wifi scan, copy the result to target_ssid
        scan_wifi(NBR_FIND_SCAN);
        snprintf(target_ssid, SSID_LEN, "%s", nbr_find_scan_result);

        connect_to_network(target_ssid);

    } else {
        boot_ap();
    }

    // Initialize UDP recv callback function
    printf("Initializing recv callback...");
    if (udp_recv_callback_init()) {
        print_red;
        printf("failed.\n");
        print_reset;
    } else {
        printf("success!\n");
    }

    // Some threads use semaphores to signal each other when buffers are
    // written. If a thread tries to aquire a semaphore that is unavailable,
    // it yields to the next thread in the scheduler.
    PT_SEM_SAFE_INIT(&new_udp_recv_s, 0);
    PT_SEM_SAFE_INIT(&new_udp_ack_s, 0);

    // Launch multicore
    multicore_reset_core1();
    multicore_launch_core1(&core_1_main);

    // Start protothreads
    printf("Starting Protothreads on Core 0!\n\n");
    print_reset;

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
