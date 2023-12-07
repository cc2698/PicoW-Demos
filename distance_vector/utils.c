
// C libraries
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Pico
#include "boards/pico_w.h"
#include "pico/cyw43_arch.h"
#include "pico/stdlib.h"

// Local
#include "utils.h"

void led_on()
{
    cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, HIGH);
}

void led_off()
{
    cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, LOW);
}

void sleep_ms_progress_bar(unsigned int bar_ms, unsigned int bar_len)
{
    // Bar length and dot interval
    int ms       = bar_ms > 9999 ? 9999 : bar_ms;
    int len      = bar_len > 40 ? 40 : bar_len;
    int interval = (int) (ms / len);

    // Print progress bar bookends
    printf("progress: [%*c]", len, ' ');

    // Return cursor to start of bar
    for (int i = 0; i < len + 1; i++) {
        printf("\b");
    }

    // Animate dots
    for (int i = 0; i < len; i++) {
        printf(".");
        sleep_ms(interval);
    }
    printf("\n");
}

void test_printf_colors()
{
    print_bold;
    printf("Bold\n");
    print_red;
    printf("Red\n");
    print_green;
    printf("Green\n");
    print_yellow;
    printf("Yellow\n");
    print_blue;
    printf("Blue\n");
    print_magenta;
    printf("Magenta\n");
    print_cyan;
    printf("Cyan\n");
    print_orange;
    printf("Orange\n");
    print_reset;
    printf("Normal\n");

    print_green_bright;
    printf("Bright Green\n");
    print_orange_bright;
    printf("Bright Orange\n");

    print_bold;
    print_green;
    printf("Bold\n");
    printf("Green\n");
    print_reset;

    print_bold;
    print_orange;
    printf("Bold\n");
    printf("Orange\n");
    print_reset;

    print_reset;
    printf("Normal\n");
}
