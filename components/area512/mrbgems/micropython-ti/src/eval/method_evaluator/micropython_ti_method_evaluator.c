#include "micropython_ti_method_evaluator.h"
#include "micropython_ti_builtin.h"
#include "micropython_ti_define_info.h"
#include "micropython_ti_diagnostic.h"
#include "micropython_ti_eval.h"
#include "micropython_ti_t.h"
#include "micropython_ti_t_frame.h"
#include "micropython_ti_type.h"
#include <stdio.h>
#include <string.h>

#define MICROPYTHON_TI_CALL_ARGUMENT_CAPACITY 64

typedef struct {
  int diagnostic_start_byte_offset;
  int diagnostic_end_byte_offset;
  uint16_t t_node_index;
  const uint8_t *keyword_name;
  size_t keyword_name_length;
} MicroPythonTiCallArgument;

typedef struct {
  MicroPythonTiCallArgument
    positional_arguments[MICROPYTHON_TI_CALL_ARGUMENT_CAPACITY];
  MicroPythonTiCallArgument
    keyword_arguments[MICROPYTHON_TI_CALL_ARGUMENT_CAPACITY];
  int positional_argument_count;
  int keyword_argument_count;
  int has_positional_splat;
  int has_keyword_splat;
} MicroPythonTiCallArguments;

typedef struct {
  int diagnostic_start_byte_offset;
  int diagnostic_end_byte_offset;
  char diagnostic_message[TI_BUILTIN_METHOD_MISMATCH_MESSAGE_CAPACITY];
} MicroPythonTiBuiltinMethodMismatch;

static int
set_array_variant_t_node_index(
  const MicroPythonTiBuiltinMethod *builtin_method,
  uint16_t *array_variant_t_node_index
) {

  uint8_t class_identifiers[4];

  int class_identifier_count =
    micropython_ti_get_builtin_return_array_variant_classes(
      builtin_method,
      class_identifiers
    );

  *array_variant_t_node_index = 0;

  for (int index = 0; index < class_identifier_count; index++) {
    uint16_t t_node_index =
      micropython_ti_new_t(class_identifiers[index], 0, 0);

    *array_variant_t_node_index =
      micropython_ti_make_union(*array_variant_t_node_index, t_node_index);

    if (*array_variant_t_node_index == 0)
      return 0;
  }

  return 1;
}

uint16_t
micropython_ti_make_method_return_t_node_index(
  const MicroPythonTiBuiltinMethod *builtin_method
) {

  uint8_t return_class_identifiers[4];

  int return_class_identifier_count =
    micropython_ti_get_builtin_return_classes(
      builtin_method,
      return_class_identifiers
    );

  uint16_t method_return_t_node_index = 0;

  for (int index = 0; index < return_class_identifier_count; index++) {
    uint16_t array_variant_t_node_index = 0;

    if (
      (
        return_class_identifiers[index] == MICROPYTHON_TI_CLASS_LIST ||
        return_class_identifiers[index] == MICROPYTHON_TI_CLASS_TUPLE
      ) &&
      !set_array_variant_t_node_index(
        builtin_method,
        &array_variant_t_node_index
      )
    ) {

      return 0;
    }

    uint16_t union_member_t_node_index =
      micropython_ti_new_t(
        return_class_identifiers[index],
        0,
        array_variant_t_node_index
      );

    method_return_t_node_index =
      micropython_ti_make_union(
        method_return_t_node_index,
        union_member_t_node_index
      );

    if (method_return_t_node_index == 0)
      return 0;
  }

  return method_return_t_node_index;
}

static void
append_call_argument(
  MicroPythonTiCallArgument
    destination_arguments[MICROPYTHON_TI_CALL_ARGUMENT_CAPACITY],
  int *destination_argument_count,
  TSNode diagnostic_node,
  uint16_t t_node_index,
  const uint8_t *keyword_name,
  size_t keyword_name_length
) {

  if (*destination_argument_count >= MICROPYTHON_TI_CALL_ARGUMENT_CAPACITY)
    return;

  MicroPythonTiCallArgument *call_argument =
    &destination_arguments[(*destination_argument_count)++];

  call_argument->diagnostic_start_byte_offset =
    (int)ts_node_start_byte(diagnostic_node);

  call_argument->diagnostic_end_byte_offset =
    (int)ts_node_end_byte(diagnostic_node);

  call_argument->t_node_index = t_node_index;
  call_argument->keyword_name = keyword_name;
  call_argument->keyword_name_length = keyword_name_length;
}

