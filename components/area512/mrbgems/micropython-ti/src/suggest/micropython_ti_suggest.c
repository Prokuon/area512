#include "micropython_ti_suggest.h"
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
#include <stddef.h>
#include <string.h>
#include <tree_sitter/tree-sitter-python.h>

typedef struct {
  uint32_t expected_end_byte_offset;
  TSNode target;
  uint32_t target_byte_length;
} SuggestTargetSearch;

static int
is_identifier_byte(uint8_t byte) {
  return (byte >= 'a' && byte <= 'z') || (byte >= 'A' && byte <= 'Z') ||
    (byte >= '0' && byte <= '9') || byte == '_';
}

static int
find_suggest_prefix(
  const MicroPythonTiContext *context,
  int cursor_byte_offset,
  size_t *dot_offset,
  const uint8_t **prefix,
  size_t *prefix_length,
  int *has_receiver
) {

  size_t cursor = (size_t)cursor_byte_offset;
  size_t start = cursor;

  while (
    start > 0 &&
    is_identifier_byte((uint8_t)context->source[start - 1])
  )
    start--;

  *has_receiver = start > 0 && context->source[start - 1] == '.';
  *dot_offset = start;

  if (*has_receiver)
    *dot_offset = start - 1;

  *prefix = (const uint8_t *)context->source + start;
  *prefix_length = cursor - start;

  return 1;
}

static void
find_suggest_target(TSNode node, int depth, SuggestTargetSearch *search) {
  if (depth > MICROPYTHON_TI_EVAL_DEPTH_LIMIT)
    return;

  uint32_t start_byte_offset = ts_node_start_byte(node);
  uint32_t end_byte_offset = ts_node_end_byte(node);

  if (end_byte_offset == search->expected_end_byte_offset) {
    uint32_t byte_length = end_byte_offset - start_byte_offset;

    if (
      ts_node_is_null(search->target) ||
      byte_length > search->target_byte_length
    ) {

      search->target = node;
      search->target_byte_length = byte_length;
    }
  }

  uint32_t child_count = ts_node_named_child_count(node);

  for (uint32_t child_index = 0; child_index < child_count; child_index++)
    find_suggest_target(
      ts_node_named_child(node, child_index),
      depth + 1,
      search
    );
}

static int
matches_prefix(
  const uint8_t *name,
  size_t name_length,
  const uint8_t *prefix,
  size_t prefix_length
) {

  return name_length >= prefix_length &&
    memcmp(name, prefix, prefix_length) == 0;
}

static int
has_suggestion(
  const TiSuggestionList *out,
  const char *contents,
  const char *detail
) {

  for (int index = 0; index < out->count; index++) {
    const TiSuggestion *suggestion = &out->items[index];

    if (
      strcmp(suggestion->contents, contents) == 0 &&
      strcmp(suggestion->detail, detail) == 0
    ) {

      return 1;
    }
  }

  return 0;
}

static void
append_builtin_suggestions(
  uint8_t class_id,
  int use_static_methods,
  int show_class_name,
  const uint8_t *prefix,
  size_t prefix_length,
  int max_addition_count,
  TiSuggestionList *out
) {

  const MicroPythonTiBuiltinMethod *methods[TI_SUGGESTION_CAPACITY];
  int initial_suggestion_count = out->count;

  if (out->count >= TI_SUGGESTION_CAPACITY)
    return;

  int method_count =
    micropython_ti_collect_builtin_methods_matching_partial_method_name(
      class_id,
      use_static_methods,
      prefix,
      prefix_length,
      methods,
      TI_SUGGESTION_CAPACITY
    );

  for (int index = 0; index < method_count; index++) {
    const MicroPythonTiBuiltinMethod *method = methods[index];
    const char *name = micropython_ti_get_builtin_method_name(method);
    const char *signature = micropython_ti_get_builtin_signature(method);

    if (has_suggestion(out, name, signature))
      continue;

    if (
      out->count >= TI_SUGGESTION_CAPACITY ||
      out->count - initial_suggestion_count >= max_addition_count
    )
      return;

    TiSuggestion *suggestion = &out->items[out->count++];
    suggestion->detail = signature;
    suggestion->document = micropython_ti_get_builtin_document(method);
    suggestion->contents = name;
    suggestion->contents_length = (int)strlen(name);

    suggestion->class_name = NULL;

    if (show_class_name)
      suggestion->class_name = micropython_ti_get_builtin_class_name(class_id);
  }
}

