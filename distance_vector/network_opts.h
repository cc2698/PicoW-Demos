#ifndef NETWORK_OPTS_H
#define NETWORK_OPTS_H

// Maximum number of nodes that can join the network
#define MAX_NODES 5

// Max SSID length
#define SSID_LEN 30

// Max IP address length (slightly bigger than an IPv4 address)
#define IP_ADDR_LEN 20

// Routing algorithm values
#define DIST_IF_NO_ROUTE MAX_NODES
#define POISON_DIST      (DIST_IF_NO_ROUTE + 1)
#define NO_ROUTE         -1
#define DEFAULT_COST     1

#endif