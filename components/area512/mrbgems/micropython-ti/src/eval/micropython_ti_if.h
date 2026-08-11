#ifndef MICROPYTHON_TI_IF_H
#define MICROPYTHON_TI_IF_H

#include "micropython_ti_context.h"
#include <stdint.h>

uint16_t
micropython_ti_eval_if(MicroPythonTiContext *context, TSNode node, int depth);

#endif
