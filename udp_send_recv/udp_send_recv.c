/*
 * Should compile into two separate binaries, one that is the sender and one
 * that is the receiver.
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

// Hardware
#include "hardware/gpio.h"
#include "hardware/spi.h"
#include "hardware/sync.h"
#include "hardware/timer.h"
#include "hardware/uart.h"

// Lightweight IP
#include "lwip/debug.h"
#include "lwip/dns.h"
#include "lwip/netif.h"
#include "lwip/opt.h"
#include "lwip/pbuf.h"
#include "lwip/stats.h"
#include "lwip/tcp.h"
#include "lwip/udp.h"

// DHCP
#include "dhcpserver/dhcpserver.h"

#define AP 0
// #define STA 1

// UDP constants
#define UDP_PORT        4444 // Same port number on both devices
#define UDP_MSG_LEN_MAX 1400

// IP addresses
#define AP_ADDR      "192.168.4.10"
#define STATION_ADDR "192.168.4.1"
char udp_target_pico[20] = "255.255.255.255";

// Wifi name and password
#define WIFI_SSID     "picow_test"
#define WIFI_PASSWORD "password"

// UDP send/recv constructs
char recv_data[UDP_MSG_LEN_MAX];
char send_data[UDP_MSG_LEN_MAX];
struct pt_sem new_udp_send_s, new_udp_recv_s;

// Is the device paired?
int paired = false;

/*
 *	UDP CALLBACK SETUP
 */

// UDP recv function
void udpecho_raw_recv(void* arg, struct udp_pcb* upcb, struct pbuf* p,
                      const ip_addr_t* addr, u16_t port)
{
    printf("recv func\n");
    // TODO: I don't know what this does
    LWIP_UNUSED_ARG(arg);

    if (p != NULL) {
        // printf("p payload in call back: = %s\n", p->payload);

        // Copy the payload into the recv buffer
        memcpy(recv_data, p->payload, UDP_MSG_LEN_MAX);

        /* Free the pbuf */
        pbuf_free(p);

        // // can signal from an ISR -- BUT NEVER wait in an ISR
        // // dont waste time if actaullly playing
        // if (!(play && (mode == echo)))
        //     PT_SEM_SIGNAL(pt, &new_udp_recv_s);

        // Signal that the recv buffer has been written
        PT_SEM_SIGNAL(pt, &new_udp_recv_s);
    } else {
        printf("ERROR: NULL pt in callback");
    }
}

static struct udp_pcb* udpecho_raw_pcb;

// Define the recv callback function
void udpecho_raw_init(void)
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
            udp_recv(udpecho_raw_pcb, udpecho_raw_recv, NULL);
            // printf("Set up recv callback\n");
        } else {
            printf("bind error");
        }
    } else {
        printf("ERROR: udpecho_raw_pcb was NULL");
    }
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

    static struct udp_pcb* pcb;
    pcb              = udp_new();
    pcb->remote_port = UDP_PORT;
    pcb->local_port  = UDP_PORT;

    static int counter = 0;

    while (true) {

        // Wait until there is something to send
        PT_SEM_WAIT(pt, &new_udp_send_s);

        // // in paired mode, the two picos talk just to each other
        // // before pairing, the echo unit talks to the laptop
        // if (mode == echo) {
        //     if (paired == true) {
        //         ipaddr_aton(udp_target_pico, &addr);
        //     } else {
        //         ipaddr_aton(udp_target_pico, &addr);
        //     }
        // }
        // // broadcast mode makes sure that another pico sees the packet
        // // to sent an address and for testing
        // else if (mode == send) {
        //     if (paired == true) {
        //         ipaddr_aton(udp_target_pico, &addr);
        //     } else {
        //         ipaddr_aton(UDP_TARGET_BROADCAST, &addr);
        //     }
        // }

#if AP == 1
        // Set dest addr to station IP address
        ipaddr_aton(STATION_ADDR, &addr);

#else
        // Set dest addr to access point IP address
        ipaddr_aton(AP_ADDR, &addr);
#endif

        // // get the length specified by another thread
        // static int udp_send_length;
        // switch (packet_length) {
        // case command:
        //     udp_send_length = 32;
        //     break;
        // case data:
        //     udp_send_length = send_data_size;
        //     break;
        // case ack:
        //     udp_send_length = 5;
        //     break;
        // }

        // Allocate pbuf
        static int udp_send_length;
        udp_send_length = strlen(send_data);
        struct pbuf* p =
            pbuf_alloc(PBUF_TRANSPORT, udp_send_length + 1, PBUF_RAM);

        // Zero the payload and write
        char* req = (char*) p->payload; // Cast from void* to char*
        memset(req, 0, udp_send_length + 1);
        memcpy(req, send_data, udp_send_length);

        // Send packet
        // cyw43_arch_lwip_begin();
        printf("Send data: %s\n", send_data);
        err_t er = udp_sendto(pcb, p, &addr, UDP_PORT); // port
        // cyw43_arch_lwip_end();

        // Free the packet buffer
        pbuf_free(p);
        if (er == ERR_OK) {
            // printf("Sent packet %d\n", counter);
            counter++;
        } else {
            printf("Failed to send UDP packet! error=%d", er);
        }
    }
    PT_END(pt);
}

