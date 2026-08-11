#ifndef MICROPYTHON_TI_COLLECT_SUGGESTIONS_H
#define MICROPYTHON_TI_COLLECT_SUGGESTIONS_H

#include "micropython_ti_context.h"
#include "picoruby_ti_suggest.h"

int micropython_ti_collect_suggestions_at_cursor(
  MicroPythonTiContext *context,
  TSNode root,
  int cursor_byte_offset,
  TiSuggestionList *out
);

#endif
