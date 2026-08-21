#include "micropython_ti_class.h"
#include "micropython_ti_builtin_database.h"
#include "micropython_ti_define_info.h"
#include "micropython_ti_t.h"
#include "micropython_ti_t_frame.h"
#include <stdint.h>

void
micropython_ti_eval_class(MicroPythonTiContext *context, TSNode node) {
  uint16_t name_id;

  if (
    !micropython_ti_intern_node_name(
      context,
      ts_node_child_by_field_name(node, "name", 4),
      &name_id
    )
  ) {

    context->failed = 1;
    return;
  }

  uint16_t define_row = micropython_ti_calculate_row(node);

  MicroPythonTiDefineInfo *define_info =
    micropython_ti_set_define_info(name_id, 0, define_row, 1);

  if (!define_info)
    return;

  uint16_t class_t_node_index =
    micropython_ti_new_t(
      MICROPYTHON_TI_CLASS_TYPE,
      MICROPYTHON_TI_T_FLAG_STATIC,
      0
    );

  if (
    class_t_node_index == 0 ||
    !micropython_ti_set_value_t(
      context->current_class_name_id,
      context->current_define_name_id,
      name_id,
      class_t_node_index
    )
  )
    context->failed = 1;
}