// ==================================================
// udp recv thread
// ==================================================
static PT_THREAD(protothread_udp_recv(struct pt* pt))
{
    PT_BEGIN(pt);

    static char arg1[32], arg2[32], arg3[32], arg4[32];
    static char* token;

    // data structure for interval timer
    // PT_INTERVAL_INIT() ;

    while (1) {
        // wait for new packet
        // signalled by LWIP receive ISR
        PT_SEM_WAIT(pt, &new_udp_recv_s);

        // // parse command
        // token = strtok(recv_data, "  ");
        // strcpy(arg1, token);
        // token = strtok(NULL, "  ");
        // strcpy(arg2, token);

        // // is this a pairing packet (starts with IP)
        // // if so, parse address
        // // process packet to get time
        // if ((strcmp(arg1, "IP") == 0) && !play) {
        //     if (mode == echo) {
        //         // if I'm the echo unit, grab the address of the other pico
        //         // for the send thread to use
        //         strcpy(udp_target_pico, arg2);
        //         //
        //         paired = true;
        //         // then send back echo-unit address to send-pico
        //         memset(send_data, 0, UDP_MSG_LEN_MAX);
        //         sprintf(send_data, "IP %s",
        //                 ip4addr_ntoa(netif_ip4_addr(netif_list)));
        //         packet_length = command;
        //         // local effects
        //         printf("sent back IP %s\n\r",
        //                ip4addr_ntoa(netif_ip4_addr(netif_list)));
        //         blink_time = 500;
        //         // tell send threead
        //         PT_SEM_SIGNAL(pt, &new_udp_send_s);
        //         PT_YIELD(pt);
        //     } else {
        //         // if I'm the send unit, then just save for future transmit
        //         strcpy(udp_target_pico, arg2);
        //     }
        // } // end  if(strcmp(arg1,"IP")==0)

        // // is it ack packet ?
        // else if ((strcmp(arg1, "ack") == 0) && !play) {
        //     if (mode == send) {
        //         // print a long-long 64 bit int
        //         printf("%lld usec ack\n\r", PT_GET_TIME_usec() - time1);
        //     }
        //     if ((mode == echo) && !play) {
        //         memset(send_data, 0, UDP_MSG_LEN_MAX);
        //         sprintf(send_data, "ack");
        //         packet_length = ack;
        //         // tell send threead
        //         PT_SEM_SIGNAL(pt, &new_udp_send_s);
        //         PT_YIELD(pt);
        //     }
        // }

        printf("%s", recv_data);

        PT_SEM_SIGNAL(pt, &new_udp_send_s);

        // NEVER exit while
    } // END WHILE(1)
    PT_END(pt);
} // recv thread

