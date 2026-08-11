#include "micropython_ti_name.h"
#include "picoruby_ti_arena.h"
#include <string.h>

static MicroPythonTiName *names;
static uint8_t *name_bytes;
static int name_count;
static uint16_t name_byte_length;

int
micropython_ti_initialize_names(void) {
  names =
    ti_allocate_from_arena(
      sizeof(MicroPythonTiName) * MICROPYTHON_TI_NAME_CAPACITY
    );

  name_bytes =
    ti_allocate_from_arena(MICROPYTHON_TI_NAME_BYTE_CAPACITY);

  if (!names || !name_bytes)
    return 0;

  name_count = 0;
  name_byte_length = 0;

  return 1;
}

int
micropython_ti_intern_name(
  const uint8_t *name,
  size_t name_byte_length_to_intern,
  uint16_t *name_id
) {

  if (
    !name ||
    name_byte_length_to_intern == 0 ||
    name_byte_length_to_intern > UINT16_MAX ||
    !name_id
  ) {

    return 0;
  }

  for (int index = 0; index < name_count; index++) {
    const MicroPythonTiName *interned_name = &names[index];

    if (
      interned_name->byte_length == name_byte_length_to_intern &&
      memcmp(
        name_bytes + interned_name->byte_offset,
        name,
        name_byte_length_to_intern
      ) == 0
    ) {

      *name_id = (uint16_t)(index + 1);

      return 1;
    }
  }

  if (
    name_count >= MICROPYTHON_TI_NAME_CAPACITY ||
    name_byte_length_to_intern >
    (size_t)(MICROPYTHON_TI_NAME_BYTE_CAPACITY - name_byte_length)
  ) {

    return 0;
  }

  memcpy(name_bytes + name_byte_length, name, name_byte_length_to_intern);

  MicroPythonTiName *interned_name = &names[name_count++];
  interned_name->byte_offset = name_byte_length;
  interned_name->byte_length = (uint16_t)name_byte_length_to_intern;

  name_byte_length = (uint16_t)(name_byte_length + name_byte_length_to_intern);

  *name_id = (uint16_t)name_count;

  return 1;
}

const MicroPythonTiName *
micropython_ti_get_name(uint16_t name_id) {
  if (name_id == 0 || name_id > name_count)
    return NULL;

  return &names[name_id - 1];
}

const uint8_t *
micropython_ti_get_name_bytes(const MicroPythonTiName *name) {
  if (!name || name->byte_offset >= name_byte_length)
    return NULL;

  return name_bytes + name->byte_offset;
}
