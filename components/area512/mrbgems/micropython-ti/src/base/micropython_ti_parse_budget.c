#include "micropython_ti_parse_budget.h"
#include <stdbool.h>

typedef struct {
  const char *source_bytes;
  uint32_t source_byte_length;
} SourceInput;

static void (*start_parse_budget)(void);
static int (*is_parse_budget_exhausted)(void);
static int parse_cancelled;
static int cancelled_source_index = -1;

static const char *
read_source_input(
  void *payload,
  uint32_t byte_index,
  TSPoint position,
  uint32_t *bytes_read
) {

  (void)position;

  const SourceInput *source_input = payload;

  if (byte_index >= source_input->source_byte_length) {
    *bytes_read = 0;
    return "";
  }

  *bytes_read = source_input->source_byte_length - byte_index;

  return source_input->source_bytes + byte_index;
}

static bool
cancel_parse_when_budget_exhausted(TSParseState *state) {
  (void)state;

  if (!is_parse_budget_exhausted || !is_parse_budget_exhausted())
    return false;

  parse_cancelled = 1;

  return true;
}

void
micropython_ti_set_parse_budget(MicroPythonTiParseBudget parse_budget) {
  start_parse_budget = parse_budget.start_parse_budget;
  is_parse_budget_exhausted = parse_budget.is_parse_budget_exhausted;
}

TSTree *
micropython_ti_parse_source_within_budget(
  TSParser *parser,
  const char *source_bytes,
  int source_byte_length,
  int source_index
) {

  SourceInput source_input = {source_bytes, (uint32_t)source_byte_length};

  if (start_parse_budget)
    start_parse_budget();

  TSTree *tree =
    ts_parser_parse_with_options(
      parser,
      NULL,
      (TSInput){&source_input, read_source_input, TSInputEncodingUTF8, NULL},
      (TSParseOptions){NULL, cancel_parse_when_budget_exhausted}
    );

  if (!tree && parse_cancelled)
    cancelled_source_index = source_index;

  return tree;
}

void
micropython_ti_clear_parse_cancellation(void) {
  parse_cancelled = 0;
  cancelled_source_index = -1;
}

int
micropython_ti_did_cancel_parse(void) {
  return parse_cancelled;
}

int
micropython_ti_get_cancelled_source_index(void) {
  return cancelled_source_index;
}
