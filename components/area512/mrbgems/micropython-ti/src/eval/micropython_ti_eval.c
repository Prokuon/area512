#include "micropython_ti_eval.h"
#include "micropython_ti_bind.h"
#include "micropython_ti_builtin.h"
#include "micropython_ti_case.h"
#include "micropython_ti_class.h"
#include "micropython_ti_def.h"
#include "micropython_ti_define_info.h"
#include "micropython_ti_eval_handlers.h"
#include "micropython_ti_if.h"
#include "micropython_ti_method_evaluator.h"
#include "micropython_ti_name.h"
#include "micropython_ti_parse_budget.h"
#include "micropython_ti_return.h"
#include "micropython_ti_subscript.h"
#include "micropython_ti_t.h"
#include "micropython_ti_t_frame.h"
#include "picoruby_ti_arena.h"
#include <string.h>
#include <tree_sitter/tree-sitter-python.h>

static uint16_t
new_builtin_t(uint8_t class_id) {
  return micropython_ti_new_t(class_id, 0, 0);
}

static uint16_t
eval_named_children(MicroPythonTiContext *context, TSNode node, int depth) {
  uint32_t child_count = ts_node_named_child_count(node);

  if (child_count == 0)
    return new_builtin_t(MICROPYTHON_TI_CLASS_NONETYPE);

  uint16_t last_evaluated_t_node_index =
    new_builtin_t(MICROPYTHON_TI_CLASS_NONETYPE);

  for (uint32_t child_index = 0; child_index < child_count; child_index++)
    last_evaluated_t_node_index =
      micropython_ti_eval_expression(
        context,
        ts_node_named_child(node, child_index),
        depth + 1
      );

  return last_evaluated_t_node_index;
}

uint16_t
micropython_ti_eval_block(
  MicroPythonTiContext *context,
  TSNode block,
  int depth
) {

  if (
    ts_node_is_null(block) ||
    depth > MICROPYTHON_TI_EVAL_DEPTH_LIMIT ||
    context->failed
  ) {

    return 0;
  }

  return eval_named_children(context, block, depth);
}

static uint16_t
eval_string(const MicroPythonTiContext *context, TSNode node) {
  size_t byte_length;

  const uint8_t *bytes =
    micropython_ti_get_node_bytes(context, node, &byte_length);

  int is_bytes_literal = bytes && (bytes[0] == 'b' || bytes[0] == 'B');

  return new_builtin_t(
    is_bytes_literal ? MICROPYTHON_TI_CLASS_BYTES : MICROPYTHON_TI_CLASS_STR
  );
}

static uint16_t
eval_comparison(MicroPythonTiContext *context, TSNode node, int depth) {
  uint32_t child_count = ts_node_named_child_count(node);

  for (uint32_t child_index = 0; child_index < child_count; child_index++)
    micropython_ti_eval_expression(
      context,
      ts_node_named_child(node, child_index),
      depth + 1
    );

  return new_builtin_t(MICROPYTHON_TI_CLASS_BOOL);
}

static uint16_t
eval_boolean_operator(MicroPythonTiContext *context, TSNode node, int depth) {
  uint32_t child_count = ts_node_named_child_count(node);
  uint16_t result_t_node_index = 0;

  for (uint32_t child_index = 0; child_index < child_count; child_index++)
    result_t_node_index =
      micropython_ti_make_union(
        result_t_node_index,
        micropython_ti_eval_expression(
          context,
          ts_node_named_child(node, child_index),
          depth + 1
        )
      );

  return result_t_node_index;
}

static uint16_t
eval_conditional_expression(
  MicroPythonTiContext *context,
  TSNode node,
  int depth
) {

  uint16_t consequence_t_node_index =
    micropython_ti_eval_expression(
      context,
      ts_node_named_child(node, 0),
      depth + 1
    );

  micropython_ti_eval_expression(
    context,
    ts_node_named_child(node, 1),
    depth + 1
  );

  uint16_t alternative_t_node_index =
    micropython_ti_eval_expression(
      context,
      ts_node_named_child(node, 2),
      depth + 1
    );

  return micropython_ti_make_union(
    consequence_t_node_index,
    alternative_t_node_index
  );
}

