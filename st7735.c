#include "st7735.h"

#include <string.h>

#include "hardware/spi.h"
#include "pico/stdlib.h"

#define TFT_SPI_PORT spi0
#define TFT_WIDTH 128
#define TFT_HEIGHT 160

#define TFT_PIN_SCK 18
#define TFT_PIN_MOSI 19
#define TFT_PIN_RST 20
#define TFT_PIN_DC 16
#define TFT_PIN_CS 17
#define TFT_PIN_BLK 21

#define ST7735_SWRESET 0x01
#define ST7735_SLPOUT 0x11
#define ST7735_CASET 0x2A
#define ST7735_RASET 0x2B
#define ST7735_RAMWR 0x2C
#define ST7735_MADCTL 0x36
#define ST7735_COLMOD 0x3A
#define ST7735_INVON 0x21
#define ST7735_NORON 0x13
#define ST7735_DISPON 0x29

#define RGB565(r, g, b) (uint16_t)((((r) & 0xF8) << 8) | (((g) & 0xFC) << 3) | ((b) >> 3))

enum {
  COLOR_BLACK = RGB565(0, 0, 0),
  COLOR_WHITE = RGB565(255, 255, 255),
  COLOR_GREEN = RGB565(30, 190, 95),
  COLOR_BLUE = RGB565(45, 110, 230),
  COLOR_RED = RGB565(220, 40, 55),
  COLOR_DARK = RGB565(12, 18, 28),
};

typedef struct {
  char character;
  uint8_t rows[7];
} glyph_t;

static const glyph_t font[] = {
  { ' ', { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 } },
  { '0', { 0x0E, 0x11, 0x13, 0x15, 0x19, 0x11, 0x0E } },
  { '1', { 0x04, 0x0C, 0x04, 0x04, 0x04, 0x04, 0x0E } },
  { '?', { 0x0E, 0x11, 0x01, 0x02, 0x04, 0x00, 0x04 } },
  { 'U', { 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0E } },
  { 'B', { 0x1E, 0x11, 0x11, 0x1E, 0x11, 0x11, 0x1E } },
  { 'L', { 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x1F } },
  { 'S', { 0x0F, 0x10, 0x10, 0x0E, 0x01, 0x01, 0x1E } },
  { 'W', { 0x11, 0x11, 0x11, 0x15, 0x15, 0x15, 0x0A } },
  { 'b', { 0x10, 0x10, 0x16, 0x19, 0x11, 0x11, 0x1E } },
  { 'c', { 0x00, 0x00, 0x0F, 0x10, 0x10, 0x10, 0x0F } },
  { 'd', { 0x01, 0x01, 0x0F, 0x11, 0x11, 0x11, 0x0F } },
  { 'e', { 0x00, 0x00, 0x0E, 0x11, 0x1F, 0x10, 0x0E } },
  { 'i', { 0x04, 0x00, 0x0C, 0x04, 0x04, 0x04, 0x0E } },
  { 'k', { 0x10, 0x12, 0x14, 0x18, 0x14, 0x12, 0x11 } },
  { 'l', { 0x0C, 0x04, 0x04, 0x04, 0x04, 0x04, 0x0E } },
  { 'n', { 0x00, 0x00, 0x16, 0x19, 0x11, 0x11, 0x11 } },
  { 'o', { 0x00, 0x00, 0x0E, 0x11, 0x11, 0x11, 0x0E } },
  { 's', { 0x00, 0x00, 0x0F, 0x10, 0x0E, 0x01, 0x1E } },
  { 't', { 0x08, 0x08, 0x1E, 0x08, 0x08, 0x09, 0x06 } },
  { 'u', { 0x00, 0x00, 0x11, 0x11, 0x11, 0x13, 0x0D } },
  { 'w', { 0x00, 0x00, 0x11, 0x11, 0x15, 0x15, 0x0A } },
  { 'x', { 0x00, 0x00, 0x11, 0x0A, 0x04, 0x0A, 0x11 } },
};

static void tft_select(void)
{
  gpio_put(TFT_PIN_CS, 0);
}

static void tft_deselect(void)
{
  gpio_put(TFT_PIN_CS, 1);
}

static void tft_write_bytes(const uint8_t *data, size_t len)
{
  spi_write_blocking(TFT_SPI_PORT, data, len);
}

static void tft_write_command(uint8_t command)
{
  gpio_put(TFT_PIN_DC, 0);
  tft_select();
  tft_write_bytes(&command, 1);
  tft_deselect();
}

static void tft_write_data(const uint8_t *data, size_t len)
{
  gpio_put(TFT_PIN_DC, 1);
  tft_select();
  tft_write_bytes(data, len);
  tft_deselect();
}

static void tft_write_data_byte(uint8_t data)
{
  tft_write_data(&data, 1);
}

static void tft_set_address_window(uint16_t x, uint16_t y, uint16_t width, uint16_t height)
{
  uint16_t x_end = x + width - 1;
  uint16_t y_end = y + height - 1;
  uint8_t data[4];

  tft_write_command(ST7735_CASET);
  data[0] = x >> 8;
  data[1] = x & 0xFF;
  data[2] = x_end >> 8;
  data[3] = x_end & 0xFF;
  tft_write_data(data, sizeof(data));

  tft_write_command(ST7735_RASET);
  data[0] = y >> 8;
  data[1] = y & 0xFF;
  data[2] = y_end >> 8;
  data[3] = y_end & 0xFF;
  tft_write_data(data, sizeof(data));

  tft_write_command(ST7735_RAMWR);
}