static void
collect_keyword_argument(
  MicroPythonTiContext *context,
  TSNode keyword_argument_node,
  int evaluation_depth,
  MicroPythonTiCallArguments *call_arguments
) {

  TSNode keyword_name_node =
    ts_node_child_by_field_name(keyword_argument_node, "name", 4);

  TSNode keyword_value_node =
    ts_node_child_by_field_name(keyword_argument_node, "value", 5);

  size_t keyword_name_length;

  const uint8_t *keyword_name =
    micropython_ti_get_node_bytes(
      context,
      keyword_name_node,
      &keyword_name_length
    );

  uint16_t t_node_index =
    micropython_ti_eval_expression(
      context,
      keyword_value_node,
      evaluation_depth + 1
    );

  TSNode diagnostic_node = keyword_argument_node;

  if (!ts_node_is_null(keyword_value_node))
    diagnostic_node = keyword_value_node;

  append_call_argument(
    call_arguments->keyword_arguments,
    &call_arguments->keyword_argument_count,
    diagnostic_node,
    t_node_index,
    keyword_name,
    keyword_name_length
  );
}

static void
collect_call_arguments(
  MicroPythonTiContext *context,
  TSNode call_node,
  int evaluation_depth,
  MicroPythonTiCallArguments *call_arguments
) {

  memset(call_arguments, 0, sizeof(*call_arguments));

  TSNode argument_list_node =
    ts_node_child_by_field_name(call_node, "arguments", 9);

  if (ts_node_is_null(argument_list_node))
    return;

  uint32_t call_argument_count = ts_node_named_child_count(argument_list_node);

  for (
    uint32_t call_argument_index = 0;
    call_argument_index < call_argument_count;
    call_argument_index++
  ) {

    TSNode call_argument_node =
      ts_node_named_child(argument_list_node, call_argument_index);

    if (micropython_ti_node_type_equals(call_argument_node, "list_splat")) {
      call_arguments->has_positional_splat = 1;
      continue;
    }

    if (
      micropython_ti_node_type_equals(call_argument_node, "dictionary_splat")
    ) {

      call_arguments->has_keyword_splat = 1;
      continue;
    }

    if (
      micropython_ti_node_type_equals(
        call_argument_node,
        "keyword_argument"
      )
    ) {

      collect_keyword_argument(
        context,
        call_argument_node,
        evaluation_depth,
        call_arguments
      );

      continue;
    }

    uint16_t t_node_index =
      micropython_ti_eval_expression(
        context,
        call_argument_node,
        evaluation_depth + 1
      );

    append_call_argument(
      call_arguments->positional_arguments,
      &call_arguments->positional_argument_count,
      call_argument_node,
      t_node_index,
      NULL,
      0
    );
  }
}

static int
contains_class_identifier(
  const uint8_t class_identifiers[4],
  int class_identifier_count,
  uint8_t class_identifier
) {

  for (int index = 0; index < class_identifier_count; index++) {
    if (class_identifiers[index] == class_identifier)
      return 1;
  }

  return 0;
}

static int
accepts_any_class(
  const uint8_t class_identifiers[4],
  int class_identifier_count
) {

  return contains_class_identifier(
      class_identifiers,
      class_identifier_count,
      MICROPYTHON_TI_CLASS_UNTYPED
    ) ||
    contains_class_identifier(
      class_identifiers,
      class_identifier_count,
      MICROPYTHON_TI_CLASS_OBJECT
    );
}

static int
argument_type_matches(
  const MicroPythonTiBuiltinArgument *builtin_argument,
  uint16_t actual_t_node_index
) {

  if (actual_t_node_index == 0)
    return 1;

  uint8_t expected_class_identifiers[4];

  int expected_class_identifier_count =
    micropython_ti_get_builtin_argument_classes(
      builtin_argument,
      expected_class_identifiers
    );

  if (
    expected_class_identifier_count == 0 ||
    accepts_any_class(
      expected_class_identifiers,
      expected_class_identifier_count
    )
  ) {

    return 1;
  }

  for (
    const MicroPythonTiT *actual_t_node =
      micropython_ti_get_t(actual_t_node_index);
    actual_t_node;
    actual_t_node = micropython_ti_get_t(actual_t_node->union_next)
  ) {

    if (actual_t_node->object_class_id == MICROPYTHON_TI_CLASS_UNTYPED)
      return 1;

    if (
      contains_class_identifier(
        expected_class_identifiers,
        expected_class_identifier_count,
        actual_t_node->object_class_id
      )
    ) {

      return 1;
    }
  }

  return 0;
}

