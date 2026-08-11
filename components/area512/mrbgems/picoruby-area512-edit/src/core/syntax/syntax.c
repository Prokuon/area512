#include "core/syntax/syntax.h"
#include <string.h>

bool
editor_is_ruby_filename(const char *path, int path_byte_length) {
  return path && path_byte_length >= 3 &&
         memcmp(path + path_byte_length - 3, ".rb", 3) == 0;
}

bool
editor_is_python_filename(const char *path, int path_byte_length) {
  return path && path_byte_length >= 3 &&
         memcmp(path + path_byte_length - 3, ".py", 3) == 0;
}

VimSyntax
editor_syntax_for_filename(const char *path, int path_byte_length) {
  if (editor_is_ruby_filename(path, path_byte_length))
    return VIM_SYNTAX_RUBY;

  if (editor_is_python_filename(path, path_byte_length))
    return VIM_SYNTAX_PYTHON;

  return VIM_SYNTAX_NONE;
}
