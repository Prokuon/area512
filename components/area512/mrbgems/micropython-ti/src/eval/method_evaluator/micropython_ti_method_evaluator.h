#ifndef MICROPYTHON_TI_METHOD_EVALUATOR_H
#define MICROPYTHON_TI_METHOD_EVALUATOR_H

#include "micropython_ti_builtin.h"
#include "micropython_ti_context.h"
#include <stdint.h>

uint16_t
micropython_ti_eval_call(MicroPythonTiContext *context, TSNode node, int depth);

uint16_t micropython_ti_make_method_return_t_node_index(
  const MicroPythonTiBuiltinMethod *builtin_method
);

#endif
