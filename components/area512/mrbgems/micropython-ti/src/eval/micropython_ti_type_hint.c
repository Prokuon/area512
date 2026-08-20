#include "micropython_ti_type_hint.h"
#include "micropython_ti_builtin.h"
#include "micropython_ti_define_info.h"
#include "micropython_ti_eval.h"
#include "micropython_ti_t.h"

static TSNode
type_hint_expression_node(TSNode type_hint_node) {
  if (!micropython_ti_node_type_equals(type_hint_node, "type"))
    return type_hint_node;

  return ts_node_named_child(type_hint_node, 0);
}

static int
is_union_operator(TSNode expression_node) {
  if (!micropython_ti_node_type_equals(expression_node, "binary_operator"))
    return 0;

  return micropython_ti_node_type_equals(
    ts_node_child_by_field_name(expression_node, "operator", 8),
    "|"
  );
}

static uint16_t make_type_hint_t_node_index(
  MicroPythonTiContext *context,
  TSNode type_hint_node,
  int depth
);

static uint16_t
make_union_type_hint_t_node_index(
  MicroPythonTiContext *context,
  TSNode union_operator_node,
  int depth
) {

  uint32_t union_member_count =
    ts_node_named_child_count(union_operator_node);

  uint16_t union_t_node_index = 0;

  for (
    uint32_t union_member_index = 0;
    union_member_index < union_member_count;
    union_member_index++
  ) {

    uint16_t union_member_t_node_index =
      make_type_hint_t_node_index(
        context,
        ts_node_named_child(union_operator_node, union_member_index),
        depth + 1
      );

    union_t_node_index =
      micropython_ti_make_union(union_t_node_index, union_member_t_node_index);
  }

  return union_t_node_index;
}

static uint16_t
make_type_hint_t_node_index(
  MicroPythonTiContext *context,
  TSNode type_hint_node,
  int depth
) {

  if (
    ts_node_is_null(type_hint_node) ||
    depth > MICROPYTHON_TI_EVAL_DEPTH_LIMIT
  ) {

    return 0;
  }

  TSNode expression_node = type_hint_expression_node(type_hint_node);

  if (ts_node_is_null(expression_node))
    return micropython_ti_new_t(MICROPYTHON_TI_CLASS_UNTYPED, 0, 0);

  if (is_union_operator(expression_node))
    return make_union_type_hint_t_node_index(context, expression_node, depth);

  if (micropython_ti_node_type_equals(expression_node, "none"))
    return micropython_ti_new_t(MICROPYTHON_TI_CLASS_NONETYPE, 0, 0);

  if (!micropython_ti_node_type_equals(expression_node, "identifier"))
    return micropython_ti_new_t(MICROPYTHON_TI_CLASS_UNTYPED, 0, 0);

  size_t name_byte_length;

  const uint8_t *name_bytes =
    micropython_ti_get_node_bytes(context, expression_node, &name_byte_length);

  if (name_bytes) {
    uint8_t builtin_class_id =
      micropython_ti_get_builtin_class_id(name_bytes, name_byte_length);

    if (builtin_class_id != MICROPYTHON_TI_CLASS_NONE)
      return micropython_ti_new_t(builtin_class_id, 0, 0);
  }

  uint16_t class_name_id;

  if (
    !micropython_ti_intern_node_name(
      context,
      expression_node,
      &class_name_id
    )
  ) {

    return micropython_ti_new_t(MICROPYTHON_TI_CLASS_UNTYPED, 0, 0);
  }

  uint8_t defined_class_id =
    micropython_ti_get_defined_class_id(class_name_id);

  if (defined_class_id != MICROPYTHON_TI_CLASS_NONE) {
    return micropython_ti_new_t(
      defined_class_id,
      MICROPYTHON_TI_T_FLAG_DEFINED_CLASS,
      0
    );
  }

  return 0;
}

uint16_t
micropython_ti_make_type_hint_t_node_index(
  MicroPythonTiContext *context,
  TSNode type_hint_node
) {

  return make_type_hint_t_node_index(context, type_hint_node, 0);
}
