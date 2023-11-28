#ifndef NODE_H
#define NODE_H

// Local
#include "layout.h"
#include "network_opts.h"

// Default node ID numbers
#define MASTER_ID  0
#define DEFAULT_ID -1

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

    int ID_is_nbr[MAX_NODES]; // True if ID is a neighbor
    int knows_nbrs;           // Has the node been assigned an ID and found its
                              // neighbors
} node_t;

// Neighbor struct
typedef struct nbr {
    int ID;
} nbr_t;

// Contains all of the properties of this node
extern node_t self;

// Create a new node with default values
node_t new_node(int is_master);

// Print results of neighbor search
void print_neighbors();

#endif