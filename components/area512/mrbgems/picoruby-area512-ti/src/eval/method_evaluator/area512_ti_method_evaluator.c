#include "area512_ti_method_evaluator.h"
#include "area512_ti_builtin.h"
#include "area512_ti_diagnostic.h"
#include "area512_ti_eval.h"
#include "area512_ti_t.h"
#include "area512_ti_t_frame.h"
#include "area512_ti_type.h"
#include <stdio.h>
#include <string.h>

#define TI_CALL_ARGUMENT_CAPACITY 64

typedef struct {
  const pm_node_t *node;
  uint16_t t_node_index;
  const uint8_t *keyword_name;
  size_t keyword_name_length;
} TiCallArgument;

typedef struct {
  TiCallArgument positionals[TI_CALL_ARGUMENT_CAPACITY];
  TiCallArgument keywords[TI_CALL_ARGUMENT_CAPACITY];
  int positional_count;
  int keyword_count;
} TiCallArguments;

typedef struct {
  pm_location_t location;
  char message[TI_DIAGNOSTIC_MESSAGE_CAPACITY];
} TiMethodMismatch;

static int
make_array_variants(const TiBuiltinMethod *method, uint16_t *variants) {
  uint8_t class_ids[4];

  int class_count =
    ti_get_builtin_return_array_variant_classes(method, class_ids);

  *variants = 0;

  for (int index = 0; index < class_count; index++) {
    uint16_t t_node_index = ti_new_t(class_ids[index], 0, 0);

    *variants = ti_make_union(*variants, t_node_index);

    if (*variants == 0)
      return 0;
  }

  return 1;
}

static uint16_t
make_method_return(const TiBuiltinMethod *method) {
  uint8_t return_class_identifiers[4];

  int return_class_count =
    ti_get_builtin_return_classes(method, return_class_identifiers);

  uint16_t result = 0;

  for (int index = 0; index < return_class_count; index++) {
    uint16_t variants = 0;

    if (
        return_class_identifiers[index] == TI_CLASS_ARRAY &&
        !make_array_variants(method, &variants)
      ) {

      return 0;
    }

    uint16_t union_next_t_node_index =
      ti_new_t(return_class_identifiers[index], 0, variants);

    result = ti_make_union(result, union_next_t_node_index);

    if (result == 0)
      return 0;
  }

  return result;
}

static void
append_call_argument(
  TiCallArgument arguments[TI_CALL_ARGUMENT_CAPACITY],
  int *argument_count,
  const pm_node_t *node,
  uint16_t t_node_index,
  const uint8_t *keyword_name,
  size_t keyword_name_length
) {

  if (*argument_count >= TI_CALL_ARGUMENT_CAPACITY)
    return;

  TiCallArgument *argument = &arguments[(*argument_count)++];

  argument->node = node;
  argument->t_node_index = t_node_index;
  argument->keyword_name = keyword_name;
  argument->keyword_name_length = keyword_name_length;
}

static void
collect_keyword_arguments(
  TiContext *context,
  const pm_keyword_hash_node_t *keyword_hash,
  int depth,
  TiCallArguments *arguments
) {

  for (size_t index = 0; index < keyword_hash->elements.size; index++) {
    const pm_node_t *element = keyword_hash->elements.nodes[index];

    if (PM_NODE_TYPE(element) == PM_ASSOC_NODE) {
      const pm_assoc_node_t *association = (const pm_assoc_node_t *)element;
      const uint8_t *keyword_name = NULL;
      size_t keyword_name_length = 0;

      if (
        association->key &&
        PM_NODE_TYPE(association->key) == PM_SYMBOL_NODE
      ) {
        const pm_symbol_node_t *symbol =
          (const pm_symbol_node_t *)association->key;

        keyword_name = pm_string_source(&symbol->unescaped);
        keyword_name_length = pm_string_length(&symbol->unescaped);
      }

      uint16_t t_node_index =
        ti_eval_expression(context, association->value, depth + 1);

      append_call_argument(
        arguments->keywords,
        &arguments->keyword_count,
        association->value ? association->value : element,
        t_node_index,
        keyword_name,
        keyword_name_length
      );

      continue;
    }

    append_call_argument(
      arguments->keywords,
      &arguments->keyword_count,
      element,
      0,
      NULL,
      0
    );
  }
}

