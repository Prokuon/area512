#include "py/runtime.h"

#include <stdbool.h>
#include <stdint.h>

#include "gpio.h"

typedef struct {
  mp_obj_base_t base;
  mp_obj_t pin_object;
  int direction_flags;
  int alt_function;
  int pull_flags;
  bool is_open_drain;
  bool is_initializing;
} area512_micropython_gpio_instance_t;

static uint8_t
fetch_pin_number_or_raise(mp_obj_t pin_object) {
  int pin_number;

  if (mp_obj_is_str(pin_object))
    pin_number =
      GPIO_pin_num_from_char((const uint8_t *)mp_obj_str_get_str(pin_object));
  else if (mp_obj_is_int(pin_object))
    pin_number = mp_obj_get_int(pin_object);
  else
    pin_number = -1;

  if (pin_number < 0)
    mp_raise_msg(&mp_type_ValueError, MP_ERROR_TEXT("Wrong GPIO pin value"));

  return (uint8_t)pin_number;
}

static mp_obj_t
set_gpio_direction(mp_obj_t gpio_object, mp_obj_t flags_object) {
  area512_micropython_gpio_instance_t *gpio_instance =
    MP_OBJ_TO_PTR(gpio_object);
  int flags = mp_obj_get_int(flags_object);
  int direction_flags = flags & (IN | OUT | HIGH_Z);

  if (direction_flags == 0 && !gpio_instance->is_initializing)
    return MP_OBJ_NEW_SMALL_INT(0);

  int direction_flag_count =
    (flags & IN) + ((flags & OUT) >> 1) + ((flags & HIGH_Z) >> 2);

  if (direction_flag_count > 1)
    mp_raise_msg(
      &mp_type_ValueError,
      MP_ERROR_TEXT("IN, OUT and HIGH_Z are exclusive")
    );

  if (direction_flag_count == 1)
    GPIO_set_dir(
      fetch_pin_number_or_raise(gpio_instance->pin_object),
      (uint8_t)direction_flags
    );

  gpio_instance->direction_flags = direction_flags;

  return MP_OBJ_NEW_SMALL_INT(0);
}
static MP_DEFINE_CONST_FUN_OBJ_2(
  set_gpio_direction_callable,
  set_gpio_direction
);

static mp_obj_t
set_gpio_pull(mp_obj_t gpio_object, mp_obj_t flags_object) {
  area512_micropython_gpio_instance_t *gpio_instance =
    MP_OBJ_TO_PTR(gpio_object);
  int pull_flags = mp_obj_get_int(flags_object) & (PULL_UP | PULL_DOWN);

  if (pull_flags == 0 && !gpio_instance->is_initializing)
    return MP_OBJ_NEW_SMALL_INT(0);

  if (pull_flags == PULL_UP)
    GPIO_pull_up(fetch_pin_number_or_raise(gpio_instance->pin_object));
  else if (pull_flags == PULL_DOWN)
    GPIO_pull_down(fetch_pin_number_or_raise(gpio_instance->pin_object));
  else if (pull_flags != 0)
    mp_raise_msg(
      &mp_type_ValueError,
      MP_ERROR_TEXT("PULL_UP and PULL_DOWN are exclusive")
    );

  gpio_instance->pull_flags = pull_flags;

  return MP_OBJ_NEW_SMALL_INT(0);
}
static MP_DEFINE_CONST_FUN_OBJ_2(set_gpio_pull_callable, set_gpio_pull);

static mp_obj_t
set_gpio_open_drain(mp_obj_t gpio_object, mp_obj_t flags_object) {
  area512_micropython_gpio_instance_t *gpio_instance =
    MP_OBJ_TO_PTR(gpio_object);

  gpio_instance->is_open_drain =
    (mp_obj_get_int(flags_object) & OPEN_DRAIN) != 0;

  if (gpio_instance->is_open_drain)
    GPIO_open_drain(fetch_pin_number_or_raise(gpio_instance->pin_object));

  return MP_OBJ_NEW_SMALL_INT(0);
}
static MP_DEFINE_CONST_FUN_OBJ_2(
  set_gpio_open_drain_callable,
  set_gpio_open_drain
);

