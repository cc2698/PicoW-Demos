#ifndef UTILS_H
#define UTILS_H

// LED on/off
#define HIGH 1
#define LOW  0

// Bold
#define print_bold printf("\x1b[1m")

// Color
#define print_black   printf("\x1b[30m")
#define print_red     printf("\x1b[31m")
#define print_green   printf("\x1b[32m")
#define print_yellow  printf("\x1b[33m")
#define print_blue    printf("\x1b[34m")
#define print_magenta printf("\x1b[35m")
#define print_cyan    printf("\x1b[36m")
#define print_white   printf("\x1b[37m")
#define print_orange  printf("\x1b[38;2;255;165;0m")

// Highlighted text
#define print_green_bright  printf("\x1b[92m")
#define print_orange_bright printf("\x1b[98;2;255;165;0m")

// Highlighted text
#define print_green_highlight  printf("\x1b[42m")
#define print_orange_highlight printf("\x1b[48;2;255;165;0m")

// Reset
#define print_reset printf("\x1b[0m")

// LED functions
void led_on();
void led_off();

// Sleep and animate a progress bar
void sleep_ms_progress_bar(unsigned int bar_ms, unsigned int bar_len);

// Test printf colors
void test_printf_colors();

#endif