static uint16_t
eval_attribute(MicroPythonTiContext *context, TSNode node, int depth) {
  TSNode object_node = ts_node_child_by_field_name(node, "object", 6);
  TSNode attribute_node = ts_node_child_by_field_name(node, "attribute", 9);

  uint16_t receiver_t_node_index =
    micropython_ti_eval_expression(context, object_node, depth + 1);

  const MicroPythonTiT *receiver_t_node =
    micropython_ti_get_t(receiver_t_node_index);

  if (!receiver_t_node || receiver_t_node->union_next != 0)
    return 0;

  size_t attribute_name_length;

  const uint8_t *attribute_name =
    micropython_ti_get_node_bytes(
      context,
      attribute_node,
      &attribute_name_length
    );

  if (!attribute_name)
    return 0;

  if ((receiver_t_node->t_flags & MICROPYTHON_TI_T_FLAG_DEFINED_CLASS) != 0) {
    uint16_t attribute_name_id;

    if (
      !micropython_ti_intern_node_name(
        context,
        attribute_node,
        &attribute_name_id
      )
    ) {

      return 0;
    }

    if ((receiver_t_node->t_flags & MICROPYTHON_TI_T_FLAG_STATIC) == 0) {
      uint16_t instance_attribute_t_node_index =
        micropython_ti_get_instance_attribute_t(
          receiver_t_node->object_class_id,
          attribute_name_id
        );

      if (instance_attribute_t_node_index != 0)
        return instance_attribute_t_node_index;
    }

    return micropython_ti_get_method_t(
      receiver_t_node->object_class_id,
      attribute_name_id
    );
  }

  const MicroPythonTiBuiltinMethod *builtin_method;

  if ((receiver_t_node->t_flags & MICROPYTHON_TI_T_FLAG_STATIC) != 0) {
    builtin_method =
      micropython_ti_get_builtin_static_method(
        receiver_t_node->object_class_id,
        attribute_name,
        attribute_name_length
      );
  } else {
    builtin_method =
      micropython_ti_get_builtin_instance_method(
        receiver_t_node->object_class_id,
        attribute_name,
        attribute_name_length
      );
  }

  if (!builtin_method)
    return 0;

  return micropython_ti_make_method_return_t_node_index(builtin_method);
}

