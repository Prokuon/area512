#ifndef MICROPYTHON_TI_BUILTIN_H
#define MICROPYTHON_TI_BUILTIN_H

#include "micropython_ti_builtin_database.h"
#include <stddef.h>
#include <stdint.h>

uint8_t micropython_ti_get_builtin_class_id(const uint8_t *name, size_t length);
const MicroPythonTiBuiltinMethod *micropython_ti_get_builtin_instance_method(
  uint8_t class_id,
  const uint8_t *name,
  size_t length
);
const MicroPythonTiBuiltinMethod *micropython_ti_get_builtin_static_method(
  uint8_t class_id,
  const uint8_t *name,
  size_t length
);
const MicroPythonTiBuiltinArgument *micropython_ti_get_builtin_argument(
  const MicroPythonTiBuiltinMethod *method,
  int argument_index
);
const char *micropython_ti_get_builtin_argument_name(
  const MicroPythonTiBuiltinArgument *argument
);
int micropython_ti_get_builtin_argument_classes(
  const MicroPythonTiBuiltinArgument *argument,
  uint8_t out_class_ids[4]
);
int micropython_ti_collect_builtin_methods_matching_partial_method_name(
  uint8_t class_id,
  int use_static_methods,
  const uint8_t *partial_method_name,
  size_t partial_method_name_length,
  const MicroPythonTiBuiltinMethod **out,
  int out_capacity
);
const char *
micropython_ti_get_builtin_method_name(const MicroPythonTiBuiltinMethod *method
);
const char *
micropython_ti_get_builtin_signature(const MicroPythonTiBuiltinMethod *method);
const char *
micropython_ti_get_builtin_document(const MicroPythonTiBuiltinMethod *method);
const char *micropython_ti_get_builtin_class_name(uint8_t class_id);
int micropython_ti_get_builtin_return_classes(
  const MicroPythonTiBuiltinMethod *method,
  uint8_t out_class_ids[4]
);
int micropython_ti_get_builtin_return_array_variant_classes(
  const MicroPythonTiBuiltinMethod *method,
  uint8_t out_class_ids[4]
);

#endif
