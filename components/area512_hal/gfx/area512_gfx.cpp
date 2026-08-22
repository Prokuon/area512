
#include <lgfx/v1/LGFXBase.hpp>
#include <lgfx/v1/LGFX_Sprite.hpp>

#include <ctype.h>
#include <new>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "area512_display.h"
#include "area512_hal.h"
#include "sdkconfig.h"

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

// efontJA fonts so full-width glyphs render, matching the terminal.
#define AREA512_SPRITE_FONT (&lgfx::v1::fonts::efontJA_10)
#define AREA512_FILER_FONT (&lgfx::v1::fonts::efontJA_12)

static constexpr int SPRITE_PIXEL_BYTE_SIZE = 2;
static constexpr int SCREEN_SLOT_WIDTH = 240;
static constexpr int SCREEN_SLOT_HEIGHT = 135;
static constexpr int ROW_SLOT_WIDTH = 240;
static constexpr int ROW_SLOT_HEIGHT = 13;
static constexpr int ROW_SLOT_COUNT = 2;
static constexpr size_t SCREEN_SLOT_BYTE_SIZE = (size_t)SCREEN_SLOT_WIDTH *
  SCREEN_SLOT_HEIGHT * SPRITE_PIXEL_BYTE_SIZE;
static constexpr size_t ROW_SLOT_BYTE_SIZE = (size_t)ROW_SLOT_WIDTH *
  ROW_SLOT_HEIGHT * SPRITE_PIXEL_BYTE_SIZE;
static constexpr int SPRITE_SLOT_COUNT = ROW_SLOT_COUNT + 1;

typedef struct {
  uint8_t *buffer;
  size_t buffer_byte_size;
  void *occupying_sprite;
} SpriteSlot;

alignas(4) static uint8_t s_screen_slot_buffer[SCREEN_SLOT_BYTE_SIZE];
alignas(4) static uint8_t
  s_row_slot_buffers[ROW_SLOT_COUNT][ROW_SLOT_BYTE_SIZE];

static SpriteSlot s_sprite_slots[SPRITE_SLOT_COUNT] = {
  {s_row_slot_buffers[0], ROW_SLOT_BYTE_SIZE, nullptr},
  {s_row_slot_buffers[1], ROW_SLOT_BYTE_SIZE, nullptr},
  {s_screen_slot_buffer, SCREEN_SLOT_BYTE_SIZE, nullptr},
};

static SpriteSlot *
find_free_sprite_slot(size_t required_byte_size) {
  for (int slot_index = 0; slot_index < SPRITE_SLOT_COUNT; slot_index++) {
    SpriteSlot *slot = &s_sprite_slots[slot_index];

    if (
      slot->occupying_sprite == nullptr &&
      slot->buffer_byte_size >= required_byte_size
    ) {

      return slot;
    }
  }

  return nullptr;
}

static SpriteSlot *
find_sprite_slot_holding(const void *sprite) {
  for (int slot_index = 0; slot_index < SPRITE_SLOT_COUNT; slot_index++) {
    SpriteSlot *slot = &s_sprite_slots[slot_index];

    if (slot->occupying_sprite == sprite)
      return slot;
  }

  return nullptr;
}

static lgfx::v1::LGFX_Device *
area512_gfx_device(void) {
  return static_cast<lgfx::v1::LGFX_Device *>(area512_display_device());
}

static void *
area512_sprite_new_with_font(int w, int h, const lgfx::v1::IFont *font) {
  lgfx::v1::LGFX_Device *dev = area512_gfx_device();
  if (dev == nullptr || w <= 0 || h <= 0) {
    return nullptr;
  }

  lgfx::v1::LGFX_Sprite *spr = new (std::nothrow) lgfx::v1::LGFX_Sprite(dev);
  if (spr == nullptr) {
    return nullptr;
  }

  spr->setColorDepth(16);

  size_t required_byte_size = (size_t)w * (size_t)h * SPRITE_PIXEL_BYTE_SIZE;
  SpriteSlot *slot = find_free_sprite_slot(required_byte_size);

  if (slot != nullptr) {
    spr->setBuffer(slot->buffer, w, h, 16);
    slot->occupying_sprite = spr;

  } else {
    // Cardputer has no PSRAM; keep the sprite in internal RAM.
    spr->setPsram(false);

    if (spr->createSprite(w, h) == nullptr) {
      delete spr;
      return nullptr;
    }
  }

  spr->setFont(font);
  spr->setTextSize(1);
  spr->setTextDatum(lgfx::v1::textdatum_t::top_left);
  spr->fillScreen((uint32_t)0x000000);

  return spr;
}

static const lgfx::v1::IFont *
area512_sprite_font_for_size(int font_size) {
  switch (font_size) {
  case 10:
    return AREA512_SPRITE_FONT;
  case 12:
    return AREA512_FILER_FONT;
  case 14:
    return &lgfx::v1::fonts::efontJA_14;
  case 16:
    return &lgfx::v1::fonts::efontJA_16;
  case 24:
    return &lgfx::v1::fonts::efontJA_24;
  default:
    return nullptr;
  }
}

