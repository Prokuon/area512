#include "micropython_ti_builtin.h"
#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

static void
test_str_method_lookup(void) {
  const MicroPythonTiBuiltinMethod *method =
    micropython_ti_get_builtin_instance_method(
      MICROPYTHON_TI_CLASS_STR,
      (const uint8_t *)"find",
      4
    );

  assert(method);
  assert(
    strcmp(
      micropython_ti_get_builtin_signature(method),
      "find(sub: str, start: SupportsIndex | None = ..., "
      "end: SupportsIndex | None = ...) -> int"
    ) == 0
  );
}

static void
test_inherited_methods(void) {
  const MicroPythonTiBuiltinMethod *instance_method =
    micropython_ti_get_builtin_instance_method(
      MICROPYTHON_TI_CLASS_BOOL,
      (const uint8_t *)"to_bytes",
      8
    );
  assert(instance_method);
  assert(instance_method->origin_class_identifier == MICROPYTHON_TI_CLASS_INT);

  const MicroPythonTiBuiltinMethod *static_method =
    micropython_ti_get_builtin_static_method(
      MICROPYTHON_TI_CLASS_BOOL,
      (const uint8_t *)"from_bytes",
      10
    );
  assert(static_method);
  assert(static_method->origin_class_identifier == MICROPYTHON_TI_CLASS_INT);
}

static void
test_static_method_is_not_an_instance_method(void) {
  assert(
    micropython_ti_get_builtin_static_method(
      MICROPYTHON_TI_CLASS_CONSOLE,
      (const uint8_t *)"reset",
      5
    )
  );
  assert(
    !micropython_ti_get_builtin_instance_method(
      MICROPYTHON_TI_CLASS_CONSOLE,
      (const uint8_t *)"reset",
      5
    )
  );
}

static void
test_type_variable_return_type(void) {
  const MicroPythonTiBuiltinMethod *method =
    micropython_ti_get_builtin_instance_method(
      MICROPYTHON_TI_CLASS_LIST,
      (const uint8_t *)"pop",
      3
    );
  uint8_t return_class_identifiers[4] = {0};

  assert(method);
  assert(
    micropython_ti_get_builtin_return_classes(
      method,
      return_class_identifiers
    ) == 1
  );
  assert(return_class_identifiers[0] == MICROPYTHON_TI_CLASS_UNTYPED);
}

static void
test_union_return_type(void) {
  const MicroPythonTiBuiltinMethod *method =
    micropython_ti_get_builtin_static_method(
      MICROPYTHON_TI_CLASS_GC,
      (const uint8_t *)"collect",
      7
    );
  uint8_t return_class_identifiers[4] = {0};

  assert(method);
  assert(
    micropython_ti_get_builtin_return_classes(
      method,
      return_class_identifiers
    ) == 2
  );
  assert(return_class_identifiers[0] == MICROPYTHON_TI_CLASS_INT);
  assert(return_class_identifiers[1] == MICROPYTHON_TI_CLASS_NONETYPE);
}

static void
test_list_return_variant_type(void) {
  const MicroPythonTiBuiltinMethod *method =
    micropython_ti_get_builtin_instance_method(
      MICROPYTHON_TI_CLASS_STR,
      (const uint8_t *)"split",
      5
    );
  uint8_t return_class_identifiers[4] = {0};
  uint8_t variant_class_identifiers[4] = {0};

  assert(method);
  assert(
    micropython_ti_get_builtin_return_classes(
      method,
      return_class_identifiers
    ) == 1
  );
  assert(return_class_identifiers[0] == MICROPYTHON_TI_CLASS_LIST);
  assert(
    micropython_ti_get_builtin_return_array_variant_classes(
      method,
      variant_class_identifiers
    ) == 1
  );
  assert(variant_class_identifiers[0] == MICROPYTHON_TI_CLASS_STR);
}

static void
test_builtin_argument_type(void) {
  const MicroPythonTiBuiltinMethod *method =
    micropython_ti_get_builtin_instance_method(
      MICROPYTHON_TI_CLASS_STR,
      (const uint8_t *)"find",
      4
    );

  assert(method);
  assert(method->argument_count == 3);

  const MicroPythonTiBuiltinArgument *first_argument =
    micropython_ti_get_builtin_argument(method, 0);
  uint8_t class_identifiers[4] = {0};

  assert(first_argument);
  assert(first_argument->kind == MICROPYTHON_TI_BUILTIN_ARGUMENT_REQUIRED);
  assert(
    strcmp(
      micropython_ti_get_builtin_argument_name(first_argument),
      "sub"
    ) == 0
  );
  assert(
    micropython_ti_get_builtin_argument_classes(
      first_argument,
      class_identifiers
    ) == 1
  );
  assert(class_identifiers[0] == MICROPYTHON_TI_CLASS_STR);

  const MicroPythonTiBuiltinArgument *second_argument =
    micropython_ti_get_builtin_argument(method, 1);

  assert(second_argument);
  assert(second_argument->kind == MICROPYTHON_TI_BUILTIN_ARGUMENT_OPTIONAL);
}

