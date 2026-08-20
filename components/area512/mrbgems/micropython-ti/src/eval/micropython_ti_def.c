#include "micropython_ti_def.h"
#include "micropython_ti_builtin_database.h"
#include "micropython_ti_define_info.h"
#include "micropython_ti_eval.h"
#include "micropython_ti_t.h"
#include "micropython_ti_t_frame.h"
#include "micropython_ti_type_hint.h"
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

  TSNode name_node = parameter_name_node(parameter);

  if (micropython_ti_node_is_self_identifier(context, name_node))
    return 1;

  size_t byte_length;

  const uint8_t *bytes =
    micropython_ti_get_node_bytes(context, name_node, &byte_length);

  return bytes && byte_length == 3 && memcmp(bytes, "cls", 3) == 0;
}

static int
is_initializer_name(const MicroPythonTiContext *context, TSNode name_node) {
  size_t name_byte_length;

  const uint8_t *name_bytes =
    micropython_ti_get_node_bytes(context, name_node, &name_byte_length);

  return name_bytes &&
    name_byte_length == 8 &&
    memcmp(name_bytes, "__init__", 8) == 0;
}

static void
bind_self_parameter(MicroPythonTiContext *context, TSNode parameters) {
  if (ts_node_is_null(parameters) || ts_node_named_child_count(parameters) == 0)
    return;

  TSNode first_parameter = ts_node_named_child(parameters, 0);
  TSNode first_parameter_name_node = parameter_name_node(first_parameter);

  if (
    !micropython_ti_node_is_self_identifier(context, first_parameter_name_node)
  ) {

    return;
  }

  uint16_t self_name_id;

  if (
    !micropython_ti_intern_node_name(
      context,
      first_parameter_name_node,
      &self_name_id
    )
  ) {

    context->failed = 1;
    return;
  }

  uint16_t self_t_node_index =
    micropython_ti_new_t(
      context->current_class_id,
      MICROPYTHON_TI_T_FLAG_DEFINED_CLASS,
      0
    );

  if (
    !micropython_ti_set_value_t(
      context->current_class_name_id,
      context->current_define_name_id,
      self_name_id,
      self_t_node_index
    )
  ) {

    context->failed = 1;
  }
}

static uint16_t
make_define_arg_t_node_index(
  MicroPythonTiContext *context,
  TSNode parameter
) {

  uint16_t default_value_t_node_index =
    micropython_ti_eval_expression(
      context,
      ts_node_child_by_field_name(parameter, "value", 5),
      0
    );

  uint16_t type_hint_t_node_index =
    micropython_ti_make_type_hint_t_node_index(
      context,
      ts_node_child_by_field_name(parameter, "type", 4)
    );

  if (type_hint_t_node_index != 0)
    return type_hint_t_node_index;

  if (default_value_t_node_index != 0)
    return default_value_t_node_index;

  return micropython_ti_new_t(MICROPYTHON_TI_CLASS_UNTYPED, 0, 0);
}

static void
append_define_arg(
  MicroPythonTiContext *context,
  MicroPythonTiDefineInfo *define_info,
  TSNode name_node,
  uint16_t t_node_index,
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

  uint8_t define_arg_index = define_info->define_arg_count++;

  define_info->define_arg_name_ids[define_arg_index] = name_id;
  define_info->define_arg_t_node_indexes[define_arg_index] = t_node_index;
  define_info->define_arg_kinds[define_arg_index] = (uint8_t)define_arg_kind;
}

static void
bind_define_args(
  MicroPythonTiContext *context,
  const MicroPythonTiDefineInfo *define_info
) {

  if (!define_info)
    return;

  for (
    int define_arg_index = 0;
    define_arg_index < define_info->define_arg_count;
    define_arg_index++
  ) {

    if (
      !micropython_ti_set_value_t(
        context->current_class_name_id,
        context->current_define_name_id,
        define_info->define_arg_name_ids[define_arg_index],
        define_info->define_arg_t_node_indexes[define_arg_index]
      )
    ) {

      context->failed = 1;
    }
  }
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
    uint32_t parameter_index = first_parameter_index;
    parameter_index < parameter_count;
    parameter_index++
  ) {

    TSNode parameter = ts_node_named_child(parameters, parameter_index);
    const char *parameter_type = ts_node_type(parameter);

    uint16_t define_arg_t_node_index =
      make_define_arg_t_node_index(context, parameter);

    if (
      strcmp(parameter_type, "identifier") == 0 ||
      strcmp(parameter_type, "typed_parameter") == 0 ||
      strcmp(parameter_type, "default_parameter") == 0 ||
      strcmp(parameter_type, "typed_default_parameter") == 0
    ) {

      append_define_arg(
        context,
        define_info,
        parameter_name_node(parameter),
        define_arg_t_node_index,
        MICROPYTHON_TI_DEFINE_ARG_REQUIRED
      );

      continue;
    }

    if (strcmp(parameter_type, "list_splat_pattern") == 0) {
      append_define_arg(
        context,
        define_info,
        parameter_name_node(parameter),
        define_arg_t_node_index,
        MICROPYTHON_TI_DEFINE_ARG_REST
      );

      continue;
    }

    if (strcmp(parameter_type, "dictionary_splat_pattern") == 0) {
      append_define_arg(
        context,
        define_info,
        parameter_name_node(parameter),
        define_arg_t_node_index,
        MICROPYTHON_TI_DEFINE_ARG_KEYWORD_REST
      );
    }
  }
}

void
micropython_ti_eval_def(MicroPythonTiContext *context, TSNode node) {
  TSNode name_node = ts_node_child_by_field_name(node, "name", 4);
  uint16_t name_id;

  if (!micropython_ti_intern_node_name(context, name_node, &name_id)) {
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

  uint16_t outer_define_name_id = context->current_define_name_id;
  context->current_define_name_id = name_id;

  set_define_args(
    context,
    define_info,
    ts_node_child_by_field_name(node, "parameters", 10)
  );

  if (!context->is_preload_source || is_initializer_name(context, name_node)) {
    bind_define_args(context, define_info);

    bind_self_parameter(
      context,
      ts_node_child_by_field_name(node, "parameters", 10)
    );

    micropython_ti_eval_block(
      context,
      ts_node_child_by_field_name(node, "body", 4),
      0
    );
  }

  context->current_define_name_id = outer_define_name_id;

  uint16_t return_t_node_index =
    micropython_ti_make_type_hint_t_node_index(
      context,
      ts_node_child_by_field_name(node, "return_type", 11)
    );

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