static char *
copy_name_to_arena(const MicroPythonTiName *name) {
  const uint8_t *bytes = micropython_ti_get_name_bytes(name);

  if (!bytes)
    return NULL;

  char *copy =
    ti_allocate_from_arena((size_t)name->byte_length + 1);

  if (!copy)
    return NULL;

  memcpy(copy, bytes, name->byte_length);
  copy[name->byte_length] = '\0';

  return copy;
}

static void
append_class_suggestion(
  const char *class_name,
  size_t class_name_length,
  TiSuggestionList *out
) {

  if (has_suggestion(out, class_name, class_name))
    return;

  TiSuggestion *suggestion = &out->items[out->count++];
  suggestion->detail = class_name;
  suggestion->document = "";
  suggestion->contents = class_name;
  suggestion->contents_length = (int)class_name_length;
  suggestion->class_name = NULL;
}

static void
append_builtin_class_suggestions(
  const uint8_t *prefix,
  size_t prefix_length,
  TiSuggestionList *out
) {

  for (
    uint8_t class_id = 1;
    class_id < micropython_ti_builtin_class_count &&
    out->count < TI_SUGGESTION_CAPACITY;
    class_id++
  ) {

    const char *class_name = micropython_ti_get_builtin_class_name(class_id);
    size_t class_name_length = strlen(class_name);

    if (
      !matches_prefix(
        (const uint8_t *)class_name,
        class_name_length,
        prefix,
        prefix_length
      )
    ) {

      continue;
    }

    append_class_suggestion(class_name, class_name_length, out);
  }
}

static void
append_defined_class_suggestions(
  MicroPythonTiContext *context,
  const uint8_t *prefix,
  size_t prefix_length,
  TiSuggestionList *out
) {

  for (
    int index = 0;
    index < micropython_ti_get_define_info_count() &&
    out->count < TI_SUGGESTION_CAPACITY;
    index++
  ) {

    MicroPythonTiDefineInfo *define_info =
      micropython_ti_get_define_info(index);

    if (!define_info || !define_info->is_class)
      continue;

    const MicroPythonTiName *name =
      micropython_ti_get_name(define_info->name_id);

    const uint8_t *name_bytes = micropython_ti_get_name_bytes(name);

    if (
      !name_bytes ||
      !matches_prefix(name_bytes, name->byte_length, prefix, prefix_length)
    ) {

      continue;
    }

    char *contents = copy_name_to_arena(name);

    if (!contents) {
      context->failed = 1;
      return;
    }

    append_class_suggestion(contents, name->byte_length, out);
  }
}

static size_t
write_type_string_or_untyped(
  uint16_t t_node_index,
  char *buffer,
  size_t capacity
) {

  int written_length =
    micropython_ti_type_to_string(t_node_index, buffer, capacity);

  if (written_length > 0)
    return (size_t)written_length;

  if (capacity < sizeof("untyped"))
    return 0;

  memcpy(buffer, "untyped", sizeof("untyped"));

  return sizeof("untyped") - 1;
}

static size_t
count_define_arg_splat_bytes(uint8_t define_arg_kind) {
  if (define_arg_kind == MICROPYTHON_TI_DEFINE_ARG_REST)
    return 1;

  if (define_arg_kind == MICROPYTHON_TI_DEFINE_ARG_KEYWORD_REST)
    return 2;

  return 0;
}

