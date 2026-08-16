#ifndef MICROPYTHON_TI_CASE_H
#define MICROPYTHON_TI_CASE_H

#include "micropython_ti_context.h"
#include <stdint.h>

uint16_t
micropython_ti_eval_case(MicroPythonTiContext *context, TSNode node, int depth);

#endif