static uint16_t
make_argument_t_node_index(
  const MicroPythonTiBuiltinArgument *builtin_argument
) {

  uint8_t class_identifiers[4];

  int class_identifier_count =
    micropython_ti_get_builtin_argument_classes(
      builtin_argument,
      class_identifiers
    );

  uint16_t argument_t_node_index = 0;

  for (int index = 0; index < class_identifier_count; index++) {
    uint16_t union_member_t_node_index =
      micropython_ti_new_t(class_identifiers[index], 0, 0);

    argument_t_node_index =
      micropython_ti_make_union(
        argument_t_node_index,
        union_member_t_node_index
      );
  }

  return argument_t_node_index;
}

static void
set_type_mismatch(
  const char *class_name,
  const char *method_name,
  const MicroPythonTiBuiltinArgument *builtin_argument,
  const MicroPythonTiCallArgument *call_argument,
  MicroPythonTiBuiltinMethodMismatch *builtin_method_mismatch
) {

  char expected_type_string[MICROPYTHON_TI_TYPE_STRING_CAPACITY];
  char actual_type_string[MICROPYTHON_TI_TYPE_STRING_CAPACITY];

  micropython_ti_type_to_string(
    make_argument_t_node_index(builtin_argument),
    expected_type_string,
    sizeof(expected_type_string)
  );

  micropython_ti_type_to_string(
    call_argument->t_node_index,
    actual_type_string,
    sizeof(actual_type_string)
  );

  builtin_method_mismatch->diagnostic_start_byte_offset =
    call_argument->diagnostic_start_byte_offset;

  builtin_method_mismatch->diagnostic_end_byte_offset =
    call_argument->diagnostic_end_byte_offset;

  snprintf(
    builtin_method_mismatch->diagnostic_message,
    sizeof(builtin_method_mismatch->diagnostic_message),
    "type mismatch: expected %s, but got %s for %s.%s",
    expected_type_string,
    actual_type_string,
    class_name,
    method_name
  );
}

static void
set_argument_count_mismatch(
  TSNode call_node,
  const char *class_name,
  const char *method_name,
  const char *argument_count_description,
  MicroPythonTiBuiltinMethodMismatch *builtin_method_mismatch
) {

  builtin_method_mismatch->diagnostic_start_byte_offset =
    (int)ts_node_start_byte(call_node);

  builtin_method_mismatch->diagnostic_end_byte_offset =
    (int)ts_node_end_byte(call_node);

  snprintf(
    builtin_method_mismatch->diagnostic_message,
    sizeof(builtin_method_mismatch->diagnostic_message),
    "%s arguments for %s.%s",
    argument_count_description,
    class_name,
    method_name
  );
}

static int
count_remaining_required_positional_arguments(
  const MicroPythonTiBuiltinMethod *builtin_method,
  int builtin_argument_index
) {

  int required_positional_argument_count = 0;

  for (
    int remaining_argument_index = builtin_argument_index;
    remaining_argument_index < builtin_method->argument_count;
    remaining_argument_index++
  ) {

    const MicroPythonTiBuiltinArgument *builtin_argument =
      micropython_ti_get_builtin_argument(
        builtin_method,
        remaining_argument_index
      );

    if (
      builtin_argument &&
      builtin_argument->kind == MICROPYTHON_TI_BUILTIN_ARGUMENT_REQUIRED
    ) {

      required_positional_argument_count++;
    }
  }

  return required_positional_argument_count;
}

static int
keyword_name_matches(
  const MicroPythonTiBuiltinArgument *builtin_argument,
  const MicroPythonTiCallArgument *call_argument
) {

  const char *expected_keyword_name =
    micropython_ti_get_builtin_argument_name(builtin_argument);

  return expected_keyword_name &&
    strlen(expected_keyword_name) == call_argument->keyword_name_length &&
    call_argument->keyword_name &&
    memcmp(
      expected_keyword_name,
      call_argument->keyword_name,
      call_argument->keyword_name_length
    ) == 0;
}

