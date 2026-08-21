#include "micropython_ti_hover.h"
#include "micropython_ti_builtin.h"
#include "micropython_ti_collect_suggestions.h"
#include "micropython_ti_context.h"
#include "micropython_ti_cursor_scope.h"
#include "micropython_ti_define_info.h"
#include "micropython_ti_eval.h"
#include "micropython_ti_name.h"
#include "micropython_ti_parse_budget.h"
#include "micropython_ti_t.h"
#include "micropython_ti_t_frame.h"
#include "micropython_ti_type.h"
#include "picoruby_ti_arena.h"
#include <string.h>
#include <tree_sitter/tree-sitter-python.h>

typedef struct {
  uint32_t cursor_byte_offset;
  TSNode target;
  uint32_t target_byte_length;
} HoverTargetSearch;

static void
find_hover_target(TSNode node, int depth, HoverTargetSearch *search) {
  if (depth > MICROPYTHON_TI_EVAL_DEPTH_LIMIT)
    return;

  uint32_t start_byte_offset = ts_node_start_byte(node);
  uint32_t end_byte_offset = ts_node_end_byte(node);

  if (
    search->cursor_byte_offset < start_byte_offset ||
    search->cursor_byte_offset >= end_byte_offset
  )
    return;

  if (micropython_ti_node_type_equals(node, "identifier")) {
    uint32_t byte_length = end_byte_offset - start_byte_offset;

    if (
      ts_node_is_null(search->target) ||
      byte_length < search->target_byte_length
    ) {

      search->target = node;
      search->target_byte_length = byte_length;
    }
  }

  uint32_t child_count = ts_node_named_child_count(node);

  for (uint32_t child_index = 0; child_index < child_count; child_index++)
    find_hover_target(
      ts_node_named_child(node, child_index),
      depth + 1,
      search
    );
}

static int
is_attribute_name(TSNode parent, TSNode node) {
  return micropython_ti_node_type_equals(parent, "attribute") &&
    ts_node_eq(node, ts_node_child_by_field_name(parent, "attribute", 9));
}

static int
is_hover_excluded(TSNode parent, TSNode node) {
  const char *parent_type = ts_node_type(parent);

  if (
    strcmp(parent_type, "function_definition") == 0 ||
    strcmp(parent_type, "class_definition") == 0 ||
    strcmp(parent_type, "keyword_argument") == 0
  ) {

    return ts_node_eq(node, ts_node_child_by_field_name(parent, "name", 4));
  }

  if (
    strcmp(parent_type, "parameters") == 0 ||
    strcmp(parent_type, "lambda_parameters") == 0 ||
    strcmp(parent_type, "typed_parameter") == 0 ||
    strcmp(parent_type, "default_parameter") == 0 ||
    strcmp(parent_type, "typed_default_parameter") == 0 ||
    strcmp(parent_type, "import_statement") == 0 ||
    strcmp(parent_type, "import_from_statement") == 0 ||
    strcmp(parent_type, "aliased_import") == 0 ||
    strcmp(parent_type, "dotted_name") == 0
  ) {

    return 1;
  }

  return is_attribute_name(parent, node);
}

static int
is_class_name(MicroPythonTiContext *context, TSNode node) {
  size_t name_byte_length;
  const uint8_t *name_bytes =
    micropython_ti_get_node_bytes(context, node, &name_byte_length);

  if (
    name_bytes &&
    micropython_ti_get_builtin_class_id(name_bytes, name_byte_length) !=
    MICROPYTHON_TI_CLASS_NONE
  ) {

    return 1;
  }

  uint16_t name_id;

  if (!micropython_ti_intern_node_name(context, node, &name_id))
    return 0;

  return micropython_ti_get_defined_class_id(name_id) !=
    MICROPYTHON_TI_CLASS_NONE;
}

static int
is_called_method_name(TSNode node) {
  TSNode parent = ts_node_parent(node);

  if (micropython_ti_node_type_equals(parent, "attribute")) {
    if (!is_attribute_name(parent, node))
      return 0;

    node = parent;
    parent = ts_node_parent(parent);
  }

  return micropython_ti_node_type_equals(parent, "call") &&
    ts_node_eq(node, ts_node_child_by_field_name(parent, "function", 8));
}

static void
set_method_hover(
  MicroPythonTiContext *context,
  TSNode root,
  TSNode method_name_node,
  TiHoverInfo *out
) {

  size_t method_name_length;

  const uint8_t *method_name =
    micropython_ti_get_node_bytes(
      context,
      method_name_node,
      &method_name_length
    );

  if (!method_name)
    return;

  TiSuggestionList suggestions;
  memset(&suggestions, 0, sizeof(suggestions));

  int method_name_end_byte_offset = (int)ts_node_end_byte(method_name_node);

  micropython_ti_collect_suggestions_at_cursor(
    context,
    root,
    method_name_end_byte_offset,
    &suggestions
  );

  for (int index = 0; index < suggestions.count; index++) {
    const TiSuggestion *suggestion = &suggestions.items[index];

    if (
      suggestion->contents_length == (int)method_name_length &&
      memcmp(suggestion->contents, method_name, method_name_length) == 0
    ) {

      out->method_signature = suggestion->detail;
      out->method_document = suggestion->document;
      out->method_name_length = suggestion->contents_length;
      out->is_method = 1;
      out->found = 1;

      break;
    }
  }
}

