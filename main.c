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

#include "bsp/board.h"
#include "tusb.h"
#include "hardware/flash.h"
#include "hardware/sync.h"
#include "pico/stdlib.h"
#include "st7735.h"

//--------------------------------------------------------------------+
// MACRO CONSTANT TYPEDEF PROTYPES
//--------------------------------------------------------------------+

#define SWITCH_PIN 28
#define BUTTON_DEBOUNCE_MS 30
#define BOOT_SELECTION_MAGIC 0x4C455342u
#define BOOT_SELECTION_VERSION 1u
#define BOOT_SELECTION_STORAGE_OFFSET 0x1FF000u

_Static_assert(BOOT_SELECTION_STORAGE_OFFSET % FLASH_SECTOR_SIZE == 0,
              "boot selection storage must be sector-aligned");

/* Blink pattern
 * - 250 ms  : device not mounted
 * - 1000 ms : device mounted
 * - 2500 ms : device is suspended
 */
enum  {
  BLINK_NOT_MOUNTED = 250,
  BLINK_MOUNTED = 1000,
  BLINK_SUSPENDED = 2500,
};


static uint32_t blink_interval_ms = BLINK_NOT_MOUNTED;
static bool tft_refresh_requested = true;
static uint8_t current_switch_value = '0';

typedef struct {
  uint32_t magic;
  uint32_t version;
  uint8_t switch_value;
  uint8_t inverted_switch_value;
  uint8_t reserved[6];
} boot_selection_config_t;

void led_blinking_task(void);
void cdc_task(void);
void button_task(void);
void tft_display_task(void);

static bool is_valid_switch_value(uint8_t switch_value)
{
  return switch_value == '0' || switch_value == '1';
}

static const boot_selection_config_t *get_stored_boot_selection_config(void)
{
  return (const boot_selection_config_t *)(XIP_BASE + BOOT_SELECTION_STORAGE_OFFSET);
}

static bool boot_selection_config_is_valid(const boot_selection_config_t *config)
{
  return config->magic == BOOT_SELECTION_MAGIC &&
         config->version == BOOT_SELECTION_VERSION &&
         is_valid_switch_value(config->switch_value) &&
         config->inverted_switch_value == (uint8_t)~config->switch_value;
}

static void load_boot_selection(void)
{
  const boot_selection_config_t *config = get_stored_boot_selection_config();

  if (boot_selection_config_is_valid(config)) {
    current_switch_value = config->switch_value;
  } else {
    current_switch_value = '0';
  }
}

static void save_boot_selection(void)
{
  uint8_t page[FLASH_PAGE_SIZE];
  boot_selection_config_t config = {
    .magic = BOOT_SELECTION_MAGIC,
    .version = BOOT_SELECTION_VERSION,
    .switch_value = current_switch_value,
    .inverted_switch_value = (uint8_t)~current_switch_value,
  };

  const boot_selection_config_t *stored_config = get_stored_boot_selection_config();
  if (boot_selection_config_is_valid(stored_config) &&
      stored_config->switch_value == current_switch_value) {
    return;
  }

  memset(page, 0xFF, sizeof(page));
  memcpy(page, &config, sizeof(config));

  uint32_t interrupts = save_and_disable_interrupts();
  flash_range_erase(BOOT_SELECTION_STORAGE_OFFSET, FLASH_SECTOR_SIZE);
  flash_range_program(BOOT_SELECTION_STORAGE_OFFSET, page, sizeof(page));
  restore_interrupts(interrupts);
}

uint8_t read_switch_value(void)
{
  return current_switch_value;
}

/*------------- MAIN -------------*/
int main(void)
{

  gpio_init(SWITCH_PIN);
  gpio_set_dir(SWITCH_PIN, false);
  gpio_set_pulls(SWITCH_PIN, false, true);

  board_init();
  load_boot_selection();
  tft_init();
  tusb_init();

  while (1)
  {
    tud_task(); // tinyusb device task
    led_blinking_task();
    button_task();
    tft_display_task();

    cdc_task();
  }

  return 0;
}