static int
find_unmatched_keyword_argument(
  const MicroPythonTiBuiltinArgument *builtin_argument,
  const MicroPythonTiCallArguments *call_arguments,
  const uint8_t
    keyword_argument_matched_flags[MICROPYTHON_TI_CALL_ARGUMENT_CAPACITY]
) {

  for (
    int keyword_argument_index = 0;
    keyword_argument_index < call_arguments->keyword_argument_count;
    keyword_argument_index++
  ) {

    if (
      !keyword_argument_matched_flags[keyword_argument_index] &&
      keyword_name_matches(
        builtin_argument,
        &call_arguments->keyword_arguments[keyword_argument_index]
      )
    ) {

      return keyword_argument_index;
    }
  }

  return -1;
}

static int
match_argument_type_or_set_mismatch(
  const char *class_name,
  const char *method_name,
  const MicroPythonTiBuiltinArgument *builtin_argument,
  const MicroPythonTiCallArgument *call_argument,
  MicroPythonTiBuiltinMethodMismatch *builtin_method_mismatch
) {

  if (argument_type_matches(builtin_argument, call_argument->t_node_index))
    return 1;

  set_type_mismatch(
    class_name,
    method_name,
    builtin_argument,
    call_argument,
    builtin_method_mismatch
  );

  return 0;
}

