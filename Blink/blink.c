/**
 * Copyright (c) 2022 Raspberry Pi (Trading) Ltd.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "boards/pico_w.h"
#include "pico/cyw43_arch.h"
#include "pico/stdlib.h"

// Blink interval (ms)
#define INTERVAL 750

#define led_on()                                                               \
    do {                                                                       \
        cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, 1);                         \
    } while (0);

#define led_off()                                                              \
    do {                                                                       \
        cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, 0);                         \
    } while (0);

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
        printf("Blink\n");

        led_on();
        sleep_ms(INTERVAL);
        led_off();
        sleep_ms(INTERVAL);
    }
}