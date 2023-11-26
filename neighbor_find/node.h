#ifndef NODE_H
#define NODE_H

// Maximum number of nodes that can join the network
#define MAX_NODES 5

// Default node ID numbers
#define MASTER_ID  1
#define DEFAULT_ID 0

// Node
typedef struct node {
    int ID;                   // ID number
    int parent_ID;            // Parent node ID number
    char ip_addr[20];         // IPv4 address
    int ID_is_nbr[MAX_NODES]; // True if ID is a neighbor

} node_t;

// Neighbor
typedef struct nbr {
    int ID;
} nbr_t;

// My own metadata
extern node_t self;

// Create a new node with default values
node_t new_node(int is_master);

#endif