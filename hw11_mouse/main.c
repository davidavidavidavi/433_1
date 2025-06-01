/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2019 Ha Thach (tinyusb.org)
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 *
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

#include "bsp/board_api.h"
#include "tusb.h"
#include "hardware/gpio.h"

#include "usb_descriptors.h"

//--------------------------------------------------------------------+
// MACRO CONSTANT TYPEDEF PROTOTES
//--------------------------------------------------------------------+

/* Blink pattern
 * - 250 ms  : device not mounted
 * - 1000 ms : device mounted
 * - 2500 ms : device is suspended
 */
enum {
  BLINK_NOT_MOUNTED = 250,
  BLINK_MOUNTED     = 1000,
  BLINK_SUSPENDED   = 2500,
};

static uint32_t blink_interval_ms = BLINK_NOT_MOUNTED;

// Button pins
#define PIN_MODE_SWITCH 17
#define PIN_MOUSE_DOWN  18
#define PIN_MOUSE_RIGHT 19
#define PIN_MOUSE_LEFT  20
#define PIN_MOUSE_UP    21
#define PIN_STATUS_LED  16

// Mouse movement constants
#define MOUSE_SPEED_LEVELS       4
#define MOUSE_SPEED_INTERVAL_MS  500   // Time between speed increases
#define MOUSE_CIRCLE_RADIUS      5     // Radius of circle in pixels
#define MOUSE_CIRCLE_SPEED       0.1f  // Radians per update

// Mode definitions
typedef enum {
  MODE_REGULAR,
  MODE_REMOTE
} mouse_mode_t;

// Button debouncing constants
#define DEBOUNCE_DELAY_MS       50
#define MODE_DEBOUNCE_DELAY_MS  200

// Button state tracking
typedef struct {
  uint32_t press_time;
  bool     is_pressed;
  bool     last_state;
  uint32_t last_debounce_time;
} button_state_t;

// Global variables
static mouse_mode_t   current_mode        = MODE_REGULAR;
static button_state_t button_states[4];         // For up, down, left, right
static button_state_t mode_button_state = {0};  // For mode switch
static float          circle_angle       = 0.0f; // For remote mode circle

//--------------------------------------------------------------------+
// PROTOTYPES
//--------------------------------------------------------------------+
void led_blinking_task(void);
void hid_task(void);
void init_gpio(void);
void update_mouse_position(void);

//--------------------------------------------------------------------+
// MAIN
//--------------------------------------------------------------------+
int main(void)
{
  board_init();
  init_gpio();

  // init device stack on configured roothub port
  tud_init(BOARD_TUD_RHPORT);

  if (board_init_after_tusb) {
    board_init_after_tusb();
  }

  while (1)
  {
    tud_task();          // tinyusb device task
    led_blinking_task();
    update_mouse_position();
  }
}

//--------------------------------------------------------------------+
// DEVICE CALLBACKS
//--------------------------------------------------------------------+

// Invoked when device is mounted
void tud_mount_cb(void)
{
  blink_interval_ms = BLINK_MOUNTED;
}

// Invoked when device is unmounted
void tud_umount_cb(void)
{
  blink_interval_ms = BLINK_NOT_MOUNTED;
}

// Invoked when usb bus is suspended
void tud_suspend_cb(bool remote_wakeup_en)
{
  (void) remote_wakeup_en;
  blink_interval_ms = BLINK_SUSPENDED;
}

// Invoked when usb bus is resumed
void tud_resume_cb(void)
{
  blink_interval_ms = tud_mounted() ? BLINK_MOUNTED : BLINK_NOT_MOUNTED;
}

//--------------------------------------------------------------------+
// USB HID
//--------------------------------------------------------------------+

static void send_hid_report(uint8_t report_id, uint32_t btn)
{
  // skip if hid is not ready yet
  if (!tud_hid_ready()) return;

  switch (report_id)
  {
    case REPORT_ID_KEYBOARD:
    {
      // use to avoid send multiple consecutive zero report for keyboard
      static bool has_keyboard_key = false;

      if (btn)
      {
        uint8_t keycode[6] = { 0 };
        keycode[0] = HID_KEY_A;

        tud_hid_keyboard_report(REPORT_ID_KEYBOARD, 0, keycode);
        has_keyboard_key = true;
      }
      else
      {
        // send empty key report if previously has key pressed
        if (has_keyboard_key) tud_hid_keyboard_report(REPORT_ID_KEYBOARD, 0, NULL);
        has_keyboard_key = false;
      }
    }
    break;

    case REPORT_ID_MOUSE:
    {
      // Mouse report is sent directly in update_mouse_position()
      // so we do nothing here
    }
    break;

    case REPORT_ID_CONSUMER_CONTROL:
    {
      // use to avoid send multiple consecutive zero report
      static bool has_consumer_key = false;

      if (btn)
      {
        // volume down
        uint16_t volume_down = HID_USAGE_CONSUMER_VOLUME_DECREMENT;
        tud_hid_report(REPORT_ID_CONSUMER_CONTROL, &volume_down, 2);
        has_consumer_key = true;
      }
      else
      {
        // send empty key report (release key) if previously has key pressed
        uint16_t empty_key = 0;
        if (has_consumer_key) tud_hid_report(REPORT_ID_CONSUMER_CONTROL, &empty_key, 2);
        has_consumer_key = false;
      }
    }
    break;

    case REPORT_ID_GAMEPAD:
    {
      // use to avoid send multiple consecutive zero report for gamepad
      static bool has_gamepad_key = false;

      hid_gamepad_report_t report =
      {
        .x       = 0,
        .y       = 0,
        .z       = 0,
        .rz      = 0,
        .rx      = 0,
        .ry      = 0,
        .hat     = 0,
        .buttons = 0
      };

      if (btn)
      {
        report.hat     = GAMEPAD_HAT_UP;
        report.buttons = GAMEPAD_BUTTON_A;
        tud_hid_report(REPORT_ID_GAMEPAD, &report, sizeof(report));
        has_gamepad_key = true;
      }
      else
      {
        report.hat     = GAMEPAD_HAT_CENTERED;
        report.buttons = 0;
        if (has_gamepad_key) tud_hid_report(REPORT_ID_GAMEPAD, &report, sizeof(report));
        has_gamepad_key = false;
      }
    }
    break;

    default: break;
  }
}

