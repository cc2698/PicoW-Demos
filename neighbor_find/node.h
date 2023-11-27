#ifndef NODE_H
#define NODE_H

// Maximum number of nodes that can join the network
#define MAX_NODES 5

// Default node ID numbers
#define MASTER_ID  1
#define DEFAULT_ID 0

// Max SSID length
#define SSID_LEN 50

// Max IP address length (slightly bigger than an IPv4 address)
#define IP_ADDR_LEN 20

// Node struct
typedef struct node {
    int ID;        // ID number
    int parent_ID; // Parent node ID number

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