//--------------------------------------------------------------------+
// BUTTON TASK
//--------------------------------------------------------------------+
void button_task(void)
{
  static bool initialized = false;
  static bool debounced_pressed = false;
  static bool last_raw_pressed = false;
  static uint32_t last_change_ms = 0;

  bool raw_pressed = gpio_get(SWITCH_PIN);
  uint32_t now = board_millis();

  if (!initialized) {
    initialized = true;
    debounced_pressed = raw_pressed;
    last_raw_pressed = raw_pressed;
    last_change_ms = now;
    return;
  }

  if (raw_pressed != last_raw_pressed) {
    last_raw_pressed = raw_pressed;
    last_change_ms = now;
    return;
  }

  if (raw_pressed == debounced_pressed || now - last_change_ms < BUTTON_DEBOUNCE_MS) {
    return;
  }

  debounced_pressed = raw_pressed;
  if (debounced_pressed) {
    current_switch_value = current_switch_value == '0' ? '1' : '0';
    save_boot_selection();
    tft_refresh_requested = true;
  }
}

//--------------------------------------------------------------------+
// Device callbacks
//--------------------------------------------------------------------+

// Invoked when device is mounted
void tud_mount_cb(void)
{
  blink_interval_ms = BLINK_MOUNTED;
  tft_set_backlight(true);
  tft_refresh_requested = true;
}

// Invoked when device is unmounted
void tud_umount_cb(void)
{
  blink_interval_ms = BLINK_NOT_MOUNTED;
}

// Invoked when usb bus is suspended
// remote_wakeup_en : if host allow us  to perform remote wakeup
// Within 7ms, device must draw an average of current less than 2.5 mA from bus
void tud_suspend_cb(bool remote_wakeup_en)
{
  (void) remote_wakeup_en;
  blink_interval_ms = BLINK_SUSPENDED;
  tft_set_backlight(false);
}

// Invoked when usb bus is resumed
void tud_resume_cb(void)
{
  blink_interval_ms = BLINK_MOUNTED;
  tft_set_backlight(true);
  tft_refresh_requested = true;
}


//--------------------------------------------------------------------+
// USB CDC
//--------------------------------------------------------------------+
void cdc_task(void)
{
  // connected() check for DTR bit
  // Most but not all terminal client set this when making connection
  // if ( tud_cdc_connected() )
  {
    // connected and there are data available
    if ( tud_cdc_available() )
    {
      // read datas
      char buf[64];
      uint32_t count = tud_cdc_read(buf, sizeof(buf));
      (void) count;

      // Echo back
      // Note: Skip echo by commenting out write() and write_flush()
      // for throughput test e.g
      //    $ dd if=/dev/zero of=/dev/ttyACM0 count=10000
      tud_cdc_write(buf, count);
      tud_cdc_write_flush();
    }
  }
}

// Invoked when cdc when line state changed e.g connected/disconnected
void tud_cdc_line_state_cb(uint8_t itf, bool dtr, bool rts)
{
  (void) itf;
  (void) rts;

  // TODO set some indicator
  if ( dtr )
  {
    // Terminal connected
  }else
  {
    // Terminal disconnected
  }
}

// Invoked when CDC interface received data from host
void tud_cdc_rx_cb(uint8_t itf)
{
  (void) itf;
}

//--------------------------------------------------------------------+
// BLINKING TASK
//--------------------------------------------------------------------+
void led_blinking_task(void)
{
  static uint32_t start_ms = 0;
  static bool led_state = false;

  // Blink every interval ms
  if ( board_millis() - start_ms < blink_interval_ms) return; // not enough time
  start_ms += blink_interval_ms;

  board_led_write(led_state);
  led_state = 1 - led_state; // toggle
}

//--------------------------------------------------------------------+
// TFT DISPLAY TASK
//--------------------------------------------------------------------+
void tft_display_task(void)
{
  static uint8_t previous_switch_value = 0;
  uint8_t switch_value = read_switch_value();

  if (!tft_refresh_requested && switch_value == previous_switch_value) {
    return;
  }

  tft_show_boot_selection(switch_value);
  previous_switch_value = switch_value;
  tft_refresh_requested = false;
}