static mp_obj_t
set_gpio_function(
  mp_obj_t gpio_object,
  mp_obj_t flags_object,
  mp_obj_t alt_function_object
) {

  area512_micropython_gpio_instance_t *gpio_instance =
    MP_OBJ_TO_PTR(gpio_object);
  int flags = mp_obj_get_int(flags_object);
  int alt_function = mp_obj_get_int(alt_function_object);

  if (alt_function > 0 && (flags & ALT) == ALT) {
    GPIO_set_function(
      fetch_pin_number_or_raise(gpio_instance->pin_object),
      (uint8_t)alt_function
    );

    gpio_instance->alt_function = alt_function;

  } else {
    gpio_instance->alt_function = 0;
  }

  return MP_OBJ_NEW_SMALL_INT(0);
}
static MP_DEFINE_CONST_FUN_OBJ_3(set_gpio_function_callable, set_gpio_function);

static mp_obj_t
set_gpio_mode(size_t argument_count, const mp_obj_t *arguments) {
  mp_obj_t alt_function_object =
    argument_count >= 3 ? arguments[2] : MP_OBJ_NEW_SMALL_INT(0);

  set_gpio_direction(arguments[0], arguments[1]);
  set_gpio_pull(arguments[0], arguments[1]);
  set_gpio_open_drain(arguments[0], arguments[1]);
  set_gpio_function(arguments[0], arguments[1], alt_function_object);

  return MP_OBJ_NEW_SMALL_INT(0);
}
static MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(
  set_gpio_mode_callable,
  2,
  3,
  set_gpio_mode
);

static mp_obj_t
create_gpio_object(
  const mp_obj_type_t *gpio_type,
  size_t argument_count,
  size_t keyword_argument_count,
  const mp_obj_t *arguments
) {

  mp_arg_check_num(argument_count, keyword_argument_count, 2, 3, false);

  area512_micropython_gpio_instance_t *gpio_instance =
    mp_obj_malloc(area512_micropython_gpio_instance_t, gpio_type);

  gpio_instance->pin_object = arguments[0];
  gpio_instance->direction_flags = 0;
  gpio_instance->alt_function = 0;
  gpio_instance->pull_flags = 0;
  gpio_instance->is_open_drain = false;
  gpio_instance->is_initializing = true;

  GPIO_init(fetch_pin_number_or_raise(arguments[0]));

  mp_obj_t mode_arguments[3] = {
    MP_OBJ_FROM_PTR(gpio_instance),
    arguments[1],
    argument_count >= 3 ? arguments[2] : MP_OBJ_NEW_SMALL_INT(0),
  };

  set_gpio_mode(3, mode_arguments);

  if (gpio_instance->direction_flags == 0 && gpio_instance->alt_function == 0)
    mp_raise_msg(
      &mp_type_ValueError,
      MP_ERROR_TEXT("You must specify one of IN, OUT, HIGH_Z, and ALT")
    );

  gpio_instance->is_initializing = false;

  return MP_OBJ_FROM_PTR(gpio_instance);
}

static mp_obj_t
fetch_gpio_pin(mp_obj_t gpio_object) {
  area512_micropython_gpio_instance_t *gpio_instance =
    MP_OBJ_TO_PTR(gpio_object);

  return gpio_instance->pin_object;
}
static MP_DEFINE_CONST_FUN_OBJ_1(fetch_gpio_pin_callable, fetch_gpio_pin);

static mp_obj_t
read_gpio(mp_obj_t gpio_object) {
  area512_micropython_gpio_instance_t *gpio_instance =
    MP_OBJ_TO_PTR(gpio_object);

  return MP_OBJ_NEW_SMALL_INT(
    GPIO_read(fetch_pin_number_or_raise(gpio_instance->pin_object))
  );
}
static MP_DEFINE_CONST_FUN_OBJ_1(read_gpio_callable, read_gpio);

static mp_obj_t
write_gpio(mp_obj_t gpio_object, mp_obj_t value_object) {
  area512_micropython_gpio_instance_t *gpio_instance =
    MP_OBJ_TO_PTR(gpio_object);

  GPIO_write(
    fetch_pin_number_or_raise(gpio_instance->pin_object),
    (uint8_t)mp_obj_get_int(value_object)
  );

  return MP_OBJ_NEW_SMALL_INT(0);
}
static MP_DEFINE_CONST_FUN_OBJ_2(write_gpio_callable, write_gpio);

