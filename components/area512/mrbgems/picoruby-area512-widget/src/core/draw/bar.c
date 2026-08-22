#if defined(PICORB_VM_MRUBYC)

#include "area512_hal.h"
#include "core/widget.h"

#include <stdio.h>

#define WIDGET_LINE_BUFFER_SIZE 128

void
area512_widget_draw_header(
  void *sprite,
  const char *title,
  const char *right_text
) {

  int width = area512_gfx_width();
  int text_y = area512_widget_vcenter_text_y(0, WIDGET_HEADER_HEIGHT);
  char clipped[WIDGET_LINE_BUFFER_SIZE];

  area512_sprite_rect(
    sprite,
    0,
    0,
    width - 1,
    WIDGET_HEADER_HEIGHT,
    area512_theme_border_color()
  );

  area512_widget_clip(sprite, title, width - 10, clipped, sizeof clipped);
  area512_sprite_text(
    sprite,
    5,
    text_y,
    clipped,
    area512_theme_emphasis_color()
  );

  if (right_text[0]) {
    area512_widget_clip(sprite, right_text, width / 2, clipped, sizeof clipped);

    int right_x = width - 5 - area512_widget_text_width(sprite, clipped);
    area512_sprite_text(
      sprite,
      right_x,
      text_y,
      clipped,
      area512_theme_text_color()
    );
  }
}

void
area512_widget_draw_footer(void *sprite, const char *message) {
  int width = area512_gfx_width();
  int footer_y = area512_gfx_height() - WIDGET_FOOTER_HEIGHT;
  int text_y = footer_y + 1;
  char clipped[WIDGET_LINE_BUFFER_SIZE];

  area512_sprite_line(
    sprite,
    0,
    footer_y,
    width - 1,
    footer_y,
    area512_theme_border_color()
  );

  area512_widget_clip(sprite, message, width - 6, clipped, sizeof clipped);
  area512_sprite_text(sprite, 3, text_y, clipped, area512_theme_text_color());
}

void
area512_widget_draw_hints(
  void *sprite,
  const WidgetHint *hints,
  int hint_count
) {

  int footer_y = area512_gfx_height() - WIDGET_FOOTER_HEIGHT;
  int text_y = footer_y + 1;
  int x = 3;

  area512_sprite_line(
    sprite,
    0,
    footer_y,
    area512_gfx_width() - 1,
    footer_y,
    area512_theme_border_color()
  );

  for (int i = 0; i < hint_count; i++) {
    char key_label[WIDGET_MENU_ITEM_MAX];

    snprintf(key_label, sizeof key_label, "[%s]", hints[i].key);
    area512_sprite_text(
      sprite,
      x,
      text_y,
      key_label,
      area512_theme_emphasis_color()
    );

    x += area512_widget_text_width(sprite, key_label) + WIDGET_CHAR_WIDTH;
    area512_sprite_text(
      sprite,
      x,
      text_y,
      hints[i].label,
      area512_theme_text_color()
    );

    x += area512_widget_text_width(sprite, hints[i].label) + WIDGET_CHAR_WIDTH;
  }
}

void
area512_widget_draw_separator(void *sprite, int y) {
  area512_sprite_line(
    sprite,
    0,
    y,
    area512_gfx_width() - 1,
    y,
    area512_theme_border_color()
  );
}

void
area512_widget_draw_vertical_separator(void *sprite, int x, int y, int height) {

  if (height <= 0)
    return;

  area512_sprite_line(
    sprite,
    x,
    y,
    x,
    y + height - 1,
    area512_theme_border_color()
  );
}

void
area512_widget_draw_tabs(
  void *sprite,
  int y,
  const char *const *labels,
  int label_count,
  int active_index
) {

  int x = 0;

  for (int i = 0; i < label_count; i++) {
    int tab_width = area512_widget_text_width(sprite, labels[i]) + 12;
    int active = (i == active_index);

    if (active) {
      area512_sprite_fill_rect(
        sprite,
        x,
        y,
        tab_width,
        15,
        area512_theme_box_color()
      );
      area512_sprite_rect(
        sprite,
        x,
        y,
        tab_width,
        15,
        area512_theme_selected_color()
      );
    } else {
      area512_sprite_line(
        sprite,
        x,
        y + 14,
        x + tab_width - 1,
        y + 14,
        area512_theme_border_color()
      );
    }

    area512_sprite_text(
      sprite,
      x + 6,
      area512_widget_vcenter_text_y(y, 15),
      labels[i],
      active ? area512_theme_selected_color() : area512_theme_text_color()
    );

    x += tab_width;
  }

  if (x < area512_gfx_width()) {
    area512_sprite_line(
      sprite,
      x,
      y + 14,
      area512_gfx_width() - 1,
      y + 14,
      area512_theme_border_color()
    );
  }
}

#endif
