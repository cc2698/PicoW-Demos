
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
    printf("Initializing node...\n");

    // Return value
    node_t n;

    // Initialize ID and parent ID
    n.ID        = is_master ? MASTER_ID : DEFAULT_ID;
    n.parent_ID = DEFAULT_ID;

    // Initialize counter to 0
    n.counter = 0;

    // Get the unique board ID
    char unique_board_id[SSID_LEN];
    pico_get_unique_board_id_string(unique_board_id,
                                    2 * PICO_UNIQUE_BOARD_ID_SIZE_BYTES + 2);
    printf("\tUnique ID = 0x%s\n", unique_board_id);

#ifdef USE_LAYOUT

    // If physical_ID = -1 after this process, then this board isn't registered
    n.physical_ID = -1;

    // Set physical ID number
    for (int i = 0; i < NUM_BOARDS; i++) {
        if (strcmp(unique_board_id, board_IDs[i]) == 0) {
            // If it matches to an ID in the list of IDs, return the index
            n.physical_ID = i;
        }
    }

    // Print physical ID number
    printf("\tPhys ID   = %d\n", n.physical_ID);

    // Initialize SSID as pidog_<physical ID>_<hex ID>
    snprintf(n.wifi_ssid, SSID_LEN, "pidog_%d_%s", n.physical_ID,
             unique_board_id);
#else
    // Initialize SSID as pidog_<hex ID>
    snprintf(n.wifi_ssid, SSID_LEN, "pidog_%s", unique_board_id);
#endif

    // Initialize with default IP address
    snprintf(n.ip_addr, IP_ADDR_LEN, "%s", "255.255.255.255");

    // Initialize with no neighbors
    for (int i = 0; i < MAX_NODES; i++) {
        n.ID_is_nbr[i] = false;
    }

    // Doesn't know neighbors at initialization
    n.knows_nbrs = false;

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

#ifdef USE_LAYOUT
    // Print the same results as above, but using physical IDs instead

    print_yellow;
    printf("\n");
    printf("USING PHYSICAL IDS:\n");
    printf("My ID:        %d\n", self.physical_ID);

    // Print parent
    if (self.parent_ID == DEFAULT_ID) {
        printf("Parent ID:    n/a (root node)\n");
    } else {
        printf("Parent ID:    %d\n", ID_to_phys_ID[self.parent_ID]);
    }

    // Print neighbors
    printf("Neighbors:  [ ");
    for (int i = 0; i < MAX_NODES; i++) {
        if (self.ID_is_nbr[i]) {
            printf("%d ", ID_to_phys_ID[i]);
        }
    }
    printf("]\n");

    print_adj_list(self.physical_ID);

#endif

    print_reset;
}
