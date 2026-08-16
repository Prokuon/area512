#include "micropython_ti_subscript.h"
#include "micropython_ti_builtin.h"
#include "micropython_ti_eval.h"
#include "micropython_ti_method_evaluator.h"
#include "micropython_ti_t.h"

static uint16_t
make_sequence_from_variants(uint16_t variants, uint8_t class_id) {
  if (variants == 0)
    variants = micropython_ti_new_t(MICROPYTHON_TI_CLASS_UNTYPED, 0, 0);

  return micropython_ti_new_t(class_id, 0, variants);
}

uint16_t
micropython_ti_make_sequence(
  MicroPythonTiContext *context,
  TSNode node,
  int depth,
  uint8_t class_id
) {

  uint16_t variants = 0;
  uint32_t element_count = ts_node_named_child_count(node);

  for (
    uint32_t element_index = 0;
    element_index < element_count;
    element_index++
  ) {

    uint16_t variant_t_node_index =
      micropython_ti_eval_expression(
        context,
        ts_node_named_child(node, element_index),
        depth + 1
      );

    if (variant_t_node_index == 0)
      return make_sequence_from_variants(0, class_id);

    variants = micropython_ti_make_union(variants, variant_t_node_index);

    if (variants == 0)
      return make_sequence_from_variants(0, class_id);
  }

  return make_sequence_from_variants(variants, class_id);
}

uint16_t
micropython_ti_make_comprehension_sequence(
  MicroPythonTiContext *context,
  TSNode node,
  int depth,
  uint8_t class_id
) {

  uint32_t child_count = ts_node_named_child_count(node);

  for (uint32_t child_index = 1; child_index < child_count; child_index++) {
    TSNode clause = ts_node_named_child(node, child_index);

    if (micropython_ti_node_type_equals(clause, "for_in_clause")) {
      micropython_ti_eval_expression(
        context,
        ts_node_child_by_field_name(clause, "right", 5),
        depth + 1
      );

      continue;
    }

    if (micropython_ti_node_type_equals(clause, "if_clause")) {
      micropython_ti_eval_expression(
        context,
        ts_node_named_child(clause, 0),
        depth + 1
      );
    }
  }

  uint16_t variants =
    micropython_ti_eval_expression(
      context,
      ts_node_named_child(node, 0),
      depth + 1
    );

  return make_sequence_from_variants(variants, class_id);
}

uint16_t
micropython_ti_eval_subscript(
  MicroPythonTiContext *context,
  TSNode node,
  int depth
) {

  uint16_t receiver_t_node_index =
    micropython_ti_eval_expression(
      context,
      ts_node_child_by_field_name(node, "value", 5),
      depth + 1
    );

  micropython_ti_eval_expression(
    context,
    ts_node_child_by_field_name(node, "subscript", 9),
    depth + 1
  );

  const MicroPythonTiT *receiver_t_node =
    micropython_ti_get_t(receiver_t_node_index);

  if (!receiver_t_node || receiver_t_node->union_next != 0)
    return 0;

  if (
    (
      receiver_t_node->object_class_id == MICROPYTHON_TI_CLASS_LIST ||
      receiver_t_node->object_class_id == MICROPYTHON_TI_CLASS_TUPLE
    ) &&
    receiver_t_node->variants != 0
  ) {

    return receiver_t_node->variants;
  }

  const MicroPythonTiBuiltinMethod *builtin_method =
    micropython_ti_get_builtin_instance_method(
      receiver_t_node->object_class_id,
      (const uint8_t *)"__getitem__",
      sizeof("__getitem__") - 1
    );

  if (!builtin_method)
    return 0;

  return micropython_ti_make_method_return_t_node_index(builtin_method);
}
