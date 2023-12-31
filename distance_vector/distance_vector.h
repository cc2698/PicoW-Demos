#ifndef DISTANCE_VECTOR_H
#define DISTANCE_VECTOR_H

// Local
#include "node.h"

// Maximum length a distance vector could be when represented as a string
#define DV_MAX_LEN (3 * MAX_NODES)

// Initialize distance vector routing. Setup distance vectors for node_t [n] and
// all of its neighbors.
void init_dist_vector_routing(node_t* n);

// Recalculate distance vector using neighboring distance vectors
bool update_dist_vector_by_nbr_id(node_t* n, int nbr_ID);

// Convert a string to a distance vector, and store it as the distance vector of
// neighbor <nbr_ID>
void str_to_dv(node_t* n, int nbr_ID, char* dv_str);

// Convert a distance vector to a string, if [poison == true] do poisoned
// reverse assuming this distance vector is being crafted for node #[recv_ID]
void dv_to_str(char* buf, node_t* n, int recv_ID, int dv[], bool poison);

// Print a distance vector
void print_dist_vector(node_t* n, int ID);

// Print a routing table
void print_routing_table(node_t* n);

#endif