#ifndef MICROPYTHON_TI_EVAL_HANDLERS_H
#define MICROPYTHON_TI_EVAL_HANDLERS_H

#include "micropython_ti_context.h"
#include <stdint.h>

uint16_t
micropython_ti_handle_identifier(MicroPythonTiContext *context, TSNode node);

#endif
