#ifndef MICROPYTHON_TI_BIND_H
#define MICROPYTHON_TI_BIND_H

#include "micropython_ti_context.h"
#include <stdint.h>

uint16_t micropython_ti_bind_scalar_assignment(
  MicroPythonTiContext *context,
  TSNode assignment_node,
  int depth
);

#endif
