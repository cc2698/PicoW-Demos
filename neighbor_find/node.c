
// C libraries
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Pico
#include "pico/unique_id.h"

// Local
#include "node.h"

node_t self;

node_t new_node(int is_master)
{
    node_t n;

    // Initialize ID and parent ID
    n.ID        = is_master ? MASTER_ID : DEFAULT_ID;
    n.parent_ID = DEFAULT_ID;

    // Initialize SSID as pidog_<hex ID>
    char unique_board_id[SSID_LEN];
    pico_get_unique_board_id_string(unique_board_id,
                                    2 * PICO_UNIQUE_BOARD_ID_SIZE_BYTES + 2);
    snprintf(n.wifi_ssid, SSID_LEN, "pidog_%s", unique_board_id);

    // Initialize with default IP address
    snprintf(n.ip_addr, IP_ADDR_LEN, "%s", "255.255.255.255");

    // Initialize with no neighbors
    for (int i = 0; i < MAX_NODES; i++) {
        n.ID_is_nbr[i] = false;
    }

    // Initialize as uninitialized (makes perfect sense right)
    n.is_initialized = false;

    return n;
}