uint16_t
micropython_ti_eval_expression(
  MicroPythonTiContext *context,
  TSNode node,
  int depth
) {

  if (
    ts_node_is_null(node) ||
    depth > MICROPYTHON_TI_EVAL_DEPTH_LIMIT ||
    context->failed
  ) {

    return 0;
  }

  const char *node_type = ts_node_type(node);

  if (strcmp(node_type, "integer") == 0)
    return new_builtin_t(MICROPYTHON_TI_CLASS_INT);

  if (strcmp(node_type, "float") == 0)
    return new_builtin_t(MICROPYTHON_TI_CLASS_FLOAT);

  if (
    strcmp(node_type, "string") == 0 ||
    strcmp(node_type, "concatenated_string") == 0
  )
    return eval_string(context, node);

  if (strcmp(node_type, "true") == 0 || strcmp(node_type, "false") == 0)
    return new_builtin_t(MICROPYTHON_TI_CLASS_BOOL);

  if (strcmp(node_type, "none") == 0)
    return new_builtin_t(MICROPYTHON_TI_CLASS_NONETYPE);

  if (strcmp(node_type, "list") == 0)
    return micropython_ti_make_sequence(
      context,
      node,
      depth,
      MICROPYTHON_TI_CLASS_LIST
    );

  if (strcmp(node_type, "tuple") == 0)
    return micropython_ti_make_sequence(
      context,
      node,
      depth,
      MICROPYTHON_TI_CLASS_TUPLE
    );

  if (strcmp(node_type, "list_comprehension") == 0)
    return micropython_ti_make_comprehension_sequence(
      context,
      node,
      depth,
      MICROPYTHON_TI_CLASS_LIST
    );

  if (
    strcmp(node_type, "set") == 0 ||
    strcmp(node_type, "set_comprehension") == 0
  )
    return new_builtin_t(MICROPYTHON_TI_CLASS_UNTYPED);

  if (
    strcmp(node_type, "dictionary") == 0 ||
    strcmp(node_type, "dictionary_comprehension") == 0
  )
    return new_builtin_t(MICROPYTHON_TI_CLASS_DICT);

  if (strcmp(node_type, "lambda") == 0)
    return new_builtin_t(MICROPYTHON_TI_CLASS_UNTYPED);

  if (strcmp(node_type, "identifier") == 0)
    return micropython_ti_handle_identifier(context, node);

  if (strcmp(node_type, "attribute") == 0)
    return eval_attribute(context, node, depth);

  if (strcmp(node_type, "call") == 0)
    return micropython_ti_eval_call(context, node, depth);

  if (strcmp(node_type, "subscript") == 0)
    return micropython_ti_eval_subscript(context, node, depth);

  if (strcmp(node_type, "parenthesized_expression") == 0)
    return micropython_ti_eval_expression(
      context,
      ts_node_named_child(node, 0),
      depth + 1
    );

  if (strcmp(node_type, "comparison_operator") == 0)
    return eval_comparison(context, node, depth);

  if (strcmp(node_type, "boolean_operator") == 0)
    return eval_boolean_operator(context, node, depth);

  if (strcmp(node_type, "conditional_expression") == 0)
    return eval_conditional_expression(context, node, depth);

  if (strcmp(node_type, "if_statement") == 0)
    return micropython_ti_eval_if(context, node, depth);

  if (strcmp(node_type, "match_statement") == 0)
    return micropython_ti_eval_case(context, node, depth);

  if (strcmp(node_type, "return_statement") == 0)
    return micropython_ti_eval_return(context, node, depth);

  if (strcmp(node_type, "assignment") == 0)
    return micropython_ti_bind_scalar_assignment(context, node, depth);

  if (strcmp(node_type, "block") == 0 || strcmp(node_type, "module") == 0)
    return micropython_ti_eval_block(context, node, depth);

  if (strcmp(node_type, "expression_statement") == 0)
    return eval_named_children(context, node, depth);

  micropython_ti_eval_node(context, node);

  return 0;
}

static TSNode
imported_binding_name(TSNode name_node, TSNode *imported_name_node) {
  *imported_name_node = name_node;

  if (!micropython_ti_node_type_equals(name_node, "aliased_import"))
    return name_node;

  TSNode alias_node = ts_node_child_by_field_name(name_node, "alias", 5);

  if (!ts_node_is_null(alias_node))
    *imported_name_node = alias_node;

  return ts_node_child_by_field_name(name_node, "name", 4);
}

static size_t
top_level_module_name_length(const uint8_t *module_name, size_t byte_length) {
  for (size_t index = 0; index < byte_length; index++) {
    if (module_name[index] == '.')
      return index;
  }

  return byte_length;
}

static void
bind_imported_module(MicroPythonTiContext *context, TSNode name_node) {
  if (ts_node_is_null(name_node))
    return;

  TSNode imported_name_node;

  TSNode module_name_node =
    imported_binding_name(name_node, &imported_name_node);

  size_t module_name_length;

  const uint8_t *module_name =
    micropython_ti_get_node_bytes(
      context,
      module_name_node,
      &module_name_length
    );

  if (!module_name)
    return;

  uint16_t name_id;

  if (!micropython_ti_intern_node_name(context, imported_name_node, &name_id))
    return;

  uint8_t module_class_id =
    micropython_ti_get_builtin_class_id(
      module_name,
      top_level_module_name_length(module_name, module_name_length)
    );

  uint16_t module_t_node_index =
    micropython_ti_new_t(module_class_id, MICROPYTHON_TI_T_FLAG_STATIC, 0);

  if (
    !micropython_ti_set_value_t(
      context->current_class_name_id,
      context->current_define_name_id,
      name_id,
      module_t_node_index
    )
  )
    context->failed = 1;
}

