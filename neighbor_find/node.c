
// C libraries
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Pico
#include "pico/unique_id.h"

// Local
#include "node.h"
#include "utils.h"

node_t self;

node_t new_node(int is_master)
{
    node_t n;

    // Initialize ID and parent ID
    n.ID        = is_master ? MASTER_ID : DEFAULT_ID;
    n.parent_ID = DEFAULT_ID;

    // Initialize counter to 0
    n.counter = 0;

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

void print_neighbors()
{
    print_green;
    printf("\n");
    printf("NEIGHBOR SEARCH RESULTS:\n");
    printf("My ID:        %d\n", self.ID);

    // Print parent
    if (self.parent_ID == DEFAULT_ID) {
        printf("Parent ID:    n/a (root node)\n");
    } else {
        printf("Parent ID:    %d\n", self.parent_ID);
    }

    // Print neighbors
    printf("Neighbors:  [ ");
    for (int i = 0; i < MAX_NODES; i++) {
        if (self.ID_is_nbr[i]) {
            printf("%d ", i);
        }
    }
    printf("]\n");
    print_reset;
}
