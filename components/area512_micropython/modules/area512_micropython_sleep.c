#include "py/mphal.h"
#include "py/runtime.h"

#define MILLISECOND_PER_SECOND (1000)

static mp_obj_t
sleep_for_seconds(mp_obj_t seconds_object) {
  mp_float_t seconds = mp_obj_get_float(seconds_object);

  if (seconds > 0)
    mp_hal_delay_ms((mp_uint_t)(seconds * MILLISECOND_PER_SECOND));

  return mp_const_none;
}

MP_DEFINE_CONST_FUN_OBJ_1(
  area512_micropython_sleep_seconds_callable,
  sleep_for_seconds
);

static mp_obj_t
sleep_for_milliseconds(mp_obj_t milliseconds_object) {
  mp_int_t milliseconds = mp_obj_get_int(milliseconds_object);

  if (milliseconds > 0)
    mp_hal_delay_ms((mp_uint_t)milliseconds);

  return mp_const_none;
}

MP_DEFINE_CONST_FUN_OBJ_1(
  area512_micropython_sleep_milliseconds_callable,
  sleep_for_milliseconds
);
