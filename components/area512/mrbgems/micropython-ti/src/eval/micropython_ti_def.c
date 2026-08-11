#include "micropython_ti_def.h"
#include "micropython_ti_builtin_database.h"
#include "micropython_ti_define_info.h"
#include "micropython_ti_eval.h"
#include "micropython_ti_t.h"
#include "micropython_ti_t_frame.h"
#include <stdint.h>
#include <string.h>

static TSNode
parameter_name_node(TSNode parameter) {
  TSNode name_node = ts_node_child_by_field_name(parameter, "name", 4);

  if (!ts_node_is_null(name_node))
    return name_node;

  if (micropython_ti_node_type_equals(parameter, "identifier"))
    return parameter;

  return ts_node_named_child(parameter, 0);
}

static int
is_receiver_parameter(
  const MicroPythonTiContext *context,
  TSNode parameter
) {

  size_t byte_length;

  const uint8_t *bytes =
    micropython_ti_get_node_bytes(
      context,
      parameter_name_node(parameter),
      &byte_length
    );

  if (!bytes)
    return 0;

  return (byte_length == 4 && memcmp(bytes, "self", 4) == 0) ||
         (byte_length == 3 && memcmp(bytes, "cls", 3) == 0);
}

static void
append_define_arg(
  MicroPythonTiContext *context,
  MicroPythonTiDefineInfo *define_info,
  TSNode name_node,
  MicroPythonTiDefineArgKind define_arg_kind
) {

  if (
    !define_info ||
    ts_node_is_null(name_node) ||
    define_info->define_arg_count >= MICROPYTHON_TI_DEFINE_ARG_CAPACITY
  ) {

    return;
  }

  uint16_t name_id;

  if (!micropython_ti_intern_node_name(context, name_node, &name_id)) {
    context->failed = 1;
    return;
  }

  uint8_t index = define_info->define_arg_count++;
  define_info->define_arg_name_ids[index] = name_id;
  define_info->define_arg_kinds[index] = define_arg_kind;
}

static void
set_define_args(
  MicroPythonTiContext *context,
  MicroPythonTiDefineInfo *define_info,
  TSNode parameters
) {

  if (!define_info || ts_node_is_null(parameters))
    return;

  define_info->define_arg_count = 0;

  uint32_t parameter_count = ts_node_named_child_count(parameters);

  uint32_t first_parameter_index = 0;

  if (
    context->current_class_name_id != 0 &&
    parameter_count > 0 &&
    is_receiver_parameter(context, ts_node_named_child(parameters, 0))
  ) {

    first_parameter_index = 1;
  }

  for (
    uint32_t index = first_parameter_index;
    index < parameter_count;
    index++
  ) {

    TSNode parameter = ts_node_named_child(parameters, index);
    const char *parameter_type = ts_node_type(parameter);

    if (
      strcmp(parameter_type, "identifier") == 0 ||
      strcmp(parameter_type, "typed_parameter") == 0
    ) {

      append_define_arg(
        context,
        define_info,
        parameter_name_node(parameter),
        MICROPYTHON_TI_DEFINE_ARG_REQUIRED
      );

      continue;
    }

    if (strcmp(parameter_type, "list_splat_pattern") == 0) {
      append_define_arg(
        context,
        define_info,
        parameter_name_node(parameter),
        MICROPYTHON_TI_DEFINE_ARG_REST
      );

      continue;
    }

    if (strcmp(parameter_type, "dictionary_splat_pattern") == 0) {
      append_define_arg(
        context,
        define_info,
        parameter_name_node(parameter),
        MICROPYTHON_TI_DEFINE_ARG_KEYWORD_REST
      );
    }
  }
}

void
micropython_ti_eval_def(MicroPythonTiContext *context, TSNode node) {
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
    micropython_ti_set_define_info(
      name_id,
      context->current_class_name_id,
      define_row,
      0
    );

  if (!define_info)
    return;

  set_define_args(
    context,
    define_info,
    ts_node_child_by_field_name(node, "parameters", 10)
  );

  uint16_t outer_return_t_node_index = context->return_t_node_index;
  context->return_t_node_index = 0;

  micropython_ti_eval_block(
    context,
    ts_node_child_by_field_name(node, "body", 4),
    0
  );

  uint16_t return_t_node_index = context->return_t_node_index;

  context->return_t_node_index = outer_return_t_node_index;

  if (return_t_node_index == 0) {
    return_t_node_index =
      micropython_ti_new_t(MICROPYTHON_TI_CLASS_NONETYPE, 0, 0);
  }

  if (return_t_node_index == 0)
    return;

  define_info->return_t_node_index = return_t_node_index;

  if (
    !micropython_ti_set_method_t(
      context->current_class_id,
      name_id,
      return_t_node_index
    )
  ) {

    context->failed = 1;
  }
}
