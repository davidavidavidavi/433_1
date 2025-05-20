/**
 * Copyright (c) 2020 Raspberry Pi (Trading) Ltd.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "pico/stdlib.h"
#include <stdio.h>
#include "hardware/gpio.h"

// Pico W devices use a GPIO on the WIFI chip for the LED,
// so when building for Pico W, CYW43_WL_GPIO_LED_PIN will be defined
#ifdef CYW43_WL_GPIO_LED_PIN
#include "pico/cyw43_arch.h"
#endif

#define BUTTON_PIN 3  // Using GPIO 3 for the button
static uint32_t button_press_count = 0;
static bool led_state = false;  // Track LED state globally

// Perform initialisation
int pico_led_init(void) {
#if defined(PICO_DEFAULT_LED_PIN)
    // A device like Pico that uses a GPIO for the LED will define PICO_DEFAULT_LED_PIN
    // so we can use normal GPIO functionality to turn the led on and off
    gpio_init(PICO_DEFAULT_LED_PIN);
    gpio_set_dir(PICO_DEFAULT_LED_PIN, GPIO_OUT);
    return PICO_OK;
#elif defined(CYW43_WL_GPIO_LED_PIN)
    // For Pico W devices we need to initialise the driver etc
    return cyw43_arch_init();
#endif
}

// Turn the led on or off
void pico_set_led(bool led_on) {
#if defined(PICO_DEFAULT_LED_PIN)
    // Just set the GPIO on or off
    gpio_put(PICO_DEFAULT_LED_PIN, led_on);
#elif defined(CYW43_WL_GPIO_LED_PIN)
    // Ask the wifi "driver" to set the GPIO on or off
    cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, led_on);
#endif
}

// Button press callback
void gpio_callback(uint gpio, uint32_t events) {
    if (gpio == BUTTON_PIN && (events & GPIO_IRQ_EDGE_FALL)) {
        // Simple debounce - wait a bit before processing
        sleep_ms(20);
        if (!gpio_get(BUTTON_PIN)) {  // Check if button is still pressed
            button_press_count++;
            led_state = !led_state;  // Toggle LED state
            pico_set_led(led_state);
            printf("Button pressed! LED is now %s. Total toggles: %lu\n", 
                   led_state ? "ON" : "OFF", button_press_count);
        }
    }
}

int main() {
    // Initialize USB serial
    stdio_init_all();
    
    // Initialize LED
    int rc = pico_led_init();
    hard_assert(rc == PICO_OK);
    
    // Initialize button
    gpio_init(BUTTON_PIN);
    gpio_set_dir(BUTTON_PIN, GPIO_IN);
    gpio_pull_up(BUTTON_PIN);
    
    // Set up button interrupt
    gpio_set_irq_enabled_with_callback(BUTTON_PIN, GPIO_IRQ_EDGE_FALL, true, &gpio_callback);
    
    printf("Button press counter started.\n");
    printf("Press the button on GPIO %d to toggle LED and see the count.\n", BUTTON_PIN);
    printf("Initial LED state: OFF\n");
    
    // Main loop
    while (true) {
        tight_loop_contents();
    }
}
