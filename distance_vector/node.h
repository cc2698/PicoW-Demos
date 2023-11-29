#ifndef NODE_H
#define NODE_H

// C Libraries
#include <stdbool.h>

// Local
#include "layout.h"
#include "network_opts.h"

// Default node ID numbers
#define MASTER_ID  0
#define DEFAULT_ID -1

// Neighbor struct
typedef struct nbr {
    int ID;                     // ID number
    int cost;                   // Cost of sending a packet to this neighbor
    int dist_vector[MAX_NODES]; // Estimate of neighbor's distance vector

    bool up_to_date; // Does this neighbor have the most
                     // up-to-date copy of my distance vector?

    bool new_dv; // Have I stored a new DV for this node that I haven't looked
                 // at yet?
} nbr_t;

// Node struct
typedef struct node {

    int ID;        // ID number
    int parent_ID; // Parent node ID number

#ifdef USE_LAYOUT
    int physical_ID; // Physical ID number
#endif

    unsigned int counter; // Count number of packets sent

    char wifi_ssid[SSID_LEN];  // My SSID when hosting an access point
    char ip_addr[IP_ADDR_LEN]; // IPv4 address

    int knows_nbrs; // Has the node been assigned an ID and found its neighbors

    int ID_is_nbr[MAX_NODES]; // Hashmap (<ID>, <bool>), true if ID is neighbor
    nbr_t* nbrs[MAX_NODES];   // List of my neighbors
    int ID_to_nbrs_index[MAX_NODES]; // Hashmap (<ID>, <index in nbrs[]>)
    int num_nbrs;                    // Number of entries in the nbrs[] array

    int dist_vector[MAX_NODES];   // My distance vector
    int routing_table[MAX_NODES]; // My routing table

} node_t;

// Contains all of the properties of this node
extern node_t self;

// Create a new node with default values
node_t new_node(int is_master);

// Print results of neighbor search
void print_neighbors();

#endif