static mp_obj_t
is_gpio_high(mp_obj_t gpio_object) {
  area512_micropython_gpio_instance_t *gpio_instance =
    MP_OBJ_TO_PTR(gpio_object);

  return mp_obj_new_bool(
    GPIO_read(fetch_pin_number_or_raise(gpio_instance->pin_object)) != 0
  );
}
static MP_DEFINE_CONST_FUN_OBJ_1(is_gpio_high_callable, is_gpio_high);

static mp_obj_t
is_gpio_low(mp_obj_t gpio_object) {
  area512_micropython_gpio_instance_t *gpio_instance =
    MP_OBJ_TO_PTR(gpio_object);

  return mp_obj_new_bool(
    GPIO_read(fetch_pin_number_or_raise(gpio_instance->pin_object)) == 0
  );
}
static MP_DEFINE_CONST_FUN_OBJ_1(is_gpio_low_callable, is_gpio_low);

static mp_obj_t
read_gpio_at(mp_obj_t pin_object) {
  return MP_OBJ_NEW_SMALL_INT(GPIO_read(fetch_pin_number_or_raise(pin_object)));
}
static MP_DEFINE_CONST_FUN_OBJ_1(read_gpio_at_callable, read_gpio_at);

static mp_obj_t
write_gpio_at(mp_obj_t pin_object, mp_obj_t value_object) {
  GPIO_write(
    fetch_pin_number_or_raise(pin_object),
    (uint8_t)mp_obj_get_int(value_object)
  );

  return MP_OBJ_NEW_SMALL_INT(0);
}
static MP_DEFINE_CONST_FUN_OBJ_2(write_gpio_at_callable, write_gpio_at);

// Mirrors picoruby-gpio's c_high_at_q, which reads the pin argument as a plain
// integer instead of resolving pin names the way its sibling methods do.
static mp_obj_t
is_gpio_high_at(mp_obj_t pin_object) {
  return mp_obj_new_bool(GPIO_read((uint8_t)mp_obj_get_int(pin_object)) != 0);
}
static MP_DEFINE_CONST_FUN_OBJ_1(is_gpio_high_at_callable, is_gpio_high_at);

static mp_obj_t
is_gpio_low_at(mp_obj_t pin_object) {
  return mp_obj_new_bool(GPIO_read(fetch_pin_number_or_raise(pin_object)) == 0);
}
static MP_DEFINE_CONST_FUN_OBJ_1(is_gpio_low_at_callable, is_gpio_low_at);

static mp_obj_t
set_gpio_direction_at(mp_obj_t pin_object, mp_obj_t flags_object) {
  GPIO_set_dir(
    fetch_pin_number_or_raise(pin_object),
    (uint8_t)mp_obj_get_int(flags_object)
  );

  return MP_OBJ_NEW_SMALL_INT(0);
}
static MP_DEFINE_CONST_FUN_OBJ_2(
  set_gpio_direction_at_callable,
  set_gpio_direction_at
);

static mp_obj_t
set_gpio_function_at(mp_obj_t pin_object, mp_obj_t alt_function_object) {
  GPIO_set_function(
    fetch_pin_number_or_raise(pin_object),
    (uint8_t)mp_obj_get_int(alt_function_object)
  );

  return MP_OBJ_NEW_SMALL_INT(0);
}
static MP_DEFINE_CONST_FUN_OBJ_2(
  set_gpio_function_at_callable,
  set_gpio_function_at
);

static mp_obj_t
set_gpio_pull_up_at(mp_obj_t pin_object) {
  GPIO_pull_up(fetch_pin_number_or_raise(pin_object));

  return MP_OBJ_NEW_SMALL_INT(0);
}
static MP_DEFINE_CONST_FUN_OBJ_1(
  set_gpio_pull_up_at_callable,
  set_gpio_pull_up_at
);

static mp_obj_t
set_gpio_pull_down_at(mp_obj_t pin_object) {
  GPIO_pull_down(fetch_pin_number_or_raise(pin_object));

  return MP_OBJ_NEW_SMALL_INT(0);
}
static MP_DEFINE_CONST_FUN_OBJ_1(
  set_gpio_pull_down_at_callable,
  set_gpio_pull_down_at
);