// =================================================
// input thread
// =================================================
static PT_THREAD(protothread_serial(struct pt* pt))
{
    PT_BEGIN(pt);
    static char cmd[16], arg1[16], arg2[16];
    static char* token;
    //
    // if (mode == send)
    //     printf("Type 'help' for commands\n\r");

    while (1) {
        // the yield time is not strictly necessary for protothreads
        // but gives a little slack for the async processes
        // so that the output is in the correct order (most of the time)
        PT_YIELD_usec(100000);

        // if (mode == send) {
        //     // print prompt
        //     sprintf(pt_serial_out_buffer, "cmd> ");
        // } else {
        //     sprintf(pt_serial_out_buffer, "no cmd in recv mode ");
        // }

        if (AP == 1) {
            sprintf(pt_serial_out_buffer, "cmd> ");
        }

        // spawn a thread to do the non-blocking write
        serial_write;

        // spawn a thread to do the non-blocking serial read
        serial_read;

        // Write message to send buffer
        memset(send_data, 0, UDP_MSG_LEN_MAX);
        sprintf(send_data, "%s", pt_serial_in_buffer);

        PT_SEM_SIGNAL(pt, &new_udp_send_s);

        //
        //
        //
        //
        //

        // // tokenize
        // token = strtok(pt_serial_in_buffer, "  ");
        // strcpy(cmd, token);
        // token = strtok(NULL, "  ");
        // strcpy(arg1, token);
        // token = strtok(NULL, "  ");
        // strcpy(arg2, token);
        // // token = strtok(NULL, "  ");
        // // strcpy(arg3, token) ;
        // // token = strtok(NULL, "  ");
        // // strcpy(arg4, token) ;
        // // token = strtok(NULL, "  ");
        // // strcpy(arg5, token) ;
        // // token = strtok(NULL, "  ");
        // // strcpy(arg6, token) ;

        // // parse by command
        // if (strcmp(cmd, "help") == 0) {
        //     // commands
        //     // printf("set mode [send, recv]\n\r");
        //     printf("play frequency\n\r");
        //     printf("stop \n\r");
        //     printf("pair \n\r");
        //     printf("ack \n\r");
        //     // printf("data array_size \n\r");
        //     //
        //     //  need start data and end data commands
        // }

        // /* this is now done in MAIN before network setup
        // // set the unit mode
        // else if(strcmp(cmd,"set")==0){
        //     if(strcmp(arg1,"recv")==0) {
        //         mode = echo ;
        //     }
        //     else if(strcmp(arg1,"send")==0) mode = send ;
        //     else printf("bad mode");
        //         //printf("%d\n", mode);
        // }
        // */

        // // identify other pico on the same subnet
        // // not needed if autp_setup defined
        // else if (strcmp(cmd, "pair") == 0) {
        //     if (mode == send) {
        //         // broadcast sender's IP addr
        //         memset(send_data, 0, UDP_MSG_LEN_MAX);
        //         sprintf(send_data, "IP %s",
        //                 ip4addr_ntoa(netif_ip4_addr(netif_list)));
        //         packet_length = command;
        //         PT_SEM_SIGNAL(pt, &new_udp_send_s);
        //         // diagnostics:
        //         printf("send IP %s\n",
        //                ip4addr_ntoa(netif_ip4_addr(netif_list)));
        //         // boradcast until paired
        //         printf("sendto IP %s\n", udp_target_pico);
        //         // probably shoulld be some error checking here
        //         paired = true;
        //     } else
        //         printf("No pairing in recv mode -- set send\n");
        // }

        // // send ack packet
        // else if (strcmp(cmd, "ack") == 0) {
        //     if (mode == send) {
        //         memset(send_data, 0, UDP_MSG_LEN_MAX);
        //         sprintf(send_data, "ack");
        //         packet_length = ack;
        //         time1         = PT_GET_TIME_usec();
        //         PT_SEM_SIGNAL(pt, &new_udp_send_s);
        //         // yield so that send thread gets faster access
        //         PT_YIELD(pt);
        //     } else
        //         printf("No ack in recv mode -- set send\n");
        // }

        // // send DDS to the other pico in the alarm ISR
        // else if (strcmp(cmd, "play") == 0) {
        //     packet_length   = data;
        //     play            = true;
        //     tx_buffer_index = 0;
        //     rx_buffer_index = 0;
        //     if (mode == send) {
        //         sscanf(arg1, "%f", &Fout);
        //         main_inc   = (unsigned int) (Fout * 4294967296 / Fs);
        //         main_accum = 0;
        //     }
        // }

        // else if (strcmp(cmd, "stop") == 0) {
        //     main_inc   = 0;
        //     main_accum = 0;
        //     PT_YIELD_usec(50000);
        //     play = false;
        // }

        // // no valid command
        // else
        //     printf("Huh? Type help. \n\r");

        // NEVER exit while
    } // END WHILE(1)

    PT_END(pt);
} // serial thread

// Bruce Land's TCP server structure. Stores metadata for an access point hosted
// by a Pico-W.
typedef struct TCP_SERVER_T_ {
    struct tcp_pcb* server_pcb;
    bool complete;
    ip_addr_t gw;
    async_context_t* context;
} TCP_SERVER_T;

/*
 *  MAIN
 */

#define WIFI_SSID     "picow_test"
#define WIFI_PASSWORD "password"

