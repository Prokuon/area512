#ifndef AREA512_EDIT_TI_LOADER_H
#define AREA512_EDIT_TI_LOADER_H

#include "core/editor.h"

int prepend_ti_preload_sources(
  const Vim *vim,
  VimString *content,
  int *source_byte_offset
);

#endif
