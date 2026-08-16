#include "micropython_ti_context.h"
#include "micropython_ti_name.h"
#include <string.h>

int
micropython_ti_intern_node_name(
  MicroPythonTiContext *context,
  TSNode node,
  uint16_t *name_id
) {

  if (!context)
    return 0;

  size_t byte_length;

  const uint8_t *bytes =
    micropython_ti_get_node_bytes(context, node, &byte_length);

  if (!micropython_ti_intern_name(bytes, byte_length, name_id)) {
    context->failed = 1;
    return 0;
  }

  return 1;
}

const uint8_t *
micropython_ti_get_node_bytes(
  const MicroPythonTiContext *context,
  TSNode node,
  size_t *byte_length
) {

  *byte_length = 0;

  if (!context || !context->source || ts_node_is_null(node))
    return NULL;

  uint32_t start_byte_offset = ts_node_start_byte(node);
  uint32_t end_byte_offset = ts_node_end_byte(node);

  if (
    end_byte_offset <= start_byte_offset ||
    end_byte_offset > (uint32_t)context->source_byte_length
  ) {

    return NULL;
  }

  *byte_length = end_byte_offset - start_byte_offset;

  return (const uint8_t *)context->source + start_byte_offset;
}

uint16_t
micropython_ti_calculate_row(TSNode node) {
  if (ts_node_is_null(node))
    return 1;

  uint32_t row = ts_node_start_point(node).row;

  if (row >= UINT16_MAX)
    return UINT16_MAX;

  return (uint16_t)(row + 1);
}

int
micropython_ti_node_type_equals(TSNode node, const char *node_type) {
  return !ts_node_is_null(node) &&
    strcmp(ts_node_type(node), node_type) == 0;
}
