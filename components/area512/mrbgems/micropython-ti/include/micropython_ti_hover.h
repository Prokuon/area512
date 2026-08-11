#ifndef MICROPYTHON_TI_HOVER_H
#define MICROPYTHON_TI_HOVER_H

#include "picoruby_ti_hover.h"

int micropython_ti_find_hover_at_cursor(
  const TiSourceList *sources,
  int cursor_byte_offset,
  TiHoverInfo *out
);

#endif
