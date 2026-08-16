#ifndef EDIT_SYNTAX_PYTHON_HIGHLIGHT_H
#define EDIT_SYNTAX_PYTHON_HIGHLIGHT_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void editor_python_highlight_run(
  const uint8_t *source,
  int source_byte_length,
  void (*write_segment)(
    void *writer_context,
    const char *text,
    int text_byte_length,
    uint32_t color
  ),
  void *writer_context
);

#ifdef __cplusplus
}
#endif

#endif