static char *
make_signature_content(
  const MicroPythonTiDefineInfo *define_info,
  const MicroPythonTiName *name
) {

  char type_string[MICROPYTHON_TI_TYPE_STRING_CAPACITY];
  size_t length = name->byte_length + 2;

  for (
    int define_arg_index = 0;
    define_arg_index < define_info->define_arg_count;
    define_arg_index++
  ) {

    const MicroPythonTiName *define_arg_name =
      micropython_ti_get_name(
        define_info->define_arg_name_ids[define_arg_index]
      );

    if (define_arg_name)
      length += define_arg_name->byte_length;

    length +=
      count_define_arg_splat_bytes(
        define_info->define_arg_kinds[define_arg_index]
      );

    length += sizeof(": ") - 1;

    length +=
      write_type_string_or_untyped(
        define_info->define_arg_t_node_indexes[define_arg_index],
        type_string,
        sizeof(type_string)
      );

    if (define_arg_index > 0)
      length += 2;
  }

  length += sizeof(" -> ") - 1;

  length +=
    write_type_string_or_untyped(
      define_info->return_t_node_index,
      type_string,
      sizeof(type_string)
    );

  char *detail = ti_allocate_from_arena(length + 1);

  if (!detail)
    return NULL;

  const uint8_t *name_bytes = micropython_ti_get_name_bytes(name);

  if (!name_bytes)
    return NULL;

  size_t offset = 0;
  memcpy(detail + offset, name_bytes, name->byte_length);
  offset += name->byte_length;
  detail[offset++] = '(';

  for (
    int define_arg_index = 0;
    define_arg_index < define_info->define_arg_count;
    define_arg_index++
  ) {

    const MicroPythonTiName *define_arg_name =
      micropython_ti_get_name(
        define_info->define_arg_name_ids[define_arg_index]
      );

    if (define_arg_index > 0) {
      detail[offset++] = ',';
      detail[offset++] = ' ';
    }

    if (!define_arg_name)
      continue;

    const uint8_t *define_arg_name_bytes =
      micropython_ti_get_name_bytes(define_arg_name);

    if (!define_arg_name_bytes)
      return NULL;

    size_t splat_byte_count =
      count_define_arg_splat_bytes(
        define_info->define_arg_kinds[define_arg_index]
      );

    for (size_t splat_byte_index = 0; splat_byte_index < splat_byte_count;
         splat_byte_index++)
      detail[offset++] = '*';

    memcpy(
      detail + offset,
      define_arg_name_bytes,
      define_arg_name->byte_length
    );

    offset += define_arg_name->byte_length;

    detail[offset++] = ':';
    detail[offset++] = ' ';

    offset +=
      write_type_string_or_untyped(
        define_info->define_arg_t_node_indexes[define_arg_index],
        detail + offset,
        length + 1 - offset
      );
  }

  detail[offset++] = ')';
  memcpy(detail + offset, " -> ", sizeof(" -> ") - 1);
  offset += sizeof(" -> ") - 1;

  offset +=
    write_type_string_or_untyped(
      define_info->return_t_node_index,
      detail + offset,
      length + 1 - offset
    );

  detail[offset] = '\0';

  return detail;
}

static char *
make_instance_attribute_detail(
  const MicroPythonTiName *name,
  uint16_t t_node_index
) {

  char type_string[MICROPYTHON_TI_TYPE_STRING_CAPACITY];

  size_t type_string_length =
    write_type_string_or_untyped(
      t_node_index,
      type_string,
      sizeof(type_string)
    );

  const uint8_t *name_bytes = micropython_ti_get_name_bytes(name);

  if (!name_bytes)
    return NULL;

  size_t detail_length =
    name->byte_length + (sizeof(": ") - 1) + type_string_length;

  char *detail = ti_allocate_from_arena(detail_length + 1);

  if (!detail)
    return NULL;

  size_t offset = 0;

  memcpy(detail + offset, name_bytes, name->byte_length);
  offset += name->byte_length;

  memcpy(detail + offset, ": ", sizeof(": ") - 1);
  offset += sizeof(": ") - 1;

  memcpy(detail + offset, type_string, type_string_length);
  offset += type_string_length;

  detail[offset] = '\0';

  return detail;
}

