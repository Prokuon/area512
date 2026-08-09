#ifndef AREA512_EDIT_TI_LOADER_H
#define AREA512_EDIT_TI_LOADER_H

#include "core/editor.h"
#include "picoruby_ti_source.h"

typedef struct {
  VimString *contents;
  TiSource *sources;
  TiSourceList list;
} TiLoadedSources;

int load_ti_sources(
  const Vim *vim,
  VimString *content,
  TiLoadedSources *loaded_sources
);
void free_ti_sources(TiLoadedSources *loaded_sources);

#endif
