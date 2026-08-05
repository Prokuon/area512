#ifndef AREA512_EDIT_HOVER_POPUP_H
#define AREA512_EDIT_HOVER_POPUP_H

#include "core/editor.h"

void show_hover_popup(
  Vim *vim,
  const char *signature,
  int signature_byte_length,
  int name_byte_length,
  const char *document
);

#endif
