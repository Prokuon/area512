#ifndef EDIT_SYNTAX_H
#define EDIT_SYNTAX_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
  VIM_SYNTAX_NONE = 0,
  VIM_SYNTAX_RUBY,
  VIM_SYNTAX_PYTHON,
} VimSyntax;

bool editor_is_ruby_filename(const char *path, int path_byte_length);
bool editor_is_python_filename(const char *path, int path_byte_length);
VimSyntax editor_syntax_for_filename(const char *path, int path_byte_length);

#ifdef __cplusplus
}
#endif

#endif
