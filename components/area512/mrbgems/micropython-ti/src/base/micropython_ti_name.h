#ifndef MICROPYTHON_TI_NAME_H
#define MICROPYTHON_TI_NAME_H

#include <stddef.h>
#include <stdint.h>

#define MICROPYTHON_TI_NAME_CAPACITY 512
#define MICROPYTHON_TI_NAME_BYTE_CAPACITY 4096

typedef struct {
  uint16_t byte_offset;
  uint16_t byte_length;
} MicroPythonTiName;

int micropython_ti_initialize_names(void);
int micropython_ti_intern_name(
  const uint8_t *name,
  size_t name_byte_length,
  uint16_t *name_id
);
const MicroPythonTiName *micropython_ti_get_name(uint16_t name_id);
const uint8_t *micropython_ti_get_name_bytes(const MicroPythonTiName *name);

#endif
