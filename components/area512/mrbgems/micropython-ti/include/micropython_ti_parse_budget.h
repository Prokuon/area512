#ifndef MICROPYTHON_TI_PARSE_BUDGET_H
#define MICROPYTHON_TI_PARSE_BUDGET_H

#include <tree_sitter/api.h>

typedef struct {
  void (*start_parse_budget)(void);
  int (*is_parse_budget_exhausted)(void);
} MicroPythonTiParseBudget;

void micropython_ti_set_parse_budget(MicroPythonTiParseBudget parse_budget);

TSTree *micropython_ti_parse_source_within_budget(
  TSParser *parser,
  const char *source_bytes,
  int source_byte_length,
  int source_index
);

void micropython_ti_clear_parse_cancellation(void);
int micropython_ti_did_cancel_parse(void);
int micropython_ti_get_cancelled_source_index(void);

#endif
