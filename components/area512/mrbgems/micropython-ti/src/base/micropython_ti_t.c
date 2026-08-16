#include "micropython_ti_t.h"
#include "micropython_ti_builtin_database.h"
#include "picoruby_ti_arena.h"
#include <stddef.h>

static MicroPythonTiT *t_nodes;
static uint16_t t_count;

int
micropython_ti_initialize_t(void) {
  t_nodes =
    ti_allocate_from_arena(
      sizeof(MicroPythonTiT) * MICROPYTHON_TI_T_CAPACITY
    );

  if (!t_nodes)
    return 0;

  t_count = 1;
  t_nodes[0] = (MicroPythonTiT){0};

  return 1;
}

static int
is_equal_t_with_next(
  const MicroPythonTiT *t_node,
  uint8_t object_class_id,
  uint8_t t_flags,
  uint16_t variants,
  uint16_t union_next
) {

  return t_node->object_class_id == object_class_id &&
    t_node->t_flags == t_flags && t_node->variants == variants &&
    t_node->union_next == union_next;
}

static uint16_t
new_t_with_next(
  uint8_t object_class_id,
  uint8_t t_flags,
  uint16_t variants,
  uint16_t union_next
) {

  for (uint16_t index = 1; index < t_count; index++) {
    if (
      is_equal_t_with_next(
        &t_nodes[index],
        object_class_id,
        t_flags,
        variants,
        union_next
      )
    ) {

      return index;
    }
  }

  if (t_count >= MICROPYTHON_TI_T_CAPACITY)
    return 0;

  uint16_t index = t_count++;

  t_nodes[index].object_class_id = object_class_id;
  t_nodes[index].t_flags = t_flags;
  t_nodes[index].variants = variants;
  t_nodes[index].union_next = union_next;

  return index;
}

uint16_t
micropython_ti_new_t(
  uint8_t object_class_id,
  uint8_t t_flags,
  uint16_t variants
) {

  if (object_class_id == 0)
    return 0;

  return new_t_with_next(object_class_id, t_flags, variants, 0);
}

static int
is_equal_t(const MicroPythonTiT *first, const MicroPythonTiT *second) {
  return first->object_class_id == second->object_class_id &&
    first->t_flags == second->t_flags &&
    first->variants == second->variants;
}

static int
contains_union_next(uint16_t t_node_index, const MicroPythonTiT *union_next_t) {
  while (t_node_index != 0) {
    const MicroPythonTiT *current = &t_nodes[t_node_index];

    if (is_equal_t(current, union_next_t))
      return 1;

    t_node_index = current->union_next;
  }

  return 0;
}

static uint16_t
append_union_next(uint16_t t_node_index, const MicroPythonTiT *union_next_t) {
  uint16_t indexes[MICROPYTHON_TI_UNION_CAPACITY];
  uint16_t count = 0;

  while (t_node_index != 0) {
    indexes[count++] = t_node_index;
    t_node_index = t_nodes[t_node_index].union_next;
  }

  if (count >= MICROPYTHON_TI_UNION_CAPACITY)
    return micropython_ti_new_t(
      MICROPYTHON_TI_CLASS_UNTYPED,
      0,
      0
    );

  uint16_t union_next_t_node_index =
    new_t_with_next(
      union_next_t->object_class_id,
      union_next_t->t_flags,
      union_next_t->variants,
      0
    );

  if (union_next_t_node_index == 0)
    return 0;

  while (count > 0) {
    const MicroPythonTiT *current = &t_nodes[indexes[--count]];

    union_next_t_node_index =
      new_t_with_next(
        current->object_class_id,
        current->t_flags,
        current->variants,
        union_next_t_node_index
      );

    if (union_next_t_node_index == 0)
      return 0;
  }

  return union_next_t_node_index;
}

uint16_t
micropython_ti_make_union(
  uint16_t first_t_node_index,
  uint16_t second_t_node_index
) {

  if (first_t_node_index == 0)
    return second_t_node_index;

  if (second_t_node_index == 0 || first_t_node_index == second_t_node_index)
    return first_t_node_index;

  if (t_nodes[first_t_node_index].object_class_id ==
      MICROPYTHON_TI_CLASS_UNTYPED)
    return first_t_node_index;

  uint16_t result = first_t_node_index;
  uint16_t union_next_t_node_index = second_t_node_index;

  while (union_next_t_node_index != 0) {
    MicroPythonTiT union_next_t = t_nodes[union_next_t_node_index];

    if (union_next_t.object_class_id == MICROPYTHON_TI_CLASS_UNTYPED)
      return micropython_ti_new_t(
        MICROPYTHON_TI_CLASS_UNTYPED,
        0,
        0
      );

    if (
      !contains_union_next(result, &union_next_t)
    ) {

      result =
        append_union_next(result, &union_next_t);

      if (result == 0)
        return 0;

      if (t_nodes[result].object_class_id == MICROPYTHON_TI_CLASS_UNTYPED)
        return result;
    }

    union_next_t_node_index = union_next_t.union_next;
  }

  return result;
}

const MicroPythonTiT *
micropython_ti_get_t(uint16_t t_node_index) {
  if (t_node_index == 0 || t_node_index >= t_count)
    return NULL;

  return &t_nodes[t_node_index];
}

int
micropython_ti_get_t_count(void) {
  return t_count;
}
