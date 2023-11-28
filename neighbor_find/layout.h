#ifndef LAYOUT_H
#define LAYOUT_H

// Local
#include "network_opts.h"

// Define the USE_LAYOUT macro to use a preset layout of the network.
// (EXISTS FOR TESTING PURPOSES ONLY)
#define USE_LAYOUT

#ifdef USE_LAYOUT

// Which layout number to use
#    define LAYOUT 0

// End of list indicator
#    define EOL -1

// Number of Pico-W boards that I own.
#    define NUM_BOARDS 4

// The list of unique board IDs of my Pico-Ws
extern char board_IDs[NUM_BOARDS][20];

// Connectivity array for the network
extern int conn_array[MAX_NODES][MAX_NODES];

// Convert assigned ID to physical ID
extern int ID_to_phys_ID[MAX_NODES];

// Initialize the connectivity array for testing, returns 0 on success.
int init_layout();

// Print the adjacency list
void print_adj_list(int phys_ID);

#endif

#endif