int main()
{
    // =======================
    // init the serial
    stdio_init_all();

    // // Initialize SPI channel (channel, baud rate set to 20MHz)
    // // connected to spi DAC
    // spi_init(SPI_PORT, 20000000) ;
    // // Format (channel, data bits per transfer, polarity, phase, order)
    // spi_set_format(SPI_PORT, 16, 0, 0, 0);
    // // Map SPI signals to GPIO ports
    // //gpio_set_function(PIN_MISO, GPIO_FUNC_SPI);
    // gpio_set_function(PIN_SCK, GPIO_FUNC_SPI);
    // gpio_set_function(PIN_MOSI, GPIO_FUNC_SPI);
    // gpio_set_function(PIN_CS, GPIO_FUNC_SPI) ;

    // // connecting gpio2 to Vdd sets 'send' mode
    // #define mode_sel 2
    // gpio_init(mode_sel) ;
    // gpio_set_dir(mode_sel, GPIO_IN) ;
    // // turn pulldown on
    // gpio_set_pulls (mode_sel, false, true) ;

    // int i;
    // for (i=0; i<sin_table_len; i++) {
    //   // sine table is in 12 bit range
    //   sine_table[i] = (short)(2040 * sin(2*3.1416*(float)i/sin_table_len) +
    //   2048) ;
    // }

    // =======================
    // choose station vs access point
    // (receive vs send)
    int ap = AP;
    // jumper gpio 2 high for 'send' mode
    // start 'send' mode unit first!
    // ap = gpio_get(mode_sel);
    //
    if (ap) {
        // mode                = send;

        // Allocate TCP server state
        TCP_SERVER_T* state = calloc(1, sizeof(TCP_SERVER_T));
        if (!state) {
            printf("failed to allocate state\n");
            return 1;
        }
        printf("TCP server state allocated!\n");

        // Initialize Wifi chip
        if (cyw43_arch_init()) {
            printf("failed to initialise\n");
            return 1;
        }
        printf("cyw43 initialized!\n");

        // access point SSID and PASSWORD
        // WPA2 authorization
        const char* ap_name  = "picow_test";
        const char* password = "password";

        cyw43_arch_enable_ap_mode(ap_name, password, CYW43_AUTH_WPA2_AES_PSK);
        printf("Access point initialized!\n");

        // 'state' is a pointer to type TCP_SERVER_T
        // set up the access point IP address and mask
        ip4_addr_t mask;
        IP4_ADDR(ip_2_ip4(&state->gw), 192, 168, 4, 10);
        IP4_ADDR(ip_2_ip4(&mask), 255, 255, 255, 0);

        // station address (as set below)
        sprintf(udp_target_pico, "%s", STATION_ADDR);

#ifdef auto_setup
        paired = true;
#endif

        // Start the dhcp server
        // Even though in the porgram DHCP is not required, LWIP
        // seems to need it!
        // and set picoW IP address from 'state' structure
        // set 'mask' as defined above
        dhcp_server_t dhcp_server;
        dhcp_server_init(&dhcp_server, &state->gw, &mask);
    }

    else {
        // mode = echo;
        sleep_ms(1000);
        // =======================
        // init the staTION network
        if (cyw43_arch_init()) {
            printf("failed to initialise\n");
            return 1;
        }

        // hook up to local WIFI
        cyw43_arch_enable_sta_mode();

        // power managment
        // cyw43_wifi_pm(&cyw43_state, CYW43_DEFAULT_PM & ~0xf);

        printf("Connecting to Wi-Fi...\n");
        if (cyw43_arch_wifi_connect_timeout_ms(
                WIFI_SSID, WIFI_PASSWORD, CYW43_AUTH_WPA2_AES_PSK, 30000)) {
            printf("failed to connect.\n");
            return 1;
        } else {
            printf("Connected to Wi-Fi:\n\tSSID = %s\n\tPASS = %s", WIFI_SSID,
                   WIFI_PASSWORD);

            // optional print addr
            // printf("Connected: picoW IP addr: %s\n",
            // ip4addr_ntoa(netif_ip4_addr(netif_list)));
            // and use known ap target
            sprintf(udp_target_pico, "%s", AP_ADDR);
            // set local addr by overridding DHCP
            ip_addr_t ip;
            IP4_ADDR(&ip, 192, 168, 4, 1);
            netif_set_ipaddr(netif_default, &ip);
// printf("modified: picoW IP addr: %s\n",
// ip4addr_ntoa(netif_ip4_addr(netif_list)));
#ifdef auto_setup
            paired = true;
            play   = true;
#endif
        }
    }

    //============================
    // set up UDP recenve ISR handler
    udpecho_raw_init();

    // =====================================
    // init the thread control semaphores
    // for the send/receive
    // recv semaphone is set by an ISR
    PT_SEM_INIT(&new_udp_send_s, 0);
    PT_SEM_INIT(&new_udp_recv_s, 0);

    // =====================================
    // core 1
    // start core 1 threads
    // multicore_reset_core1();
    // multicore_launch_core1(&core1_main);

    // === config threads ========================
    // for core 0

    // printf("Starting threads\n") ;
    pt_add_thread(protothread_udp_recv);
    pt_add_thread(protothread_udp_send);
    pt_add_thread(protothread_serial);

    //
    // === initalize the scheduler ===============
    pt_schedule_start;

    cyw43_arch_deinit();
    return 0;
}