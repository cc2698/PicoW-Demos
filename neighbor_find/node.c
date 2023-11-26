
// C libraries
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Local
#include "node.h"

node_t self;

node_t new_node(int is_master)
{
    node_t n;

    // Initialize fields
    n.ID = is_master ? MASTER_ID : DEFAULT_ID;
    snprintf(n.ip_addr, 20, "%s", "255.255.255.255");

    // Initialize with no neighbors
    for (int i = 0; i < MAX_NODES; i++) {
        n.ID_is_nbr[i] = false;
    }

    return n;
}
