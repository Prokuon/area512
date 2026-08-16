#include "micropython_ti_t_frame.h"
#include "micropython_ti_builtin_database.h"
#include "micropython_ti_t.h"
#include "picoruby_ti_arena.h"
#include <stddef.h>
#include <string.h>

#define MICROPYTHON_TI_T_FRAME_CAPACITY 512

typedef struct {
  uint16_t name_id;
  uint16_t t_node_index;
  uint8_t object_class_id;
  uint8_t is_method;
} MicroPythonTiTFrameEntry;

static MicroPythonTiTFrameEntry *t_frame;

int
micropython_ti_initialize_t_frame(void) {
  t_frame =
    ti_allocate_from_arena(
      sizeof(MicroPythonTiTFrameEntry) * MICROPYTHON_TI_T_FRAME_CAPACITY
    );

  if (!t_frame)
    return 0;

  memset(
    t_frame,
    0,
    sizeof(MicroPythonTiTFrameEntry) * MICROPYTHON_TI_T_FRAME_CAPACITY
  );

  return 1;
}

static MicroPythonTiTFrameEntry *
find_t_frame_entry(uint8_t object_class_id, uint16_t name_id, int is_method) {
  unsigned int index =
    (name_id + object_class_id + (unsigned int)is_method) %
    MICROPYTHON_TI_T_FRAME_CAPACITY;

  for (
    int checked_slot_count = 0;
    checked_slot_count < MICROPYTHON_TI_T_FRAME_CAPACITY;
    checked_slot_count++
  ) {

    MicroPythonTiTFrameEntry *entry = &t_frame[index];

    if (
      entry->name_id == 0 ||
      (
        entry->name_id == name_id &&
        entry->object_class_id == object_class_id &&
        entry->is_method == is_method
      )
    ) {

      return entry;
    }

    index = (index + 1U) % MICROPYTHON_TI_T_FRAME_CAPACITY;
  }

  return NULL;
}

int
micropython_ti_set_value_t(uint16_t name_id, uint16_t t_node_index) {
  if (name_id == 0 || t_node_index == 0)
    return 1;

  MicroPythonTiTFrameEntry *entry =
    find_t_frame_entry(
      MICROPYTHON_TI_CLASS_NONE,
      name_id,
      0
    );

  if (!entry)
    return 0;

  if (entry->name_id == 0) {
    entry->name_id = name_id;
    entry->t_node_index = t_node_index;

    return 1;
  }

  uint16_t union_t_node_index =
    micropython_ti_make_union(
      entry->t_node_index,
      t_node_index
    );

  if (union_t_node_index == 0)
    return 0;

  entry->t_node_index = union_t_node_index;

  return 1;
}

uint16_t
micropython_ti_get_value_t(uint16_t name_id) {
  if (!t_frame || name_id == 0)
    return 0;

  MicroPythonTiTFrameEntry *entry =
    find_t_frame_entry(
      MICROPYTHON_TI_CLASS_NONE,
      name_id,
      0
    );

  if (!entry || entry->name_id != name_id)
    return 0;

  return entry->t_node_index;
}

int
micropython_ti_set_method_t(
  uint8_t object_class_id,
  uint16_t name_id,
  uint16_t t_node_index
) {

  if (name_id == 0 || t_node_index == 0)
    return 1;

  MicroPythonTiTFrameEntry *entry =
    find_t_frame_entry(object_class_id, name_id, 1);

  if (!entry)
    return 0;

  if (entry->name_id == 0) {
    entry->name_id = name_id;
    entry->t_node_index = t_node_index;
    entry->object_class_id = object_class_id;
    entry->is_method = 1;

    return 1;
  }

  uint16_t union_t_node_index =
    micropython_ti_make_union(
      entry->t_node_index,
      t_node_index
    );

  if (union_t_node_index == 0)
    return 0;

  entry->t_node_index = union_t_node_index;

  return 1;
}

uint16_t
micropython_ti_get_method_t(uint8_t object_class_id, uint16_t name_id) {
  if (!t_frame || name_id == 0)
    return 0;

  MicroPythonTiTFrameEntry *entry =
    find_t_frame_entry(object_class_id, name_id, 1);

  if (!entry || entry->name_id != name_id)
    return 0;

  return entry->t_node_index;
}
