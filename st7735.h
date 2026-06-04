#ifndef ST7735_H_
#define ST7735_H_

#include <stdbool.h>
#include <stdint.h>

void tft_init(void);
void tft_set_backlight(bool enabled);
void tft_show_boot_selection(uint8_t switch_value);

#endif