static void tft_fill_rect(uint16_t x, uint16_t y, uint16_t width, uint16_t height, uint16_t color)
{
  uint8_t color_bytes[64 * 2];
  uint32_t pixels_remaining = (uint32_t)width * height;

  for (size_t i = 0; i < sizeof(color_bytes); i += 2) {
    color_bytes[i] = color >> 8;
    color_bytes[i + 1] = color & 0xFF;
  }

  tft_set_address_window(x, y, width, height);
  gpio_put(TFT_PIN_DC, 1);
  tft_select();
  while (pixels_remaining > 0) {
    uint32_t pixels_to_write = pixels_remaining > 64 ? 64 : pixels_remaining;
    tft_write_bytes(color_bytes, pixels_to_write * 2);
    pixels_remaining -= pixels_to_write;
  }
  tft_deselect();
}

static const uint8_t *find_glyph(char character)
{
  for (size_t i = 0; i < sizeof(font) / sizeof(font[0]); i++) {
    if (font[i].character == character) {
      return font[i].rows;
    }
  }

  return font[0].rows;
}

static void tft_draw_char(uint16_t x, uint16_t y, char character, uint16_t color, uint16_t background, uint8_t scale)
{
  const uint8_t *glyph = find_glyph(character);

  for (uint8_t row = 0; row < 7; row++) {
    for (uint8_t col = 0; col < 5; col++) {
      uint16_t pixel_color = (glyph[row] & (1u << (4 - col))) ? color : background;
      tft_fill_rect(x + col * scale, y + row * scale, scale, scale, pixel_color);
    }
  }
}

static uint16_t text_width(const char *text, uint8_t scale)
{
  size_t len = strlen(text);
  if (len == 0) {
    return 0;
  }

  return (uint16_t)((len * 6 - 1) * scale);
}

static void tft_draw_text(uint16_t x, uint16_t y, const char *text, uint16_t color, uint16_t background, uint8_t scale)
{
  while (*text) {
    tft_draw_char(x, y, *text, color, background, scale);
    x += 6 * scale;
    text++;
  }
}

static void tft_draw_centered_text(uint16_t y, const char *text, uint16_t color, uint16_t background, uint8_t scale)
{
  uint16_t width = text_width(text, scale);
  uint16_t x = width < TFT_WIDTH ? (TFT_WIDTH - width) / 2 : 0;

  tft_draw_text(x, y, text, color, background, scale);
}

void tft_init(void)
{
  spi_init(TFT_SPI_PORT, 24000000);
  gpio_set_function(TFT_PIN_SCK, GPIO_FUNC_SPI);
  gpio_set_function(TFT_PIN_MOSI, GPIO_FUNC_SPI);

  gpio_init(TFT_PIN_RST);
  gpio_set_dir(TFT_PIN_RST, GPIO_OUT);
  gpio_init(TFT_PIN_DC);
  gpio_set_dir(TFT_PIN_DC, GPIO_OUT);
  gpio_init(TFT_PIN_CS);
  gpio_set_dir(TFT_PIN_CS, GPIO_OUT);
  gpio_init(TFT_PIN_BLK);
  gpio_set_dir(TFT_PIN_BLK, GPIO_OUT);

  tft_deselect();
  tft_set_backlight(false);

  gpio_put(TFT_PIN_RST, 0);
  sleep_ms(20);
  gpio_put(TFT_PIN_RST, 1);
  sleep_ms(120);

  tft_write_command(ST7735_SWRESET);
  sleep_ms(150);
  tft_write_command(ST7735_SLPOUT);
  sleep_ms(120);
  tft_write_command(ST7735_COLMOD);
  tft_write_data_byte(0x05);
  tft_write_command(ST7735_MADCTL);
  tft_write_data_byte(0x00);
  tft_write_command(ST7735_INVON);
  tft_write_command(ST7735_NORON);
  sleep_ms(10);
  tft_write_command(ST7735_DISPON);
  sleep_ms(100);

  tft_fill_rect(0, 0, TFT_WIDTH, TFT_HEIGHT, COLOR_BLACK);
  tft_set_backlight(true);
}

void tft_set_backlight(bool enabled)
{
  gpio_put(TFT_PIN_BLK, enabled);
}

void tft_show_boot_selection(uint8_t switch_value)
{
  const char *system_name = "Unknown";
  const char *switch_text = "Switch ?";
  uint16_t accent = COLOR_RED;

  if (switch_value == '0') {
    system_name = "Ubuntu";
    switch_text = "Switch 0";
    accent = COLOR_GREEN;
  } else if (switch_value == '1') {
    system_name = "Windows";
    switch_text = "Switch 1";
    accent = COLOR_BLUE;
  }

  tft_fill_rect(0, 0, TFT_WIDTH, TFT_HEIGHT, COLOR_DARK);
  tft_fill_rect(0, 0, TFT_WIDTH, 8, accent);
  tft_fill_rect(0, TFT_HEIGHT - 8, TFT_WIDTH, 8, accent);
  tft_draw_centered_text(36, "Boot", COLOR_WHITE, COLOR_DARK, 3);
  tft_draw_centered_text(74, system_name, accent, COLOR_DARK, 3);
  tft_draw_centered_text(122, switch_text, COLOR_WHITE, COLOR_DARK, 2);
}