static void
append_define_info_suggestions_for_owner(
  MicroPythonTiContext *context,
  uint16_t class_name_id,
  const uint8_t *prefix,
  size_t prefix_length,
  const char *class_name,
  int max_addition_count,
  TiSuggestionList *out
) {

  int initial_suggestion_count = out->count;

  for (
    int index = 0;
    index < micropython_ti_get_define_info_count() &&
    out->count < TI_SUGGESTION_CAPACITY &&
    out->count - initial_suggestion_count < max_addition_count;
    index++
  ) {

    MicroPythonTiDefineInfo *define_info =
      micropython_ti_get_define_info(index);

    if (
      !define_info ||
      define_info->is_class ||
      define_info->owner_class_name_id != class_name_id
    ) {

      continue;
    }

    const MicroPythonTiName *name =
      micropython_ti_get_name(define_info->name_id);

    const uint8_t *name_bytes = micropython_ti_get_name_bytes(name);

    if (
      !name_bytes ||
      !matches_prefix(name_bytes, name->byte_length, prefix, prefix_length)
    ) {

      continue;
    }

    char *contents = copy_name_to_arena(name);
    char *detail = make_signature_content(define_info, name);

    if (!contents || !detail) {
      context->failed = 1;
      return;
    }

    if (has_suggestion(out, contents, detail))
      continue;

    TiSuggestion *suggestion = &out->items[out->count++];
    suggestion->detail = detail;
    suggestion->document = "";
    suggestion->contents = contents;
    suggestion->contents_length = (int)name->byte_length;
    suggestion->class_name = class_name;
  }
}

static MicroPythonTiDefineInfo *
find_user_class_define_info(uint8_t object_class_id) {
  int user_class_index = object_class_id - MICROPYTHON_TI_CLASS_USER_BASE;
  int current_class_index = 0;

  for (int index = 0; index < micropython_ti_get_define_info_count(); index++) {
    MicroPythonTiDefineInfo *define_info =
      micropython_ti_get_define_info(index);

    if (!define_info || !define_info->is_class)
      continue;

    if (current_class_index == user_class_index)
      return define_info;

    current_class_index++;
  }

  return NULL;
}

static const char *
make_user_class_name(uint8_t object_class_id) {
  MicroPythonTiDefineInfo *class_define_info =
    find_user_class_define_info(object_class_id);

  if (!class_define_info)
    return NULL;

  return copy_name_to_arena(
    micropython_ti_get_name(class_define_info->name_id)
  );
}

static void
append_define_info_suggestions(
  MicroPythonTiContext *context,
  uint8_t object_class_id,
  const uint8_t *prefix,
  size_t prefix_length,
  const char *class_name,
  int max_addition_count,
  TiSuggestionList *out
) {

  MicroPythonTiDefineInfo *class_define_info =
    find_user_class_define_info(object_class_id);

  if (!class_define_info)
    return;

  append_define_info_suggestions_for_owner(
    context,
    class_define_info->name_id,
    prefix,
    prefix_length,
    class_name,
    max_addition_count,
    out
  );
}

static void
append_instance_attribute_suggestions(
  MicroPythonTiContext *context,
  uint8_t object_class_id,
  const uint8_t *prefix,
  size_t prefix_length,
  const char *class_name,
  int max_addition_count,
  TiSuggestionList *out
) {

  int initial_suggestion_count = out->count;
  int search_slot_index = 0;
  uint16_t instance_attribute_name_id;
  uint16_t instance_attribute_t_node_index;

  while (
    micropython_ti_find_instance_attribute_and_advance_slot(
      object_class_id,
      &search_slot_index,
      &instance_attribute_name_id,
      &instance_attribute_t_node_index
    )
  ) {

    const MicroPythonTiName *name =
      micropython_ti_get_name(instance_attribute_name_id);

    const uint8_t *name_bytes = micropython_ti_get_name_bytes(name);

    if (
      !name_bytes ||
      !matches_prefix(name_bytes, name->byte_length, prefix, prefix_length)
    ) {

      continue;
    }

    char *contents = copy_name_to_arena(name);

    char *detail =
      make_instance_attribute_detail(name, instance_attribute_t_node_index);

    if (!contents || !detail) {
      context->failed = 1;
      return;
    }

    if (has_suggestion(out, contents, detail))
      continue;

    if (
      out->count >= TI_SUGGESTION_CAPACITY ||
      out->count - initial_suggestion_count >= max_addition_count
    )
      return;

    TiSuggestion *suggestion = &out->items[out->count++];
    suggestion->detail = detail;
    suggestion->document = "";
    suggestion->contents = contents;
    suggestion->contents_length = (int)name->byte_length;
    suggestion->class_name = class_name;
  }
}

