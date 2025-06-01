/**
 * Copyright (c) 2020 Raspberry Pi (Trading) Ltd.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#include "pico/stdlib.h"
#include "hardware/pio.h"
#include "hardware/clocks.h"
#include "hardware/pwm.h"
#include "ws2812.pio.h"

/**
 * NOTE:
 *  Take into consideration if your WS2812 is a RGB or RGBW variant.
 *
 *  If it is RGBW, you need to set IS_RGBW to true and provide 4 bytes per 
 *  pixel (Red, Green, Blue, White) and use urgbw_u32().
 *
 *  If it is RGB, set IS_RGBW to false and provide 3 bytes per pixel (Red, Green, Blue) and use urgb_u32().
 *  When RGBW is used with urgb_u32(), the White channel will be ignored (off).
 */
#define IS_RGBW      false
#define NUM_PIXELS   4

// WS2812 timing (rainbow cycle over ~5 seconds)
#define LED_UPDATE_MS    28   // update LEDs every 28 ms (~180 steps × 28 ms ≈ 5040 ms)

// Servo configuration (0.3 ms → 3.0 ms)
#define SERVO_PIN        15     // PWM pin for servo
#define SERVO_MIN_PULSE  300    // 0.3 ms in microseconds
#define SERVO_MAX_PULSE  3000   // 3.0 ms in microseconds
#define SERVO_FREQ       50     // 50 Hz (20 ms period)
#define SERVO_UPDATE_MS  10     // update servo every 10 ms
#define SERVO_STEPS      500    // 500 steps × 10 ms = 5000 ms (5 s one way)

#ifdef PICO_DEFAULT_WS2812_PIN
  #define WS2812_PIN PICO_DEFAULT_WS2812_PIN
#else
  #define WS2812_PIN 2
#endif

#if WS2812_PIN >= NUM_BANK0_GPIOS
  #error Attempting to use a pin>=32 on a platform that does not support it
#endif

// Struct for RGB values
typedef struct {
    unsigned char r;
    unsigned char g;
    unsigned char b;
} wsColor;

// Send a 24-bit GRB pixel to PIO
static inline void put_pixel(PIO pio, uint sm, uint32_t pixel_grb) {
    pio_sm_put_blocking(pio, sm, pixel_grb << 8u);
}
static inline uint32_t urgb_u32(uint8_t r, uint8_t g, uint8_t b) {
    return ((uint32_t)(r) << 8)  |
           ((uint32_t)(g) << 16) |
           (uint32_t)(b);
}
// Convert HSB to RGB
wsColor HSBtoRGB(float hue, float sat, float brightness) {
    float red = 0.0f, green = 0.0f, blue = 0.0f;
    if (sat == 0.0f) {
        red = green = blue = brightness;
    } else {
        if (hue >= 360.0f) hue = 0.0f;
        int slice = (int)(hue / 60.0f);
        float hue_frac = (hue / 60.0f) - slice;
        float aa = brightness * (1.0f - sat);
        float bb = brightness * (1.0f - sat * hue_frac);
        float cc = brightness * (1.0f - sat * (1.0f - hue_frac));

        switch (slice) {
            case 0: red = brightness; green = cc;     blue = aa; break;
            case 1: red = bb;        green = brightness; blue = aa; break;
            case 2: red = aa;        green = brightness; blue = cc; break;
            case 3: red = aa;        green = bb;     blue = brightness; break;
            case 4: red = cc;        green = aa;     blue = brightness; break;
            case 5: red = brightness;green = aa;     blue = bb; break;
            default: red = green = blue = 0.0f; break;
        }
    }
    wsColor col;
    col.r = (unsigned char)(red * 255.0f);
    col.g = (unsigned char)(green * 255.0f);
    col.b = (unsigned char)(blue * 255.0f);
    return col;
}
// Update all WS2812 LEDs
void set_led_colors(PIO pio, uint sm, uint8_t *r, uint8_t *g, uint8_t *b) {
    for (int i = 0; i < NUM_PIXELS; i++) {
        put_pixel(pio, sm, urgb_u32(r[i], g[i], b[i]));
    }
    sleep_ms(1); // latch/reset
}