static int
area512_sprite_font_height_for_size(int font_size) {
  switch (font_size) {
  case 12:
    return 13;
  case 14:
    return 15;
  case 16:
    return 17;
  case 24:
    return 25;
  case 10:
    return 12;
  default:
    return 0;
  }
}

static bool
read_text_line(FILE *file, char *line, int line_size) {
  int len = 0;

  while (len < line_size - 1) {
    int ch = fgetc(file);

    if (ch == EOF)
      break;

    line[len++] = (char)ch;

    if (ch == '\n')
      break;
  }

  line[len] = 0;

  return len > 0;
}

static bool
parse_hex_byte(const char **cursor, uint8_t *out) {
  const char *p = *cursor;
  while (*p && !(p[0] == '0' && (p[1] == 'x' || p[1] == 'X'))) {
    ++p;
  }

  if (!*p) {
    *cursor = p;
    return false;
  }

  p += 2;
  if (!isxdigit((unsigned char)p[0]) || !isxdigit((unsigned char)p[1])) {
    *cursor = p;
    return false;
  }

  char hex[3] = {p[0], p[1], 0};
  *out = (uint8_t)strtoul(hex, nullptr, 16);
  *cursor = p + 2;

  return true;
}

extern "C" {

// -----------------------------------------------------------------------------
// Sprite lifecycle
// -----------------------------------------------------------------------------

void *
area512_sprite_new(int w, int h) {
  return area512_sprite_new_with_font(w, h, AREA512_SPRITE_FONT);
}

void *
area512_sprite_new_with_font_size(int w, int h, int font_size) {
  const lgfx::v1::IFont *font = area512_sprite_font_for_size(font_size);

  if (font == nullptr)
    return nullptr;

  return area512_sprite_new_with_font(w, h, font);
}

void
area512_sprite_set_font_size(void *p, int font_size) {
  if (p == nullptr)
    return;

  const lgfx::v1::IFont *font = area512_sprite_font_for_size(font_size);

  if (font == nullptr)
    return;

  lgfx::v1::LGFX_Sprite *spr = static_cast<lgfx::v1::LGFX_Sprite *>(p);
  spr->setFont(font);
  spr->setTextSize(1);
  spr->setTextDatum(lgfx::v1::textdatum_t::top_left);
}

int
area512_sprite_font_height(int font_size) {
  return area512_sprite_font_height_for_size(font_size);
}

void
area512_sprite_delete(void *p) {
  if (p == nullptr) {
    return;
  }

  lgfx::v1::LGFX_Sprite *spr = static_cast<lgfx::v1::LGFX_Sprite *>(p);
  SpriteSlot *slot = find_sprite_slot_holding(spr);

  if (slot != nullptr)
    slot->occupying_sprite = nullptr;

  spr->deleteSprite();

  delete spr;
}

int
area512_sprite_width(void *p) {
  if (p == nullptr)
    return 0;

  return (int)static_cast<lgfx::v1::LGFX_Sprite *>(p)->width();
}

int
area512_sprite_height(void *p) {
  if (p == nullptr)
    return 0;

  return (int)static_cast<lgfx::v1::LGFX_Sprite *>(p)->height();
}

// -----------------------------------------------------------------------------
// Sprite drawing
// -----------------------------------------------------------------------------

void
area512_sprite_fill(void *p, uint32_t color) {
  if (p == nullptr)
    return;

  static_cast<lgfx::v1::LGFX_Sprite *>(p)->fillScreen(color);
}

void
area512_sprite_pixel(void *p, int x, int y, uint32_t color) {
  if (p == nullptr)
    return;

  static_cast<lgfx::v1::LGFX_Sprite *>(p)->drawPixel(x, y, color);
}

void
area512_sprite_line(void *p, int x0, int y0, int x1, int y1, uint32_t color) {
  if (p == nullptr)
    return;

  static_cast<lgfx::v1::LGFX_Sprite *>(p)->drawLine(x0, y0, x1, y1, color);
}

void
area512_sprite_rect(void *p, int x, int y, int w, int h, uint32_t color) {
  if (p == nullptr)
    return;

  static_cast<lgfx::v1::LGFX_Sprite *>(p)->drawRect(x, y, w, h, color);
}

void
area512_sprite_fill_rect(void *p, int x, int y, int w, int h, uint32_t color) {
  if (p == nullptr)
    return;

  static_cast<lgfx::v1::LGFX_Sprite *>(p)->fillRect(x, y, w, h, color);
}

void
area512_sprite_circle(void *p, int x, int y, int r, uint32_t color) {
  if (p == nullptr)
    return;

  static_cast<lgfx::v1::LGFX_Sprite *>(p)->drawCircle(x, y, r, color);
}

void
area512_sprite_fill_circle(void *p, int x, int y, int r, uint32_t color) {
  if (p == nullptr)
    return;

  static_cast<lgfx::v1::LGFX_Sprite *>(p)->fillCircle(x, y, r, color);
}

// Transparent background; x,y is top-left.
void
area512_sprite_text(void *p, int x, int y, const char *str, uint32_t color) {
  if (p == nullptr || str == nullptr)
    return;

  lgfx::v1::LGFX_Sprite *spr = static_cast<lgfx::v1::LGFX_Sprite *>(p);
  spr->setTextColor(color);
  spr->drawString(str, x, y);
}

int
area512_sprite_text_width(void *p, const char *str) {
  if (p == nullptr || str == nullptr)
    return 0;

  return (int)static_cast<lgfx::v1::LGFX_Sprite *>(p)->textWidth(str);
}

// -----------------------------------------------------------------------------
// Sprite transfer to screen
// -----------------------------------------------------------------------------

void
area512_sprite_push(void *p, int x, int y) {
  if (p == nullptr)
    return;

  lgfx::v1::LGFX_Sprite *spr = static_cast<lgfx::v1::LGFX_Sprite *>(p);
  spr->pushSprite(x, y);

  lgfx::v1::LGFX_Device *dev = area512_gfx_device();
  if (dev)
    dev->waitDMA();
}

// Overlay, keying out transp-colored pixels.
void
area512_sprite_push_transparent(void *p, int x, int y, uint32_t transp) {
  if (p == nullptr)
    return;

  lgfx::v1::LGFX_Sprite *spr = static_cast<lgfx::v1::LGFX_Sprite *>(p);
  spr->pushSprite(x, y, transp);

  lgfx::v1::LGFX_Device *dev = area512_gfx_device();
  if (dev)
    dev->waitDMA();
}

// -----------------------------------------------------------------------------
// Screen (shared device) operations
// -----------------------------------------------------------------------------

int
area512_gfx_width(void) {
  lgfx::v1::LGFX_Device *dev = area512_gfx_device();

  return dev ? (int)dev->width() : 0;
}

int
area512_gfx_height(void) {
  lgfx::v1::LGFX_Device *dev = area512_gfx_device();

  return dev ? (int)dev->height() : 0;
}

void
area512_gfx_fill_screen(uint32_t color) {
  lgfx::v1::LGFX_Device *dev = area512_gfx_device();

  if (dev)
    dev->fillScreen(color);
}

void
area512_gfx_set_brightness(int brightness) {
  lgfx::v1::LGFX_Device *dev = area512_gfx_device();

  if (dev == nullptr)
    return;

  if (brightness < 0)
    brightness = 0;

  if (brightness > 255)
    brightness = 255;

  dev->setBrightness((uint8_t)brightness);
}

int
area512_gfx_show_header_image(const char *path, int hold_milliseconds) {
  lgfx::v1::LGFX_Device *dev = area512_gfx_device();
  if (dev == nullptr || path == nullptr)
    return 0;

  char full_path[AREA512_PATH_MAX];
  if (area512_resolve_data_path(path, full_path, sizeof full_path) != 0)
    return 0;

  FILE *file = fopen(full_path, "rb");
  if (file == nullptr)
    return 0;

  // Images are authored at the panel's exact size, so use its dimensions.
  const int img_w = dev->width();
  const int img_h = dev->height();
  bool in_array = false;
  bool ok = false;
  char line[192];

  while (read_text_line(file, line, sizeof(line))) {
    if (strchr(line, '{')) {
      in_array = true;
      break;
    }
  }

  const int rowbytes = (img_w + 7) / 8;
  uint8_t *row = nullptr;

  if (in_array && rowbytes > 0) {
    row = (uint8_t *)malloc((size_t)rowbytes);
  }

  if (row != nullptr) {
    uint32_t set_bit_color;
    uint32_t clear_bit_color;

    area512_theme_pick_bitmap_colors(&set_bit_color, &clear_bit_color);

    // LGFX converts RGB888 to the panel's color depth itself.
    const lgfx::v1::rgb888_t color_on(set_bit_color);
    const lgfx::v1::rgb888_t color_off(clear_bit_color);
    int y = 0;
    int col = 0;

    while (read_text_line(file, line, sizeof(line)) && y < img_h) {
      const char *p = line;
      uint8_t byte;

      while (parse_hex_byte(&p, &byte)) {
        row[col++] = byte;

        if (col >= rowbytes) {
          dev->startWrite();
          dev->drawBitmap(0, y, row, img_w, 1, color_on, color_off);
          dev->endWrite();
          ++y;
          col = 0;

          if (y >= img_h)
            break;
        }
      }
    }

    ok = (y >= img_h);

    free(row);

    if (ok && hold_milliseconds > 0) {
      vTaskDelay(pdMS_TO_TICKS(hold_milliseconds));
    }
  }

  fclose(file);
  return ok ? 1 : 0;
}

} // extern "C"
