#include "area512_ti_diagnostic.h"
#include "area512_ti_arena.h"
#include "area512_ti_context.h"
#include "area512_ti_eval.h"
#include <stddef.h>
#include <string.h>

void
ti_add_diagnostic(
  TiContext *context,
  pm_location_t location,
  const char *message
) {

  if (
    !context ||
    context->round != 2 ||
    !context->diagnostics ||
    !message ||
    context->diagnostics->count >= TI_MAX_DIAGNOSTICS
  ) {
    return;
  }

  TiDiagnostic *diagnostic =
    &context->diagnostics->items[context->diagnostics->count++];

  diagnostic->start_byte_offset =
    (int)(location.start - context->source);
  diagnostic->end_byte_offset =
    (int)(location.end - context->source);

  size_t message_byte_length = strlen(message);

  char *message_copy =
    ti_allocate_from_arena(message_byte_length + 1);

  if (!message_copy) {
    context->diagnostics->count--;
    context->failed = 1;
    return;
  }

  memcpy(message_copy, message, message_byte_length + 1);
  diagnostic->message = message_copy;
}

int
ti_fill_diagnostics(
  const char *source,
  int source_byte_length,
  TiDiagnosticList *out
) {

  if (out)
    memset(out, 0, sizeof(*out));

  if (!source || source_byte_length < 0 || !out)
    return 0;

  pm_parser_t parser;

  pm_parser_init(
    &parser,
    (const uint8_t *)source,
    (size_t)source_byte_length,
    NULL
  );

  pm_node_t *root = pm_parse(&parser);
  if (!root) {
    pm_parser_free(&parser);
    return 0;
  }

  TiContext context = {
    .parser = &parser,
    .source = (const uint8_t *)source,
    .source_length = (size_t)source_byte_length,
    .diagnostics = out,
  };

  ti_evaluation_loop(&context, root);

  pm_node_destroy(&parser, root);
  pm_parser_free(&parser);

  if (context.failed || ti_did_arena_overflow()) {
    memset(out, 0, sizeof(*out));
    return 0;
  }

  return out->count;
}
