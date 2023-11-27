
// Pico
#include "pico/unique_id.h"
#include "boards/pico_w.h"
#include "pico/cyw43_arch.h"
#include "pico/stdlib.h"

int main()
{
    // Initialize all stdio types
    stdio_init_all();

    // Initialize Wifi chip
    printf("Initializing cyw43...");
    if (cyw43_arch_init()) {
        printf("failed to initialise.\n");
        return 1;
    } else {
        printf("initialized!\n");
    }

    while (true) {
        // Get the unique board ID
        char unique_board_id[20];
        pico_get_unique_board_id_string(
            unique_board_id, 2 * PICO_UNIQUE_BOARD_ID_SIZE_BYTES + 2);

        printf("%s\n", unique_board_id);
        sleep_ms(3000);
    }
}