#include "core/ti/loader.h"
#include "port/area512_editor_file.h"
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
  VimString *combined_source
) {

  VimString resolved_preload_path;
  vim_string_init(&resolved_preload_path);

  int appended =
    build_ti_preload_path(
      &vim->filepath,
      loader_directory_byte_length,
      preload_path,
      preload_path_byte_length,
      &resolved_preload_path
    );

  if (
    appended &&
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

  VimString preload_source;
  vim_string_init(&preload_source);

  if (
    appended &&
    load_edit_file(
      resolved_preload_path.bytes,
      resolved_preload_path.byte_length,
      &preload_source
    )
  ) {
    appended =
      vim_string_append(
        combined_source,
        preload_source.bytes,
        preload_source.byte_length
      ) &&
      vim_string_append_byte(combined_source, '\n');
  }

  vim_string_free(&preload_source);
  vim_string_free(&resolved_preload_path);

  return appended;
}

int
prepend_ti_preload_sources(
  const Vim *vim,
  VimString *content,
  int *source_byte_offset
) {

  *source_byte_offset = 0;

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

  if (
    !load_edit_file(
      loader_path.bytes,
      loader_path.byte_length,
      &loader_manifest
    )
  ) {

    vim_string_free(&loader_manifest);
    vim_string_free(&loader_path);

    return 1;
  }

  VimString combined_source;
  vim_string_init(&combined_source);

  int appended = 1;
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
      appended =
        append_ti_preload_source(
          vim,
          loader_directory_byte_length,
          loader_manifest.bytes + line_start_byte_offset,
          preload_path_byte_length,
          &combined_source
        );

      if (!appended)
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

  if (appended) {
    *source_byte_offset = combined_source.byte_length;

    appended =
      vim_string_append(
        &combined_source,
        content->bytes,
        content->byte_length
      );
  }

  if (appended) {
    vim_string_free(content);
    *content = combined_source;
  } else {
    vim_string_free(&combined_source);
    *source_byte_offset = 0;
  }

  vim_string_free(&loader_manifest);
  vim_string_free(&loader_path);

  return appended;
}
