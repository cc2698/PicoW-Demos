#ifndef NODE_H
#define NODE_H

// Local
#include "layout.h"
#include "network_opts.h"

// Default node ID numbers
#define MASTER_ID  0
#define DEFAULT_ID -1

// Value in the distance vector if no route is currently known
#define NO_ROUTE MAX_NODES

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

    int ID_is_nbr[MAX_NODES]; // Hashmap (<ID>, <bool>), true if neighbor
    nbr_t* nbrs[MAX_NODES];   // List of my neighbors
    int num_nbrs;             // Number of entries in the nbrs[] array

    int distance_vector[MAX_NODES]; // My distance vector
    int routing_table[MAX_NODES];   // My routing table

} node_t;

// Neighbor struct
typedef struct nbr {
    int ID;                         // ID number
    int cost;                       // Cost of sending a packet to this neighbor
    int distance_vector[MAX_NODES]; // My distance vector
} nbr_t;

// Contains all of the properties of this node
extern node_t self;

// Create a new node with default values
node_t new_node(int is_master);

// Allocate a new nbr_t for each true value in self.ID_is_nbr[]
void init_neighbors();

// Recalculate distance vector using neighbors
void calculate_distance_vector();

// Convert a string to a distance vector, and store it as the distance vector of
// neighbor <nbr_ID>
void str_to_dv(node_t* n, int nbr_ID, char* dv_str);

// Convert a distance vector to a string, use poisoned reverse with <nbr_ID>
void dv_to_str(int dv[], int nbr_ID);

void print_distance_vector(int dv[]);

// Print results of neighbor search
void print_neighbors();

#endif