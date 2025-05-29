#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/spi.h"
#include <math.h>

#define SPI_CS_PIN 15  // Using GP17 as CS pin
#define SAMPLE_RATE 12000  // 12kHz
#define SINE_FREQ 2        // 2Hz
#define TRIANGLE_FREQ 1    // 1Hz
#define NUM_SAMPLES 12000  // Exactly 1 second worth of samples at 12kHz

static inline void cs_select(uint cs_pin) {
    asm volatile("nop \n nop \n nop"); // FIXME
    gpio_put(cs_pin, 0);
    asm volatile("nop \n nop \n nop"); // FIXME
}

static inline void cs_deselect(uint cs_pin) {
    asm volatile("nop \n nop \n nop"); // FIXME
    gpio_put(cs_pin, 1);
    asm volatile("nop \n nop \n nop"); // FIXME
}

int main()
{ 
    gpio_init(SPI_CS_PIN);

    stdio_init_all();
    // Initialize SPI pins
    spi_init(spi0, 12 * 1000000);  // 12MHz SPI clock


    gpio_set_function(PICO_DEFAULT_SPI_RX_PIN, GPIO_FUNC_SPI);
    gpio_set_function(PICO_DEFAULT_SPI_SCK_PIN, GPIO_FUNC_SPI);
    gpio_set_function(PICO_DEFAULT_SPI_TX_PIN, GPIO_FUNC_SPI);
    gpio_set_dir(SPI_CS_PIN, GPIO_OUT);
    gpio_put(SPI_CS_PIN, 1);  // Set CS high initially (inactive)
    


    // Generate sine wave samples
    uint16_t sine_wave[NUM_SAMPLES];
    for(int i = 0; i < NUM_SAMPLES; i++) {
        float t = (float)i / NUM_SAMPLES;
        sine_wave[i] = (uint16_t)(512 + 511 * sinf(2 * M_PI * t));
    }

    // Generate triangle wave samples
    uint16_t triangle_wave[NUM_SAMPLES];
    for(int i = 0; i < NUM_SAMPLES; i++) {
        float t = (float)i / NUM_SAMPLES;
        float phase = fmodf(2 * M_PI * TRIANGLE_FREQ * t, 2 * M_PI);
        if(phase < M_PI) {
            triangle_wave[i] = (uint16_t)(1023 * (phase / M_PI));
        } else {
            triangle_wave[i] = (uint16_t)(1023 * (2 - phase / M_PI));
        }
    }

    uint8_t data[2];
    int sample_index = 0;
    bool output_a = true;  // true for output A, false for output B

    while (true) {
        uint16_t value;
        if(output_a) {
            value = sine_wave[sample_index];
            // First byte: 0111 (4 bits) + upper 4 bits of value
            data[0] = 0b01110000 | ((value >> 6) & 0b00001111);
            // Second byte: lower 6 bits of value + 00 (2 bits)
            data[1] = (value << 2) & 0b11111100;
        } else {
            value = triangle_wave[sample_index];
            // First byte: 1111 (4 bits) + upper 4 bits of value
            data[0] = 0b11110000 | ((value >> 6) & 0b00001111);
            // Second byte: lower 6 bits of value + 00 (2 bits)
            data[1] = (value << 2) & 0b11111100;
        }

        cs_select(SPI_CS_PIN);
        spi_write_blocking(spi0, data, 2);
        cs_deselect(SPI_CS_PIN);

        // Update sample index and output selection
        sample_index = (sample_index + 1) % NUM_SAMPLES;
        output_a = !output_a;  // Alternate between outputs

        sleep_ms(1);
    }
}