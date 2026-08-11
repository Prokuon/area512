#include "micropython_ti_return.h"
#include "micropython_ti_builtin_database.h"
#include "micropython_ti_eval.h"
#include "micropython_ti_t.h"

uint16_t
micropython_ti_eval_return(
  MicroPythonTiContext *context,
  TSNode node,
  int depth
) {

  uint16_t return_t_node_index =
    micropython_ti_new_t(MICROPYTHON_TI_CLASS_NONETYPE, 0, 0);

  uint32_t child_count = ts_node_named_child_count(node);

  if (child_count > 0) {
    return_t_node_index = 0;

    for (uint32_t child_index = 0; child_index < child_count; child_index++) {
      uint16_t child_t_node_index =
        micropython_ti_eval_expression(
          context,
          ts_node_named_child(node, child_index),
          depth + 1
        );

      return_t_node_index =
        micropython_ti_make_union(return_t_node_index, child_t_node_index);
    }
  }

  context->return_t_node_index =
    micropython_ti_make_union(
      context->return_t_node_index,
      return_t_node_index
    );

  return return_t_node_index;
}
