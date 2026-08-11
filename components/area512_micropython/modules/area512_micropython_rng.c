#include "py/runtime.h"
#include "py/smallint.h"

#include <stdint.h>

extern uint8_t rng_random_byte_impl(void);

#define RANDOM_INT_BYTE_COUNT (4)
#define BIT_PER_BYTE (8)
#define HEX_DIGIT_COUNT (16)
#define UUID_CHARACTER_LENGTH (36)
#define UUID_VERSION_INDEX (14)
#define UUID_VARIANT_INDEX (19)
#define UUID_VARIANT_DIGIT_COUNT (4)

static const char hex_digit_table[] = "0123456789abcdef";
static const char uuid_variant_digit_table[] = "89ab";
static const int uuid_hyphen_index_table[] = {8, 13, 18, 23};

static mp_obj_t
generate_random_int(void) {
  mp_uint_t random_int = 0;

  for (int byte_index = 0; byte_index < RANDOM_INT_BYTE_COUNT; byte_index++)
    random_int = (random_int << BIT_PER_BYTE) | rng_random_byte_impl();

  return mp_obj_new_int_from_uint(random_int & MP_SMALL_INT_MAX);
}

static MP_DEFINE_CONST_FUN_OBJ_0(
  generate_random_int_callable,
  generate_random_int
);

static mp_obj_t
generate_random_bytes(mp_obj_t length_object) {
  mp_int_t byte_length = mp_obj_get_int(length_object);

  if (byte_length < 0)
    mp_raise_msg(
      &mp_type_ValueError,
      MP_ERROR_TEXT("negative length")
    );

  if (byte_length == 0)
    return mp_obj_new_bytes((const byte *)"", 0);

  char *random_bytes = m_new(char, byte_length);

  for (mp_int_t byte_index = 0; byte_index < byte_length; byte_index++)
    random_bytes[byte_index] = (char)rng_random_byte_impl();

  mp_obj_t result_object =
    mp_obj_new_bytes(
      (const byte *)random_bytes,
      (size_t)byte_length
    );

  m_del(char, random_bytes, byte_length);

  return result_object;
}

static MP_DEFINE_CONST_FUN_OBJ_1(
  generate_random_bytes_callable,
  generate_random_bytes
);

static mp_obj_t
generate_uuid(void) {
  char uuid_text[UUID_CHARACTER_LENGTH];

  for (int text_index = 0; text_index < UUID_CHARACTER_LENGTH; text_index++)
    uuid_text[text_index] =
      hex_digit_table[rng_random_byte_impl() % HEX_DIGIT_COUNT];

  for (
    size_t hyphen_table_index = 0;
    hyphen_table_index < MP_ARRAY_SIZE(uuid_hyphen_index_table);
    hyphen_table_index++
  ) {

    uuid_text[uuid_hyphen_index_table[hyphen_table_index]] = '-';
  }

  uuid_text[UUID_VERSION_INDEX] = '4';
  uuid_text[UUID_VARIANT_INDEX] =
    uuid_variant_digit_table[rng_random_byte_impl() % UUID_VARIANT_DIGIT_COUNT];

  return mp_obj_new_str(uuid_text, UUID_CHARACTER_LENGTH);
}

static MP_DEFINE_CONST_FUN_OBJ_0(generate_uuid_callable, generate_uuid);

static const mp_rom_map_elem_t rng_locals_table[] = {
  {MP_ROM_QSTR(MP_QSTR_random_int), MP_ROM_PTR(&generate_random_int_callable)},
  {
    MP_ROM_QSTR(MP_QSTR_random_string),
    MP_ROM_PTR(&generate_random_bytes_callable),
  },
  {MP_ROM_QSTR(MP_QSTR_uuid), MP_ROM_PTR(&generate_uuid_callable)},
};

static MP_DEFINE_CONST_DICT(rng_locals_dictionary, rng_locals_table);

MP_DEFINE_CONST_OBJ_TYPE(
  area512_micropython_rng_type,
  MP_QSTR_RNG,
  MP_TYPE_FLAG_NONE,
  locals_dict,
  &rng_locals_dictionary
);
