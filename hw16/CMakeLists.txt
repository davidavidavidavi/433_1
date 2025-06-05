#include <stdlib.h>
#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/pwm.h"

#define DIR_PIN       16
#define PWM_PIN       17
#define COUNTER_MAX   9999      /* 10,000 steps (0..9999) */
#define INCREMENT     1         /* change duty by 1% */

int main(void)
{
    stdio_init_all();

    /* Configure the direction (phase) output */
    gpio_init(DIR_PIN);
    gpio_set_dir(DIR_PIN, GPIO_OUT);

    /* Configure PWM on the enable pin */
    gpio_set_function(PWM_PIN, GPIO_FUNC_PWM);
    uint slice_index = pwm_gpio_to_slice_num(PWM_PIN);
    pwm_set_wrap(slice_index, COUNTER_MAX);   /* set top value */
    pwm_set_enabled(slice_index, true);

    int duty_percent = 0;    /* valid range: -100 .. +100 */

    while (true)
    {
        int ch = getchar_timeout_us(0);
        if (ch != PICO_ERROR_TIMEOUT)
        {
            if (ch == '+') 
            {
                if (duty_percent < 100)
                    duty_percent += INCREMENT;
            }
            else if (ch == '-')
            {
                if (duty_percent > -100)
                    duty_percent -= INCREMENT;
            }

            /* Compute PWM level from absolute duty (0..(COUNTER_MAX+1)) */
            uint16_t level = (uint16_t)((abs(duty_percent) * (COUNTER_MAX + 1)) / 100);

            /* Drive direction pin: high for non‐negative, low for negative */
            gpio_put(DIR_PIN, (duty_percent >= 0) ? 1 : 0);

            /* Update PWM output */
            pwm_set_chan_level(
                slice_index,
                pwm_gpio_to_channel(PWM_PIN),
                level
            );

            printf("Duty cycle: %d%%\n", duty_percent);
        }
        sleep_ms(10);
    }

    return 0;
}
