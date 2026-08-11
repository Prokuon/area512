#ifndef MICROPYTHON_TI_EVAL_H
#define MICROPYTHON_TI_EVAL_H

#include "micropython_ti_context.h"
#include "picoruby_ti_source.h"
#include <stdint.h>

#define MICROPYTHON_TI_EVAL_DEPTH_LIMIT 64

int micropython_ti_evaluate_sources(
  const TiSourceList *sources,
  TiDiagnosticList *diagnostics
);
uint16_t micropython_ti_eval_block(
  MicroPythonTiContext *context,
  TSNode block,
  int depth
);
uint16_t micropython_ti_eval_expression(
  MicroPythonTiContext *context,
  TSNode node,
  int depth
);
void micropython_ti_eval_node(MicroPythonTiContext *context, TSNode node);

#endif
