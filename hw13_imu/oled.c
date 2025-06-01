#include <stdio.h>
#include <math.h>
#include <stdlib.h>
#include "pico/stdlib.h"
#include "hardware/i2c.h"
#include "hardware/adc.h"
#include "ssd1306.h"
#include "font.h"
// config registers
#define CONFIG 0x1A
#define GYRO_CONFIG 0x1B
#define ACCEL_CONFIG 0x1C
#define PWR_MGMT_1 0x6B
#define PWR_MGMT_2 0x6C
// sensor data registers:
#define ACCEL_XOUT_H 0x3B
#define ACCEL_XOUT_L 0x3C
#define ACCEL_YOUT_H 0x3D
#define ACCEL_YOUT_L 0x3E
#define ACCEL_ZOUT_H 0x3F
#define ACCEL_ZOUT_L 0x40
#define TEMP_OUT_H   0x41
#define TEMP_OUT_L   0x42
#define GYRO_XOUT_H  0x43
#define GYRO_XOUT_L  0x44
#define GYRO_YOUT_H  0x45
#define GYRO_YOUT_L  0x46
#define GYRO_ZOUT_H  0x47
#define GYRO_ZOUT_L  0x48
#define WHO_AM_I     0x75
// I2C defines
// This example will use I2C0 on GPIO8 (SDA) and GPIO9 (SCL) running at 400KHz.
// Pins can be changed, see the GPIO function select table in the datasheet for information on GPIO assignments
#define I2C_PORT i2c0
#define I2C_SDA 8
#define I2C_SCL 9
#define I2C1_PORT i2c1
#define I2C1_SDA 10
#define I2C1_SCL 11
#define LED_PIN 25
#define IMU_ADDR 0x68  // MPU6050 I2C address

// Function to write to IMU register
void write_imu_reg(uint8_t reg, uint8_t data) {
    uint8_t buf[2];
    buf[0] = reg;
    buf[1] = data;
    i2c_write_blocking(I2C1_PORT, IMU_ADDR, buf, 2, false);
}

// Function to read IMU data
void read_imu_data(int16_t* accel_x, int16_t* accel_y, int16_t* accel_z,
                  int16_t* temp, int16_t* gyro_x, int16_t* gyro_y, int16_t* gyro_z) {
    uint8_t buf[14];
    uint8_t reg = ACCEL_XOUT_H;
    
    // Burst read 14 bytes starting from ACCEL_XOUT_H
    i2c_write_blocking(I2C1_PORT, IMU_ADDR, &reg, 1, true);
    i2c_read_blocking(I2C1_PORT, IMU_ADDR, buf, 14, false);
    
    // Combine bytes into 16-bit values
    *accel_x = (buf[0] << 8) | buf[1];
    *accel_y = (buf[2] << 8) | buf[3];
    *accel_z = (buf[4] << 8) | buf[5];
    *temp = (buf[6] << 8) | buf[7];
    *gyro_x = (buf[8] << 8) | buf[9];
    *gyro_y = (buf[10] << 8) | buf[11];
    *gyro_z = (buf[12] << 8) | buf[13];
}

