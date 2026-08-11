#include "py/runtime.h"

#include <stdint.h>

// Declared here rather than through picoruby-adc/include/adc.h: that header
// includes picoruby.h, which pulls in mrubyc, mrc and prism. The linker
// resolves these across components. ADC_read_voltage returns picorb_float_t,
// which MRBC_USE_FLOAT=2 makes double.
extern int ADC_pin_num_from_char(const uint8_t *pin_name);
extern int ADC_init(uint8_t pin_number);
extern uint32_t ADC_read_raw(uint8_t pin_number);
extern double ADC_read_voltage(uint8_t pin_number);

typedef struct {
  mp_obj_base_t base;
  int adc_pin_number;
} area512_micropython_adc_instance_t;

static uint8_t
fetch_adc_pin_number_or_raise(mp_obj_t pin_object) {
  int pin_number;

  if (mp_obj_is_str(pin_object))
    pin_number =
      ADC_pin_num_from_char((const uint8_t *)mp_obj_str_get_str(pin_object));
  else if (mp_obj_is_int(pin_object))
    pin_number = mp_obj_get_int(pin_object);
  else
    pin_number = -1;

  if (pin_number < 0)
    mp_raise_msg(&mp_type_ValueError, MP_ERROR_TEXT("Wrong ADC pin value"));

  return (uint8_t)pin_number;
}

static mp_obj_t
create_adc_object(
  const mp_obj_type_t *adc_type,
  size_t argument_count,
  size_t keyword_argument_count,
  const mp_obj_t *arguments
) {

  mp_arg_check_num(argument_count, keyword_argument_count, 1, 1, false);

  int adc_pin_number = ADC_init(fetch_adc_pin_number_or_raise(arguments[0]));

  if (adc_pin_number < 0)
    mp_raise_msg(&mp_type_ValueError, MP_ERROR_TEXT("Wrong ADC pin value"));

  area512_micropython_adc_instance_t *adc_instance =
    mp_obj_malloc(area512_micropython_adc_instance_t, adc_type);

  adc_instance->adc_pin_number = adc_pin_number;

  return MP_OBJ_FROM_PTR(adc_instance);
}

static mp_obj_t
fetch_adc_input(mp_obj_t adc_object) {
  area512_micropython_adc_instance_t *adc_instance = MP_OBJ_TO_PTR(adc_object);

  return MP_OBJ_NEW_SMALL_INT(adc_instance->adc_pin_number);
}
static MP_DEFINE_CONST_FUN_OBJ_1(fetch_adc_input_callable, fetch_adc_input);

static mp_obj_t
read_adc_voltage(mp_obj_t adc_object) {
  area512_micropython_adc_instance_t *adc_instance = MP_OBJ_TO_PTR(adc_object);

  uint8_t pin_number = (uint8_t)adc_instance->adc_pin_number;

  return mp_obj_new_float((mp_float_t)ADC_read_voltage(pin_number));
}
static MP_DEFINE_CONST_FUN_OBJ_1(read_adc_voltage_callable, read_adc_voltage);

static mp_obj_t
read_adc_raw(mp_obj_t adc_object) {
  area512_micropython_adc_instance_t *adc_instance = MP_OBJ_TO_PTR(adc_object);

  uint8_t pin_number = (uint8_t)adc_instance->adc_pin_number;

  return mp_obj_new_int_from_uint(ADC_read_raw(pin_number));
}
static MP_DEFINE_CONST_FUN_OBJ_1(read_adc_raw_callable, read_adc_raw);

static const mp_rom_map_elem_t adc_locals_table[] = {
  {MP_ROM_QSTR(MP_QSTR_input), MP_ROM_PTR(&fetch_adc_input_callable)},
  {MP_ROM_QSTR(MP_QSTR_read), MP_ROM_PTR(&read_adc_voltage_callable)},
  {MP_ROM_QSTR(MP_QSTR_read_voltage), MP_ROM_PTR(&read_adc_voltage_callable)},
  {MP_ROM_QSTR(MP_QSTR_read_raw), MP_ROM_PTR(&read_adc_raw_callable)},
};
static MP_DEFINE_CONST_DICT(adc_locals_dictionary, adc_locals_table);

MP_DEFINE_CONST_OBJ_TYPE(
  area512_micropython_adc_type,
  MP_QSTR_ADC,
  MP_TYPE_FLAG_NONE,
  make_new,
  create_adc_object,
  locals_dict,
  &adc_locals_dictionary
);
