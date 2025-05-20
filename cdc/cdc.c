#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/adc.h"

#define ADC_VREF 3.3f
#define ADC_RANGE 4095

int main() {
    adc_init(); // init the adc module
    adc_gpio_init(26); // set ADC0 pin to be adc input instead of GPIO
    adc_select_input(0); // select to read from ADC0

    // Initialize LED pin
    gpio_init(21);
    gpio_set_dir(21, GPIO_OUT);
    gpio_put(21, 0);  // Start with LED off

    // Initialize button pin
    gpio_init(3);
    gpio_set_dir(3, GPIO_IN);
    gpio_pull_up(3);  // Enable pull-up for the button

    stdio_init_all();
    while (!stdio_usb_connected()) {
        sleep_ms(100);
    }
    printf("Start!\n");
    gpio_put(21, 1);  // Turn LED on when USB is connected
 
    while (1) {
        // Simple debounce: if button is pressed, wait a bit and check again
        if (!gpio_get(3)) {  // Button is pressed (active low)
            sleep_ms(50);    // Wait for bounce to settle
            if (!gpio_get(3)) {  // Check again after delay
                gpio_put(21, 0);  // Turn LED off
                
                // Main sampling loop
                while (1) {
                    int num_samples;
                    printf("Enter number of samples (1-100): ");
                    scanf("%d", &num_samples);
                    
                    // Validate input
                    if (num_samples < 1 || num_samples > 100) {
                        printf("Invalid input. Please enter a number between 1 and 100.\n");
                        continue;
                    }
                    
                    printf("Taking %d samples at 100Hz...\n", num_samples);
                    
                    // Take samples
                    for (int i = 0; i < num_samples; i++) {
                        uint16_t adc_value = adc_read();
                        float voltage = (adc_value * ADC_VREF) / ADC_RANGE;
                        printf("Sample %d: %.3f V\n", i + 1, voltage);
                        sleep_ms(10);  // 100Hz = 10ms between samples
                    }
                }
            }
        }
        sleep_ms(100);
    }
}
