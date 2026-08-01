#ifndef AREA512_TI_DIAGNOSTIC_H
#define AREA512_TI_DIAGNOSTIC_H

#include <prism.h>

#define TI_MAX_DIAGNOSTICS 64
#define TI_DIAGNOSTIC_MESSAGE_CAPACITY 256

typedef struct {
  int start_byte_offset;
  int end_byte_offset;
  const char *message;
} TiDiagnostic;

typedef struct {
  TiDiagnostic items[TI_MAX_DIAGNOSTICS];
  int count;
} TiDiagnosticList;

struct TiContext;

void ti_add_diagnostic(
  struct TiContext *context,
  pm_location_t location,
  const char *message
);

int ti_fill_diagnostics(
  const char *source,
  int source_byte_length,
  TiDiagnosticList *out
);

#endif