static void
test_keyword_argument_kind(void) {
  const MicroPythonTiBuiltinMethod *method =
    micropython_ti_get_builtin_instance_method(
      MICROPYTHON_TI_CLASS_INT,
      (const uint8_t *)"to_bytes",
      8
    );

  assert(method);
  assert(method->argument_count == 3);

  const MicroPythonTiBuiltinArgument *keyword_argument =
    micropython_ti_get_builtin_argument(method, 2);
  uint8_t class_identifiers[4] = {0};

  assert(keyword_argument);
  assert(
    keyword_argument->kind == MICROPYTHON_TI_BUILTIN_ARGUMENT_OPTIONAL_KEYWORD
  );
  assert(
    strcmp(
      micropython_ti_get_builtin_argument_name(keyword_argument),
      "signed"
    ) == 0
  );
  assert(
    micropython_ti_get_builtin_argument_classes(
      keyword_argument,
      class_identifiers
    ) == 1
  );
  assert(class_identifiers[0] == MICROPYTHON_TI_CLASS_BOOL);
}

static void
test_prefix_lookup(void) {
  const MicroPythonTiBuiltinMethod *methods[64];
  int count =
    micropython_ti_collect_builtin_methods_matching_partial_method_name(
      MICROPYTHON_TI_CLASS_STR,
      0,
      (const uint8_t *)"s",
      1,
      methods,
      64
    );

  assert(count > 0);

  int found_split = 0;
  int found_startswith = 0;
  int found_strip = 0;

  for (int index = 0; index < count; index++) {
    const char *name = micropython_ti_get_builtin_method_name(methods[index]);

    assert(name[0] == 's');

    if (index > 0) {
      assert(
        strcmp(
          micropython_ti_get_builtin_method_name(methods[index - 1]),
          name
        ) <= 0
      );
    }

    if (strcmp(name, "split") == 0)
      found_split = 1;
    if (strcmp(name, "startswith") == 0)
      found_startswith = 1;
    if (strcmp(name, "strip") == 0)
      found_strip = 1;
  }

  assert(found_split);
  assert(found_startswith);
  assert(found_strip);
}

static void
test_class_name_lookup(void) {
  assert(
    micropython_ti_get_builtin_class_id((const uint8_t *)"str", 3) ==
    MICROPYTHON_TI_CLASS_STR
  );
  assert(
    micropython_ti_get_builtin_class_id((const uint8_t *)"gc", 2) ==
    MICROPYTHON_TI_CLASS_GC
  );
  assert(
    micropython_ti_get_builtin_class_id((const uint8_t *)"Nothing", 7) ==
    MICROPYTHON_TI_CLASS_NONE
  );
  assert(
    strcmp(
      micropython_ti_get_builtin_class_name(MICROPYTHON_TI_CLASS_NONETYPE),
      "NoneType"
    ) == 0
  );
}

static void
test_pool_boundaries(void) {
  for (
    uint16_t class_id = 1;
    class_id < micropython_ti_builtin_class_count;
    class_id++
  ) {

    const MicroPythonTiBuiltinClass *class_entry =
      &micropython_ti_builtin_classes[class_id];

    assert(class_entry->name_offset < micropython_ti_builtin_name_pool_size);
    assert(memchr(
      micropython_ti_builtin_name_pool + class_entry->name_offset,
      '\0',
      micropython_ti_builtin_name_pool_size - class_entry->name_offset
    ));
  }

  for (
    uint16_t index = 0;
    index < micropython_ti_builtin_method_count;
    index++
  ) {

    const MicroPythonTiBuiltinMethod *method =
      &micropython_ti_builtin_methods[index];

    assert(method->name_offset < micropython_ti_builtin_name_pool_size);
    assert(
      method->signature_offset < micropython_ti_builtin_signature_pool_size
    );
    assert(method->document_offset < micropython_ti_builtin_document_pool_size);
    assert(memchr(
      micropython_ti_builtin_name_pool + method->name_offset,
      '\0',
      micropython_ti_builtin_name_pool_size - method->name_offset
    ));
    assert(memchr(
      micropython_ti_builtin_signature_pool + method->signature_offset,
      '\0',
      micropython_ti_builtin_signature_pool_size - method->signature_offset
    ));
    assert(memchr(
      micropython_ti_builtin_document_pool + method->document_offset,
      '\0',
      micropython_ti_builtin_document_pool_size - method->document_offset
    ));
  }

  for (
    uint16_t index = 0;
    index < micropython_ti_builtin_argument_count;
    index++
  ) {

    const MicroPythonTiBuiltinArgument *argument =
      &micropython_ti_builtin_arguments[index];

    assert(argument->name_offset < micropython_ti_builtin_name_pool_size);

    if (argument->name_offset != 0) {
      assert(memchr(
        micropython_ti_builtin_name_pool + argument->name_offset,
        '\0',
        micropython_ti_builtin_name_pool_size - argument->name_offset
      ));
    }
  }
}

int
main(void) {
  test_str_method_lookup();
  test_inherited_methods();
  test_static_method_is_not_an_instance_method();
  test_type_variable_return_type();
  test_union_return_type();
  test_list_return_variant_type();
  test_builtin_argument_type();
  test_keyword_argument_kind();
  test_prefix_lookup();
  test_class_name_lookup();
  test_pool_boundaries();

  return 0;
}
