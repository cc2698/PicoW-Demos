
// C libraries
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Local
#include "packet.h"

// List of valid packet types
const char* packet_types[NUM_PACKET_TYPES] = {"data", "ack", "token"};

bool is_valid_packet_type(char* s)
{
    int valid = false;

    // Search packet_types for a match
    for (int i = 0; i < NUM_PACKET_TYPES; i++) {
        if (strcmp(packet_types[i], s) == 0) {
            valid = true;
        }
    }

    return valid;
}

void copy_field(char* field, char* token)
{
    // I use snprintf() here because it is extremely dangerous if a header field
    // gets written without a NULL terminator, which could happen if the token
    // exceeds the length of the buffer. The undefined behavior causes
    // non-reproducible bugs, usually ending with the Pico-W freezing.

    if (token == NULL) {
        snprintf(field, TOK_LEN, "n/a");
    } else {
        snprintf(field, TOK_LEN, "%s", token);
    }
}

packet_t new_packet(char* type, int dest, int src, char* addr, int ack,
                    uint64_t t, char* m)
{
    packet_t op;

    snprintf(op.packet_type, TOK_LEN, "%s", type);
    snprintf(op.ip_addr, TOK_LEN, "%s", addr);
    op.dest_id   = dest;
    op.src_id    = src;
    op.ack_num   = ack;
    op.timestamp = t;
    snprintf(op.msg, UDP_MSG_LEN_MAX, "%s", m);

    return op;
}

void packet_to_str(char* buf, packet_t p)
{
    // Copy the contents of the packet into the buffer
    snprintf(buf, UDP_MSG_LEN_MAX, "%s;%d;%d;%s;%d;%llu;%s", p.packet_type,
             p.dest_id, p.src_id, p.ip_addr, p.ack_num, p.timestamp, p.msg);
}

packet_t str_to_packet(char* s)
{
    // Not strictly necessary, but the strtok() function is destructive of
    // its inputs therefore we copy the recv buffer into a temporary
    // variable to avoid messing with it directly.
    char tbuf[UDP_MSG_LEN_MAX];
    snprintf(tbuf, UDP_MSG_LEN_MAX, "%s", s);

    packet_t op;

    char* token;

    // Data or ACK
    token = strtok(tbuf, ";");
    copy_field(op.packet_type, token);

    // Dest ID
    char dest_id_str[TOK_LEN];
    token = strtok(NULL, ";");
    copy_field(dest_id_str, token);
    if (strcmp(dest_id_str, "n/a") != 0) {
        op.dest_id = atoi(dest_id_str);
    }

    // Src ID
    char src_id_str[TOK_LEN];
    token = strtok(NULL, ";");
    copy_field(src_id_str, token);
    if (strcmp(src_id_str, "n/a") != 0) {
        op.src_id = atoi(src_id_str);
    }

    // Source IP address
    token = strtok(NULL, ";");
    copy_field(op.ip_addr, token);

    // ACK number
    char ack_num_str[TOK_LEN];
    token = strtok(NULL, ";");
    copy_field(ack_num_str, token);
    if (strcmp(ack_num_str, "n/a") != 0) {
        op.ack_num = atoi(ack_num_str);
    }

    // Timestamp
    char timestamp_str[TOK_LEN];
    token = strtok(NULL, ";");
    copy_field(timestamp_str, token);
    if (strcmp(timestamp_str, "n/a") != 0) {
        op.timestamp = strtoull(timestamp_str, NULL, 10);
    }

    // Contents
    token = strtok(NULL, ";");
    copy_field(op.msg, token);

    return op;
}

void print_packet(char* payload, packet_t p)
{
    if (payload != NULL) {
        printf("|\tPayload: { %s }\n", payload);
    }
    printf("|\ttype:      %s\n", p.packet_type);
    printf("|\tdst ID:    %d\n", p.dest_id);
    printf("|\tsrc ID:    %d\n", p.src_id);
    printf("|\tsrc IP:    %s\n", p.ip_addr);
    printf("|\tack #:     %d\n", p.ack_num);
    printf("|\tmsg:       %s\n", p.msg);
}
