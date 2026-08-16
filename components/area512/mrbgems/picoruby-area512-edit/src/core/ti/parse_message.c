#include "core/ti/parse_message.h"
#include "core/render/footer.h"
#include "micropython_ti_parse_budget.h"
#include <string.h>

#define TI_PARSE_MESSAGE_CAPACITY 64

static const char CANCELLED_MESSAGE_SUFFIX[] = ": too large to analyze";

void
show_ti_parse_cancelled_message(
  Vim *vim,
  const TiLoadedSources *loaded_sources
) {

  if (!micropython_ti_did_cancel_parse())
    return;

  int source_index = micropython_ti_get_cancelled_source_index();

  if (source_index < 0 || source_index >= loaded_sources->list.count)
    return;

  const VimString *path = &loaded_sources->paths[source_index];

  if (path->byte_length <= 0)
    return;

  int base_name_start_byte_offset = 0;

  for (int index = 0; index < path->byte_length; index++)
    if (path->bytes[index] == '/')
      base_name_start_byte_offset = index + 1;

  int base_name_byte_length = path->byte_length - base_name_start_byte_offset;

  if (
    base_name_byte_length >
    (int)(TI_PARSE_MESSAGE_CAPACITY - sizeof(CANCELLED_MESSAGE_SUFFIX))
  ) {

    base_name_byte_length =
      (int)(TI_PARSE_MESSAGE_CAPACITY - sizeof(CANCELLED_MESSAGE_SUFFIX));
  }

  char message[TI_PARSE_MESSAGE_CAPACITY];

  memcpy(
    message,
    path->bytes + base_name_start_byte_offset,
    (size_t)base_name_byte_length
  );

  memcpy(
    message + base_name_byte_length,
    CANCELLED_MESSAGE_SUFFIX,
    sizeof(CANCELLED_MESSAGE_SUFFIX) - 1
  );

  int message_byte_length =
    base_name_byte_length + (int)sizeof(CANCELLED_MESSAGE_SUFFIX) - 1;

  show_message(vim, message, message_byte_length);
}
