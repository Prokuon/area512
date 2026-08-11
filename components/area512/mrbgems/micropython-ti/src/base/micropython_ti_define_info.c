#include "micropython_ti_define_info.h"
#include "micropython_ti_builtin_database.h"
#include "picoruby_ti_arena.h"
#include <stddef.h>
#include <string.h>

static MicroPythonTiDefineInfo *define_infos;
static int define_info_count;

int
micropython_ti_initialize_define_infos(void) {
  define_infos =
    ti_allocate_from_arena(
      sizeof(MicroPythonTiDefineInfo) * MICROPYTHON_TI_DEFINE_INFO_CAPACITY
    );

  if (!define_infos)
    return 0;

  define_info_count = 0;

  return 1;
}

MicroPythonTiDefineInfo *
micropython_ti_set_define_info(
  uint16_t name_id,
  uint16_t owner_class_name_id,
  uint16_t define_row,
  int is_class
) {

  if (name_id == 0)
    return NULL;

  for (int index = 0; index < define_info_count; index++) {
    MicroPythonTiDefineInfo *define_info = &define_infos[index];

    if (
      define_info->name_id == name_id &&
      define_info->owner_class_name_id == owner_class_name_id &&
      define_info->is_class == is_class
    ) {

      return define_info;
    }
  }

  if (define_info_count >= MICROPYTHON_TI_DEFINE_INFO_CAPACITY)
    return NULL;

  MicroPythonTiDefineInfo *define_info = &define_infos[define_info_count++];

  memset(define_info, 0, sizeof(*define_info));

  define_info->name_id = name_id;
  define_info->owner_class_name_id = owner_class_name_id;
  define_info->define_row = define_row;
  define_info->is_class = is_class ? 1U : 0U;

  return define_info;
}

MicroPythonTiDefineInfo *
micropython_ti_get_define_info(int index) {
  if (index < 0 || index >= define_info_count)
    return NULL;

  return &define_infos[index];
}

int
micropython_ti_get_define_info_count(void) {
  return define_info_count;
}

uint8_t
micropython_ti_get_defined_class_id(uint16_t name_id) {
  int user_class_index = 0;

  for (int index = 0; index < define_info_count; index++) {
    MicroPythonTiDefineInfo *define_info = &define_infos[index];

    if (!define_info->is_class)
      continue;

    if (define_info->name_id == name_id) {
      return (uint8_t)(MICROPYTHON_TI_CLASS_USER_BASE + user_class_index);
    }

    user_class_index++;
  }

  return MICROPYTHON_TI_CLASS_NONE;
}
