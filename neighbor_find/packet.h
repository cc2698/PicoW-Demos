#ifndef PACKET_H
#define PACKET_H

// Max length of a UDP packet
#define UDP_MSG_LEN_MAX 1400

// Max length of header fields
#define TOK_LEN 40

// Packet types
#define NUM_PACKET_TYPES 3

// Structure that stores an outgoing packet
typedef struct packet {
    char packet_type[TOK_LEN];
    int dest_id;
    int src_id;
    char ip_addr[TOK_LEN];
    unsigned int ack_num;
    uint64_t timestamp;
    char msg[UDP_MSG_LEN_MAX];
} packet_t;

// Check if a string is a valid packet type
bool is_valid_packet_type(char* s);

// Copy a token into a header field, returns "n/a" if the token is NULL.
void copy_field(char* field, char* token);

// Convert a packet to a string
void packet_to_str(char* buf, packet_t p);

// Create a new packet
packet_t new_packet(char* type, int dest, int src, char* addr, unsigned int ack,
                    uint64_t t, char* m);

// Convert a string to a packet
packet_t str_to_packet(char* s);

void print_packet(char* payload, packet_t p);

#endif