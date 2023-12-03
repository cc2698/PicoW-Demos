#ifndef LAYOUT_H
#define LAYOUT_H

// C Libraries
#include <stdbool.h>

// Local
#include "network_opts.h"

// Define the USE_LAYOUT macro to use a preset layout of the network.
// (CMake will compile a separate set of binaries with this defined)
// (Exists for testing purposes only)
#ifdef USE_LAYOUT

// Which layout number to use
#    define LAYOUT 4

// End of list indicator
#    define EOL -1

// Number of Pico-W boards that I own.
#    define NUM_BOARDS 5

// The list of unique board IDs of my Pico-Ws
extern char board_IDs[NUM_BOARDS][20];

// Connectivity array for the network
extern int conn_array[MAX_NODES][MAX_NODES];

// Convert assigned ID to physical ID
extern int ID_to_phys_ID[MAX_NODES];

// Initialize the connectivity array for testing, returns 0 on success.
int init_layout();

// Print the adjacency list
void print_adj_list(int phys_ID, bool full_list);

#endif

#endif