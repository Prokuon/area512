#ifndef MICROPYTHON_TI_DEFINE_INFO_H
#define MICROPYTHON_TI_DEFINE_INFO_H

#include <stdint.h>

#define MICROPYTHON_TI_DEFINE_INFO_CAPACITY 64
#define MICROPYTHON_TI_DEFINE_ARG_CAPACITY 8

typedef enum {
  MICROPYTHON_TI_DEFINE_ARG_REQUIRED,
  MICROPYTHON_TI_DEFINE_ARG_REST,
  MICROPYTHON_TI_DEFINE_ARG_KEYWORD_REST,
} MicroPythonTiDefineArgKind;

typedef struct {
  uint16_t name_id;
  uint16_t owner_class_name_id;
  uint16_t return_t_node_index;
  uint16_t define_row;
  uint8_t define_arg_count;
  uint8_t is_class;
  uint16_t define_arg_name_ids[MICROPYTHON_TI_DEFINE_ARG_CAPACITY];
  MicroPythonTiDefineArgKind
    define_arg_kinds[MICROPYTHON_TI_DEFINE_ARG_CAPACITY];
} MicroPythonTiDefineInfo;

int micropython_ti_initialize_define_infos(void);
MicroPythonTiDefineInfo *micropython_ti_set_define_info(
  uint16_t name_id,
  uint16_t owner_class_name_id,
  uint16_t define_row,
  int is_class
);
MicroPythonTiDefineInfo *micropython_ti_get_define_info(int index);
int micropython_ti_get_define_info_count(void);
uint8_t micropython_ti_get_defined_class_id(uint16_t name_id);

#endif
