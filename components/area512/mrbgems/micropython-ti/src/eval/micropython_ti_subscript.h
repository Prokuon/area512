#ifndef MICROPYTHON_TI_SUBSCRIPT_H
#define MICROPYTHON_TI_SUBSCRIPT_H

#include "micropython_ti_context.h"
#include <stdint.h>

uint16_t micropython_ti_make_sequence(
  MicroPythonTiContext *context,
  TSNode node,
  int depth,
  uint8_t class_id
);
uint16_t micropython_ti_make_comprehension_sequence(
  MicroPythonTiContext *context,
  TSNode node,
  int depth,
  uint8_t class_id
);
uint16_t micropython_ti_eval_subscript(
  MicroPythonTiContext *context,
  TSNode node,
  int depth
);

#endif