static int
match_builtin_method(
  TSNode call_node,
  const char *class_name,
  const char *method_name,
  const MicroPythonTiBuiltinMethod *builtin_method,
  const MicroPythonTiCallArguments *call_arguments,
  MicroPythonTiBuiltinMethodMismatch *builtin_method_mismatch
) {

  int positional_argument_index = 0;

  uint8_t
    keyword_argument_matched_flags[MICROPYTHON_TI_CALL_ARGUMENT_CAPACITY] = {0};

  int has_rest_keyword_argument = 0;

  for (
    int builtin_argument_index = 0;
    builtin_argument_index < builtin_method->argument_count;
    builtin_argument_index++
  ) {

    const MicroPythonTiBuiltinArgument *builtin_argument =
      micropython_ti_get_builtin_argument(
        builtin_method,
        builtin_argument_index
      );

    if (!builtin_argument)
      continue;

    switch (builtin_argument->kind) {
    case MICROPYTHON_TI_BUILTIN_ARGUMENT_REQUIRED:
      if (
        positional_argument_index >=
        call_arguments->positional_argument_count
      ) {

        int matching_keyword_argument_index =
          find_unmatched_keyword_argument(
            builtin_argument,
            call_arguments,
            keyword_argument_matched_flags
          );

        if (matching_keyword_argument_index >= 0) {
          if (
            !match_argument_type_or_set_mismatch(
              class_name,
              method_name,
              builtin_argument,
              &call_arguments->keyword_arguments[
                matching_keyword_argument_index
              ],
              builtin_method_mismatch
            )
          ) {

            return 0;
          }

          keyword_argument_matched_flags[matching_keyword_argument_index] = 1;

          break;
        }

        if (
          call_arguments->has_positional_splat ||
          call_arguments->has_keyword_splat
        )
          break;

        set_argument_count_mismatch(
          call_node,
          class_name,
          method_name,
          "too few",
          builtin_method_mismatch
        );

        return 0;
      }

      if (
        !match_argument_type_or_set_mismatch(
          class_name,
          method_name,
          builtin_argument,
          &call_arguments->positional_arguments[positional_argument_index],
          builtin_method_mismatch
        )
      ) {

        return 0;
      }

      positional_argument_index++;

      break;

    case MICROPYTHON_TI_BUILTIN_ARGUMENT_OPTIONAL: {
      int remaining_call_positional_argument_count =
        call_arguments->positional_argument_count - positional_argument_index;

      int remaining_required_positional_argument_count =
        count_remaining_required_positional_arguments(
          builtin_method,
          builtin_argument_index + 1
        );

      if (
        remaining_call_positional_argument_count <=
        remaining_required_positional_argument_count
      ) {

        int matching_keyword_argument_index =
          find_unmatched_keyword_argument(
            builtin_argument,
            call_arguments,
            keyword_argument_matched_flags
          );

        if (matching_keyword_argument_index < 0)
          break;

        if (
          !match_argument_type_or_set_mismatch(
            class_name,
            method_name,
            builtin_argument,
            &call_arguments
              ->keyword_arguments[matching_keyword_argument_index],
            builtin_method_mismatch
          )
        ) {

          return 0;
        }

        keyword_argument_matched_flags[matching_keyword_argument_index] = 1;

        break;
      }

      if (
        !match_argument_type_or_set_mismatch(
          class_name,
          method_name,
          builtin_argument,
          &call_arguments->positional_arguments[positional_argument_index],
          builtin_method_mismatch
        )
      ) {

        return 0;
      }

      positional_argument_index++;

      break;
    }

    case MICROPYTHON_TI_BUILTIN_ARGUMENT_REST: {
      int rest_positional_end_index =
        call_arguments->positional_argument_count -
        count_remaining_required_positional_arguments(
          builtin_method,
          builtin_argument_index + 1
        );

      while (positional_argument_index < rest_positional_end_index) {
        if (
          !match_argument_type_or_set_mismatch(
            class_name,
            method_name,
            builtin_argument,
            &call_arguments->positional_arguments[positional_argument_index],
            builtin_method_mismatch
          )
        ) {

          return 0;
        }

        positional_argument_index++;
      }

      break;
    }

    case MICROPYTHON_TI_BUILTIN_ARGUMENT_REQUIRED_KEYWORD:
    case MICROPYTHON_TI_BUILTIN_ARGUMENT_OPTIONAL_KEYWORD: {
      int matching_keyword_argument_index =
        find_unmatched_keyword_argument(
          builtin_argument,
          call_arguments,
          keyword_argument_matched_flags
        );

      if (matching_keyword_argument_index < 0) {
        if (
          builtin_argument->kind ==
          MICROPYTHON_TI_BUILTIN_ARGUMENT_REQUIRED_KEYWORD &&
          !call_arguments->has_keyword_splat
        ) {

          set_argument_count_mismatch(
            call_node,
            class_name,
            method_name,
            "too few",
            builtin_method_mismatch
          );

          return 0;
        }

        break;
      }

      const MicroPythonTiCallArgument *call_argument =
        &call_arguments->keyword_arguments[matching_keyword_argument_index];

      if (
        !match_argument_type_or_set_mismatch(
          class_name,
          method_name,
          builtin_argument,
          call_argument,
          builtin_method_mismatch
        )
      ) {

        return 0;
      }

      keyword_argument_matched_flags[matching_keyword_argument_index] = 1;

      break;
    }

    case MICROPYTHON_TI_BUILTIN_ARGUMENT_REST_KEYWORD:
      has_rest_keyword_argument = 1;

      for (
        int keyword_argument_index = 0;
        keyword_argument_index < call_arguments->keyword_argument_count;
        keyword_argument_index++
      ) {

        if (keyword_argument_matched_flags[keyword_argument_index])
          continue;

        const MicroPythonTiCallArgument *call_argument =
          &call_arguments->keyword_arguments[keyword_argument_index];

        if (
          !match_argument_type_or_set_mismatch(
            class_name,
            method_name,
            builtin_argument,
            call_argument,
            builtin_method_mismatch
          )
        ) {

          return 0;
        }

        keyword_argument_matched_flags[keyword_argument_index] = 1;
      }

      break;
    }
  }

  if (
    positional_argument_index < call_arguments->positional_argument_count &&
    !call_arguments->has_positional_splat
  ) {

    set_argument_count_mismatch(
      call_node,
      class_name,
      method_name,
      "too many",
      builtin_method_mismatch
    );

    return 0;
  }

  if (!has_rest_keyword_argument && !call_arguments->has_keyword_splat) {
    for (
      int keyword_argument_index = 0;
      keyword_argument_index < call_arguments->keyword_argument_count;
      keyword_argument_index++
    ) {

      if (!keyword_argument_matched_flags[keyword_argument_index]) {
        set_argument_count_mismatch(
          call_node,
          class_name,
          method_name,
          "too many",
          builtin_method_mismatch
        );

        return 0;
      }
    }
  }

  return 1;
}

static uint16_t
evaluate_builtin_method(
  MicroPythonTiContext *context,
  TSNode call_node,
  int evaluation_depth,
  uint8_t class_identifier,
  const MicroPythonTiBuiltinMethod *builtin_method
) {

  MicroPythonTiCallArguments call_arguments;

  collect_call_arguments(context, call_node, evaluation_depth, &call_arguments);

  const char *class_name =
    micropython_ti_get_builtin_class_name(class_identifier);

  MicroPythonTiBuiltinMethodMismatch builtin_method_mismatch = {0};

  if (
    !match_builtin_method(
      call_node,
      class_name,
      micropython_ti_get_builtin_method_name(builtin_method),
      builtin_method,
      &call_arguments,
      &builtin_method_mismatch
    )
  ) {

    micropython_ti_add_diagnostic(
      context,
      builtin_method_mismatch.diagnostic_start_byte_offset,
      builtin_method_mismatch.diagnostic_end_byte_offset,
      builtin_method_mismatch.diagnostic_message
    );
  }

  return micropython_ti_make_method_return_t_node_index(builtin_method);
}

