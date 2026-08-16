#include "micropython_ti_diagnostic.h"
#include "micropython_ti_context.h"
#include "micropython_ti_eval.h"
#include "picoruby_ti_arena.h"
#include <stddef.h>
#include <string.h>

void
micropython_ti_add_diagnostic(
  MicroPythonTiContext *context,
  int start_byte_offset,
  int end_byte_offset,
  const char *message
) {

  if (
    !context ||
    context->round != 2 ||
    !context->diagnostics ||
    !message ||
    context->diagnostics->count >= TI_DIAGNOSTIC_CAPACITY
  ) {

    return;
  }

  size_t message_byte_length = strlen(message);

  char *message_copy =
    ti_allocate_from_arena(message_byte_length + 1);

  if (!message_copy) {
    context->failed = 1;
    return;
  }

  memcpy(message_copy, message, message_byte_length + 1);

  TiDiagnostic *diagnostic =
    &context->diagnostics->items[context->diagnostics->count++];

  diagnostic->start_byte_offset = start_byte_offset;
  diagnostic->end_byte_offset = end_byte_offset;
  diagnostic->message = message_copy;
}

int
micropython_ti_fill_diagnostics(
  const TiSourceList *sources,
  TiDiagnosticList *out
) {

  if (out)
    memset(out, 0, sizeof(*out));

  if (!sources || !out)
    return 0;

  if (!micropython_ti_evaluate_sources(sources, out)) {
    memset(out, 0, sizeof(*out));
    return 0;
  }

  return out->count;
}
