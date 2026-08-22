#include "area512_micropython_sprite.h"

#include "area512_hal.h"
#include "core/widget.h"

#include "py/runtime.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "io-console.h"

// -----------------------------------------------------------------------------
// Argument and value helpers
// -----------------------------------------------------------------------------

static uint32_t
fetch_color_or_raise(mp_obj_t color_object) {
  return (uint32_t)mp_obj_get_int(color_object);
}

static int
copy_sequence_strings(
  mp_obj_t sequence_object,
  const char **strings,
  int maximum
) {

  size_t item_count;
  mp_obj_t *items;

  mp_obj_get_array(sequence_object, &item_count, &items);

  if (item_count > (size_t)maximum)
    item_count = (size_t)maximum;

  for (size_t i = 0; i < item_count; i++)
    strings[i] = mp_obj_str_get_str(items[i]);

  return (int)item_count;
}

static int
copy_sequence_integers(mp_obj_t sequence_object, int *integers, int maximum) {

  size_t item_count;
  mp_obj_t *items;

  mp_obj_get_array(sequence_object, &item_count, &items);

  if (item_count > (size_t)maximum)
    item_count = (size_t)maximum;

  for (size_t i = 0; i < item_count; i++)
    integers[i] = mp_obj_get_int(items[i]);

  return (int)item_count;
}

static int
copy_table_columns(
  mp_obj_t width_sequence,
  mp_obj_t text_sequence,
  int widths[WIDGET_TABLE_MAX_COLUMNS],
  const char *texts[WIDGET_TABLE_MAX_COLUMNS]
) {

  int width_count =
    copy_sequence_integers(width_sequence, widths, WIDGET_TABLE_MAX_COLUMNS);
  int text_count =
    copy_sequence_strings(text_sequence, texts, WIDGET_TABLE_MAX_COLUMNS);

  return width_count < text_count ? width_count : text_count;
}

// -----------------------------------------------------------------------------
// Theme and metrics
// -----------------------------------------------------------------------------

