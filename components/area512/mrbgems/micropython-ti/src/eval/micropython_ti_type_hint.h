#ifndef MICROPYTHON_TI_TYPE_HINT_H
#define MICROPYTHON_TI_TYPE_HINT_H

#include "micropython_ti_context.h"
#include <stdint.h>

uint16_t micropython_ti_make_type_hint_t_node_index(
  MicroPythonTiContext *context,
  TSNode type_hint_node
);

#endif
