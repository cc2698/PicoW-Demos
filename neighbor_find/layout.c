
// C libraries
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Local
#include "layout.h"

#ifdef USE_LAYOUT

// =================================================
// Layouts
// =================================================
#    if LAYOUT == 0

// Two nodes
unsigned int network_adj_list[MAX_NODES][MAX_NODES] = {
    {1, EOL}, // 0
    {0, EOL}, // 1
    {EOL},    // 2
    {EOL}     // 3
};

#    elif LAYOUT == 1

// Four nodes in a line
unsigned int network_adj_list[MAX_NODES][MAX_NODES] = {
    {1, EOL},    // 0
    {0, 2, EOL}, // 1
    {1, 3, EOL}, // 2
    {2, EOL}     // 3
};

#    elif LAYOUT == 2

// Four nodes
// 1 center node and 3 leaf nodes
unsigned int network_adj_list[MAX_NODES][MAX_NODES] = {
    {1, EOL},       // 0
    {0, 2, 3, EOL}, // 1
    {1, EOL},       // 2
    {1, EOL}        // 3
};

#    elif LAYOUT == 3

// Four nodes
// 3 nodes in a triange with 1 leaf node hanging off a corner
unsigned int network_adj_list[MAX_NODES][MAX_NODES] = {
    {1, 2, EOL},    // 0
    {0, 2, EOL},    // 1
    {0, 1, 3, EOL}, // 2
    {2, EOL}        // 3
};

#    endif
// =================================================

char board_IDs[NUM_BOARDS][20] = {
    "E6614864D32F7622", //
    "E6614864D36FAF21", //
    "E6614864D388AD21", //
    "E6614864D3138C21"  //
};

int conn_array[MAX_NODES][MAX_NODES];

int ID_to_phys_ID[MAX_NODES];

int init_layout()
{
    printf("Initializing layout #%d...", LAYOUT);

    // For each row in the adjacency list, check if the first two entries are
    // zero. If they are, then the row has no values.
    for (int i = 0; i < MAX_NODES; i++) {
        if (network_adj_list[i][0] == 0 && network_adj_list[i][1] == 0) {
            network_adj_list[i][0] = EOL;
        }
    }
    // For each row in the adjacency list, iterate through its entries and set
    // the corresponding values in the connectivity array.
    for (int ID_1 = 0; ID_1 < MAX_NODES; ID_1++) {
        for (int j = 0; network_adj_list[ID_1][j] != EOL; j++) {

            // ID of adjacent node
            int ID_2 = network_adj_list[ID_1][j];

            // Check for self-adjacency
            if (ID_1 == ID_2) {
                printf("ERROR: A node cannot be adjacent to itself.\n");
                printf("\tID_1 = %d, ID_2 = %d\n", ID_1, ID_2);

                return 1;
            }

            // Set (ID_1, ID_2) to 'true' in the connectivity array
            conn_array[ID_1][ID_2] = true;
        }
    }

    // Check for conflicting entries
    for (int i = 0; i < MAX_NODES; i++) {
        for (int j = 0; j < MAX_NODES; j++) {
            if (conn_array[i][j] != conn_array[j][i]) {
                // Print indices of conflicting entries
                printf("ERROR: Conflicting connectivity entries.\n");
                printf("\tconn_array[%d][%d] = %d, conn_array[%d][%d] = %d\n",
                       i, j, conn_array[i][j], j, i, conn_array[j][i]);

                return 1;
            }
        }
    }

    printf("success!\n");

    return 0;
}

void print_adj_list(int phys_ID)
{
    printf("\n");

    // Print header
    printf(" Phys. ID | Phys. Nbrs\n");
    printf("----------|");
    for (int i = 0; i < (4 + 3 * MAX_NODES + 1); i++) {
        printf("-");
    }
    printf("\n");

    // Print entries
    for (int i = 0; i < MAX_NODES; i++) {
        if (i == phys_ID) {
            printf(" --> %3d  |  [ ", i);
        } else {
            printf("     %3d  |  [ ", i);
        }

        for (int j = 0; network_adj_list[i][j] != EOL; j++) {
            printf("%d ", network_adj_list[i][j]);
        }
        printf("]\n");
    }

    printf("\n");
}

#endif
