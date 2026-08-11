#ifndef MICROPYTHON_TI_DIAGNOSTIC_H
#define MICROPYTHON_TI_DIAGNOSTIC_H

#include "picoruby_ti_diagnostic.h"

struct MicroPythonTiContext;

void micropython_ti_add_diagnostic(
  struct MicroPythonTiContext *context,
  int start_byte_offset,
  int end_byte_offset,
  const char *message
);

int micropython_ti_fill_diagnostics(
  const TiSourceList *sources,
  TiDiagnosticList *out
);

#endif
