#include "core/syntax/python/indent.h"
#include <stdint.h>
#include <string.h>
#include <tree_sitter/api.h>
#include <tree_sitter/tree-sitter-python.h>

#define PYTHON_AUTO_INDENT_MAX_TOKEN_COUNT 96

typedef struct {
  const char *node_types[PYTHON_AUTO_INDENT_MAX_TOKEN_COUNT];
  int token_count;
  int parenthesis_depth;
  int bracket_depth;
  int brace_depth;
} editor_python_auto_indent_tokens_t;

static void
collect_python_auto_indent_tokens(
  editor_python_auto_indent_tokens_t *tokens,
  TSNode node
) {

  if (ts_node_is_missing(node))
    return;

  uint32_t child_count = ts_node_child_count(node);

  if (child_count > 0) {
    for (uint32_t child_index = 0; child_index < child_count; child_index++)
      collect_python_auto_indent_tokens(
        tokens,
        ts_node_child(node, child_index)
      );

    return;
  }

  const char *node_type = ts_node_type(node);

  if (strcmp(node_type, "comment") == 0)
    return;

  if (strcmp(node_type, "(") == 0)
    tokens->parenthesis_depth++;

  else if (strcmp(node_type, ")") == 0 && tokens->parenthesis_depth > 0)
    tokens->parenthesis_depth--;

  else if (strcmp(node_type, "[") == 0)
    tokens->bracket_depth++;

  else if (strcmp(node_type, "]") == 0 && tokens->bracket_depth > 0)
    tokens->bracket_depth--;

  else if (strcmp(node_type, "{") == 0)
    tokens->brace_depth++;

  else if (strcmp(node_type, "}") == 0 && tokens->brace_depth > 0)
    tokens->brace_depth--;

  if (tokens->token_count < PYTHON_AUTO_INDENT_MAX_TOKEN_COUNT)
    tokens->node_types[tokens->token_count++] = node_type;
}

static void
python_auto_indent_tokenize(
  editor_python_auto_indent_tokens_t *tokens,
  const char *line,
  int line_byte_length
) {

  memset(tokens, 0, sizeof(*tokens));

  if (!line || line_byte_length <= 0)
    return;

  TSParser *parser = ts_parser_new();
  TSTree *tree = NULL;

  if (
    parser &&
    ts_parser_set_language(
      parser,
      tree_sitter_python()
    )
  ) {

    tree =
      ts_parser_parse_string(
        parser,
        NULL,
        line,
        (uint32_t)line_byte_length
      );
  }

  if (tree) {
    TSNode root_node = ts_tree_root_node(tree);
    collect_python_auto_indent_tokens(tokens, root_node);
    ts_tree_delete(tree);
  }

  if (parser)
    ts_parser_delete(parser);
}

static int
python_auto_indent_is_suite_keyword(const char *node_type) {
  return strcmp(node_type, "if") == 0 || strcmp(node_type, "elif") == 0 ||
         strcmp(node_type, "else") == 0 || strcmp(node_type, "for") == 0 ||
         strcmp(node_type, "while") == 0 || strcmp(node_type, "def") == 0 ||
         strcmp(node_type, "class") == 0 || strcmp(node_type, "try") == 0 ||
         strcmp(node_type, "except") == 0 ||
         strcmp(node_type, "finally") == 0 ||
         strcmp(node_type, "with") == 0 || strcmp(node_type, "match") == 0 ||
         strcmp(node_type, "case") == 0;
}

static int
python_auto_indent_is_continuation_type(const char *node_type) {
  return strcmp(node_type, ".") == 0 || strcmp(node_type, ",") == 0 ||
         strcmp(node_type, "+") == 0 || strcmp(node_type, "-") == 0 ||
         strcmp(node_type, "*") == 0 || strcmp(node_type, "**") == 0 ||
         strcmp(node_type, "/") == 0 || strcmp(node_type, "//") == 0 ||
         strcmp(node_type, "%") == 0 || strcmp(node_type, "@") == 0 ||
         strcmp(node_type, "and") == 0 || strcmp(node_type, "or") == 0 ||
         strcmp(node_type, "|") == 0 || strcmp(node_type, "&") == 0 ||
         strcmp(node_type, "^") == 0 || strcmp(node_type, "=") == 0 ||
         strcmp(node_type, ":=") == 0;
}

static int
python_auto_indent_has_unclosed_delimiter(
  const editor_python_auto_indent_tokens_t *tokens
) {

  return tokens->parenthesis_depth > 0 || tokens->bracket_depth > 0 ||
         tokens->brace_depth > 0;
}

static int
python_auto_indent_line_ends_with_colon(
  const char *line,
  int line_byte_length
) {

  while (
    line_byte_length > 0 &&
    (line[line_byte_length - 1] == ' ' || line[line_byte_length - 1] == '\t')
  ) {

    line_byte_length--;
  }

  return line_byte_length > 0 && line[line_byte_length - 1] == ':';
}

int
editor_python_auto_indent_should_increase(
  const char *line,
  int line_byte_length
) {

  editor_python_auto_indent_tokens_t tokens;
  python_auto_indent_tokenize(&tokens, line, line_byte_length);

  if (tokens.token_count == 0)
    return 0;

  const char *last_token_type = tokens.node_types[tokens.token_count - 1];

  if (python_auto_indent_line_ends_with_colon(line, line_byte_length)) {
    for (int token_index = 0; token_index < tokens.token_count; token_index++)
      if (
        python_auto_indent_is_suite_keyword(
          tokens.node_types[token_index]
        )
      )
        return 1;
  }

  if (python_auto_indent_has_unclosed_delimiter(&tokens))
    return 1;

  return python_auto_indent_is_continuation_type(last_token_type);
}

int
editor_python_auto_indent_should_decrease(
  const char *line,
  int line_byte_length
) {

  editor_python_auto_indent_tokens_t tokens;
  python_auto_indent_tokenize(&tokens, line, line_byte_length);

  if (tokens.token_count == 0)
    return 0;

  const char *first_token_type = tokens.node_types[0];

  return strcmp(first_token_type, "elif") == 0 ||
         strcmp(first_token_type, "else") == 0 ||
         strcmp(first_token_type, "except") == 0 ||
         strcmp(first_token_type, "finally") == 0 ||
         strcmp(first_token_type, "case") == 0;
}
