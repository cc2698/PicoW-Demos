
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
    for (int i = 0; i < MAX_NODES; i++) {
        if (n->ID_is_nbr[i]) {
            // Calculate cost to this neighbor (Placeholder) (Can be changed
            // later to be a function of RSSI)
            int nb_cost = DEFAULT_COST;

            // Set distance to neighbor as 1
            n->distance_vector[i] = nb_cost;

            // Next-hop node for a neighbor is itself
            n->routing_table[i] = i;

            // Add a new neighbor
            nbr_t* nb = malloc(sizeof(struct nbr));
            nb->ID    = i;
            nb->cost  = nb_cost;
            for (int j = 0; j < MAX_NODES; j++) {
                nb->distance_vector[j] = DIST_IF_NO_ROUTE;
            }
            n->nbrs[n->num_nbrs] = nb;
        }
    }

    // Set distance to self as 0
    n->distance_vector[n->ID] = 0;
}

void update_distance_vector(node_t* n, int nbr_ID)
{
    // TODO
}

void str_to_dv(node_t* n, int nbr_ID, char* dv)
{
    // TODO
}

void dv_to_str(char* buf, int ID, int dv[], bool poison)
{
    // TODO
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

void print_distance_vector(int ID, int dv[])
{
    print_table("dv", ID, dv);
}

void print_routing_table(int ID, int rt[])
{
    print_table("rt", ID, rt);
}