// Every 10ms, send 1 report for each HID profile (keyboard, mouse, etc.)
void hid_task(void)
{
  const uint32_t interval_ms = 10;
  static uint32_t start_ms   = 0;

  if (board_millis() - start_ms < interval_ms) return; // not enough time
  start_ms += interval_ms;

  uint32_t const btn = board_button_read();

  // Remote wakeup
  if (tud_suspended() && btn)
  {
    // Wake up host if we are in suspend mode
    // and REMOTE_WAKEUP feature is enabled by host
    tud_remote_wakeup();
  }
  else
  {
    // Send first report in chain; others will be sent by tud_hid_report_complete_cb()
    send_hid_report(REPORT_ID_MOUSE, btn);
  }
}

// Invoked when sent REPORT successfully to host
void tud_hid_report_complete_cb(uint8_t instance, uint8_t const* report, uint16_t len)
{
  (void) instance;
  (void) len;

  uint8_t next_report_id = report[0] + 1u;

  if (next_report_id < REPORT_ID_COUNT)
  {
    send_hid_report(next_report_id, board_button_read());
  }
}

// Invoked when GET_REPORT control request is received
uint16_t tud_hid_get_report_cb(uint8_t instance,
                               uint8_t report_id,
                               hid_report_type_t report_type,
                               uint8_t* buffer,
                               uint16_t reqlen)
{
  (void) instance;
  (void) report_id;
  (void) report_type;
  (void) buffer;
  (void) reqlen;

  // Not implemented
  return 0;
}

// Invoked when SET_REPORT control request is received or data is received on OUT endpoint
void tud_hid_set_report_cb(uint8_t instance,
                           uint8_t report_id,
                           hid_report_type_t report_type,
                           uint8_t const* buffer,
                           uint16_t bufsize)
{
  (void) instance;

  if (report_type == HID_REPORT_TYPE_OUTPUT)
  {
    // Set keyboard LED (e.g. Capslock)
    if (report_id == REPORT_ID_KEYBOARD)
    {
      if (bufsize < 1) return;

      uint8_t const kbd_leds = buffer[0];

      if (kbd_leds & KEYBOARD_LED_CAPSLOCK)
      {
        // Capslock On: disable blink, turn LED on
        blink_interval_ms = 0;
        board_led_write(true);
      }
      else
      {
        // Capslock Off: resume normal blink
        board_led_write(false);
        blink_interval_ms = BLINK_MOUNTED;
      }
    }
  }
}

//--------------------------------------------------------------------+
// BLINKING TASK
//--------------------------------------------------------------------+
void led_blinking_task(void)
{
  static uint32_t start_ms = 0;
  static bool     led_state = false;

  // blink is disabled
  if (!blink_interval_ms) return;

  // Blink every interval_ms
  if (board_millis() - start_ms < blink_interval_ms) return;
  start_ms += blink_interval_ms;

  board_led_write(led_state);
  led_state = !led_state;
}

//--------------------------------------------------------------------+
// INITIALIZE GPIO PINS
//--------------------------------------------------------------------+
void init_gpio(void)
{
  uint32_t now = board_millis();

  // MODE SWITCH (active low)
  gpio_init(PIN_MODE_SWITCH);
  gpio_set_dir(PIN_MODE_SWITCH, GPIO_IN);
  gpio_pull_up(PIN_MODE_SWITCH);  // use internal pull-up

  mode_button_state.last_state         = !gpio_get(PIN_MODE_SWITCH); // invert because active low
  mode_button_state.is_pressed         = false;
  mode_button_state.last_debounce_time = now;
  mode_button_state.press_time         = 0;

  // MOUSE CONTROL BUTTONS (active low)
  int pins[4] = { PIN_MOUSE_UP, PIN_MOUSE_DOWN, PIN_MOUSE_LEFT, PIN_MOUSE_RIGHT };
  for (int i = 0; i < 4; i++)
  {
    gpio_init(pins[i]);
    gpio_set_dir(pins[i], GPIO_IN);
    gpio_pull_up(pins[i]);  // use internal pull-up

    button_states[i].last_state         = !gpio_get(pins[i]); // invert because active low
    button_states[i].is_pressed         = false;
    button_states[i].last_debounce_time = now;
    button_states[i].press_time         = 0;
  }

  // STATUS LED
  gpio_init(PIN_STATUS_LED);
  gpio_set_dir(PIN_STATUS_LED, GPIO_OUT);
}

