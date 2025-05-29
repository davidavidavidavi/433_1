/**
 * Copyright (c) 2020 Raspberry Pi (Trading) Ltd.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <stdio.h>
#include "pico/stdlib.h"

int main() {
    stdio_init_all();
    
    // Wait for USB serial to be ready
    sleep_ms(2000);
    
    volatile float f1, f2;
    printf("Enter two floats to use: ");
    scanf("%f %f", &f1, &f2);
    
    volatile float f_add, f_sub, f_mult, f_div;
    uint64_t t_add, t_sub, t_mult, t_div;
    absolute_time_t t1, t2;
    
    // Addition timing
    t1 = get_absolute_time();
    for(int i = 0; i < 1000; i++) {
        f_add = f1 + f2;
    }
    t2 = get_absolute_time();
    t_add = to_us_since_boot(t2) - to_us_since_boot(t1);
    
    // Subtraction timing
    t1 = get_absolute_time();
    for(int i = 0; i < 1000; i++) {
        f_sub = f1 - f2;
    }
    t2 = get_absolute_time();
    t_sub = to_us_since_boot(t2) - to_us_since_boot(t1);
    
    // Multiplication timing
    t1 = get_absolute_time();
    for(int i = 0; i < 1000; i++) {
        f_mult = f1 * f2;
    }
    t2 = get_absolute_time();
    t_mult = to_us_since_boot(t2) - to_us_since_boot(t1);
    
    // Division timing
    t1 = get_absolute_time();
    for(int i = 0; i < 1000; i++) {
        f_div = f1 / f2;
    }
    t2 = get_absolute_time();
    t_div = to_us_since_boot(t2) - to_us_since_boot(t1);
    
    // Calculate clock cycles (150MHz = 6.667ns per cycle)
    float cycles_add = (t_add / 1000.0) * 150.0;
    float cycles_sub = (t_sub / 1000.0) * 150.0;
    float cycles_mult = (t_mult / 1000.0) * 150.0;
    float cycles_div = (t_div / 1000.0) * 150.0;
    
    printf("\nResults: \n");
    printf("%f + %f = %f (%.1f cycles)\n", f1, f2, f_add, cycles_add);
    printf("%f - %f = %f (%.1f cycles)\n", f1, f2, f_sub, cycles_sub);
    printf("%f * %f = %f (%.1f cycles)\n", f1, f2, f_mult, cycles_mult);
    printf("%f / %f = %f (%.1f cycles)\n", f1, f2, f_div, cycles_div);
    
    while (true) {
        sleep_ms(1000);
    }
}
