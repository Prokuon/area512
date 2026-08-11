#include "micropython_ti_type.h"
#include "micropython_ti_builtin.h"
#include "micropython_ti_define_info.h"
#include "micropython_ti_name.h"
#include "micropython_ti_t.h"
#include <string.h>

static void
write_text(
  char *buffer,
  size_t capacity,
  size_t *length,
  const char *text,
  size_t text_length
) {

  if (!text)
    return;

  for (
    size_t offset = 0;
    offset < text_length && *length + 1 < capacity;
    offset++
  ) {

    buffer[(*length)++] = text[offset];
  }

  if (capacity > 0)
    buffer[*length] = '\0';
}

static void
object_class_to_string(
  uint8_t class_id,
  char *buffer,
  size_t capacity,
  size_t *length
) {

  const char *builtin_name = micropython_ti_get_builtin_class_name(class_id);

  if (builtin_name) {
    write_text(
      buffer,
      capacity,
      length,
      builtin_name,
      strlen(builtin_name)
    );

    return;
  }

  int user_class_index = (int)class_id - MICROPYTHON_TI_CLASS_USER_BASE;
  int current_user_class_index = 0;

  for (int index = 0; index < micropython_ti_get_define_info_count(); index++) {
    MicroPythonTiDefineInfo *define_info =
      micropython_ti_get_define_info(index);

    if (!define_info || !define_info->is_class)
      continue;

    if (current_user_class_index++ != user_class_index)
      continue;

    const MicroPythonTiName *name =
      micropython_ti_get_name(define_info->name_id);

    const uint8_t *name_bytes = micropython_ti_get_name_bytes(name);

    if (!name_bytes)
      break;

    write_text(
      buffer,
      capacity,
      length,
      (const char *)name_bytes,
      name->byte_length
    );

    return;
  }

  write_text(
    buffer,
    capacity,
    length,
    "untyped",
    sizeof("untyped") - 1
  );
}

static void type_to_string(
  const MicroPythonTiT *t,
  char *buffer,
  size_t capacity,
  size_t *length
);

static void
sequence_type_to_string(
  const MicroPythonTiT *t,
  char *buffer,
  size_t capacity,
  size_t *length
) {

  object_class_to_string(
    t->object_class_id,
    buffer,
    capacity,
    length
  );

  write_text(buffer, capacity, length, "<", sizeof("<") - 1);

  const MicroPythonTiT *variant =
    micropython_ti_get_t(t->variants);

  if (variant)
    type_to_string(variant, buffer, capacity, length);

  write_text(buffer, capacity, length, ">", sizeof(">") - 1);
}

static void
type_to_string(
  const MicroPythonTiT *t,
  char *buffer,
  size_t capacity,
  size_t *length
) {

  if (
    (
      t->object_class_id == MICROPYTHON_TI_CLASS_LIST ||
      t->object_class_id == MICROPYTHON_TI_CLASS_TUPLE
    ) &&
    t->variants != 0
  ) {

    sequence_type_to_string(t, buffer, capacity, length);

    return;
  }

  object_class_to_string(
    t->object_class_id,
    buffer,
    capacity,
    length
  );
}

static void
union_type_to_string(
  const MicroPythonTiT *first_t_node,
  char *buffer,
  size_t capacity,
  size_t *length
) {

  write_text(
    buffer,
    capacity,
    length,
    "Union<",
    sizeof("Union<") - 1
  );

  for (
    const MicroPythonTiT *current = first_t_node;
    current;
    current = micropython_ti_get_t(current->union_next)
  ) {

    if (current != first_t_node)
      write_text(
        buffer,
        capacity,
        length,
        " ",
        sizeof(" ") - 1
      );

    type_to_string(current, buffer, capacity, length);
  }

  write_text(buffer, capacity, length, ">", sizeof(">") - 1);
}

int
micropython_ti_type_to_string(
  uint16_t t_node_index,
  char *buffer,
  size_t capacity
) {

  if (!buffer || capacity == 0)
    return 0;

  buffer[0] = '\0';

  const MicroPythonTiT *first_t_node = micropython_ti_get_t(t_node_index);

  if (!first_t_node)
    return 0;

  size_t length = 0;

  if (first_t_node->union_next != 0)
    union_type_to_string(first_t_node, buffer, capacity, &length);
  else
    type_to_string(first_t_node, buffer, capacity, &length);

  return (int)length;
}
