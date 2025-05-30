#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/i2c.h"
#include "hardware/adc.h"
#include "ssd1306.h"
#include "font.h"

// I2C defines
// This example will use I2C0 on GPIO8 (SDA) and GPIO9 (SCL) running at 400KHz.
// Pins can be changed, see the GPIO function select table in the datasheet for information on GPIO assignments
#define I2C_PORT i2c0
#define I2C_SDA 8
#define I2C_SCL 9
#define LED_PIN 25

// Function to draw a single character at position (x,y)
void drawChar(int x, int y, char c) {
    if (c < 0x20 || c > 0x7F) return; // Only handle printable ASCII
    int charIndex = c - 0x20;
    
    for (int col = 0; col < 5; col++) {
        unsigned char line = ASCII[charIndex][col];
        for (int row = 0; row < 8; row++) {
            if (line & (1 << row)) {
                ssd1306_drawPixel(x + col, y + row, 1);
            }
        }
    }
}

// Function to draw a message starting at position (x,y)
void drawMessage(int x, int y, const char* message) {
    int currentX = x;
    while (*message != '\0') {
        drawChar(currentX, y, *message);
        currentX += 6; // 5 pixels for char + 1 pixel spacing
        message++;
    }
}

int main()
{
    stdio_init_all();

    // Initialize LED
    gpio_init(LED_PIN);
    gpio_set_dir(LED_PIN, GPIO_OUT);
    
    // Initialize I2C
    i2c_init(I2C_PORT, 400*1000);
    
    gpio_set_function(I2C_SDA, GPIO_FUNC_I2C);
    gpio_set_function(I2C_SCL, GPIO_FUNC_I2C);
    gpio_pull_up(I2C_SDA);
    gpio_pull_up(I2C_SCL);
    // For more examples of I2C use see https://github.com/raspberrypi/pico-examples/tree/master/i2c

    // Initialize ADC
    adc_init();
    adc_gpio_init(26); // ADC0 is on GPIO26
    adc_select_input(0);
    
    // Initialize OLED
    ssd1306_setup();
    
    // Variables for FPS calculation
    unsigned int lastTime = 0;
    unsigned int frameCount = 0;
    float fps = 0;
    
    // Test variables
    int testVar = 15;
    char message[50];
    
    while (true) {
        // Toggle LED for heartbeat
        gpio_put(LED_PIN, !gpio_get(LED_PIN));
        
        // Clear display
        ssd1306_clear();
        
        // Read ADC and convert to voltage
        uint16_t adc_value = adc_read();
        float voltage = adc_value * 3.3f / 4096.0f;
        
        // Create and display test message
        sprintf(message, "Test var = %d", testVar);
        drawMessage(0, 0, message);
        
        // Display voltage reading
        sprintf(message, "ADC0: %.2fV", voltage);
        drawMessage(0, 8, message);
        
        // Calculate and display FPS
        unsigned int currentTime = to_us_since_boot(get_absolute_time());
        frameCount++;
        
        if (currentTime - lastTime >= 1000000) { // Every second
            fps = frameCount * 1000000.0f / (currentTime - lastTime);
            frameCount = 0;
            lastTime = currentTime;
        }
        
        sprintf(message, "FPS: %.1f", fps);
        drawMessage(0, 24, message);
        
        // Update display
        ssd1306_update();
        
        // Sleep for approximately 1 second
        sleep_ms(1000);
    }
}