static void
collect_call_arguments(
  TiContext *context,
  const pm_call_node_t *call,
  int depth,
  TiCallArguments *arguments
) {

  memset(arguments, 0, sizeof(*arguments));

  if (!call->arguments)
    return;

  for (size_t index = 0; index < call->arguments->arguments.size; index++) {
    const pm_node_t *argument = call->arguments->arguments.nodes[index];

    if (PM_NODE_TYPE(argument) == PM_KEYWORD_HASH_NODE) {
      collect_keyword_arguments(
        context,
        (const pm_keyword_hash_node_t *)argument,
        depth,
        arguments
      );

      continue;
    }

    append_call_argument(
      arguments->positionals,
      &arguments->positional_count,
      argument,
      ti_eval_expression(context, argument, depth + 1),
      NULL,
      0
    );
  }
}

static int
contains_class_id(
  const uint8_t class_ids[4],
  int class_count,
  uint8_t class_id
) {

  for (int index = 0; index < class_count; index++) {
    if (class_ids[index] == class_id)
      return 1;
  }

  return 0;
}

static int
argument_type_matches(
  const TiBuiltinArgument *builtin_argument,
  uint16_t actual_t_node_index
) {

  if (actual_t_node_index == 0)
    return 1;

  uint8_t expected_class_ids[4];
  int expected_class_count =
    ti_get_builtin_argument_classes(
      builtin_argument,
      expected_class_ids
    );

  if (
    expected_class_count == 0 ||
    contains_class_id(
      expected_class_ids,
      expected_class_count,
      TI_CLASS_UNTYPED
    )
  ) {
    return 1;
  }

  for (const T *actual_t = ti_get_t(actual_t_node_index); actual_t;
       actual_t = ti_get_t(actual_t->union_next)) {

    if (actual_t->object_class_id == TI_CLASS_UNTYPED)
      return 1;
  }

  for (const T *actual_t = ti_get_t(actual_t_node_index); actual_t;
       actual_t = ti_get_t(actual_t->union_next)) {

    if (
      !contains_class_id(
        expected_class_ids,
        expected_class_count,
        actual_t->object_class_id
      )
    ) {
      return 0;
    }
  }

  return 1;
}

static uint16_t
make_argument_t(const TiBuiltinArgument *builtin_argument) {
  uint8_t class_ids[4];
  int class_count =
    ti_get_builtin_argument_classes(builtin_argument, class_ids);

  uint16_t result = 0;

  for (int index = 0; index < class_count; index++) {
    uint16_t t_node_index = ti_new_t(class_ids[index], 0, 0);
    result = ti_make_union(result, t_node_index);
  }

  return result;
}

static void
set_type_mismatch(
  TiContext *context,
  const char *class_name,
  const char *method_name,
  const TiBuiltinArgument *builtin_argument,
  const TiCallArgument *call_argument,
  TiMethodMismatch *mismatch
) {

  char expected_type[TI_TYPE_STRING_CAPACITY];
  char actual_type[TI_TYPE_STRING_CAPACITY];

  ti_type_to_string(
    context,
    make_argument_t(builtin_argument),
    expected_type,
    sizeof(expected_type)
  );

  ti_type_to_string(
    context,
    call_argument->t_node_index,
    actual_type,
    sizeof(actual_type)
  );

  mismatch->location = call_argument->node->location;

  snprintf(
    mismatch->message,
    sizeof(mismatch->message),
    "type mismatch: expected %s, but got %s for %s.%s",
    expected_type,
    actual_type,
    class_name,
    method_name
  );
}

static void
set_argument_count_mismatch(
  const pm_call_node_t *call,
  const char *class_name,
  const char *method_name,
  const char *count_description,
  TiMethodMismatch *mismatch
) {

  mismatch->location = call->base.location;

  snprintf(
    mismatch->message,
    sizeof(mismatch->message),
    "%s arguments for %s.%s",
    count_description,
    class_name,
    method_name
  );
}

static int
remaining_required_positionals(
  const TiBuiltinMethod *method,
  int argument_index
) {

  int required_count = 0;

  for (int index = argument_index; index < method->argument_count; index++) {
    const TiBuiltinArgument *argument =
      ti_get_builtin_argument(method, index);

    if (argument && argument->kind == TI_BUILTIN_ARGUMENT_REQUIRED)
      required_count++;
  }

  return required_count;
}

