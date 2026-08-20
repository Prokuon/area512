#ifndef MICROPYTHON_TI_CURSOR_SCOPE_H
#define MICROPYTHON_TI_CURSOR_SCOPE_H

#include "micropython_ti_context.h"

void micropython_ti_set_context_scope_at_cursor(
  MicroPythonTiContext *context,
  TSNode root,
  int cursor_byte_offset
);

#endif