static void
eval_import(MicroPythonTiContext *context, TSNode node) {
  uint32_t child_count = ts_node_named_child_count(node);

  for (uint32_t child_index = 0; child_index < child_count; child_index++)
    bind_imported_module(context, ts_node_named_child(node, child_index));
}

static void
bind_imported_method(
  MicroPythonTiContext *context,
  uint8_t module_class_id,
  TSNode name_node
) {

  TSNode imported_name_node;

  TSNode method_name_node =
    imported_binding_name(name_node, &imported_name_node);

  size_t method_name_length;

  const uint8_t *method_name =
    micropython_ti_get_node_bytes(
      context,
      method_name_node,
      &method_name_length
    );

  if (!method_name)
    return;

  const MicroPythonTiBuiltinMethod *builtin_method =
    micropython_ti_get_builtin_static_method(
      module_class_id,
      method_name,
      method_name_length
    );

  if (!builtin_method)
    return;

  uint16_t name_id;

  if (!micropython_ti_intern_node_name(context, imported_name_node, &name_id))
    return;

  uint16_t return_t_node_index =
    micropython_ti_make_method_return_t_node_index(builtin_method);

  if (
    !micropython_ti_set_method_t(
      MICROPYTHON_TI_CLASS_NONE,
      name_id,
      return_t_node_index
    )
  ) {

    context->failed = 1;
  }
}

static void
eval_import_from(MicroPythonTiContext *context, TSNode node) {
  TSNode module_name_node =
    ts_node_child_by_field_name(node, "module_name", 11);

  size_t module_name_length;

  const uint8_t *module_name =
    micropython_ti_get_node_bytes(
      context,
      module_name_node,
      &module_name_length
    );

  if (!module_name)
    return;

  uint8_t module_class_id =
    micropython_ti_get_builtin_class_id(
      module_name,
      top_level_module_name_length(module_name, module_name_length)
    );

  uint32_t child_count = ts_node_named_child_count(node);

  for (uint32_t child_index = 0; child_index < child_count; child_index++) {
    TSNode name_node = ts_node_named_child(node, child_index);

    if (ts_node_eq(name_node, module_name_node))
      continue;

    bind_imported_method(context, module_class_id, name_node);
  }
}

static void
eval_assignment_children(MicroPythonTiContext *context, TSNode node) {
  uint32_t child_count = ts_node_named_child_count(node);

  for (uint32_t child_index = 0; child_index < child_count; child_index++) {
    TSNode child_node = ts_node_named_child(node, child_index);

    if (micropython_ti_node_type_equals(child_node, "assignment"))
      micropython_ti_eval_expression(context, child_node, 0);
  }
}

