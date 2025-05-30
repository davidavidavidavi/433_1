/**
 * Copyright (c) 2020 Raspberry Pi (Trading) Ltd.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <stdio.h>
#include "pico/stdlib.h"
#include "pico/multicore.h"
#include "hardware/adc.h"
#include "hardware/gpio.h"

#define FLAG_VALUE 123
#define CMD_READ_ADC 0
#define CMD_LED_ON 1
#define CMD_LED_OFF 2

#define LED_PIN 15
#define ADC_PIN 26  // A0 is GP26

// Shared variable for ADC reading
volatile uint16_t adc_value = 0;

void core1_entry() {
    // Initialize GPIO and ADC
    gpio_init(LED_PIN);
    gpio_set_dir(LED_PIN, GPIO_OUT);
    
    adc_init();
    adc_gpio_init(ADC_PIN);
    adc_select_input(0);  // Select ADC0 (GP26)

    // Signal to core 0 that we're ready
    multicore_fifo_push_blocking(FLAG_VALUE);

    while (1) {
        // Wait for command from core 0
        uint32_t cmd = multicore_fifo_pop_blocking();
        
        switch (cmd) {
            case CMD_READ_ADC:
                adc_value = adc_read();
                multicore_fifo_push_blocking(FLAG_VALUE);  // Signal completion
                break;
                
            case CMD_LED_ON:
                gpio_put(LED_PIN, 1);
                multicore_fifo_push_blocking(FLAG_VALUE);  // Signal completion
                break;
                
            case CMD_LED_OFF:
                gpio_put(LED_PIN, 0);
                multicore_fifo_push_blocking(FLAG_VALUE);  // Signal completion
                break;
        }
    }
}

int main() {
    stdio_init_all();
    
    // Wait for USB to be ready
    while (!stdio_usb_connected()) {
        sleep_ms(100);
    }
    
    printf("Hello, multicore!\n");
    printf("Commands:\n");
    printf("0 - Read ADC value\n");
    printf("1 - Turn LED on\n");
    printf("2 - Turn LED off\n");

    // Launch core 1
    multicore_launch_core1(core1_entry);

    // Wait for core 1 to be ready
    uint32_t g = multicore_fifo_pop_blocking();
    if (g != FLAG_VALUE) {
        printf("Error: Core 1 not ready!\n");
        return -1;
    }

    char cmd;
    while (1) {
        printf("\nEnter command (0-2): ");
        if (scanf(" %c", &cmd) == 1) {
            switch (cmd) {
                case '0':
                    multicore_fifo_push_blocking(CMD_READ_ADC);
                    multicore_fifo_pop_blocking();  // Wait for completion
                    printf("ADC value: %d\n", adc_value);
                    break;
                    
                case '1':
                    multicore_fifo_push_blocking(CMD_LED_ON);
                    multicore_fifo_pop_blocking();  // Wait for completion
                    printf("LED turned on\n");
                    break;
                    
                case '2':
                    multicore_fifo_push_blocking(CMD_LED_OFF);
                    multicore_fifo_pop_blocking();  // Wait for completion
                    printf("LED turned off\n");
                    break;
                    
                default:
                    printf("Invalid command\n");
                    break;
            }
        }
    }
}