int
micropython_ti_collect_suggestions_at_cursor(
  MicroPythonTiContext *context,
  TSNode root,
  int cursor_byte_offset,
  TiSuggestionList *out
) {

  size_t dot_offset;
  const uint8_t *prefix;
  size_t prefix_length;
  int has_receiver;

  if (
    !find_suggest_prefix(
      context,
      cursor_byte_offset,
      &dot_offset,
      &prefix,
      &prefix_length,
      &has_receiver
    )
  ) {

    return 0;
  }

  if (!has_receiver) {
    if (prefix_length > 0) {
      append_builtin_class_suggestions(prefix, prefix_length, out);
      append_defined_class_suggestions(context, prefix, prefix_length, out);
    }

    append_define_info_suggestions_for_owner(
      context,
      0,
      prefix,
      prefix_length,
      NULL,
      TI_SUGGESTION_CAPACITY,
      out
    );

    if (context->current_class_name_id != 0) {
      append_define_info_suggestions_for_owner(
        context,
        context->current_class_name_id,
        prefix,
        prefix_length,
        NULL,
        TI_SUGGESTION_CAPACITY,
        out
      );
    }

    append_builtin_suggestions(
      MICROPYTHON_TI_CLASS_OBJECT,
      0,
      0,
      prefix,
      prefix_length,
      TI_SUGGESTION_CAPACITY,
      out
    );

    append_builtin_suggestions(
      MICROPYTHON_TI_CLASS_BUILTINS,
      1,
      0,
      prefix,
      prefix_length,
      TI_SUGGESTION_CAPACITY,
      out
    );

    return out->count;
  }

  SuggestTargetSearch search = {
    .expected_end_byte_offset = (uint32_t)dot_offset,
  };

  find_suggest_target(root, 0, &search);

  if (ts_node_is_null(search.target))
    return 0;

  uint16_t target_t_node_index =
    micropython_ti_eval_expression(context, search.target, 0);

  const MicroPythonTiT *target_t = micropython_ti_get_t(target_t_node_index);

  if (!target_t)
    return 0;

  int show_class_name = target_t->union_next != 0;
  int target_count = 0;
  const MicroPythonTiT *counted_target_t = target_t;

  while (counted_target_t) {
    target_count++;
    counted_target_t = micropython_ti_get_t(counted_target_t->union_next);
  }

  int max_addition_count = TI_SUGGESTION_CAPACITY / target_count;

  while (target_t) {
    if ((target_t->t_flags & MICROPYTHON_TI_T_FLAG_DEFINED_CLASS) != 0) {
      if ((target_t->t_flags & MICROPYTHON_TI_T_FLAG_STATIC) == 0) {
        const char *class_name = NULL;

        if (show_class_name) {
          class_name = make_user_class_name(target_t->object_class_id);

          if (!class_name) {
            context->failed = 1;
            return 0;
          }
        }

        int initial_suggestion_count = out->count;

        append_define_info_suggestions(
          context,
          target_t->object_class_id,
          prefix,
          prefix_length,
          class_name,
          max_addition_count,
          out
        );

        int remaining_addition_count =
          max_addition_count - (out->count - initial_suggestion_count);

        append_instance_attribute_suggestions(
          context,
          target_t->object_class_id,
          prefix,
          prefix_length,
          class_name,
          remaining_addition_count,
          out
        );
      }
    } else if (target_t->object_class_id != MICROPYTHON_TI_CLASS_UNTYPED) {
      append_builtin_suggestions(
        target_t->object_class_id,
        (target_t->t_flags & MICROPYTHON_TI_T_FLAG_STATIC) != 0,
        show_class_name,
        prefix,
        prefix_length,
        max_addition_count,
        out
      );
    }

    target_t = micropython_ti_get_t(target_t->union_next);
  }

  return out->count;
}

int
micropython_ti_fill_suggestions_at_cursor(
  const TiSourceList *sources,
  int cursor_byte_offset,
  TiSuggestionList *out
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

  micropython_ti_set_context_scope_at_cursor(
    &context,
    root,
    cursor_byte_offset
  );

  if (!ti_did_arena_overflow()) {
    micropython_ti_collect_suggestions_at_cursor(
      &context,
      root,
      cursor_byte_offset,
      out
    );
  }

  ts_tree_delete(tree);
  ts_parser_delete(parser);

  if (context.failed || ti_did_arena_overflow()) {
    memset(out, 0, sizeof(*out));
    return 0;
  }

  return out->count;
}
