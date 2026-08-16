#include "micropython_ti_eval_handlers.h"
#include "micropython_ti_builtin.h"
#include "micropython_ti_define_info.h"
#include "micropython_ti_t.h"
#include "micropython_ti_t_frame.h"

uint16_t
micropython_ti_handle_identifier(MicroPythonTiContext *context, TSNode node) {
  size_t name_byte_length;

  const uint8_t *name_bytes =
    micropython_ti_get_node_bytes(context, node, &name_byte_length);

  if (name_bytes) {
    uint8_t class_id =
      micropython_ti_get_builtin_class_id(name_bytes, name_byte_length);

    if (class_id != MICROPYTHON_TI_CLASS_NONE) {
      return micropython_ti_new_t(class_id, MICROPYTHON_TI_T_FLAG_STATIC, 0);
    }
  }

  uint16_t name_id;

  if (!micropython_ti_intern_node_name(context, node, &name_id))
    return 0;

  uint8_t user_class_id = micropython_ti_get_defined_class_id(name_id);

  if (user_class_id != MICROPYTHON_TI_CLASS_NONE) {
    return micropython_ti_new_t(
      user_class_id,
      MICROPYTHON_TI_T_FLAG_DEFINED_CLASS | MICROPYTHON_TI_T_FLAG_STATIC,
      0
    );
  }

  return micropython_ti_get_value_t(name_id);
}
