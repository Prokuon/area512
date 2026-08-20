#ifndef EDIT_SYNTAX_PYTHON_INDENT_H
#define EDIT_SYNTAX_PYTHON_INDENT_H

#ifdef __cplusplus
extern "C" {
#endif

int editor_python_auto_indent_should_increase(
  const char *line,
  int line_byte_length,
  const char *previous_line,
  int previous_line_byte_length
);
int editor_python_auto_indent_should_decrease(
  const char *line,
  int line_byte_length
);

#ifdef __cplusplus
}
#endif

#endif