int main() {
    stdio_init_all();
    printf("WS2812 Rainbow + Servo Sweep (0.3 ms ↔ 3.0 ms)\n");

    // -------------------------
    // Initialize WS2812 (PIO)
    // -------------------------
    PIO pio;
    uint sm;
    uint offset;
    bool success = pio_claim_free_sm_and_add_program_for_gpio_range(
        &ws2812_program, &pio, &sm, &offset, WS2812_PIN, 1, true
    );
    hard_assert(success);
    ws2812_program_init(pio, sm, offset, WS2812_PIN, 800000, IS_RGBW);

    // -------------------------
    // Initialize Servo (PWM)
    // -------------------------
    gpio_set_function(SERVO_PIN, GPIO_FUNC_PWM);
    uint servo_slice = pwm_gpio_to_slice_num(SERVO_PIN);
    // 125 MHz / 125 → 1 MHz ⇒ 1 tick = 1 μs
    pwm_set_clkdiv(servo_slice, 125.0f);
    // 20 ms period ⇒ wrap = 20 000 − 1
    pwm_set_wrap(servo_slice, 20000 - 1);
    pwm_set_gpio_level(SERVO_PIN, SERVO_MIN_PULSE);
    pwm_set_enabled(servo_slice, true);

    // -------------------------
    // LED variables
    // -------------------------
    float hue = 0.0f;
    const float hue_step = 360.0f / 180.0f;     // 180 steps → ≈5 s
    const float led_offset = 360.0f / NUM_PIXELS;
    uint8_t r_vals[NUM_PIXELS], g_vals[NUM_PIXELS], b_vals[NUM_PIXELS];
    int led_timer = 0;   // counts milliseconds for next LED update

    // -------------------------
    // Servo variables
    // -------------------------
    int servo_timer     = 0;  // counts milliseconds for next servo update
    int servo_step      = 0;  // ranges 0..499
    int servo_direction = 1;  // +1 going toward max, −1 toward min

    while (1) {
        // Sleep exactly 1 ms, then increment our “timers”
        sleep_ms(1);
        led_timer++;
        servo_timer++;

        // -------------------
        // Update LED every 28 ms
        // -------------------
        if (led_timer >= LED_UPDATE_MS) {
            for (int i = 0; i < NUM_PIXELS; i++) {
                float this_hue = fmodf(hue + (i * led_offset), 360.0f);
                wsColor c = HSBtoRGB(this_hue, 1.0f, 1.0f);
                r_vals[i] = c.r;
                g_vals[i] = c.g;
                b_vals[i] = c.b;
            }
            set_led_colors(pio, sm, r_vals, g_vals, b_vals);
            hue = fmodf(hue + hue_step, 360.0f);
            led_timer = 0;
        }

        // ------------------------
        // Update Servo every 10 ms
        // ------------------------
        if (servo_timer >= SERVO_UPDATE_MS) {
            servo_step += servo_direction;
            if (servo_step >= SERVO_STEPS) {
                servo_step = SERVO_STEPS - 1;
                servo_direction = -1;
            } else if (servo_step < 0) {
                servo_step = 0;
                servo_direction = 1;
            }
            // Map servo_step (0..499) → pulse (300..3000 μs)
            float frac = (float)servo_step / (float)(SERVO_STEPS - 1);
            uint32_t pulse = (uint32_t)(
                SERVO_MIN_PULSE + frac * (SERVO_MAX_PULSE - SERVO_MIN_PULSE)
            );
            pwm_set_gpio_level(SERVO_PIN, pulse);
            servo_timer = 0;
        }
    }

    // (unreachable in this example)
    pwm_set_enabled(servo_slice, false);
    pio_remove_program_and_unclaim_sm(&ws2812_program, pio, sm, offset);
}