static mp_obj_t
set_gpio_open_drain_at(mp_obj_t pin_object) {
  GPIO_open_drain(fetch_pin_number_or_raise(pin_object));

  return MP_OBJ_NEW_SMALL_INT(0);
}
static MP_DEFINE_CONST_FUN_OBJ_1(
  set_gpio_open_drain_at_callable,
  set_gpio_open_drain_at
);

static const mp_rom_map_elem_t gpio_locals_table[] = {
  {MP_ROM_QSTR(MP_QSTR_IN), MP_ROM_INT(IN)},
  {MP_ROM_QSTR(MP_QSTR_OUT), MP_ROM_INT(OUT)},
  {MP_ROM_QSTR(MP_QSTR_HIGH_Z), MP_ROM_INT(HIGH_Z)},
  {MP_ROM_QSTR(MP_QSTR_PULL_UP), MP_ROM_INT(PULL_UP)},
  {MP_ROM_QSTR(MP_QSTR_PULL_DOWN), MP_ROM_INT(PULL_DOWN)},
  {MP_ROM_QSTR(MP_QSTR_OPEN_DRAIN), MP_ROM_INT(OPEN_DRAIN)},
  {MP_ROM_QSTR(MP_QSTR_ALT), MP_ROM_INT(ALT)},
  {MP_ROM_QSTR(MP_QSTR_pin), MP_ROM_PTR(&fetch_gpio_pin_callable)},
  {MP_ROM_QSTR(MP_QSTR_read), MP_ROM_PTR(&read_gpio_callable)},
  {MP_ROM_QSTR(MP_QSTR_write), MP_ROM_PTR(&write_gpio_callable)},
  {MP_ROM_QSTR(MP_QSTR_high), MP_ROM_PTR(&is_gpio_high_callable)},
  {MP_ROM_QSTR(MP_QSTR_low), MP_ROM_PTR(&is_gpio_low_callable)},
  {MP_ROM_QSTR(MP_QSTR_set_dir), MP_ROM_PTR(&set_gpio_direction_callable)},
  {MP_ROM_QSTR(MP_QSTR_set_pull), MP_ROM_PTR(&set_gpio_pull_callable)},
  {MP_ROM_QSTR(MP_QSTR_open_drain), MP_ROM_PTR(&set_gpio_open_drain_callable)},
  {MP_ROM_QSTR(MP_QSTR_set_function), MP_ROM_PTR(&set_gpio_function_callable)},
  {MP_ROM_QSTR(MP_QSTR_setmode), MP_ROM_PTR(&set_gpio_mode_callable)},
  {MP_ROM_QSTR(MP_QSTR_read_at), MP_ROM_PTR(&read_gpio_at_callable)},
  {MP_ROM_QSTR(MP_QSTR_write_at), MP_ROM_PTR(&write_gpio_at_callable)},
  {MP_ROM_QSTR(MP_QSTR_high_at), MP_ROM_PTR(&is_gpio_high_at_callable)},
  {MP_ROM_QSTR(MP_QSTR_low_at), MP_ROM_PTR(&is_gpio_low_at_callable)},
  {
    MP_ROM_QSTR(MP_QSTR_set_dir_at),
    MP_ROM_PTR(&set_gpio_direction_at_callable),
  },
  {
    MP_ROM_QSTR(MP_QSTR_set_function_at),
    MP_ROM_PTR(&set_gpio_function_at_callable),
  },
  {MP_ROM_QSTR(MP_QSTR_pull_up_at), MP_ROM_PTR(&set_gpio_pull_up_at_callable)},
  {
    MP_ROM_QSTR(MP_QSTR_pull_down_at),
    MP_ROM_PTR(&set_gpio_pull_down_at_callable),
  },
  {
    MP_ROM_QSTR(MP_QSTR_open_drain_at),
    MP_ROM_PTR(&set_gpio_open_drain_at_callable),
  },
};
static MP_DEFINE_CONST_DICT(gpio_locals_dictionary, gpio_locals_table);

MP_DEFINE_CONST_OBJ_TYPE(
  area512_micropython_gpio_type,
  MP_QSTR_GPIO,
  MP_TYPE_FLAG_NONE,
  make_new,
  create_gpio_object,
  locals_dict,
  &gpio_locals_dictionary
);
