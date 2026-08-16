#ifndef MICROPYTHON_TI_TYPE_H
#define MICROPYTHON_TI_TYPE_H

#include <stddef.h>
#include <stdint.h>

#define MICROPYTHON_TI_TYPE_STRING_CAPACITY 96

int micropython_ti_type_to_string(
  uint16_t t_node_index,
  char *buffer,
  size_t capacity
);

#endif
