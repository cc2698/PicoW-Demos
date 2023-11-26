#ifndef UTILS_H
#define UTILS_H

#define HIGH 1
#define LOW  0

// Current value of the LED GPIO pin. Either HIGH = 1 or LOW = 0
extern volatile int led_state;

#define led_on()                                                               \
    do {                                                                       \
        led_state = HIGH;                                                      \
        cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, led_state);                 \
    } while (0);

#define led_off()                                                              \
    do {                                                                       \
        led_state = LOW;                                                       \
        cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, led_state);                 \
    } while (0);

#define led_toggle()                                                           \
    do {                                                                       \
        led_state = !led_state;                                                \
        cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, led_state);                 \
    } while (0);

#endif