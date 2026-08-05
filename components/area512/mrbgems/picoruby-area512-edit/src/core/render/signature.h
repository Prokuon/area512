#ifndef AREA512_EDIT_RENDER_SIGNATURE_H
#define AREA512_EDIT_RENDER_SIGNATURE_H

#include "core/editor.h"

int minimum_signature_value(int first, int second);
int count_signature_rows(
  const char *signature,
  int signature_byte_length
);
int calculate_maximum_signature_horizontal_scroll_column_count(
  Vim *vim,
  const char *signature,
  int signature_byte_length,
  int signature_row_count
);
int draw_signature_rows(
  Vim *vim,
  int screen_row,
  int rows_available,
  const char *signature,
  int signature_byte_length,
  int name_byte_length,
  const char *class_name,
  int selected,
  int popup_horizontal_scroll_column_count
);

#endif
