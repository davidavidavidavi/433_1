#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/i2c.h"
#include "hardware/gpio.h"

// I2C defines
// This example will use I2C0 on GPIO8 (SDA) and GPIO9 (SCL) running at 400KHz.
// Pins can be changed, see the GPIO function select table in the datasheet for information on GPIO assignments
#define I2C_PORT i2c0
#define I2C_SDA 8
#define I2C_SCL 9

// MCP23008 Register Addresses
#define MCP23008_ADDR 0x20

// MCP23008 Register Addresses
#define IODIR 0x00
#define IPOL 0x01
#define GPINTEN 0x02
#define DEFVAL 0x03
#define INTCON 0x04
#define IOCON 0x05
#define GPPU 0x06
#define INTF 0x07
#define INTCAP 0x08
#define GPIO  0x09
#define OLAT  0x0A

// Function to write to MCP23008 register
void setPin(uint8_t address, uint8_t reg, uint8_t value) {
    printf("Attempting to write to register 0x%02x: 0x%02x\n", reg, value);
    uint8_t buf[] = {reg, value};
    int result = i2c_write_blocking(I2C_PORT, address, buf, 2, false);
    if (result == PICO_ERROR_GENERIC) {
        printf("ERROR: I2C write failed!\n");
    } else if (result == 2) {
        printf("Successfully wrote 2 bytes to I2C\n");
    } else {
        printf("Unexpected I2C write result: %d\n", result);
    }
}

// Function to read from MCP23008 register
uint8_t readPin(uint8_t address, uint8_t reg) {
    printf("Attempting to read from register 0x%02x\n", reg);
    uint8_t value;
    
    // First write the register address
    int result = i2c_write_blocking(I2C_PORT, address, &reg, 1, true);
    if (result == PICO_ERROR_GENERIC) {
        printf("ERROR: I2C write (register address) failed!\n");
        return 0;
    } else if (result == 1) {
        printf("Successfully wrote register address\n");
    } else {
        printf("Unexpected I2C write result: %d\n", result);
        return 0;
    }
    
    // Then read the value
    result = i2c_read_blocking(I2C_PORT, address, &value, 1, false);
    if (result == PICO_ERROR_GENERIC) {
        printf("ERROR: I2C read failed!\n");
        return 0;
    } else if (result == 1) {
        printf("Successfully read value: 0x%02x\n", value);
    } else {
        printf("Unexpected I2C read result: %d\n", result);
        return 0;
    }
    
    return value;
}

int main()
{
    // Initialize built-in LED first
    gpio_init(PICO_DEFAULT_LED_PIN);
    gpio_set_dir(PICO_DEFAULT_LED_PIN, GPIO_OUT);
    gpio_put(PICO_DEFAULT_LED_PIN, 1);  // Turn LED on to indicate initialization

    // Initialize USB serial
    stdio_init_all();
    
    // Wait for USB connection
    printf("Waiting for USB connection...\n");
    while (!stdio_usb_connected()) {
        sleep_ms(100);
    }
    printf("USB connected!\n");
    sleep_ms(1000);  // Give time for USB to stabilize

    // Initialize I2C at 100KHz
    i2c_init(I2C_PORT, 100*1000);
    
    // Configure I2C pins
    gpio_set_function(I2C_SDA, GPIO_FUNC_I2C);
    gpio_set_function(I2C_SCL, GPIO_FUNC_I2C);
    gpio_pull_up(I2C_SDA);
    gpio_pull_up(I2C_SCL);
    // For more examples of I2C use see https://github.com/raspberrypi/pico-examples/tree/master/i2c
    printf("I2C initialized at 100KHz on pins %d and %d\n", I2C_SDA, I2C_SCL);

    // Configure MCP23008: GP7 = output, GP0 = input, others = input
    setPin(MCP23008_ADDR, IODIR, 0b01111111);  // GP7 output (bit 7 = 0), GP0 input (bit 0 = 1)
    printf("MCP23008 initialized - GP7 output, GP0 input\n");

    // Turn off the LED to indicate initialization is complete
    gpio_put(PICO_DEFAULT_LED_PIN, 0);
    printf("Entering main loop...\n");
    // Main loop
    while (true) {
        // Blink Pico onboard green LED (heartbeat)
        gpio_put(PICO_DEFAULT_LED_PIN, 1);
        sleep_ms(100);
        gpio_put(PICO_DEFAULT_LED_PIN, 0);
        sleep_ms(100);

        // Read button state from GP0
        uint8_t gpio_val = readPin(MCP23008_ADDR, GPIO);

        // Print out GPIO 0 state for debugging
        if (gpio_val & 0b00000001) {
            printf("Button NOT pressed (GP0 = HIGH)\n");
        } else {
            printf("Button PRESSED (GP0 = LOW)\n");
        }

        // Control GP7 based on button press
        if ((gpio_val & 0b00000001) == 0) {
            // Button pressed : Turn on GP7 LED
            setPin(MCP23008_ADDR, OLAT, 0b10000000);
        } else {
            // Button not pressed : Turn off GP7 LED
            setPin(MCP23008_ADDR, OLAT, 0x00);
        }
    }
}