// Function to initialize IMU
void init_imu() {
    // Wake up the IMU
    write_imu_reg(PWR_MGMT_1, 0x00);
    sleep_ms(100);  // Wait for IMU to wake up
    
    // Configure accelerometer (±2g)
    write_imu_reg(ACCEL_CONFIG, 0x00);
    
    // Configure gyroscope (±2000dps)
    write_imu_reg(GYRO_CONFIG, 0x18);
}

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
    while (!stdio_usb_connected()) {
        sleep_ms(100);
    }
    printf("Start!\n");

    // Initialize LED
    gpio_init(LED_PIN);
    gpio_set_dir(LED_PIN, GPIO_OUT);
    gpio_put(LED_PIN, 1);  // Turn LED on when USB is connected
    
    // Initialize I2C0 for OLED
    i2c_init(I2C_PORT, 400*1000);
    gpio_set_function(I2C_SDA, GPIO_FUNC_I2C);
    gpio_set_function(I2C_SCL, GPIO_FUNC_I2C);
    gpio_pull_up(I2C_SDA);
    gpio_pull_up(I2C_SCL);
    
    // Initialize I2C1 for IMU
    i2c_init(I2C1_PORT, 400*1000);
    gpio_set_function(I2C1_SDA, GPIO_FUNC_I2C);
    gpio_set_function(I2C1_SCL, GPIO_FUNC_I2C);
    gpio_pull_up(I2C1_SDA);
    gpio_pull_up(I2C1_SCL);
    
    // Initialize IMU
    init_imu();
    
    // Initialize OLED
    ssd1306_setup();
    
    // Variables for FPS calculation
    unsigned int lastTime = to_us_since_boot(get_absolute_time());
    unsigned int frameCount = 0;
    float fps = 0;
    
    // IMU data variables
    int16_t accel_x, accel_y, accel_z;
    int16_t temp;
    int16_t gyro_x, gyro_y, gyro_z;
    char message[50];
    
    // Center point for vector visualization
    const int center_x = 64;  // OLED width is 128, so center is at 64
    const int center_y = 16;  // OLED height is 32, so center is at 16
    const int start_x = center_x + 10;  // Start point slightly to the right of center
    const int start_y = 16;  // Start at middle height
    
    // Variables for smooth movement
    float current_x = 0.0f;
    float current_y = 0.0f;
    const float smoothing_factor = 0.1f;  // More sticky (was 0.2f)
    
    while (true) {
        // Toggle LED for heartbeat
        gpio_put(LED_PIN, !gpio_get(LED_PIN));
        
        // Read IMU data
        read_imu_data(&accel_x, &accel_y, &accel_z, &temp, &gyro_x, &gyro_y, &gyro_z);
        
        // Convert to proper units
        float accel_x_g = accel_x * 0.000061f;
        float accel_y_g = accel_y * 0.000061f;
        float accel_z_g = accel_z * 0.000061f;
        float temp_c = (temp / 340.00f) + 36.53f;
        float gyro_x_dps = gyro_x * 0.007630f;
        float gyro_y_dps = gyro_y * 0.007630f;
        float gyro_z_dps = gyro_z * 0.007630f;
        
        // Calculate combined gyro movement (magnitude of rotation)
        float gyro_magnitude = sqrtf(gyro_x_dps * gyro_x_dps + 
                                   gyro_y_dps * gyro_y_dps + 
                                   gyro_z_dps * gyro_z_dps);
        
        // Clear display
        ssd1306_clear();
        
        // Display simplified numbers
        sprintf(message, "X:%.1f", accel_x_g);
        drawMessage(0, 0, message);
        sprintf(message, "Y:%.1f", accel_y_g);
        drawMessage(0, 8, message);
        sprintf(message, "G:%.1f", gyro_magnitude);
        drawMessage(0, 16, message);
        
        // Draw the starting point
        ssd1306_drawPixel(start_x, start_y, 1);
        
        // Apply smoothing to the acceleration values
        current_x = current_x + (-accel_x_g - current_x) * smoothing_factor;  // Inverted x direction
        current_y = current_y + (accel_y_g - current_y) * smoothing_factor;
        
        // Calculate vector end point with smoothed values
        const float scale = 60.0f;  // Increased from 20.0f to 60.0f for more pronounced movement
        int end_x = start_x + (int)(current_x * scale);
        int end_y = start_y + (int)(current_y * scale);
        
        // Clamp the end point to screen boundaries
        if (end_x < 0) end_x = 0;
        if (end_x > 127) end_x = 127;
        if (end_y < 0) end_y = 0;
        if (end_y > 31) end_y = 31;  // Changed from 63 to 31 for 32-pixel height
        
        // Draw the line
        // Using Bresenham's line algorithm for better line drawing
        int dx = abs(end_x - start_x);
        int dy = abs(end_y - start_y);
        int sx = (start_x < end_x) ? 1 : -1;
        int sy = (start_y < end_y) ? 1 : -1;
        int err = dx - dy;
        
        int x = start_x;
        int y = start_y;
        
        while (true) {
            ssd1306_drawPixel(x, y, 1);
            if (x == end_x && y == end_y) break;
            int e2 = 2 * err;
            if (e2 > -dy) {
                err -= dy;
                x += sx;
            }
            if (e2 < dx) {
                err += dx;
                y += sy;
            }
        }
        
        // Update display
        ssd1306_update();
        
        // Print to USB serial
        printf("Accel: X=%.2f Y=%.2f Z=%.2f (g)\n", accel_x_g, accel_y_g, accel_z_g);
        printf("Gyro: X=%.1f Y=%.1f Z=%.1f (dps)\n", gyro_x_dps, gyro_y_dps, gyro_z_dps);
        printf("Temp: %.1f°C\n", temp_c);
        
        // Maintain approximately 100Hz update rate
        sleep_ms(10);
    }
}
