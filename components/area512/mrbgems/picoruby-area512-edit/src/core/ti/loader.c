#include "core/ti/loader.h"
#include "port/area512_editor_file.h"
#include <stdlib.h>
#include <string.h>

#define TI_LOADER_FILENAME ".ti-loader.manifest"

static int
build_ti_loader_path(
  const VimString *filepath,
  VimString *loader_path,
  int *loader_directory_byte_length
) {

  *loader_directory_byte_length = 0;

  for (int index = 0; index < filepath->byte_length; index++)
    if (filepath->bytes[index] == '/')
      *loader_directory_byte_length = index + 1;

  return vim_string_append(
           loader_path,
           filepath->bytes,
           *loader_directory_byte_length
         ) &&
         vim_string_append_c_string(loader_path, TI_LOADER_FILENAME);
}

static int
build_ti_preload_path(
  const VimString *filepath,
  int loader_directory_byte_length,
  const char *preload_path,
  int preload_path_byte_length,
  VimString *resolved_preload_path
) {

  return vim_string_append(
           resolved_preload_path,
           filepath->bytes,
           loader_directory_byte_length
         ) &&
         vim_string_append(
           resolved_preload_path,
           preload_path,
           preload_path_byte_length
         );
}

static int
append_ti_preload_source(
  const Vim *vim,
  int loader_directory_byte_length,
  const char *preload_path,
  int preload_path_byte_length,
  TiLoadedSources *loaded_sources
) {

  VimString resolved_preload_path;
  vim_string_init(&resolved_preload_path);

  int loaded =
    build_ti_preload_path(
      &vim->filepath,
      loader_directory_byte_length,
      preload_path,
      preload_path_byte_length,
      &resolved_preload_path
    );

  if (
    loaded &&
    resolved_preload_path.byte_length == vim->filepath.byte_length &&
    memcmp(
      resolved_preload_path.bytes,
      vim->filepath.bytes,
      (size_t)vim->filepath.byte_length
    ) == 0
  ) {
    vim_string_free(&resolved_preload_path);
    return 1;
  }

  VimString *source_content =
    &loaded_sources->contents[loaded_sources->list.count];

  vim_string_init(source_content);

  if (
    loaded &&
    load_edit_file(
      resolved_preload_path.bytes,
      resolved_preload_path.byte_length,
      source_content
    )
  ) {
    loaded_sources->list.count++;
  }

  vim_string_free(&resolved_preload_path);

  return loaded;
}

void
free_ti_sources(TiLoadedSources *loaded_sources) {
  if (!loaded_sources)
    return;

  for (
    int source_index = 0;
    source_index < loaded_sources->list.count;
    source_index++
  ) {

    vim_string_free(&loaded_sources->contents[source_index]);
  }

  free(loaded_sources->contents);
  free(loaded_sources->sources);

  memset(loaded_sources, 0, sizeof(*loaded_sources));
}

int
load_ti_sources(
  const Vim *vim,
  VimString *content,
  TiLoadedSources *loaded_sources
) {

  memset(loaded_sources, 0, sizeof(*loaded_sources));

  VimString loader_path;
  vim_string_init(&loader_path);

  int loader_directory_byte_length;

  if (
    !build_ti_loader_path(
      &vim->filepath,
      &loader_path,
      &loader_directory_byte_length
    )
  ) {

    vim_string_free(&loader_path);
    return 0;
  }

  VimString loader_manifest;
  vim_string_init(&loader_manifest);

  load_edit_file(
    loader_path.bytes,
    loader_path.byte_length,
    &loader_manifest
  );

  int source_capacity = 1;

  for (int index = 0; index < loader_manifest.byte_length; index++)
    if (loader_manifest.bytes[index] == '\n')
      source_capacity++;

  if (
    loader_manifest.byte_length > 0 &&
    loader_manifest.bytes[loader_manifest.byte_length - 1] != '\n'
  ) {
    source_capacity++;
  }

  loaded_sources->contents =
    calloc((size_t)source_capacity, sizeof(VimString));

  loaded_sources->sources =
    calloc((size_t)source_capacity, sizeof(TiSource));

  if (!loaded_sources->contents || !loaded_sources->sources) {
    free_ti_sources(loaded_sources);
    vim_string_free(&loader_manifest);
    vim_string_free(&loader_path);

    return 0;
  }

  int loaded = 1;
  int line_start_byte_offset = 0;

  while (line_start_byte_offset < loader_manifest.byte_length) {
    int line_end_byte_offset = line_start_byte_offset;

    while (
      line_end_byte_offset < loader_manifest.byte_length &&
      loader_manifest.bytes[line_end_byte_offset] != '\n'
    ) {
      line_end_byte_offset++;
    }

    if (
      line_end_byte_offset > line_start_byte_offset &&
      loader_manifest.bytes[line_end_byte_offset - 1] == '\r'
    ) {
      line_end_byte_offset--;
    }

    int preload_path_byte_length =
      line_end_byte_offset - line_start_byte_offset;

    if (preload_path_byte_length > 0) {
      loaded =
        append_ti_preload_source(
          vim,
          loader_directory_byte_length,
          loader_manifest.bytes + line_start_byte_offset,
          preload_path_byte_length,
          loaded_sources
        );

      if (!loaded)
        break;
    }

    while (
      line_end_byte_offset < loader_manifest.byte_length &&
      loader_manifest.bytes[line_end_byte_offset] != '\n'
    ) {
      line_end_byte_offset++;
    }

    line_start_byte_offset = line_end_byte_offset + 1;
  }

  if (loaded) {
    loaded_sources->contents[loaded_sources->list.count++] = *content;
    vim_string_init(content);

    for (
      int source_index = 0;
      source_index < loaded_sources->list.count;
      source_index++
    ) {

      loaded_sources->sources[source_index].source =
        loaded_sources->contents[source_index].bytes;

      loaded_sources->sources[source_index].source_byte_length =
        loaded_sources->contents[source_index].byte_length;
    }

    loaded_sources->list.items = loaded_sources->sources;

  } else {
    free_ti_sources(loaded_sources);
  }

  vim_string_free(&loader_manifest);
  vim_string_free(&loader_path);

  return loaded;
}
