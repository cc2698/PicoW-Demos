
// C libraries
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Local
#include "distance_vector.h"
#include "utils.h"

void init_dist_vector_routing(node_t* n)
{
    for (int id = 0; id < MAX_NODES; id++) {
        if (n->ID_is_nbr[id]) {
            // Calculate cost to this neighbor (Placeholder) (Can be changed
            // later to be a function of RSSI)
            int nb_cost = DEFAULT_COST;

            // Set distance to neighbor as 1
            n->dist_vector[id] = nb_cost;

            // Next-hop node for a neighbor is itself
            n->routing_table[id] = id;

            // Add a new neighbor
            nbr_t* nb = malloc(sizeof(struct nbr));
            nb->ID    = id;
            nb->cost  = nb_cost;
            for (int j = 0; j < MAX_NODES; j++) {
                nb->dist_vector[j] = DIST_IF_NO_ROUTE;
            }

            // Store nbr_t pointer in nbrs[id]
            n->nbrs[id] = nb;

            // Increment the number of neighbors
            n->num_nbrs++;
        }
    }

    // Set distance to self as 0
    n->dist_vector[n->ID] = 0;
}

void update_dist_vector(node_t* n, int nbr_ID)
{
    // TODO
}

void str_to_dv(node_t* n, int nbr_ID, char* dv)
{
    // TODO
}

void dv_to_str(char* buf, node_t* n, int recv_ID, int dv[], bool poison)
{
    char dv_str[3 * MAX_NODES];

    // Index in the string where we are writing
    int index = 0;

    // Value of the distance vector to insert
    int value;

    // Check for poison reverse on the first entry, then write the first entry
    // without a delimiter before it
    value = (poison && n->routing_table[0] == recv_ID) ? POISON_DIST : dv[0];
    index += snprintf(&dv_str[index], 3 * MAX_NODES - index, "%d", value);

    for (int id = 1; id < MAX_NODES; id++) {
        // If poisoned reverse is true and I route through [recv_ID] to get
        // to this node, report distance as infinite (posion distance).
        value =
            (poison && n->routing_table[id] == recv_ID) ? POISON_DIST : dv[id];

        // Write the rest of the entries with a '-' delimiter
        index += snprintf(&dv_str[index], 3 * MAX_NODES - index, "-%d", value);
    }

    // Write to the buffer
    snprintf(buf, 3 * MAX_NODES, "%s", dv_str);
}

// Print out a distance vector or a routing table
void print_table(char* type, int ID, int values[])
{
    bool dv = (strcmp(type, "dv") == 0);
    bool rt = (strcmp(type, "rt") == 0);

    // Title
    if (dv) {
        printf("DISTANCE VECTOR: ID = %d\n", ID);
    } else if (rt) {
        printf("ROUTING TABLE: ID = %d\n", ID);
    } else {
        printf("ERROR: Unknown table type.\n");
    }

    // Header
    printf("\t Target   | ");
    for (int i = 0; i < MAX_NODES; i++) {
        printf(" %2d", i);
    }
    printf("\n");

    // Bar
    printf("\t----------|-");
    for (int i = 0; i < MAX_NODES; i++) {
        printf("---");
    }
    printf("---\n");

    // Values
    if (dv) {
        printf("\t Distance | ", ID);
    } else if (rt) {
        printf("\t Next-hop | ", ID);
    }
    for (int i = 0; i < MAX_NODES; i++) {
        if (values[i] == NO_ROUTE || values[i] == DIST_IF_NO_ROUTE) {
            // If the node hasn't been found yet print in orange
            print_orange;
            printf(" %2d", values[i]);
            print_reset;
        } else {
            printf(" %2d", values[i]);
        }
    }
    printf("\n\n");
}

void print_dist_vector(int ID, int dv[])
{
    print_table("dv", ID, dv);
}

void print_routing_table(int ID, int rt[])
{
    print_table("rt", ID, rt);
}
