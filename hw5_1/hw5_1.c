#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/spi.h"
#include <math.h>

// Union for float-int conversion
union FloatInt {
    float f;
    uint32_t i;
};

// DAC SPI Configuration (SPI0)
#define DAC_SPI_CS_PIN 15  // Using GP15 as CS pin for DAC
#define DAC_SPI_BAUD 12000 // 12kHz for DAC
#define SAMPLE_RATE 1000   // 1kHz (1000 samples per second)
#define SINE_FREQ 1        // 1Hz
#define NUM_SAMPLES 1000   // 1000 samples for 1 second at 1kHz

// RAM SPI Configuration (SPI1)
#define RAM_SPI_CS_PIN 13  // Using GP13 as CS pin for RAM
#define RAM_SPI_SCK_PIN 10 // Using GP10 as SCK pin for RAM
#define RAM_SPI_TX_PIN 11  // Using GP11 as MOSI pin for RAM
#define RAM_SPI_RX_PIN 12  // Using GP12 as MISO pin for RAM
#define RAM_SPI_BAUD 2000000 // 2MHz for RAM

// RAM Instructions
#define RAM_READ  0x03  // 0b00000011
#define RAM_WRITE 0x02  // 0b00000010
#define RAM_RDSR  0x05  // 0b00000101
#define RAM_WRSR  0x01  // 0b00000001

// RAM Mode Settings
#define RAM_MODE_BYTE      0x00  // 0b00000000
#define RAM_MODE_SEQUENTIAL 0x40  // 0b01000000
#define RAM_MODE_PAGE      0x80  // 0b10000000

// Number of samples for sine wave in RAM
#define RAM_SINE_SAMPLES 1000  // 1000 samples for 1 second

// DAC chip select functions
static inline void dac_cs_select(void) {
    asm volatile("nop \n nop \n nop");
    gpio_put(DAC_SPI_CS_PIN, 0);
    asm volatile("nop \n nop \n nop");
}

static inline void dac_cs_deselect(void) {
    asm volatile("nop \n nop \n nop");
    gpio_put(DAC_SPI_CS_PIN, 1);
    asm volatile("nop \n nop \n nop");
}

// RAM chip select functions
static inline void ram_cs_select(void) {
    asm volatile("nop \n nop \n nop");
    gpio_put(RAM_SPI_CS_PIN, 0);
    asm volatile("nop \n nop \n nop");
}

static inline void ram_cs_deselect(void) {
    asm volatile("nop \n nop \n nop");
    gpio_put(RAM_SPI_CS_PIN, 1);
    asm volatile("nop \n nop \n nop");
}

// RAM write status register
static void ram_write_status(uint8_t status) {
    ram_cs_select();
    uint8_t cmd = RAM_WRSR;
    spi_write_blocking(spi1, &cmd, 1);
    spi_write_blocking(spi1, &status, 1);
    ram_cs_deselect();
}

// RAM read status register
static uint8_t ram_read_status(void) {
    ram_cs_select();
    uint8_t cmd = RAM_RDSR;
    uint8_t status;
    spi_write_blocking(spi1, &cmd, 1);
    spi_read_blocking(spi1, 0, &status, 1);
    ram_cs_deselect();
    return status;
}

// RAM write data
static void ram_write_data(uint16_t address, const uint8_t* data, size_t len) {
    ram_cs_select();
    uint8_t cmd = RAM_WRITE;
    spi_write_blocking(spi1, &cmd, 1);
    
    // Send address (16-bit, MSB first)
    uint8_t addr_high = (address >> 8) & 0xFF;
    uint8_t addr_low = address & 0xFF;
    spi_write_blocking(spi1, &addr_high, 1);
    spi_write_blocking(spi1, &addr_low, 1);
    
    // Write data
    spi_write_blocking(spi1, data, len);
    ram_cs_deselect();
}

// RAM read data
static void ram_read_data(uint16_t address, uint8_t* data, size_t len) {
    ram_cs_select();
    uint8_t cmd = RAM_READ;
    spi_write_blocking(spi1, &cmd, 1);
    
    // Send address (16-bit, MSB first)
    uint8_t addr_high = (address >> 8) & 0xFF;
    uint8_t addr_low = address & 0xFF;
    spi_write_blocking(spi1, &addr_high, 1);
    spi_write_blocking(spi1, &addr_low, 1);
    
    // Read data
    spi_read_blocking(spi1, 0, data, len);
    ram_cs_deselect();
}

int main() {
    // Initialize GPIO first
    gpio_init(DAC_SPI_CS_PIN);
    gpio_init(RAM_SPI_CS_PIN);
    
    // Initialize stdio
    stdio_init_all();
    
    // Initialize DAC SPI (SPI0)
    spi_init(spi0, DAC_SPI_BAUD);  // 12kHz SPI clock for DAC
    gpio_set_function(PICO_DEFAULT_SPI_RX_PIN, GPIO_FUNC_SPI);
    gpio_set_function(PICO_DEFAULT_SPI_SCK_PIN, GPIO_FUNC_SPI);
    gpio_set_function(PICO_DEFAULT_SPI_TX_PIN, GPIO_FUNC_SPI);
    gpio_set_dir(DAC_SPI_CS_PIN, GPIO_OUT);
    gpio_put(DAC_SPI_CS_PIN, 1);  // Set CS high initially (inactive)
    
    // Initialize RAM SPI (SPI1)
    spi_init(spi1, RAM_SPI_BAUD);  // 2MHz SPI clock for RAM
    gpio_set_function(RAM_SPI_SCK_PIN, GPIO_FUNC_SPI);
    gpio_set_function(RAM_SPI_TX_PIN, GPIO_FUNC_SPI);
    gpio_set_function(RAM_SPI_RX_PIN, GPIO_FUNC_SPI);
    gpio_set_dir(RAM_SPI_CS_PIN, GPIO_OUT);
    gpio_put(RAM_SPI_CS_PIN, 1);  // Set CS high initially (inactive)
    
    // Wait for RAM to power up
    sleep_ms(10);
    
    // Set RAM to sequential mode
    ram_write_status(RAM_MODE_SEQUENTIAL);
    
    // Generate and store sine wave samples in RAM as floats
    float sine_samples[NUM_SAMPLES];
    for(int i = 0; i < NUM_SAMPLES; i++) {
        float t = (float)i / NUM_SAMPLES;
        sine_samples[i] = 512.0f + 511.0f * sinf(2 * M_PI * t);
    }
    
    // Write sine wave samples to RAM
    ram_write_data(0, (uint8_t*)sine_samples, sizeof(sine_samples));
    
    uint8_t data[2];
    uint16_t ram_address = 0;
    union FloatInt converter;
    
    while (true) {
        // Read float from RAM
        ram_read_data(ram_address, (uint8_t*)&converter.f, sizeof(float));
        
        // Convert float to integer DAC value (0-1023)
        uint16_t dac_value = (uint16_t)roundf(converter.f);
        
        // Format data for DAC
        data[0] = 0b01110000 | ((dac_value >> 6) & 0b00001111);
        data[1] = (dac_value << 2) & 0b11111100;
        
        // Write to DAC
        dac_cs_select();
        spi_write_blocking(spi0, data, 2);
        dac_cs_deselect();
        
        // Update RAM address
        ram_address = (ram_address + sizeof(float)) % (NUM_SAMPLES * sizeof(float));
        
        // Delay 1ms
        sleep_ms(1);
    }
}