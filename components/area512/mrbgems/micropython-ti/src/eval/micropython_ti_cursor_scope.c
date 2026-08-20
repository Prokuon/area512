#include "micropython_ti_cursor_scope.h"
#include "micropython_ti_builtin_database.h"
#include "micropython_ti_define_info.h"
#include "micropython_ti_eval.h"

typedef struct {
  uint32_t cursor_byte_offset;
  TSNode enclosing_class_node;
  uint32_t enclosing_class_byte_length;
  TSNode enclosing_define_node;
  uint32_t enclosing_define_byte_length;
} CursorScopeSearch;

static void
find_enclosing_scope_nodes(TSNode node, int depth, CursorScopeSearch *search) {
  if (depth > MICROPYTHON_TI_EVAL_DEPTH_LIMIT)
    return;

  uint32_t start_byte_offset = ts_node_start_byte(node);
  uint32_t end_byte_offset = ts_node_end_byte(node);

  if (
    search->cursor_byte_offset < start_byte_offset ||
    search->cursor_byte_offset > end_byte_offset
  )
    return;

  uint32_t byte_length = end_byte_offset - start_byte_offset;

  if (
    micropython_ti_node_type_equals(node, "class_definition") &&
    (
      ts_node_is_null(search->enclosing_class_node) ||
      byte_length < search->enclosing_class_byte_length
    )
  ) {

    search->enclosing_class_node = node;
    search->enclosing_class_byte_length = byte_length;
  }

  if (
    micropython_ti_node_type_equals(node, "function_definition") &&
    (
      ts_node_is_null(search->enclosing_define_node) ||
      byte_length < search->enclosing_define_byte_length
    )
  ) {

    search->enclosing_define_node = node;
    search->enclosing_define_byte_length = byte_length;
  }

  uint32_t child_count = ts_node_named_child_count(node);

  for (uint32_t child_index = 0; child_index < child_count; child_index++)
    find_enclosing_scope_nodes(
      ts_node_named_child(node, child_index),
      depth + 1,
      search
    );
}

void
micropython_ti_set_context_scope_at_cursor(
  MicroPythonTiContext *context,
  TSNode root,
  int cursor_byte_offset
) {

  context->current_class_name_id = 0;
  context->current_class_id = MICROPYTHON_TI_CLASS_NONE;
  context->current_define_name_id = 0;

  CursorScopeSearch search = {
    .cursor_byte_offset = (uint32_t)cursor_byte_offset,
  };

  find_enclosing_scope_nodes(root, 0, &search);

  if (!ts_node_is_null(search.enclosing_class_node)) {
    uint16_t class_name_id;

    if (
      micropython_ti_intern_node_name(
        context,
        ts_node_child_by_field_name(search.enclosing_class_node, "name", 4),
        &class_name_id
      )
    ) {

      context->current_class_name_id = class_name_id;

      context->current_class_id =
        micropython_ti_get_defined_class_id(class_name_id);
    }
  }

  if (ts_node_is_null(search.enclosing_define_node))
    return;

  uint16_t define_name_id;

  if (
    micropython_ti_intern_node_name(
      context,
      ts_node_child_by_field_name(search.enclosing_define_node, "name", 4),
      &define_name_id
    )
  ) {

    context->current_define_name_id = define_name_id;
  }
}