static int
keyword_name_matches(
  const TiBuiltinArgument *builtin_argument,
  const TiCallArgument *call_argument
) {

  const char *expected_name =
    ti_get_builtin_argument_name(builtin_argument);

  return expected_name &&
         strlen(expected_name) == call_argument->keyword_name_length &&
         call_argument->keyword_name &&
         memcmp(
           expected_name,
           call_argument->keyword_name,
           call_argument->keyword_name_length
         ) == 0;
}

static int
match_builtin_method(
  TiContext *context,
  const pm_call_node_t *call,
  const char *class_name,
  const char *method_name,
  const TiBuiltinMethod *method,
  const TiCallArguments *call_arguments,
  TiMethodMismatch *mismatch
) {

  int positional_index = 0;
  uint8_t matched_keywords[TI_CALL_ARGUMENT_CAPACITY] = {0};
  int has_rest_keywords = 0;

  for (int argument_index = 0;
       argument_index < method->argument_count;
       argument_index++) {

    const TiBuiltinArgument *builtin_argument =
      ti_get_builtin_argument(method, argument_index);

    if (!builtin_argument)
      continue;

    switch (builtin_argument->kind) {
    case TI_BUILTIN_ARGUMENT_REQUIRED:
      if (positional_index >= call_arguments->positional_count) {
        set_argument_count_mismatch(
          call,
          class_name,
          method_name,
          "too few",
          mismatch
        );
        return 0;
      }

      if (!argument_type_matches(
            builtin_argument,
            call_arguments->positionals[positional_index].t_node_index
          )) {
        set_type_mismatch(
          context,
          class_name,
          method_name,
          builtin_argument,
          &call_arguments->positionals[positional_index],
          mismatch
        );
        return 0;
      }

      positional_index++;
      break;

    case TI_BUILTIN_ARGUMENT_OPTIONAL:
      if (
        call_arguments->positional_count - positional_index <=
        remaining_required_positionals(method, argument_index + 1)
      ) {
        break;
      }

      if (!argument_type_matches(
            builtin_argument,
            call_arguments->positionals[positional_index].t_node_index
          )) {
        set_type_mismatch(
          context,
          class_name,
          method_name,
          builtin_argument,
          &call_arguments->positionals[positional_index],
          mismatch
        );
        return 0;
      }

      positional_index++;
      break;

    case TI_BUILTIN_ARGUMENT_REST: {
      int rest_end =
        call_arguments->positional_count -
        remaining_required_positionals(method, argument_index + 1);

      while (positional_index < rest_end) {
        if (!argument_type_matches(
              builtin_argument,
              call_arguments->positionals[positional_index].t_node_index
            )) {
          set_type_mismatch(
            context,
            class_name,
            method_name,
            builtin_argument,
            &call_arguments->positionals[positional_index],
            mismatch
          );
          return 0;
        }

        positional_index++;
      }

      break;
    }

    case TI_BUILTIN_ARGUMENT_REQUIRED_KEYWORD:
    case TI_BUILTIN_ARGUMENT_OPTIONAL_KEYWORD: {
      int matching_keyword_index = -1;

      for (int keyword_index = 0;
           keyword_index < call_arguments->keyword_count;
           keyword_index++) {

        if (
          !matched_keywords[keyword_index] &&
          keyword_name_matches(
            builtin_argument,
            &call_arguments->keywords[keyword_index]
          )
        ) {
          matching_keyword_index = keyword_index;
          break;
        }
      }

      if (matching_keyword_index < 0) {
        if (
          builtin_argument->kind ==
          TI_BUILTIN_ARGUMENT_REQUIRED_KEYWORD
        ) {
          set_argument_count_mismatch(
            call,
            class_name,
            method_name,
            "too few",
            mismatch
          );
          return 0;
        }

        break;
      }

      const TiCallArgument *call_argument =
        &call_arguments->keywords[matching_keyword_index];

      if (!argument_type_matches(
            builtin_argument,
            call_argument->t_node_index
          )) {
        set_type_mismatch(
          context,
          class_name,
          method_name,
          builtin_argument,
          call_argument,
          mismatch
        );
        return 0;
      }

      matched_keywords[matching_keyword_index] = 1;
      break;
    }

    case TI_BUILTIN_ARGUMENT_REST_KEYWORD:
      has_rest_keywords = 1;

      for (int keyword_index = 0;
           keyword_index < call_arguments->keyword_count;
           keyword_index++) {

        if (matched_keywords[keyword_index])
          continue;

        const TiCallArgument *call_argument =
          &call_arguments->keywords[keyword_index];

        if (!argument_type_matches(
              builtin_argument,
              call_argument->t_node_index
            )) {
          set_type_mismatch(
            context,
            class_name,
            method_name,
            builtin_argument,
            call_argument,
            mismatch
          );
          return 0;
        }

        matched_keywords[keyword_index] = 1;
      }

      break;
    }
  }

  if (positional_index < call_arguments->positional_count) {
    set_argument_count_mismatch(
      call,
      class_name,
      method_name,
      "too many",
      mismatch
    );
    return 0;
  }

  if (!has_rest_keywords) {
    for (int index = 0; index < call_arguments->keyword_count; index++) {
      if (!matched_keywords[index]) {
        set_argument_count_mismatch(
          call,
          class_name,
          method_name,
          "too many",
          mismatch
        );
        return 0;
      }
    }
  }

  return 1;
}

