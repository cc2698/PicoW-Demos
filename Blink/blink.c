/**
 * Copyright (c) 2022 Raspberry Pi (Trading) Ltd.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "boards/pico_w.h"
#include "pico/cyw43_arch.h"
#include "pico/stdlib.h"

// Blink interval (ms)
#define INTERVAL 500

int main()
{
    stdio_init_all();
    if (cyw43_arch_init()) {
        printf("Wi-Fi init failed");
        return -1;
    }
    while (true) {
        printf("Blink\n");
        cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, 1);
        sleep_ms(INTERVAL);
        cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, 0);
        sleep_ms(INTERVAL);
    }
}