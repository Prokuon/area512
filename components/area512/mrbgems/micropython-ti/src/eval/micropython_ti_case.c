#include "micropython_ti_case.h"
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

static uint16_t
eval_case_clause(MicroPythonTiContext *context, TSNode case_clause, int depth) {
  TSNode consequence_node =
    ts_node_child_by_field_name(case_clause, "consequence", 11);

  uint32_t child_count = ts_node_named_child_count(case_clause);

  for (uint32_t child_index = 0; child_index < child_count; child_index++) {
    TSNode child = ts_node_named_child(case_clause, child_index);

    if (!ts_node_eq(child, consequence_node))
      micropython_ti_eval_node(context, child);
  }

  return eval_block_or_none(context, consequence_node, depth);
}

static int
is_wildcard_case_clause(
  const MicroPythonTiContext *context,
  TSNode case_clause
) {

  if (!ts_node_is_null(ts_node_child_by_field_name(case_clause, "guard", 5)))
    return 0;

  int pattern_count = 0;
  int has_wildcard_pattern = 0;

  uint32_t child_count = ts_node_named_child_count(case_clause);

  for (uint32_t child_index = 0; child_index < child_count; child_index++) {
    TSNode child = ts_node_named_child(case_clause, child_index);

    if (!micropython_ti_node_type_equals(child, "case_pattern"))
      continue;

    pattern_count++;

    size_t byte_length;

    const uint8_t *bytes =
      micropython_ti_get_node_bytes(context, child, &byte_length);

    if (bytes && byte_length == 1 && bytes[0] == '_')
      has_wildcard_pattern = 1;
  }

  return pattern_count == 1 && has_wildcard_pattern;
}

uint16_t
micropython_ti_eval_case(
  MicroPythonTiContext *context,
  TSNode node,
  int depth
) {

  micropython_ti_eval_node(
    context,
    ts_node_child_by_field_name(node, "subject", 7)
  );

  TSNode body_node = ts_node_child_by_field_name(node, "body", 4);
  uint16_t result_t_node_index = 0;
  int has_wildcard_clause = 0;

  uint32_t child_count = ts_node_named_child_count(body_node);

  for (uint32_t child_index = 0; child_index < child_count; child_index++) {
    TSNode case_clause = ts_node_named_child(body_node, child_index);

    if (!micropython_ti_node_type_equals(case_clause, "case_clause"))
      continue;

    if (is_wildcard_case_clause(context, case_clause))
      has_wildcard_clause = 1;

    result_t_node_index =
      micropython_ti_make_union(
        result_t_node_index,
        eval_case_clause(context, case_clause, depth + 1)
      );
  }

  if (!has_wildcard_clause) {
    result_t_node_index =
      micropython_ti_make_union(
        result_t_node_index,
        micropython_ti_new_t(MICROPYTHON_TI_CLASS_NONETYPE, 0, 0)
      );
  }

  return result_t_node_index;
}
