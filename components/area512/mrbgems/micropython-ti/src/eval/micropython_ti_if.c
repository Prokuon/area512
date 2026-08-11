#include "micropython_ti_if.h"
#include "micropython_ti_builtin_database.h"
#include "micropython_ti_eval.h"
#include "micropython_ti_t.h"

static uint16_t
eval_block_or_none(MicroPythonTiContext *context, TSNode block, int depth) {
  uint16_t t_node_index = micropython_ti_eval_block(context, block, depth);

  if (t_node_index == 0)
    t_node_index = micropython_ti_new_t(MICROPYTHON_TI_CLASS_NONETYPE, 0, 0);

  return t_node_index;
}

uint16_t
micropython_ti_eval_if(MicroPythonTiContext *context, TSNode node, int depth) {
  micropython_ti_eval_expression(
    context,
    ts_node_child_by_field_name(node, "condition", 9),
    depth + 1
  );

  uint16_t result_t_node_index =
    eval_block_or_none(
      context,
      ts_node_child_by_field_name(node, "consequence", 11),
      depth + 1
    );

  uint16_t alternative_t_node_index = 0;
  int has_else_clause = 0;

  uint32_t child_count = ts_node_named_child_count(node);

  for (uint32_t child_index = 0; child_index < child_count; child_index++) {
    TSNode alternative_node = ts_node_named_child(node, child_index);

    if (micropython_ti_node_type_equals(alternative_node, "elif_clause")) {
      micropython_ti_eval_expression(
        context,
        ts_node_child_by_field_name(alternative_node, "condition", 9),
        depth + 1
      );

      alternative_t_node_index =
        micropython_ti_make_union(
          alternative_t_node_index,
          eval_block_or_none(
            context,
            ts_node_child_by_field_name(alternative_node, "consequence", 11),
            depth + 1
          )
        );

      continue;
    }

    if (micropython_ti_node_type_equals(alternative_node, "else_clause")) {
      has_else_clause = 1;

      alternative_t_node_index =
        micropython_ti_make_union(
          alternative_t_node_index,
          eval_block_or_none(
            context,
            ts_node_child_by_field_name(alternative_node, "body", 4),
            depth + 1
          )
        );
    }
  }

  if (!has_else_clause) {
    alternative_t_node_index =
      micropython_ti_make_union(
        alternative_t_node_index,
        micropython_ti_new_t(MICROPYTHON_TI_CLASS_NONETYPE, 0, 0)
      );
  }

  return micropython_ti_make_union(
    result_t_node_index,
    alternative_t_node_index
  );
}