static uint16_t
eval_unqualified_call(
  MicroPythonTiContext *context,
  TSNode call_node,
  TSNode function_node,
  int evaluation_depth
) {

  size_t method_name_length;

  const uint8_t *method_name =
    micropython_ti_get_node_bytes(context, function_node, &method_name_length);

  if (!method_name)
    return 0;

  uint16_t method_name_identifier;

  if (
    !micropython_ti_intern_node_name(
      context,
      function_node,
      &method_name_identifier
    )
  ) {

    return 0;
  }

  uint16_t defined_return_t_node_index =
    micropython_ti_get_method_t(
      context->current_class_id,
      method_name_identifier
    );

  if (defined_return_t_node_index != 0)
    return defined_return_t_node_index;

  uint8_t defined_class_identifier =
    micropython_ti_get_defined_class_id(method_name_identifier);

  if (defined_class_identifier != MICROPYTHON_TI_CLASS_NONE) {
    return micropython_ti_new_t(
      defined_class_identifier,
      MICROPYTHON_TI_T_FLAG_DEFINED_CLASS,
      0
    );
  }

  uint8_t builtin_class_identifier =
    micropython_ti_get_builtin_class_id(method_name, method_name_length);

  if (builtin_class_identifier != MICROPYTHON_TI_CLASS_NONE)
    return micropython_ti_new_t(builtin_class_identifier, 0, 0);

  const MicroPythonTiBuiltinMethod *builtins_builtin_method =
    micropython_ti_get_builtin_static_method(
      MICROPYTHON_TI_CLASS_BUILTINS,
      method_name,
      method_name_length
    );

  if (builtins_builtin_method) {
    return evaluate_builtin_method(
      context,
      call_node,
      evaluation_depth,
      MICROPYTHON_TI_CLASS_BUILTINS,
      builtins_builtin_method
    );
  }

  return 0;
}

uint16_t
micropython_ti_eval_call(
  MicroPythonTiContext *context,
  TSNode call_node,
  int evaluation_depth
) {

  TSNode function_node = ts_node_child_by_field_name(call_node, "function", 8);

  if (micropython_ti_node_type_equals(function_node, "identifier")) {
    return eval_unqualified_call(
      context,
      call_node,
      function_node,
      evaluation_depth
    );
  }

  if (!micropython_ti_node_type_equals(function_node, "attribute"))
    return 0;

  TSNode receiver_node =
    ts_node_child_by_field_name(function_node, "object", 6);

  TSNode method_name_node =
    ts_node_child_by_field_name(function_node, "attribute", 9);

  size_t method_name_length;

  const uint8_t *method_name =
    micropython_ti_get_node_bytes(
      context,
      method_name_node,
      &method_name_length
    );

  if (!method_name)
    return 0;

  uint16_t method_name_identifier;

  if (
    !micropython_ti_intern_node_name(
      context,
      method_name_node,
      &method_name_identifier
    )
  ) {

    return 0;
  }

  uint16_t receiver_t_node_index =
    micropython_ti_eval_expression(
      context,
      receiver_node,
      evaluation_depth + 1
    );

  const MicroPythonTiT *receiver_t_node =
    micropython_ti_get_t(receiver_t_node_index);

  if (!receiver_t_node || receiver_t_node->union_next != 0)
    return 0;

  if ((receiver_t_node->t_flags & MICROPYTHON_TI_T_FLAG_DEFINED_CLASS) != 0) {
    return micropython_ti_get_method_t(
      receiver_t_node->object_class_id,
      method_name_identifier
    );
  }

  const MicroPythonTiBuiltinMethod *builtin_method;

  int is_static_method =
    (receiver_t_node->t_flags & MICROPYTHON_TI_T_FLAG_STATIC) != 0;

  if (is_static_method) {
    builtin_method =
      micropython_ti_get_builtin_static_method(
        receiver_t_node->object_class_id,
        method_name,
        method_name_length
      );
  } else {
    builtin_method =
      micropython_ti_get_builtin_instance_method(
        receiver_t_node->object_class_id,
        method_name,
        method_name_length
      );
  }

  if (!builtin_method)
    return 0;

  return evaluate_builtin_method(
    context,
    call_node,
    evaluation_depth,
    receiver_t_node->object_class_id,
    builtin_method
  );
}