//--------------------------------------------------------------------+
// UPDATE MOUSE POSITION BASED ON CURRENT MODE
//--------------------------------------------------------------------+
void update_mouse_position(void)
{
  if (!tud_hid_ready()) return;

  uint32_t current_time = board_millis();
  int8_t   delta_x      = 0;
  int8_t   delta_y      = 0;

  // ——— MODE SWITCH (active low, with debounce) ———
  bool raw_mode_read = !gpio_get(PIN_MODE_SWITCH);  // inverted: 0 means button pressed
  if (raw_mode_read != mode_button_state.last_state)
  {
    mode_button_state.last_debounce_time = current_time;
  }

  if ((current_time - mode_button_state.last_debounce_time) > MODE_DEBOUNCE_DELAY_MS)
  {
    if (raw_mode_read && !mode_button_state.is_pressed)
    {
      // toggle between REGULAR and REMOTE
      current_mode = (current_mode == MODE_REGULAR) ? MODE_REMOTE : MODE_REGULAR;
      gpio_put(PIN_STATUS_LED, (current_mode == MODE_REMOTE));
    }
    mode_button_state.is_pressed = raw_mode_read;
  }
  mode_button_state.last_state = raw_mode_read;

  if (current_mode == MODE_REGULAR)
  {
    // ——— REGULAR MODE: check the four arrow buttons (active low) ———
    bool up_pressed    = false;
    bool down_pressed  = false;
    bool left_pressed  = false;
    bool right_pressed = false;

    int pins[4] = { PIN_MOUSE_UP, PIN_MOUSE_DOWN, PIN_MOUSE_LEFT, PIN_MOUSE_RIGHT };
    for (int i = 0; i < 4; i++)
    {
      bool raw = !gpio_get(pins[i]);  // inverted: 0 → pressed, 1 → not pressed

      if (raw != button_states[i].last_state)
      {
        button_states[i].last_debounce_time = current_time;
      }
      if ((current_time - button_states[i].last_debounce_time) > DEBOUNCE_DELAY_MS)
      {
        if (raw && !button_states[i].is_pressed)
        {
          // just pressed
          button_states[i].press_time = current_time;
        }
        else if (!raw)
        {
          button_states[i].press_time = 0;
        }
        button_states[i].is_pressed = raw;
      }
      button_states[i].last_state = raw;

      if (i == 0)        up_pressed    = button_states[i].is_pressed;
      else if (i == 1)   down_pressed  = button_states[i].is_pressed;
      else if (i == 2)   left_pressed  = button_states[i].is_pressed;
      else               right_pressed = button_states[i].is_pressed;
    }

    // Move only if a button is pressed
    if (up_pressed)
    {
      uint32_t hold = current_time - button_states[0].press_time;
      int speed = hold / MOUSE_SPEED_INTERVAL_MS;
      if (speed >= MOUSE_SPEED_LEVELS) speed = MOUSE_SPEED_LEVELS - 1;
      delta_y = -(speed + 1);
    }
    if (down_pressed)
    {
      uint32_t hold = current_time - button_states[1].press_time;
      int speed = hold / MOUSE_SPEED_INTERVAL_MS;
      if (speed >= MOUSE_SPEED_LEVELS) speed = MOUSE_SPEED_LEVELS - 1;
      delta_y = speed + 1;
    }
    if (left_pressed)
    {
      uint32_t hold = current_time - button_states[2].press_time;
      int speed = hold / MOUSE_SPEED_INTERVAL_MS;
      if (speed >= MOUSE_SPEED_LEVELS) speed = MOUSE_SPEED_LEVELS - 1;
      delta_x = -(speed + 1);
    }
    if (right_pressed)
    {
      uint32_t hold = current_time - button_states[3].press_time;
      int speed = hold / MOUSE_SPEED_INTERVAL_MS;
      if (speed >= MOUSE_SPEED_LEVELS) speed = MOUSE_SPEED_LEVELS - 1;
      delta_x = speed + 1;
    }
  }
  else
  {
    // ——— REMOTE MODE: move mouse in a circle ———
    circle_angle += MOUSE_CIRCLE_SPEED;
    if (circle_angle >= 2.0f * M_PI) circle_angle -= 2.0f * M_PI;

    delta_x = (int8_t)(MOUSE_CIRCLE_RADIUS * cosf(circle_angle));
    delta_y = (int8_t)(MOUSE_CIRCLE_RADIUS * sinf(circle_angle));
  }

  // Always send a mouse HID report (delta_x, delta_y will be zero if no arrow pressed)
  tud_hid_mouse_report(REPORT_ID_MOUSE, 0x00, delta_x, delta_y, 0, 0);
}
