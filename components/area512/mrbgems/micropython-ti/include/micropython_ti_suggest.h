#ifndef MICROPYTHON_TI_SUGGEST_H
#define MICROPYTHON_TI_SUGGEST_H

#include "picoruby_ti_suggest.h"

int micropython_ti_fill_suggestions_at_cursor(
  const TiSourceList *sources,
  int cursor_byte_offset,
  TiSuggestionList *out
);

#endif