static uint16_t
evaluate_builtin_method(
  TiContext *context,
  const pm_call_node_t *call,
  int depth,
  uint8_t class_id,
  const TiBuiltinMethod *method
) {

  TiCallArguments call_arguments;
  collect_call_arguments(context, call, depth, &call_arguments);

  const char *class_name = ti_get_builtin_class_name(class_id);
  TiMethodMismatch mismatch = {0};

  if (!match_builtin_method(
        context,
        call,
        class_name,
        ti_get_builtin_method_name(method),
        method,
        &call_arguments,
        &mismatch
      )) {
    ti_add_diagnostic(
      context,
      mismatch.location,
      mismatch.message
    );
  }

  return make_method_return(method);
}

uint16_t
ti_eval_method(TiContext *context, const pm_call_node_t *call, int depth) {
  uint16_t name_id;

  if (!ti_convert_constant_id(call->name, &name_id))
    return 0;

  const pm_constant_t *method_name = ti_get_constant(context, call->name);

  if (!method_name)
    return 0;

  if (!call->receiver) {
    uint16_t defined_return_t_node_index = ti_get_value_t(name_id);

    if (defined_return_t_node_index != 0)
      return defined_return_t_node_index;

    const TiBuiltinMethod *kernel_method =
      ti_get_builtin_instance_method(
        TI_CLASS_KERNEL,
        method_name->start,
        method_name->length
      );

    if (kernel_method) {
      return evaluate_builtin_method(
        context,
        call,
        depth,
        TI_CLASS_KERNEL,
        kernel_method
      );
    }

    return 0;
  }

  uint16_t receiver_t_node_index =
    ti_eval_expression(context, call->receiver, depth + 1);

  const T *receiver_t = ti_get_t(receiver_t_node_index);

  if (!receiver_t || receiver_t->union_next != 0)
    return 0;

  if ((receiver_t->t_flags & TI_T_FLAG_DEFINED_CLASS) != 0) {
    if (
      method_name->length == 3 &&
      memcmp(method_name->start, "new", 3) == 0 &&
      (receiver_t->t_flags & TI_T_FLAG_STATIC) != 0
    ) {
      return ti_new_t(
        receiver_t->object_class_id,
        receiver_t->t_flags & TI_T_FLAG_DEFINED_CLASS,
        receiver_t->variants
      );
    }

    return ti_get_value_t(name_id);
  }

  const TiBuiltinMethod *method;
  int use_static_methods =
    (receiver_t->t_flags & TI_T_FLAG_STATIC) != 0;

  if (use_static_methods) {
    method =
      ti_get_builtin_static_method(
        receiver_t->object_class_id,
        method_name->start,
        method_name->length
      );
  } else {
    method =
      ti_get_builtin_instance_method(
        receiver_t->object_class_id,
        method_name->start,
        method_name->length
      );
  }

  if (!method)
    return 0;

  return evaluate_builtin_method(
    context,
    call,
    depth,
    receiver_t->object_class_id,
    method
  );
}
