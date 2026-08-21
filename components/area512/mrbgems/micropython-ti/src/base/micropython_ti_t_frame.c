#include "micropython_ti_t_frame.h"
#include "micropython_ti_builtin_database.h"
#include "micropython_ti_t.h"
#include "picoruby_ti_arena.h"
#include <stddef.h>
#include <string.h>

#define MICROPYTHON_TI_T_FRAME_CAPACITY 512

typedef enum {
  MICROPYTHON_TI_T_FRAME_VALUE,
  MICROPYTHON_TI_T_FRAME_METHOD,
  MICROPYTHON_TI_T_FRAME_INSTANCE_ATTRIBUTE,
} MicroPythonTiTFrameEntryKind;

typedef struct {
  uint16_t name_id;
  uint16_t t_node_index;
  uint16_t owner_class_name_id;
  uint16_t owner_define_name_id;
  uint8_t object_class_id;
  uint8_t entry_kind;
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
find_t_frame_entry(
  uint8_t object_class_id,
  uint16_t owner_class_name_id,
  uint16_t owner_define_name_id,
  uint16_t name_id,
  MicroPythonTiTFrameEntryKind entry_kind
) {

  unsigned int slot_index =
    (
      name_id +
      object_class_id +
      owner_class_name_id +
      owner_define_name_id +
      (unsigned int)entry_kind
    ) %
    MICROPYTHON_TI_T_FRAME_CAPACITY;

  for (
    int checked_slot_count = 0;
    checked_slot_count < MICROPYTHON_TI_T_FRAME_CAPACITY;
    checked_slot_count++
  ) {

    MicroPythonTiTFrameEntry *entry = &t_frame[slot_index];

    if (
      entry->name_id == 0 ||
      (
        entry->name_id == name_id &&
        entry->object_class_id == object_class_id &&
        entry->owner_class_name_id == owner_class_name_id &&
        entry->owner_define_name_id == owner_define_name_id &&
        entry->entry_kind == entry_kind
      )
    ) {

      return entry;
    }

    slot_index = (slot_index + 1U) % MICROPYTHON_TI_T_FRAME_CAPACITY;
  }

  return NULL;
}

static uint16_t
get_scoped_value_t(
  uint16_t owner_class_name_id,
  uint16_t owner_define_name_id,
  uint16_t name_id
) {

  if (!t_frame || name_id == 0)
    return 0;

  MicroPythonTiTFrameEntry *entry =
    find_t_frame_entry(
      MICROPYTHON_TI_CLASS_NONE,
      owner_class_name_id,
      owner_define_name_id,
      name_id,
      MICROPYTHON_TI_T_FRAME_VALUE
    );

  if (!entry || entry->name_id != name_id)
    return 0;

  return entry->t_node_index;
}

int
micropython_ti_set_value_t(
  uint16_t owner_class_name_id,
  uint16_t owner_define_name_id,
  uint16_t name_id,
  uint16_t t_node_index
) {

  if (name_id == 0 || t_node_index == 0)
    return 1;

  MicroPythonTiTFrameEntry *entry =
    find_t_frame_entry(
      MICROPYTHON_TI_CLASS_NONE,
      owner_class_name_id,
      owner_define_name_id,
      name_id,
      MICROPYTHON_TI_T_FRAME_VALUE
    );

  if (!entry)
    return 0;

  entry->name_id = name_id;
  entry->owner_class_name_id = owner_class_name_id;
  entry->owner_define_name_id = owner_define_name_id;
  entry->t_node_index = t_node_index;

  return 1;
}

int
micropython_ti_merge_value_t(
  uint16_t owner_class_name_id,
  uint16_t owner_define_name_id,
  uint16_t name_id,
  uint16_t t_node_index
) {

  return micropython_ti_set_value_t(
    owner_class_name_id,
    owner_define_name_id,
    name_id,
    micropython_ti_make_union(
      get_scoped_value_t(
        owner_class_name_id,
        owner_define_name_id,
        name_id
      ),
      t_node_index
    )
  );
}

uint16_t
micropython_ti_get_value_t(
  uint16_t owner_class_name_id,
  uint16_t owner_define_name_id,
  uint16_t name_id
) {

  uint16_t t_node_index =
    get_scoped_value_t(owner_class_name_id, owner_define_name_id, name_id);

  if (t_node_index != 0)
    return t_node_index;

  if (owner_class_name_id == 0 && owner_define_name_id == 0)
    return 0;

  return get_scoped_value_t(
    0,
    0,
    name_id
  );
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
    find_t_frame_entry(
      object_class_id,
      0,
      0,
      name_id,
      MICROPYTHON_TI_T_FRAME_METHOD
    );

  if (!entry)
    return 0;

  entry->name_id = name_id;
  entry->object_class_id = object_class_id;
  entry->entry_kind = MICROPYTHON_TI_T_FRAME_METHOD;
  entry->t_node_index = t_node_index;

  return 1;
}

uint16_t
micropython_ti_get_method_t(
  uint8_t object_class_id,
  uint16_t name_id
) {

  if (!t_frame || name_id == 0)
    return 0;

  MicroPythonTiTFrameEntry *entry =
    find_t_frame_entry(
      object_class_id,
      0,
      0,
      name_id,
      MICROPYTHON_TI_T_FRAME_METHOD
    );

  if (!entry || entry->name_id != name_id)
    return 0;

  return entry->t_node_index;
}

static int
set_instance_attribute_t(
  uint8_t object_class_id,
  uint16_t name_id,
  uint16_t t_node_index
) {

  if (
    object_class_id == MICROPYTHON_TI_CLASS_NONE ||
    name_id == 0 ||
    t_node_index == 0
  ) {

    return 1;
  }

  MicroPythonTiTFrameEntry *entry =
    find_t_frame_entry(
      object_class_id,
      0,
      0,
      name_id,
      MICROPYTHON_TI_T_FRAME_INSTANCE_ATTRIBUTE
    );

  if (!entry)
    return 0;

  entry->name_id = name_id;
  entry->object_class_id = object_class_id;
  entry->entry_kind = MICROPYTHON_TI_T_FRAME_INSTANCE_ATTRIBUTE;
  entry->t_node_index = t_node_index;

  return 1;
}

int
micropython_ti_merge_instance_attribute_t(
  uint8_t object_class_id,
  uint16_t name_id,
  uint16_t t_node_index
) {

  return set_instance_attribute_t(
    object_class_id,
    name_id,
    micropython_ti_make_union(
      micropython_ti_get_instance_attribute_t(
        object_class_id,
        name_id
      ),
      t_node_index
    )
  );
}

uint16_t
micropython_ti_get_instance_attribute_t(
  uint8_t object_class_id,
  uint16_t name_id
) {

  if (!t_frame || name_id == 0)
    return 0;

  MicroPythonTiTFrameEntry *entry =
    find_t_frame_entry(
      object_class_id,
      0,
      0,
      name_id,
      MICROPYTHON_TI_T_FRAME_INSTANCE_ATTRIBUTE
    );

  if (!entry || entry->name_id != name_id)
    return 0;

  return entry->t_node_index;
}

int
micropython_ti_find_instance_attribute_and_advance_slot(
  uint8_t object_class_id,
  int *search_slot_index,
  uint16_t *name_id,
  uint16_t *t_node_index
) {

  if (
    !t_frame ||
    !search_slot_index ||
    !name_id ||
    !t_node_index ||
    *search_slot_index < 0
  ) {

    return 0;
  }

  for (
    int slot_index = *search_slot_index;
    slot_index < MICROPYTHON_TI_T_FRAME_CAPACITY;
    slot_index++
  ) {

    const MicroPythonTiTFrameEntry *entry = &t_frame[slot_index];

    if (
      entry->name_id == 0 ||
      entry->entry_kind != MICROPYTHON_TI_T_FRAME_INSTANCE_ATTRIBUTE ||
      entry->object_class_id != object_class_id
    ) {

      continue;
    }

    *name_id = entry->name_id;
    *t_node_index = entry->t_node_index;
    *search_slot_index = slot_index + 1;

    return 1;
  }

  return 0;
}
