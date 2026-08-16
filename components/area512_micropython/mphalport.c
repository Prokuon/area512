#include "area512_hal.h"
#include "py/gc.h"
#include "py/mphal.h"

#include <esp_heap_caps.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <rom/ets_sys.h>

#define NATIVE_ALLOCATION_MARGIN_BYTE_SIZE (8 * 1024)
#define MICROSECOND_PER_MILLISECOND (1000)

void
mp_hal_stdout_tx_strn_cooked(
  const char *python_output_bytes,
  size_t python_output_byte_length
) {

  area512_console_write(
    python_output_bytes,
    (int)python_output_byte_length
  );
}

void
mp_hal_delay_us(mp_uint_t microseconds) {
  ets_delay_us(microseconds);
}

// vTaskDelay only counts whole ticks, so busy-wait the sub-tick remainder:
// a 35ms frame wait would otherwise round down to 30ms.
void
mp_hal_delay_ms(mp_uint_t milliseconds) {
  TickType_t tick_count = milliseconds / portTICK_PERIOD_MS;

  if (tick_count)
    vTaskDelay(tick_count);

  mp_hal_delay_us(
    (milliseconds % portTICK_PERIOD_MS) * MICROSECOND_PER_MILLISECOND
  );
}

// The GC grows into the IDF heap on demand, so hold back what the run still
// needs from it: file reads, SD access and DMA descriptors.
size_t
gc_get_max_new_split(void) {
  size_t largest_free_byte_size =
    heap_caps_get_largest_free_block(MALLOC_CAP_DMA);

  return largest_free_byte_size > NATIVE_ALLOCATION_MARGIN_BYTE_SIZE
           ? largest_free_byte_size - NATIVE_ALLOCATION_MARGIN_BYTE_SIZE
           : 0;
}
