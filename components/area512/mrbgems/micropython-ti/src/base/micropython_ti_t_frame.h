#ifndef MICROPYTHON_TI_T_FRAME_H
#define MICROPYTHON_TI_T_FRAME_H

#include <stdint.h>

int micropython_ti_initialize_t_frame(void);
int micropython_ti_set_value_t(uint16_t name_id, uint16_t t_node_index);
uint16_t micropython_ti_get_value_t(uint16_t name_id);
int micropython_ti_set_method_t(
  uint8_t object_class_id,
  uint16_t name_id,
  uint16_t t_node_index
);
uint16_t micropython_ti_get_method_t(uint8_t object_class_id, uint16_t name_id);

#endif
