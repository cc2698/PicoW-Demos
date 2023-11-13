/*
 * Should compile into two separate binaries, one that is the sender and one that is the receiver.
 */

// C libraries
#include "math.h"
#include "stdio.h"
#include <stdlib.h>
#include <string.h>

// Multicore
#include "pico/cyw43_arch.h"
#include "pico/stdlib.h"
#include <pico/multicore.h>

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
