#include "area512_hal.h"

#include "py/runtime.h"

#include <stdbool.h>

#include "io-console.h"

static mp_obj_t
set_console_raw(mp_obj_t io_object) {
  io_raw_bang(false);

  return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_1(set_console_raw_callable, set_console_raw);

static mp_obj_t
set_console_cooked(mp_obj_t io_object) {
  io_cooked_bang();

  return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_1(
  set_console_cooked_callable,
  set_console_cooked
);

static mp_obj_t
restore_console_termios(mp_obj_t io_object) {
  io__restore_termios();

  return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_1(
  restore_console_termios_callable,
  restore_console_termios
);

static mp_obj_t
fetch_console_echo(mp_obj_t io_object) {
  return mp_obj_new_bool(io_echo_q());
}
static MP_DEFINE_CONST_FUN_OBJ_1(
  fetch_console_echo_callable,
  fetch_console_echo
);

static mp_obj_t
set_console_echo(mp_obj_t io_object, mp_obj_t echo_object) {
  io_echo_eq(mp_obj_is_true(echo_object));

  return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_2(set_console_echo_callable, set_console_echo);

static mp_obj_t
read_console_nonblock(mp_obj_t io_object, mp_obj_t maximum_byte_count_object) {
  int maximum_byte_count = mp_obj_get_int(maximum_byte_count_object);

  if (maximum_byte_count < 0)
    mp_raise_msg(&mp_type_ValueError, MP_ERROR_TEXT("negative maxlen"));

  if (maximum_byte_count == 0)
    return mp_const_none;

  char *buffer = m_new(char, maximum_byte_count);
  int read_byte_count = 0;

  while (read_byte_count < maximum_byte_count) {
    int character = area512_console_getchar();

    if (character < 0)
      break;

    buffer[read_byte_count] = (char)character;
    read_byte_count += 1;
  }

  mp_obj_t result_object = read_byte_count == 0
                             ? mp_const_none
                             : mp_obj_new_str(buffer, (size_t)read_byte_count);

  m_del(char, buffer, maximum_byte_count);

  return result_object;
}
static MP_DEFINE_CONST_FUN_OBJ_2(
  read_console_nonblock_callable,
  read_console_nonblock
);

static mp_obj_t
read_console_character(mp_obj_t io_object) {
  int character = area512_console_getch_block();
  char buffer[1] = {(char)character};

  return mp_obj_new_str(buffer, 1);
}
static MP_DEFINE_CONST_FUN_OBJ_1(
  read_console_character_callable,
  read_console_character
);

static mp_obj_t
create_io_object(
  const mp_obj_type_t *io_type,
  size_t argument_count,
  size_t keyword_argument_count,
  const mp_obj_t *arguments
) {

  mp_arg_check_num(argument_count, keyword_argument_count, 0, 0, false);

  return MP_OBJ_FROM_PTR(&area512_micropython_stdin_instance);
}

static const mp_rom_map_elem_t io_locals_table[] = {
  {MP_ROM_QSTR(MP_QSTR_raw), MP_ROM_PTR(&set_console_raw_callable)},
  {MP_ROM_QSTR(MP_QSTR_cooked), MP_ROM_PTR(&set_console_cooked_callable)},
  {
    MP_ROM_QSTR(MP_QSTR__restore_termios),
    MP_ROM_PTR(&restore_console_termios_callable),
  },
  {MP_ROM_QSTR(MP_QSTR_echo), MP_ROM_PTR(&fetch_console_echo_callable)},
  {MP_ROM_QSTR(MP_QSTR_set_echo), MP_ROM_PTR(&set_console_echo_callable)},
  {
    MP_ROM_QSTR(MP_QSTR_read_nonblock),
    MP_ROM_PTR(&read_console_nonblock_callable),
  },
  {MP_ROM_QSTR(MP_QSTR_getch), MP_ROM_PTR(&read_console_character_callable)},
};
static MP_DEFINE_CONST_DICT(io_locals_dictionary, io_locals_table);

MP_DEFINE_CONST_OBJ_TYPE(
  area512_micropython_io_type,
  MP_QSTR_IO,
  MP_TYPE_FLAG_NONE,
  make_new,
  create_io_object,
  locals_dict,
  &io_locals_dictionary
);

const mp_obj_base_t area512_micropython_stdin_instance = {
  &area512_micropython_io_type
};
