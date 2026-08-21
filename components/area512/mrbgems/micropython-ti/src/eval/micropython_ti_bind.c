#include "micropython_ti_bind.h"
#include "micropython_ti_eval.h"
#include "micropython_ti_t_frame.h"
#include "micropython_ti_type_hint.h"

static int
is_self_attribute_target(
  const MicroPythonTiContext *context,
  TSNode target_node
) {

  if (!micropython_ti_node_type_equals(target_node, "attribute"))
    return 0;

  return micropython_ti_node_is_self_identifier(
    context,
    ts_node_child_by_field_name(target_node, "object", 6)
  );
}

uint16_t
micropython_ti_bind_scalar_assignment(
  MicroPythonTiContext *context,
  TSNode assignment_node,
  int depth
) {

  TSNode target_node = ts_node_child_by_field_name(assignment_node, "left", 4);

  TSNode type_hint_node =
    ts_node_child_by_field_name(assignment_node, "type", 4);

  TSNode value_node = ts_node_child_by_field_name(assignment_node, "right", 5);

  uint16_t value_t_node_index =
    micropython_ti_eval_expression(context, value_node, depth + 1);

  uint16_t type_hint_t_node_index =
    micropython_ti_make_type_hint_t_node_index(context, type_hint_node);

  uint16_t bound_t_node_index = type_hint_t_node_index;

  if (bound_t_node_index == 0)
    bound_t_node_index = value_t_node_index;

  if (is_self_attribute_target(context, target_node)) {
    uint16_t attribute_name_id;

    if (
      !micropython_ti_intern_node_name(
        context,
        ts_node_child_by_field_name(target_node, "attribute", 9),
        &attribute_name_id
      )
    ) {

      context->failed = 1;
      return 0;
    }

    if (
      !micropython_ti_merge_instance_attribute_t(
        context->current_class_id,
        attribute_name_id,
        bound_t_node_index
      )
    ) {

      context->failed = 1;
    }

    return bound_t_node_index;
  }

  if (!micropython_ti_node_type_equals(target_node, "identifier"))
    return value_t_node_index;

  uint16_t name_id;

  if (!micropython_ti_intern_node_name(context, target_node, &name_id)) {
    context->failed = 1;
    return 0;
  }

  if (
    bound_t_node_index != 0 &&
    !micropython_ti_merge_value_t(
      context->current_class_name_id,
      context->current_define_name_id,
      name_id,
      bound_t_node_index
    )
  ) {

    context->failed = 1;
  }

  if (
    context->current_define_name_id == 0 &&
    !micropython_ti_merge_instance_attribute_t(
      context->current_class_id,
      name_id,
      bound_t_node_index
    )
  ) {

    context->failed = 1;
  }

  return bound_t_node_index;
}
