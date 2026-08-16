#ifndef MICROPYTHON_TI_RETURN_H
#define MICROPYTHON_TI_RETURN_H

#include "micropython_ti_context.h"
#include <stdint.h>

uint16_t micropython_ti_eval_return(
  MicroPythonTiContext *context,
  TSNode node,
  int depth
);

#endif
