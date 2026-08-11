#include "micropython_ti_bind.h"
#include "micropython_ti_eval.h"
#include "micropython_ti_t_frame.h"

uint16_t
micropython_ti_bind_scalar_assignment(
  MicroPythonTiContext *context,
  TSNode target_node,
  TSNode value_node,
  int depth
) {

  if (!micropython_ti_node_type_equals(target_node, "identifier"))
    return micropython_ti_eval_expression(context, value_node, depth + 1);

  uint16_t name_id;

  if (!micropython_ti_intern_node_name(context, target_node, &name_id)) {
    context->failed = 1;
    return 0;
  }

  uint16_t t_node_index =
    micropython_ti_eval_expression(context, value_node, depth + 1);

  if (t_node_index != 0 && !micropython_ti_set_value_t(name_id, t_node_index))
    context->failed = 1;

  return t_node_index;
}