#define DEFINE_INTEGER_METHOD(function_name, value_expression)                 \
  static mp_obj_t function_name(void) {                                        \
    return MP_OBJ_NEW_SMALL_INT(value_expression);                             \
  }                                                                            \
  static MP_DEFINE_CONST_FUN_OBJ_0(function_name##_callable, function_name)

DEFINE_INTEGER_METHOD(fetch_widget_background_color, WIDGET_COLOR_BG);
DEFINE_INTEGER_METHOD(fetch_widget_amber_color, WIDGET_COLOR_AMBER);
DEFINE_INTEGER_METHOD(fetch_widget_dim_color, WIDGET_COLOR_DIM);
DEFINE_INTEGER_METHOD(fetch_widget_gold_color, WIDGET_COLOR_GOLD);
DEFINE_INTEGER_METHOD(fetch_widget_dark_color, WIDGET_COLOR_DARK);
DEFINE_INTEGER_METHOD(
  fetch_widget_theme_background_color,
  area512_theme_background_color()
);
DEFINE_INTEGER_METHOD(
  fetch_widget_theme_text_color,
  area512_theme_text_color()
);
DEFINE_INTEGER_METHOD(
  fetch_widget_theme_emphasis_color,
  area512_theme_emphasis_color()
);
DEFINE_INTEGER_METHOD(
  fetch_widget_theme_border_color,
  area512_theme_border_color()
);
DEFINE_INTEGER_METHOD(
  fetch_widget_theme_selected_color,
  area512_theme_selected_color()
);
DEFINE_INTEGER_METHOD(
  fetch_widget_theme_box_color,
  area512_theme_box_color()
);
DEFINE_INTEGER_METHOD(fetch_widget_character_width, WIDGET_CHAR_WIDTH);
DEFINE_INTEGER_METHOD(fetch_widget_row_height, WIDGET_ROW_HEIGHT);
DEFINE_INTEGER_METHOD(fetch_widget_header_height, WIDGET_HEADER_HEIGHT);
DEFINE_INTEGER_METHOD(fetch_widget_body_top, area512_widget_body_top());
DEFINE_INTEGER_METHOD(fetch_widget_body_bottom, area512_widget_body_bottom());
DEFINE_INTEGER_METHOD(fetch_widget_body_height, area512_widget_body_height());

static mp_obj_t
measure_widget_text_width(mp_obj_t sprite_object, mp_obj_t text_object) {
  return MP_OBJ_NEW_SMALL_INT(area512_widget_text_width(
    area512_micropython_fetch_sprite_handle_or_raise(sprite_object),
    mp_obj_str_get_str(text_object)
  ));
}
static MP_DEFINE_CONST_FUN_OBJ_2(
  measure_widget_text_width_callable,
  measure_widget_text_width
);

static mp_obj_t
clip_widget_text(
  mp_obj_t sprite_object,
  mp_obj_t text_object,
  mp_obj_t width_object
) {

  char clipped[WIDGET_TEXT_VIEW_MAX];

  area512_widget_clip(
    area512_micropython_fetch_sprite_handle_or_raise(sprite_object),
    mp_obj_str_get_str(text_object),
    mp_obj_get_int(width_object),
    clipped,
    sizeof clipped
  );

  return mp_obj_new_str_from_cstr(clipped);
}
static MP_DEFINE_CONST_FUN_OBJ_3(clip_widget_text_callable, clip_widget_text);

static mp_obj_t
read_widget_key(void) {
  char byte_string[2];

  return mp_obj_new_str_from_cstr(
    area512_widget_key_name(area512_widget_read_key(), byte_string)
  );
}
static MP_DEFINE_CONST_FUN_OBJ_0(read_widget_key_callable, read_widget_key);

// -----------------------------------------------------------------------------
// Bars, panels, and text
// -----------------------------------------------------------------------------

static mp_obj_t
draw_widget_header(size_t argument_count, const mp_obj_t *arguments) {
  const char *right_text =
    argument_count >= 3 ? mp_obj_str_get_str(arguments[2]) : "";

  area512_widget_draw_header(
    area512_micropython_fetch_sprite_handle_or_raise(arguments[0]),
    mp_obj_str_get_str(arguments[1]),
    right_text
  );

  return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(
  draw_widget_header_callable,
  2,
  3,
  draw_widget_header
);

static mp_obj_t
draw_widget_footer(mp_obj_t sprite_object, mp_obj_t message_object) {
  area512_widget_draw_footer(
    area512_micropython_fetch_sprite_handle_or_raise(sprite_object),
    mp_obj_str_get_str(message_object)
  );

  return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_2(
  draw_widget_footer_callable,
  draw_widget_footer
);

static mp_obj_t
draw_widget_hints(mp_obj_t sprite_object, mp_obj_t hint_sequence) {
  void *sprite_handle =
    area512_micropython_fetch_sprite_handle_or_raise(sprite_object);

  size_t hint_count;
  mp_obj_t *hint_items;

  mp_obj_get_array(hint_sequence, &hint_count, &hint_items);

  if (hint_count > WIDGET_MENU_MAX_ITEMS)
    hint_count = WIDGET_MENU_MAX_ITEMS;

  WidgetHint hints[WIDGET_MENU_MAX_ITEMS];

  for (size_t i = 0; i < hint_count; i++) {
    size_t pair_length;
    mp_obj_t *pair_items;

    mp_obj_get_array(hint_items[i], &pair_length, &pair_items);

    if (pair_length < 2)
      mp_raise_TypeError(MP_ERROR_TEXT("expected (key, label) pair"));

    hints[i].key = mp_obj_str_get_str(pair_items[0]);
    hints[i].label = mp_obj_str_get_str(pair_items[1]);
  }

  area512_widget_draw_hints(sprite_handle, hints, (int)hint_count);

  return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_2(draw_widget_hints_callable, draw_widget_hints);

static mp_obj_t
draw_widget_separator(mp_obj_t sprite_object, mp_obj_t y_object) {
  area512_widget_draw_separator(
    area512_micropython_fetch_sprite_handle_or_raise(sprite_object),
    mp_obj_get_int(y_object)
  );

  return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_2(
  draw_widget_separator_callable,
  draw_widget_separator
);

static mp_obj_t
draw_widget_vertical_separator(
  size_t argument_count,
  const mp_obj_t *arguments
) {

  area512_widget_draw_vertical_separator(
    area512_micropython_fetch_sprite_handle_or_raise(arguments[0]),
    mp_obj_get_int(arguments[1]),
    mp_obj_get_int(arguments[2]),
    mp_obj_get_int(arguments[3])
  );

  return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(
  draw_widget_vertical_separator_callable,
  4,
  4,
  draw_widget_vertical_separator
);

static mp_obj_t
draw_widget_tabs(size_t argument_count, const mp_obj_t *arguments) {
  void *sprite_handle =
    area512_micropython_fetch_sprite_handle_or_raise(arguments[0]);
  const char *labels[WIDGET_MENU_MAX_ITEMS];
  int label_count =
    copy_sequence_strings(arguments[2], labels, WIDGET_MENU_MAX_ITEMS);

  area512_widget_draw_tabs(
    sprite_handle,
    mp_obj_get_int(arguments[1]),
    labels,
    label_count,
    mp_obj_get_int(arguments[3])
  );

  return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(
  draw_widget_tabs_callable,
  4,
  4,
  draw_widget_tabs
);

static mp_obj_t
draw_widget_battery(size_t argument_count, const mp_obj_t *arguments) {
  area512_widget_draw_battery(
    area512_micropython_fetch_sprite_handle_or_raise(arguments[0]),
    mp_obj_get_int(arguments[1]),
    mp_obj_get_int(arguments[2])
  );

  return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(
  draw_widget_battery_callable,
  3,
  3,
  draw_widget_battery
);

static mp_obj_t
draw_widget_splash(size_t argument_count, const mp_obj_t *arguments) {
  const char *subtitle =
    argument_count >= 3 ? mp_obj_str_get_str(arguments[2]) : "";

  area512_widget_draw_splash(
    area512_micropython_fetch_sprite_handle_or_raise(arguments[0]),
    mp_obj_str_get_str(arguments[1]),
    subtitle
  );

  return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(
  draw_widget_splash_callable,
  2,
  3,
  draw_widget_splash
);

static mp_obj_t
draw_widget_big_text(size_t argument_count, const mp_obj_t *arguments) {
  uint32_t color = argument_count >= 5 ? fetch_color_or_raise(arguments[4])
                                       : area512_theme_emphasis_color();

  area512_widget_draw_big_text(
    area512_micropython_fetch_sprite_handle_or_raise(arguments[0]),
    mp_obj_get_int(arguments[1]),
    mp_obj_get_int(arguments[2]),
    mp_obj_str_get_str(arguments[3]),
    color
  );

  return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(
  draw_widget_big_text_callable,
  4,
  5,
  draw_widget_big_text
);

static mp_obj_t
draw_widget_toast(mp_obj_t sprite_object, mp_obj_t message_object) {
  area512_widget_draw_toast(
    area512_micropython_fetch_sprite_handle_or_raise(sprite_object),
    mp_obj_str_get_str(message_object)
  );

  return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_2(draw_widget_toast_callable, draw_widget_toast);

static mp_obj_t
draw_widget_panel(size_t argument_count, const mp_obj_t *arguments) {
  area512_widget_draw_panel(
    area512_micropython_fetch_sprite_handle_or_raise(arguments[0]),
    mp_obj_get_int(arguments[1]),
    mp_obj_get_int(arguments[2]),
    mp_obj_get_int(arguments[3]),
    mp_obj_get_int(arguments[4])
  );

  return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(
  draw_widget_panel_callable,
  5,
  5,
  draw_widget_panel
);

static mp_obj_t
draw_widget_titled_panel(size_t argument_count, const mp_obj_t *arguments) {
  area512_widget_draw_titled_panel(
    area512_micropython_fetch_sprite_handle_or_raise(arguments[0]),
    mp_obj_get_int(arguments[1]),
    mp_obj_get_int(arguments[2]),
    mp_obj_get_int(arguments[3]),
    mp_obj_get_int(arguments[4]),
    mp_obj_str_get_str(arguments[5])
  );

  return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(
  draw_widget_titled_panel_callable,
  6,
  6,
  draw_widget_titled_panel
);

static mp_obj_t
draw_widget_text_center(size_t argument_count, const mp_obj_t *arguments) {
  uint32_t color =
    argument_count >= 4 ? fetch_color_or_raise(arguments[3])
                        : area512_theme_text_color();

  area512_widget_draw_text_center(
    area512_micropython_fetch_sprite_handle_or_raise(arguments[0]),
    mp_obj_get_int(arguments[1]),
    mp_obj_str_get_str(arguments[2]),
    color
  );

  return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(
  draw_widget_text_center_callable,
  3,
  4,
  draw_widget_text_center
);

static mp_obj_t
draw_widget_text_right(size_t argument_count, const mp_obj_t *arguments) {
  uint32_t color =
    argument_count >= 5 ? fetch_color_or_raise(arguments[4])
                        : area512_theme_text_color();

  area512_widget_draw_text_right(
    area512_micropython_fetch_sprite_handle_or_raise(arguments[0]),
    mp_obj_get_int(arguments[1]),
    mp_obj_get_int(arguments[2]),
    mp_obj_str_get_str(arguments[3]),
    color
  );

  return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(
  draw_widget_text_right_callable,
  4,
  5,
  draw_widget_text_right
);

static mp_obj_t
draw_widget_center_lines(mp_obj_t sprite_object, mp_obj_t line_sequence) {
  void *sprite_handle =
    area512_micropython_fetch_sprite_handle_or_raise(sprite_object);

  size_t line_count;
  mp_obj_t *line_items;

  mp_obj_get_array(line_sequence, &line_count, &line_items);

  if (line_count > WIDGET_MENU_MAX_ITEMS)
    line_count = WIDGET_MENU_MAX_ITEMS;

  WidgetColoredLine lines[WIDGET_MENU_MAX_ITEMS];

  for (size_t i = 0; i < line_count; i++) {
    lines[i].color = area512_theme_text_color();

    if (mp_obj_is_str(line_items[i])) {
      lines[i].text = mp_obj_str_get_str(line_items[i]);
      continue;
    }

    size_t row_length;
    mp_obj_t *row_items;

    mp_obj_get_array(line_items[i], &row_length, &row_items);

    if (row_length == 0)
      mp_raise_TypeError(MP_ERROR_TEXT("expected str or (text, color)"));

    lines[i].text = mp_obj_str_get_str(row_items[0]);

    if (row_length >= 2)
      lines[i].color = fetch_color_or_raise(row_items[1]);
  }

  area512_widget_draw_center_lines(sprite_handle, lines, (int)line_count);

  return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_2(
  draw_widget_center_lines_callable,
  draw_widget_center_lines
);

static mp_obj_t
draw_widget_wrap_text(size_t argument_count, const mp_obj_t *arguments) {
  uint32_t color =
    argument_count >= 7 ? fetch_color_or_raise(arguments[6])
                        : area512_theme_text_color();

  return MP_OBJ_NEW_SMALL_INT(area512_widget_draw_wrap_text(
    area512_micropython_fetch_sprite_handle_or_raise(arguments[0]),
    mp_obj_get_int(arguments[1]),
    mp_obj_get_int(arguments[2]),
    mp_obj_get_int(arguments[3]),
    mp_obj_get_int(arguments[4]),
    mp_obj_str_get_str(arguments[5]),
    color
  ));
}
static MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(
  draw_widget_wrap_text_callable,
  6,
  7,
  draw_widget_wrap_text
);

static mp_obj_t
draw_widget_marquee(size_t argument_count, const mp_obj_t *arguments) {
  return MP_OBJ_NEW_SMALL_INT(area512_widget_draw_marquee(
    area512_micropython_fetch_sprite_handle_or_raise(arguments[0]),
    mp_obj_get_int(arguments[1]),
    mp_obj_get_int(arguments[2]),
    mp_obj_get_int(arguments[3]),
    mp_obj_str_get_str(arguments[4]),
    mp_obj_get_int(arguments[5])
  ));
}
static MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(
  draw_widget_marquee_callable,
  6,
  6,
  draw_widget_marquee
);

// -----------------------------------------------------------------------------
// Tables, indicators, charts, and controls
// -----------------------------------------------------------------------------

static mp_obj_t
draw_widget_cell(size_t argument_count, const mp_obj_t *arguments) {
  area512_widget_draw_cell(
    area512_micropython_fetch_sprite_handle_or_raise(arguments[0]),
    mp_obj_get_int(arguments[1]),
    mp_obj_get_int(arguments[2]),
    mp_obj_get_int(arguments[3]),
    mp_obj_get_int(arguments[4]),
    mp_obj_str_get_str(arguments[5]),
    mp_obj_is_true(arguments[6])
  );

  return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(
  draw_widget_cell_callable,
  7,
  7,
  draw_widget_cell
);

static mp_obj_t
draw_widget_table_header(size_t argument_count, const mp_obj_t *arguments) {
  void *sprite_handle =
    area512_micropython_fetch_sprite_handle_or_raise(arguments[0]);
  int widths[WIDGET_TABLE_MAX_COLUMNS];
  const char *labels[WIDGET_TABLE_MAX_COLUMNS];
  int column_count =
    copy_table_columns(arguments[3], arguments[4], widths, labels);

  area512_widget_draw_table_header(
    sprite_handle,
    mp_obj_get_int(arguments[1]),
    mp_obj_get_int(arguments[2]),
    widths,
    labels,
    column_count
  );

  return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(
  draw_widget_table_header_callable,
  5,
  5,
  draw_widget_table_header
);

static mp_obj_t
draw_widget_table_row(size_t argument_count, const mp_obj_t *arguments) {
  void *sprite_handle =
    area512_micropython_fetch_sprite_handle_or_raise(arguments[0]);
  int widths[WIDGET_TABLE_MAX_COLUMNS];
  const char *texts[WIDGET_TABLE_MAX_COLUMNS];
  int column_count =
    copy_table_columns(arguments[3], arguments[4], widths, texts);

  area512_widget_draw_table_row(
    sprite_handle,
    mp_obj_get_int(arguments[1]),
    mp_obj_get_int(arguments[2]),
    widths,
    texts,
    column_count,
    mp_obj_is_true(arguments[5])
  );

  return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(
  draw_widget_table_row_callable,
  6,
  6,
  draw_widget_table_row
);

static mp_obj_t
draw_widget_field(size_t argument_count, const mp_obj_t *arguments) {
  area512_widget_draw_field(
    area512_micropython_fetch_sprite_handle_or_raise(arguments[0]),
    mp_obj_get_int(arguments[1]),
    mp_obj_get_int(arguments[2]),
    mp_obj_get_int(arguments[3]),
    mp_obj_str_get_str(arguments[4]),
    mp_obj_str_get_str(arguments[5]),
    mp_obj_is_true(arguments[6])
  );

  return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(
  draw_widget_field_callable,
  7,
  7,
  draw_widget_field
);

static mp_obj_t
draw_widget_gauge(size_t argument_count, const mp_obj_t *arguments) {
  area512_widget_draw_gauge(
    area512_micropython_fetch_sprite_handle_or_raise(arguments[0]),
    mp_obj_get_int(arguments[1]),
    mp_obj_get_int(arguments[2]),
    mp_obj_get_int(arguments[3]),
    mp_obj_get_int(arguments[4]),
    mp_obj_get_int(arguments[5])
  );

  return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(
  draw_widget_gauge_callable,
  6,
  6,
  draw_widget_gauge
);

static mp_obj_t
draw_widget_slider(size_t argument_count, const mp_obj_t *arguments) {
  area512_widget_draw_slider(
    area512_micropython_fetch_sprite_handle_or_raise(arguments[0]),
    mp_obj_get_int(arguments[1]),
    mp_obj_get_int(arguments[2]),
    mp_obj_get_int(arguments[3]),
    mp_obj_get_int(arguments[4]),
    mp_obj_get_int(arguments[5]),
    mp_obj_is_true(arguments[6])
  );

  return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(
  draw_widget_slider_callable,
  7,
  7,
  draw_widget_slider
);

static mp_obj_t
draw_widget_scrollbar(size_t argument_count, const mp_obj_t *arguments) {
  area512_widget_draw_scrollbar(
    area512_micropython_fetch_sprite_handle_or_raise(arguments[0]),
    mp_obj_get_int(arguments[1]),
    mp_obj_get_int(arguments[2]),
    mp_obj_get_int(arguments[3]),
    mp_obj_get_int(arguments[4]),
    mp_obj_get_int(arguments[5]),
    mp_obj_get_int(arguments[6])
  );

  return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(
  draw_widget_scrollbar_callable,
  7,
  7,
  draw_widget_scrollbar
);

static mp_obj_t
draw_widget_horizontal_scrollbar(
  size_t argument_count,
  const mp_obj_t *arguments
) {

  area512_widget_draw_horizontal_scrollbar(
    area512_micropython_fetch_sprite_handle_or_raise(arguments[0]),
    mp_obj_get_int(arguments[1]),
    mp_obj_get_int(arguments[2]),
    mp_obj_get_int(arguments[3]),
    mp_obj_get_int(arguments[4]),
    mp_obj_get_int(arguments[5]),
    mp_obj_get_int(arguments[6])
  );

  return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(
  draw_widget_horizontal_scrollbar_callable,
  7,
  7,
  draw_widget_horizontal_scrollbar
);

static mp_obj_t
draw_widget_badge(size_t argument_count, const mp_obj_t *arguments) {
  area512_widget_draw_badge(
    area512_micropython_fetch_sprite_handle_or_raise(arguments[0]),
    mp_obj_get_int(arguments[1]),
    mp_obj_get_int(arguments[2]),
    mp_obj_str_get_str(arguments[3])
  );

  return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(
  draw_widget_badge_callable,
  4,
  4,
  draw_widget_badge
);

static mp_obj_t
draw_widget_busy(size_t argument_count, const mp_obj_t *arguments) {
  area512_widget_draw_busy(
    area512_micropython_fetch_sprite_handle_or_raise(arguments[0]),
    mp_obj_get_int(arguments[1]),
    mp_obj_get_int(arguments[2]),
    mp_obj_get_int(arguments[3])
  );

  return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(
  draw_widget_busy_callable,
  4,
  4,
  draw_widget_busy
);

static mp_obj_t
draw_widget_page_dots(size_t argument_count, const mp_obj_t *arguments) {
  area512_widget_draw_page_dots(
    area512_micropython_fetch_sprite_handle_or_raise(arguments[0]),
    mp_obj_get_int(arguments[1]),
    mp_obj_get_int(arguments[2]),
    mp_obj_get_int(arguments[3])
  );

  return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(
  draw_widget_page_dots_callable,
  4,
  4,
  draw_widget_page_dots
);

static mp_obj_t
draw_widget_bar_chart(size_t argument_count, const mp_obj_t *arguments) {
  void *sprite_handle =
    area512_micropython_fetch_sprite_handle_or_raise(arguments[0]);
  int chart_values[WIDGET_CHART_MAX_VALUES];
  int value_count =
    copy_sequence_integers(arguments[5], chart_values, WIDGET_CHART_MAX_VALUES);

  area512_widget_draw_bar_chart(
    sprite_handle,
    mp_obj_get_int(arguments[1]),
    mp_obj_get_int(arguments[2]),
    mp_obj_get_int(arguments[3]),
    mp_obj_get_int(arguments[4]),
    chart_values,
    value_count,
    mp_obj_get_int(arguments[6])
  );

  return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(
  draw_widget_bar_chart_callable,
  7,
  7,
  draw_widget_bar_chart
);

static mp_obj_t
draw_widget_line_chart(size_t argument_count, const mp_obj_t *arguments) {
  void *sprite_handle =
    area512_micropython_fetch_sprite_handle_or_raise(arguments[0]);
  int chart_values[WIDGET_CHART_MAX_VALUES];
  int value_count =
    copy_sequence_integers(arguments[5], chart_values, WIDGET_CHART_MAX_VALUES);

  area512_widget_draw_line_chart(
    sprite_handle,
    mp_obj_get_int(arguments[1]),
    mp_obj_get_int(arguments[2]),
    mp_obj_get_int(arguments[3]),
    mp_obj_get_int(arguments[4]),
    chart_values,
    value_count,
    mp_obj_get_int(arguments[6])
  );

  return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(
  draw_widget_line_chart_callable,
  7,
  7,
  draw_widget_line_chart
);

static mp_obj_t
draw_widget_button(size_t argument_count, const mp_obj_t *arguments) {
  area512_widget_draw_button(
    area512_micropython_fetch_sprite_handle_or_raise(arguments[0]),
    mp_obj_get_int(arguments[1]),
    mp_obj_get_int(arguments[2]),
    mp_obj_get_int(arguments[3]),
    mp_obj_str_get_str(arguments[4]),
    mp_obj_is_true(arguments[5])
  );

  return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(
  draw_widget_button_callable,
  6,
  6,
  draw_widget_button
);

static mp_obj_t
draw_widget_checkbox(size_t argument_count, const mp_obj_t *arguments) {
  area512_widget_draw_checkbox(
    area512_micropython_fetch_sprite_handle_or_raise(arguments[0]),
    mp_obj_get_int(arguments[1]),
    mp_obj_get_int(arguments[2]),
    mp_obj_str_get_str(arguments[3]),
    mp_obj_is_true(arguments[4]),
    mp_obj_is_true(arguments[5])
  );

  return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(
  draw_widget_checkbox_callable,
  6,
  6,
  draw_widget_checkbox
);

static mp_obj_t
draw_widget_radio(size_t argument_count, const mp_obj_t *arguments) {
  area512_widget_draw_radio(
    area512_micropython_fetch_sprite_handle_or_raise(arguments[0]),
    mp_obj_get_int(arguments[1]),
    mp_obj_get_int(arguments[2]),
    mp_obj_str_get_str(arguments[3]),
    mp_obj_is_true(arguments[4]),
    mp_obj_is_true(arguments[5])
  );

  return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(
  draw_widget_radio_callable,
  6,
  6,
  draw_widget_radio
);

static mp_obj_t
draw_widget_toggle(size_t argument_count, const mp_obj_t *arguments) {
  area512_widget_draw_toggle(
    area512_micropython_fetch_sprite_handle_or_raise(arguments[0]),
    mp_obj_get_int(arguments[1]),
    mp_obj_get_int(arguments[2]),
    mp_obj_is_true(arguments[3]),
    mp_obj_is_true(arguments[4])
  );

  return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(
  draw_widget_toggle_callable,
  5,
  5,
  draw_widget_toggle
);

static mp_obj_t
draw_widget_spinner(size_t argument_count, const mp_obj_t *arguments) {
  area512_widget_draw_spinner(
    area512_micropython_fetch_sprite_handle_or_raise(arguments[0]),
    mp_obj_get_int(arguments[1]),
    mp_obj_get_int(arguments[2]),
    mp_obj_get_int(arguments[3]),
    mp_obj_str_get_str(arguments[4]),
    mp_obj_is_true(arguments[5])
  );

  return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(
  draw_widget_spinner_callable,
  6,
  6,
  draw_widget_spinner
);

// -----------------------------------------------------------------------------
// Modals
// -----------------------------------------------------------------------------

static mp_obj_t
run_widget_input_modal(size_t argument_count, const mp_obj_t *arguments) {
  void *sprite_handle =
    area512_micropython_fetch_sprite_handle_or_raise(arguments[0]);
  const char *label = mp_obj_str_get_str(arguments[1]);
  const char *initial =
    argument_count >= 3 ? mp_obj_str_get_str(arguments[2]) : "";
  char input[WIDGET_INPUT_MAX];

  io_raw_bang(false);
  int confirmed =
    area512_widget_run_input_modal(sprite_handle, label, initial, input);
  io_cooked_bang();

  return confirmed ? mp_obj_new_str_from_cstr(input) : mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(
  run_widget_input_modal_callable,
  2,
  3,
  run_widget_input_modal
);

static mp_obj_t
run_widget_number_modal(size_t argument_count, const mp_obj_t *arguments) {
  void *sprite_handle =
    area512_micropython_fetch_sprite_handle_or_raise(arguments[0]);
  const char *label = mp_obj_str_get_str(arguments[1]);
  int initial = mp_obj_get_int(arguments[2]);
  int minimum = mp_obj_get_int(arguments[3]);
  int maximum = mp_obj_get_int(arguments[4]);
  int number;

  io_raw_bang(false);
  int confirmed = area512_widget_run_number_modal(
    sprite_handle,
    label,
    initial,
    minimum,
    maximum,
    &number
  );
  io_cooked_bang();

  return confirmed ? MP_OBJ_NEW_SMALL_INT(number) : mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(
  run_widget_number_modal_callable,
  5,
  5,
  run_widget_number_modal
);

static mp_obj_t
run_widget_confirm_modal(mp_obj_t sprite_object, mp_obj_t question_object) {
  void *sprite_handle =
    area512_micropython_fetch_sprite_handle_or_raise(sprite_object);
  const char *question = mp_obj_str_get_str(question_object);

  io_raw_bang(false);
  int confirmed = area512_widget_run_confirm_modal(sprite_handle, question);
  io_cooked_bang();

  return mp_obj_new_bool(confirmed);
}
static MP_DEFINE_CONST_FUN_OBJ_2(
  run_widget_confirm_modal_callable,
  run_widget_confirm_modal
);

static mp_obj_t
run_widget_dialog_modal(
  mp_obj_t sprite_object,
  mp_obj_t message_object,
  mp_obj_t button_sequence
) {

  void *sprite_handle =
    area512_micropython_fetch_sprite_handle_or_raise(sprite_object);
  const char *message = mp_obj_str_get_str(message_object);
  const char *buttons[WIDGET_MENU_MAX_ITEMS];
  int button_count =
    copy_sequence_strings(button_sequence, buttons, WIDGET_MENU_MAX_ITEMS);

  io_raw_bang(false);
  int chosen_index = area512_widget_run_dialog_modal(
    sprite_handle,
    message,
    buttons,
    button_count
  );
  io_cooked_bang();

  return chosen_index < 0 ? mp_const_none : MP_OBJ_NEW_SMALL_INT(chosen_index);
}
static MP_DEFINE_CONST_FUN_OBJ_3(
  run_widget_dialog_modal_callable,
  run_widget_dialog_modal
);

static mp_obj_t
run_widget_menu_modal(size_t argument_count, const mp_obj_t *arguments) {
  void *sprite_handle =
    area512_micropython_fetch_sprite_handle_or_raise(arguments[0]);
  const char *title = mp_obj_str_get_str(arguments[1]);
  const char *items[WIDGET_MENU_MAX_ITEMS];
  int item_count =
    copy_sequence_strings(arguments[2], items, WIDGET_MENU_MAX_ITEMS);
  int initial = argument_count >= 4 ? mp_obj_get_int(arguments[3]) : 0;

  io_raw_bang(false);
  int chosen_index = area512_widget_run_menu_modal(
    sprite_handle,
    title,
    items,
    item_count,
    initial
  );
  io_cooked_bang();

  return chosen_index < 0 ? mp_const_none : MP_OBJ_NEW_SMALL_INT(chosen_index);
}
static MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(
  run_widget_menu_modal_callable,
  3,
  4,
  run_widget_menu_modal
);

static mp_obj_t
run_widget_alert_modal(mp_obj_t sprite_object, mp_obj_t message_object) {
  void *sprite_handle =
    area512_micropython_fetch_sprite_handle_or_raise(sprite_object);
  const char *message = mp_obj_str_get_str(message_object);

  io_raw_bang(false);
  area512_widget_run_alert_modal(sprite_handle, message);
  io_cooked_bang();

  return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_2(
  run_widget_alert_modal_callable,
  run_widget_alert_modal
);

static const mp_rom_map_elem_t widget_locals_table[] = {
  {MP_ROM_QSTR(MP_QSTR_bg), MP_ROM_PTR(&fetch_widget_background_color_callable)
  },
  {MP_ROM_QSTR(MP_QSTR_amber), MP_ROM_PTR(&fetch_widget_amber_color_callable)},
  {MP_ROM_QSTR(MP_QSTR_dim), MP_ROM_PTR(&fetch_widget_dim_color_callable)},
  {MP_ROM_QSTR(MP_QSTR_gold), MP_ROM_PTR(&fetch_widget_gold_color_callable)},
  {MP_ROM_QSTR(MP_QSTR_dark), MP_ROM_PTR(&fetch_widget_dark_color_callable)},
  {MP_ROM_QSTR(MP_QSTR_theme_background),
   MP_ROM_PTR(&fetch_widget_theme_background_color_callable)},
  {MP_ROM_QSTR(MP_QSTR_theme_text),
   MP_ROM_PTR(&fetch_widget_theme_text_color_callable)},
  {MP_ROM_QSTR(MP_QSTR_theme_emphasis),
   MP_ROM_PTR(&fetch_widget_theme_emphasis_color_callable)},
  {MP_ROM_QSTR(MP_QSTR_theme_border),
   MP_ROM_PTR(&fetch_widget_theme_border_color_callable)},
  {MP_ROM_QSTR(MP_QSTR_theme_selected),
   MP_ROM_PTR(&fetch_widget_theme_selected_color_callable)},
  {MP_ROM_QSTR(MP_QSTR_theme_box),
   MP_ROM_PTR(&fetch_widget_theme_box_color_callable)},
  {
    MP_ROM_QSTR(MP_QSTR_char_width),
    MP_ROM_PTR(&fetch_widget_character_width_callable),
  },
  {MP_ROM_QSTR(MP_QSTR_row_height),
   MP_ROM_PTR(&fetch_widget_row_height_callable)},
  {
    MP_ROM_QSTR(MP_QSTR_header_height),
    MP_ROM_PTR(&fetch_widget_header_height_callable),
  },
  {MP_ROM_QSTR(MP_QSTR_body_top), MP_ROM_PTR(&fetch_widget_body_top_callable)},
  {
    MP_ROM_QSTR(MP_QSTR_body_bottom),
    MP_ROM_PTR(&fetch_widget_body_bottom_callable),
  },
  {
    MP_ROM_QSTR(MP_QSTR_body_height),
    MP_ROM_PTR(&fetch_widget_body_height_callable),
  },
  {
    MP_ROM_QSTR(MP_QSTR_text_width),
    MP_ROM_PTR(&measure_widget_text_width_callable),
  },
  {MP_ROM_QSTR(MP_QSTR_clip), MP_ROM_PTR(&clip_widget_text_callable)},
  {MP_ROM_QSTR(MP_QSTR_read_key), MP_ROM_PTR(&read_widget_key_callable)},
  {MP_ROM_QSTR(MP_QSTR_header), MP_ROM_PTR(&draw_widget_header_callable)},
  {MP_ROM_QSTR(MP_QSTR_footer), MP_ROM_PTR(&draw_widget_footer_callable)},
  {MP_ROM_QSTR(MP_QSTR_hints), MP_ROM_PTR(&draw_widget_hints_callable)},
  {MP_ROM_QSTR(MP_QSTR_separator), MP_ROM_PTR(&draw_widget_separator_callable)},
  {
    MP_ROM_QSTR(MP_QSTR_vseparator),
    MP_ROM_PTR(&draw_widget_vertical_separator_callable),
  },
  {MP_ROM_QSTR(MP_QSTR_tabs), MP_ROM_PTR(&draw_widget_tabs_callable)},
  {MP_ROM_QSTR(MP_QSTR_battery), MP_ROM_PTR(&draw_widget_battery_callable)},
  {MP_ROM_QSTR(MP_QSTR_splash), MP_ROM_PTR(&draw_widget_splash_callable)},
  {MP_ROM_QSTR(MP_QSTR_big_text), MP_ROM_PTR(&draw_widget_big_text_callable)},
  {MP_ROM_QSTR(MP_QSTR_toast), MP_ROM_PTR(&draw_widget_toast_callable)},
  {MP_ROM_QSTR(MP_QSTR_panel), MP_ROM_PTR(&draw_widget_panel_callable)},
  {
    MP_ROM_QSTR(MP_QSTR_titled_panel),
    MP_ROM_PTR(&draw_widget_titled_panel_callable),
  },
  {
    MP_ROM_QSTR(MP_QSTR_text_center),
    MP_ROM_PTR(&draw_widget_text_center_callable),
  },
  {
    MP_ROM_QSTR(MP_QSTR_text_right),
    MP_ROM_PTR(&draw_widget_text_right_callable),
  },
  {
    MP_ROM_QSTR(MP_QSTR_center_lines),
    MP_ROM_PTR(&draw_widget_center_lines_callable),
  },
  {MP_ROM_QSTR(MP_QSTR_wrap_text), MP_ROM_PTR(&draw_widget_wrap_text_callable)},
  {MP_ROM_QSTR(MP_QSTR_marquee), MP_ROM_PTR(&draw_widget_marquee_callable)},
  {MP_ROM_QSTR(MP_QSTR_cell), MP_ROM_PTR(&draw_widget_cell_callable)},
  {
    MP_ROM_QSTR(MP_QSTR_table_header),
    MP_ROM_PTR(&draw_widget_table_header_callable),
  },
  {MP_ROM_QSTR(MP_QSTR_table_row), MP_ROM_PTR(&draw_widget_table_row_callable)},
  {MP_ROM_QSTR(MP_QSTR_field), MP_ROM_PTR(&draw_widget_field_callable)},
  {MP_ROM_QSTR(MP_QSTR_gauge), MP_ROM_PTR(&draw_widget_gauge_callable)},
  {MP_ROM_QSTR(MP_QSTR_slider), MP_ROM_PTR(&draw_widget_slider_callable)},
  {MP_ROM_QSTR(MP_QSTR_scrollbar), MP_ROM_PTR(&draw_widget_scrollbar_callable)},
  {
    MP_ROM_QSTR(MP_QSTR_hscrollbar),
    MP_ROM_PTR(&draw_widget_horizontal_scrollbar_callable),
  },
  {MP_ROM_QSTR(MP_QSTR_badge), MP_ROM_PTR(&draw_widget_badge_callable)},
  {MP_ROM_QSTR(MP_QSTR_busy), MP_ROM_PTR(&draw_widget_busy_callable)},
  {MP_ROM_QSTR(MP_QSTR_page_dots), MP_ROM_PTR(&draw_widget_page_dots_callable)},
  {MP_ROM_QSTR(MP_QSTR_bar_chart), MP_ROM_PTR(&draw_widget_bar_chart_callable)},
  {
    MP_ROM_QSTR(MP_QSTR_line_chart),
    MP_ROM_PTR(&draw_widget_line_chart_callable),
  },
  {MP_ROM_QSTR(MP_QSTR_button), MP_ROM_PTR(&draw_widget_button_callable)},
  {MP_ROM_QSTR(MP_QSTR_checkbox), MP_ROM_PTR(&draw_widget_checkbox_callable)},
  {MP_ROM_QSTR(MP_QSTR_radio), MP_ROM_PTR(&draw_widget_radio_callable)},
  {MP_ROM_QSTR(MP_QSTR_toggle), MP_ROM_PTR(&draw_widget_toggle_callable)},
  {MP_ROM_QSTR(MP_QSTR_spinner), MP_ROM_PTR(&draw_widget_spinner_callable)},
  {MP_ROM_QSTR(MP_QSTR_input), MP_ROM_PTR(&run_widget_input_modal_callable)},
  {
    MP_ROM_QSTR(MP_QSTR_input_number),
    MP_ROM_PTR(&run_widget_number_modal_callable),
  },
  {MP_ROM_QSTR(MP_QSTR_confirm), MP_ROM_PTR(&run_widget_confirm_modal_callable)
  },
  {MP_ROM_QSTR(MP_QSTR_dialog), MP_ROM_PTR(&run_widget_dialog_modal_callable)},
  {MP_ROM_QSTR(MP_QSTR_menu), MP_ROM_PTR(&run_widget_menu_modal_callable)},
  {MP_ROM_QSTR(MP_QSTR_alert), MP_ROM_PTR(&run_widget_alert_modal_callable)},
};
static MP_DEFINE_CONST_DICT(widget_locals_dictionary, widget_locals_table);

MP_DEFINE_CONST_OBJ_TYPE(
  area512_micropython_widget_type,
  MP_QSTR_Widget,
  MP_TYPE_FLAG_NONE,
  locals_dict,
  &widget_locals_dictionary
);

// -----------------------------------------------------------------------------
// WidgetList
// -----------------------------------------------------------------------------

typedef struct {
  mp_obj_base_t base;
  WidgetList *list;
} area512_micropython_widget_list_instance_t;

static WidgetList *
fetch_widget_list_or_raise(mp_obj_t list_object) {
  area512_micropython_widget_list_instance_t *list_instance =
    MP_OBJ_TO_PTR(list_object);

  if (list_instance->list == NULL)
    mp_raise_msg(&mp_type_ValueError, MP_ERROR_TEXT("deleted list"));

  return list_instance->list;
}

static mp_obj_t
create_widget_list_object(
  const mp_obj_type_t *list_type,
  size_t argument_count,
  size_t keyword_argument_count,
  const mp_obj_t *arguments
) {

  mp_arg_check_num(argument_count, keyword_argument_count, 0, 0, false);

  area512_micropython_widget_list_instance_t *list_instance =
    mp_obj_malloc_with_finaliser(
      area512_micropython_widget_list_instance_t,
      list_type
    );

  list_instance->list = NULL;

  // Too large for the MicroPython GC heap, which starts at a few kilobytes.
  WidgetList *list = malloc(sizeof(WidgetList));

  if (list == NULL)
    mp_raise_type(&mp_type_MemoryError);

  area512_widget_list_init(list);

  list_instance->list = list;

  return MP_OBJ_FROM_PTR(list_instance);
}

static mp_obj_t
delete_widget_list(mp_obj_t list_object) {
  area512_micropython_widget_list_instance_t *list_instance =
    MP_OBJ_TO_PTR(list_object);

  if (list_instance->list != NULL) {
    free(list_instance->list);

    list_instance->list = NULL;
  }

  return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_1(
  delete_widget_list_callable,
  delete_widget_list
);

static mp_obj_t
set_widget_list_area(size_t argument_count, const mp_obj_t *arguments) {
  WidgetList *list = fetch_widget_list_or_raise(arguments[0]);

  list->x = mp_obj_get_int(arguments[1]);
  list->y = mp_obj_get_int(arguments[2]);
  list->w = mp_obj_get_int(arguments[3]);
  list->h = mp_obj_get_int(arguments[4]);
  area512_widget_list_set_index(list, list->index);

  return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(
  set_widget_list_area_callable,
  5,
  5,
  set_widget_list_area
);

static mp_obj_t
clear_widget_list(mp_obj_t list_object) {
  area512_widget_list_clear(fetch_widget_list_or_raise(list_object));

  return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_1(clear_widget_list_callable, clear_widget_list);

static mp_obj_t
add_widget_list_row(size_t argument_count, const mp_obj_t *arguments) {
  const char *tag = argument_count >= 3 ? mp_obj_str_get_str(arguments[2]) : "";

  area512_widget_list_add(
    fetch_widget_list_or_raise(arguments[0]),
    mp_obj_str_get_str(arguments[1]),
    tag
  );

  return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(
  add_widget_list_row_callable,
  2,
  3,
  add_widget_list_row
);

static mp_obj_t
set_widget_list_empty_text(mp_obj_t list_object, mp_obj_t text_object) {
  WidgetList *list = fetch_widget_list_or_raise(list_object);
  const char *text = mp_obj_str_get_str(text_object);
  size_t text_length = strlen(text);

  if (text_length >= WIDGET_LIST_TEXT_MAX)
    text_length = WIDGET_LIST_TEXT_MAX - 1;

  memcpy(list->empty_text, text, text_length);
  list->empty_text[text_length] = 0;

  return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_2(
  set_widget_list_empty_text_callable,
  set_widget_list_empty_text
);

static mp_obj_t
set_widget_list_show_marks(mp_obj_t list_object, mp_obj_t show_marks_object) {
  fetch_widget_list_or_raise(list_object)->show_marks =
    mp_obj_is_true(show_marks_object);

  return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_2(
  set_widget_list_show_marks_callable,
  set_widget_list_show_marks
);

static mp_obj_t
toggle_widget_list_mark(mp_obj_t list_object) {
  WidgetList *list = fetch_widget_list_or_raise(list_object);

  if (list->count > 0)
    list->marks[list->index] = !list->marks[list->index];

  return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_1(
  toggle_widget_list_mark_callable,
  toggle_widget_list_mark
);

static mp_obj_t
set_widget_list_mark(
  mp_obj_t list_object,
  mp_obj_t index_object,
  mp_obj_t marked_object
) {

  WidgetList *list = fetch_widget_list_or_raise(list_object);
  int index = mp_obj_get_int(index_object);

  if (index >= 0 && index < list->count)
    list->marks[index] = mp_obj_is_true(marked_object);

  return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_3(
  set_widget_list_mark_callable,
  set_widget_list_mark
);

static mp_obj_t
is_widget_list_row_marked(mp_obj_t list_object, mp_obj_t index_object) {
  WidgetList *list = fetch_widget_list_or_raise(list_object);
  int index = mp_obj_get_int(index_object);

  return mp_obj_new_bool(
    index >= 0 && index < list->count && list->marks[index]
  );
}
static MP_DEFINE_CONST_FUN_OBJ_2(
  is_widget_list_row_marked_callable,
  is_widget_list_row_marked
);

static mp_obj_t
fetch_widget_list_count(mp_obj_t list_object) {
  return MP_OBJ_NEW_SMALL_INT(fetch_widget_list_or_raise(list_object)->count);
}
static MP_DEFINE_CONST_FUN_OBJ_1(
  fetch_widget_list_count_callable,
  fetch_widget_list_count
);

static mp_obj_t
fetch_widget_list_index(mp_obj_t list_object) {
  return MP_OBJ_NEW_SMALL_INT(fetch_widget_list_or_raise(list_object)->index);
}
static MP_DEFINE_CONST_FUN_OBJ_1(
  fetch_widget_list_index_callable,
  fetch_widget_list_index
);

static mp_obj_t
set_widget_list_index(mp_obj_t list_object, mp_obj_t index_object) {
  WidgetList *list = fetch_widget_list_or_raise(list_object);

  area512_widget_list_set_index(list, mp_obj_get_int(index_object));

  return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_2(
  set_widget_list_index_callable,
  set_widget_list_index
);

static mp_obj_t
fetch_widget_list_top(mp_obj_t list_object) {
  return MP_OBJ_NEW_SMALL_INT(fetch_widget_list_or_raise(list_object)->top);
}
static MP_DEFINE_CONST_FUN_OBJ_1(
  fetch_widget_list_top_callable,
  fetch_widget_list_top
);

static mp_obj_t
handle_widget_list_key(mp_obj_t list_object, mp_obj_t key_object) {
  return mp_obj_new_bool(area512_widget_list_handle(
    fetch_widget_list_or_raise(list_object),
    mp_obj_str_get_str(key_object)
  ));
}
static MP_DEFINE_CONST_FUN_OBJ_2(
  handle_widget_list_key_callable,
  handle_widget_list_key
);

static mp_obj_t
draw_widget_list(mp_obj_t list_object, mp_obj_t sprite_object) {
  WidgetList *list = fetch_widget_list_or_raise(list_object);

  area512_widget_list_draw(
    area512_micropython_fetch_sprite_handle_or_raise(sprite_object),
    list
  );

  return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_2(draw_widget_list_callable, draw_widget_list);

static const mp_rom_map_elem_t widget_list_locals_table[] = {
  {MP_ROM_QSTR(MP_QSTR___del__), MP_ROM_PTR(&delete_widget_list_callable)},
  {MP_ROM_QSTR(MP_QSTR_area), MP_ROM_PTR(&set_widget_list_area_callable)},
  {MP_ROM_QSTR(MP_QSTR_clear), MP_ROM_PTR(&clear_widget_list_callable)},
  {MP_ROM_QSTR(MP_QSTR_add), MP_ROM_PTR(&add_widget_list_row_callable)},
  {
    MP_ROM_QSTR(MP_QSTR_set_empty_text),
    MP_ROM_PTR(&set_widget_list_empty_text_callable),
  },
  {
    MP_ROM_QSTR(MP_QSTR_set_show_marks),
    MP_ROM_PTR(&set_widget_list_show_marks_callable),
  },
  {
    MP_ROM_QSTR(MP_QSTR_toggle_mark),
    MP_ROM_PTR(&toggle_widget_list_mark_callable),
  },
  {MP_ROM_QSTR(MP_QSTR_mark), MP_ROM_PTR(&set_widget_list_mark_callable)},
  {MP_ROM_QSTR(MP_QSTR_marked), MP_ROM_PTR(&is_widget_list_row_marked_callable)
  },
  {MP_ROM_QSTR(MP_QSTR_count), MP_ROM_PTR(&fetch_widget_list_count_callable)},
  {MP_ROM_QSTR(MP_QSTR_index), MP_ROM_PTR(&fetch_widget_list_index_callable)},
  {MP_ROM_QSTR(MP_QSTR_set_index), MP_ROM_PTR(&set_widget_list_index_callable)},
  {MP_ROM_QSTR(MP_QSTR_top), MP_ROM_PTR(&fetch_widget_list_top_callable)},
  {MP_ROM_QSTR(MP_QSTR_handle), MP_ROM_PTR(&handle_widget_list_key_callable)},
  {MP_ROM_QSTR(MP_QSTR_draw), MP_ROM_PTR(&draw_widget_list_callable)},
};
static MP_DEFINE_CONST_DICT(
  widget_list_locals_dictionary,
  widget_list_locals_table
);

MP_DEFINE_CONST_OBJ_TYPE(
  area512_micropython_widget_list_type,
  MP_QSTR_WidgetList,
  MP_TYPE_FLAG_NONE,
  make_new,
  create_widget_list_object,
  locals_dict,
  &widget_list_locals_dictionary
);

// -----------------------------------------------------------------------------
// WidgetTextView
// -----------------------------------------------------------------------------

typedef struct {
  mp_obj_base_t base;
  WidgetTextView *view;
} area512_micropython_widget_text_view_instance_t;

static WidgetTextView *
fetch_widget_text_view_or_raise(mp_obj_t view_object) {
  area512_micropython_widget_text_view_instance_t *view_instance =
    MP_OBJ_TO_PTR(view_object);

  if (view_instance->view == NULL)
    mp_raise_msg(&mp_type_ValueError, MP_ERROR_TEXT("deleted text view"));

  return view_instance->view;
}

static mp_obj_t
create_widget_text_view_object(
  const mp_obj_type_t *view_type,
  size_t argument_count,
  size_t keyword_argument_count,
  const mp_obj_t *arguments
) {

  mp_arg_check_num(argument_count, keyword_argument_count, 0, 0, false);

  area512_micropython_widget_text_view_instance_t *view_instance =
    mp_obj_malloc_with_finaliser(
      area512_micropython_widget_text_view_instance_t,
      view_type
    );

  view_instance->view = NULL;

  // Too large for the MicroPython GC heap, which starts at a few kilobytes.
  WidgetTextView *view = malloc(sizeof(WidgetTextView));

  if (view == NULL)
    mp_raise_type(&mp_type_MemoryError);

  area512_widget_text_view_init(view);

  view_instance->view = view;

  return MP_OBJ_FROM_PTR(view_instance);
}

static mp_obj_t
delete_widget_text_view(mp_obj_t view_object) {
  area512_micropython_widget_text_view_instance_t *view_instance =
    MP_OBJ_TO_PTR(view_object);

  if (view_instance->view != NULL) {
    free(view_instance->view);

    view_instance->view = NULL;
  }

  return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_1(
  delete_widget_text_view_callable,
  delete_widget_text_view
);

static mp_obj_t
set_widget_text_view_area(size_t argument_count, const mp_obj_t *arguments) {
  WidgetTextView *view = fetch_widget_text_view_or_raise(arguments[0]);

  view->x = mp_obj_get_int(arguments[1]);
  view->y = mp_obj_get_int(arguments[2]);
  view->w = mp_obj_get_int(arguments[3]);
  view->h = mp_obj_get_int(arguments[4]);

  return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(
  set_widget_text_view_area_callable,
  5,
  5,
  set_widget_text_view_area
);

static mp_obj_t
set_widget_text_view_text(mp_obj_t view_object, mp_obj_t text_object) {
  area512_widget_text_view_set_text(
    fetch_widget_text_view_or_raise(view_object),
    mp_obj_str_get_str(text_object)
  );

  return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_2(
  set_widget_text_view_text_callable,
  set_widget_text_view_text
);

static mp_obj_t
fetch_widget_text_view_scroll(mp_obj_t view_object) {
  return MP_OBJ_NEW_SMALL_INT(
    fetch_widget_text_view_or_raise(view_object)->scroll
  );
}
static MP_DEFINE_CONST_FUN_OBJ_1(
  fetch_widget_text_view_scroll_callable,
  fetch_widget_text_view_scroll
);

static mp_obj_t
set_widget_text_view_scroll(mp_obj_t view_object, mp_obj_t scroll_object) {
  // Width measurement requires a Sprite. Clamp the lower bound here; draw and
  // handle clamp the upper bound once a Sprite is available.
  int scroll = mp_obj_get_int(scroll_object);

  if (scroll < 0)
    scroll = 0;

  fetch_widget_text_view_or_raise(view_object)->scroll = scroll;

  return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_2(
  set_widget_text_view_scroll_callable,
  set_widget_text_view_scroll
);

static mp_obj_t
handle_widget_text_view_key(mp_obj_t view_object, mp_obj_t key_object) {
  // handle has no Sprite argument in the Ruby API. ASCII metrics provide a
  // deterministic approximation; draw performs exact wrapping for rendering.
  WidgetTextView *view = fetch_widget_text_view_or_raise(view_object);
  const char *key = mp_obj_str_get_str(key_object);
  int delta = 0;

  if (strcmp(key, "UP") == 0 || strcmp(key, "k") == 0 || strcmp(key, ";") == 0)
    delta = -1;

  if (strcmp(key, "DOWN") == 0 || strcmp(key, "j") == 0 ||
      strcmp(key, ".") == 0)
    delta = 1;

  if (delta == 0)
    return mp_const_false;

  view->scroll += delta;

  if (view->scroll < 0)
    view->scroll = 0;

  return mp_const_true;
}
static MP_DEFINE_CONST_FUN_OBJ_2(
  handle_widget_text_view_key_callable,
  handle_widget_text_view_key
);

static mp_obj_t
draw_widget_text_view(mp_obj_t view_object, mp_obj_t sprite_object) {
  WidgetTextView *view = fetch_widget_text_view_or_raise(view_object);
  void *sprite_handle =
    area512_micropython_fetch_sprite_handle_or_raise(sprite_object);

  area512_widget_text_view_set_scroll(sprite_handle, view, view->scroll);
  area512_widget_text_view_draw(sprite_handle, view);

  return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_2(
  draw_widget_text_view_callable,
  draw_widget_text_view
);

static const mp_rom_map_elem_t widget_text_view_locals_table[] = {
  {MP_ROM_QSTR(MP_QSTR___del__), MP_ROM_PTR(&delete_widget_text_view_callable)},
  {MP_ROM_QSTR(MP_QSTR_area), MP_ROM_PTR(&set_widget_text_view_area_callable)},
  {
    MP_ROM_QSTR(MP_QSTR_set_text),
    MP_ROM_PTR(&set_widget_text_view_text_callable),
  },
  {
    MP_ROM_QSTR(MP_QSTR_scroll),
    MP_ROM_PTR(&fetch_widget_text_view_scroll_callable),
  },
  {
    MP_ROM_QSTR(MP_QSTR_set_scroll),
    MP_ROM_PTR(&set_widget_text_view_scroll_callable),
  },
  {
    MP_ROM_QSTR(MP_QSTR_handle),
    MP_ROM_PTR(&handle_widget_text_view_key_callable),
  },
  {MP_ROM_QSTR(MP_QSTR_draw), MP_ROM_PTR(&draw_widget_text_view_callable)},
};
static MP_DEFINE_CONST_DICT(
  widget_text_view_locals_dictionary,
  widget_text_view_locals_table
);

MP_DEFINE_CONST_OBJ_TYPE(
  area512_micropython_widget_text_view_type,
  MP_QSTR_WidgetTextView,
  MP_TYPE_FLAG_NONE,
  make_new,
  create_widget_text_view_object,
  locals_dict,
  &widget_text_view_locals_dictionary
);
