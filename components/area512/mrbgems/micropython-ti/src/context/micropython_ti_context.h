#ifndef MICROPYTHON_TI_CONTEXT_H
#define MICROPYTHON_TI_CONTEXT_H

#include <stddef.h>
#include <stdint.h>
#include <tree_sitter/api.h>

#include "micropython_ti_diagnostic.h"

typedef struct MicroPythonTiContext {
  const char *source;
  int source_byte_length;
  uint16_t current_class_name_id;
  uint8_t current_class_id;
  uint16_t current_define_name_id;
  int is_preload_source;
  int round;
  int failed;
  TiDiagnosticList *diagnostics;
} MicroPythonTiContext;

int micropython_ti_intern_node_name(
  MicroPythonTiContext *context,
  TSNode node,
  uint16_t *name_id
);
const uint8_t *micropython_ti_get_node_bytes(
  const MicroPythonTiContext *context,
  TSNode node,
  size_t *byte_length
);
uint16_t micropython_ti_calculate_row(TSNode node);
int micropython_ti_node_type_equals(TSNode node, const char *node_type);
int micropython_ti_node_is_self_identifier(
  const MicroPythonTiContext *context,
  TSNode node
);

#endif