static void
set_variable_hover(
  MicroPythonTiContext *context,
  TSNode variable_name_node,
  uint16_t t_node_index,
  TiHoverInfo *out
) {

  size_t variable_name_length;

  const uint8_t *variable_name =
    micropython_ti_get_node_bytes(
      context,
      variable_name_node,
      &variable_name_length
    );

  if (
    !variable_name ||
    t_node_index == 0 ||
    micropython_ti_type_to_string(
      t_node_index,
      out->type_name,
      sizeof(out->type_name)
    ) <= 0
  ) {

    return;
  }

  if (variable_name_length >= sizeof(out->variable_name))
    variable_name_length = sizeof(out->variable_name) - 1;

  memcpy(out->variable_name, variable_name, variable_name_length);

  out->variable_name[variable_name_length] = '\0';
  out->found = 1;
}

static void
set_identifier_hover(
  MicroPythonTiContext *context,
  TSNode identifier_node,
  TiHoverInfo *out
) {

  uint16_t name_id;

  if (!micropython_ti_intern_node_name(context, identifier_node, &name_id))
    return;

  set_variable_hover(
    context,
    identifier_node,
    micropython_ti_get_value_t(
      context->current_class_name_id,
      context->current_define_name_id,
      name_id
    ),
    out
  );
}

static void
set_instance_attribute_hover(
  MicroPythonTiContext *context,
  TSNode attribute_node,
  TiHoverInfo *out
) {

  uint16_t receiver_t_node_index =
    micropython_ti_eval_expression(
      context,
      ts_node_child_by_field_name(attribute_node, "object", 6),
      0
    );

  const MicroPythonTiT *receiver_t_node =
    micropython_ti_get_t(receiver_t_node_index);

  if (
    !receiver_t_node ||
    receiver_t_node->union_next != 0 ||
    (receiver_t_node->t_flags & MICROPYTHON_TI_T_FLAG_DEFINED_CLASS) == 0 ||
    (receiver_t_node->t_flags & MICROPYTHON_TI_T_FLAG_STATIC) != 0
  ) {

    return;
  }

  TSNode attribute_name_node =
    ts_node_child_by_field_name(attribute_node, "attribute", 9);

  uint16_t attribute_name_id;

  if (
    !micropython_ti_intern_node_name(
      context,
      attribute_name_node,
      &attribute_name_id
    )
  ) {

    return;
  }

  set_variable_hover(
    context,
    attribute_name_node,
    micropython_ti_get_instance_attribute_t(
      receiver_t_node->object_class_id,
      attribute_name_id
    ),
    out
  );
}

int
micropython_ti_find_hover_at_cursor(
  const TiSourceList *sources,
  int cursor_byte_offset,
  TiHoverInfo *out
) {

  if (out)
    memset(out, 0, sizeof(*out));

  if (!sources || !sources->items || sources->count <= 0 || !out)
    return 0;

  const TiSource *source = &sources->items[sources->count - 1];
  const char *source_bytes = "";

  if (source->source)
    source_bytes = source->source;

  if (
    (!source->source && source->source_byte_length > 0) ||
    cursor_byte_offset < 0 ||
    cursor_byte_offset > source->source_byte_length ||
    !micropython_ti_evaluate_sources(sources, NULL)
  ) {

    return 0;
  }

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
      sources->count - 1
    );

  if (!tree) {
    ts_parser_delete(parser);
    return 0;
  }

  MicroPythonTiContext context = {
    .source = source_bytes,
    .source_byte_length = source->source_byte_length,
  };

  TSNode root = ts_tree_root_node(tree);

  HoverTargetSearch search = {
    .cursor_byte_offset = (uint32_t)cursor_byte_offset,
  };

  micropython_ti_set_context_scope_at_cursor(
    &context,
    root,
    cursor_byte_offset
  );

  if (!ti_did_arena_overflow())
    find_hover_target(root, 0, &search);

  if (!ts_node_is_null(search.target)) {
    TSNode parent = ts_node_parent(search.target);

    if (is_called_method_name(search.target)) {
      if (is_class_name(&context, search.target))
        set_identifier_hover(&context, search.target, out);
      else
        set_method_hover(&context, root, search.target, out);

    } else if (is_attribute_name(parent, search.target)) {
      set_instance_attribute_hover(&context, parent, out);

    } else if (
      ts_node_is_null(parent) ||
      !is_hover_excluded(parent, search.target)
    ) {

      set_identifier_hover(&context, search.target, out);
    }
  }

  ts_tree_delete(tree);
  ts_parser_delete(parser);

  return out->found;
}