void
micropython_ti_eval_node(MicroPythonTiContext *context, TSNode node) {
  if (ts_node_is_null(node) || context->failed)
    return;

  const char *node_type = ts_node_type(node);

  if (strcmp(node_type, "function_definition") == 0) {
    micropython_ti_eval_def(context, node);
    return;
  }

  if (strcmp(node_type, "class_definition") == 0) {
    uint16_t class_name_id;

    if (
      !micropython_ti_intern_node_name(
        context,
        ts_node_child_by_field_name(node, "name", 4),
        &class_name_id
      )
    ) {

      return;
    }

    micropython_ti_eval_class(context, node);

    uint16_t outer_class_name_id = context->current_class_name_id;
    uint8_t outer_class_id = context->current_class_id;
    uint16_t outer_define_name_id = context->current_define_name_id;

    context->current_class_name_id = class_name_id;
    context->current_define_name_id = 0;

    context->current_class_id =
      micropython_ti_get_defined_class_id(class_name_id);

    if (!context->failed)
      micropython_ti_eval_node(
        context,
        ts_node_child_by_field_name(node, "body", 4)
      );

    context->current_class_name_id = outer_class_name_id;
    context->current_class_id = outer_class_id;
    context->current_define_name_id = outer_define_name_id;

    return;
  }

  if (strcmp(node_type, "import_statement") == 0) {
    if (!context->is_preload_source)
      eval_import(context, node);

    return;
  }

  if (strcmp(node_type, "import_from_statement") == 0) {
    if (!context->is_preload_source)
      eval_import_from(context, node);

    return;
  }

  if (strcmp(node_type, "expression_statement") == 0) {
    if (context->is_preload_source)
      eval_assignment_children(context, node);
    else
      micropython_ti_eval_expression(context, node, 0);

    return;
  }

  if (strcmp(node_type, "return_statement") == 0) {
    if (!context->is_preload_source)
      micropython_ti_eval_expression(context, node, 0);

    return;
  }

  if (strcmp(node_type, "assignment") == 0) {
    micropython_ti_eval_expression(context, node, 0);
    return;
  }

  uint32_t child_count = ts_node_named_child_count(node);

  for (uint32_t child_index = 0; child_index < child_count; child_index++)
    micropython_ti_eval_node(context, ts_node_named_child(node, child_index));
}

static int
initialize_evaluation(void) {
  ti_reset_arena();

  if (
    !micropython_ti_initialize_names() ||
    !micropython_ti_initialize_t() ||
    !micropython_ti_initialize_t_frame() ||
    !micropython_ti_initialize_define_infos()
  ) {

    return 0;
  }

  return 1;
}

static int
evaluate_source(
  const TiSource *source,
  int source_index,
  int is_preload_source,
  int round,
  TiDiagnosticList *diagnostics
) {

  if (
    !source ||
    source->source_byte_length < 0 ||
    (!source->source && source->source_byte_length > 0)
  ) {

    return 0;
  }

  const char *source_bytes = "";

  if (source->source)
    source_bytes = source->source;

  TSParser *parser = ts_parser_new();

  if (!parser || !ts_parser_set_language(parser, tree_sitter_python())) {
    if (parser)
      ts_parser_delete(parser);

    return 0;
  }

  TSTree *tree =
    micropython_ti_parse_source_within_budget(
      parser,
      source_bytes,
      source->source_byte_length,
      source_index
    );

  if (!tree) {
    ts_parser_delete(parser);
    return 0;
  }

  MicroPythonTiContext context = {
    .source = source_bytes,
    .source_byte_length = source->source_byte_length,
    .is_preload_source = is_preload_source,
    .round = round,
    .diagnostics = diagnostics,
  };

  micropython_ti_eval_node(&context, ts_tree_root_node(tree));

  ts_tree_delete(tree);
  ts_parser_delete(parser);

  return !context.failed;
}

int
micropython_ti_evaluate_sources(
  const TiSourceList *sources,
  TiDiagnosticList *diagnostics
) {

  micropython_ti_clear_parse_cancellation();

  if (
    !sources ||
    !sources->items ||
    sources->count <= 0 ||
    !initialize_evaluation()
  ) {

    return 0;
  }

  for (int round = 1; round <= 2; round++) {
    for (int source_index = 0; source_index < sources->count; source_index++) {
      TiDiagnosticList *source_diagnostics = NULL;
      int is_preload_source = source_index < sources->count - 1;

      if (round == 2 && !is_preload_source)
        source_diagnostics = diagnostics;

      if (
        !evaluate_source(
          &sources->items[source_index],
          source_index,
          is_preload_source,
          round,
          source_diagnostics
        )
      ) {

        return 0;
      }
    }
  }

  return !ti_did_arena_overflow();
}
