#include "micropython_ti_builtin_database.h"
#include "micropython_ti_diagnostic.h"
#include "micropython_ti_hover.h"
#include "micropython_ti_suggest.h"
#include "micropython_ti_t.h"
#include "picoruby_ti_arena.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int
has_suggestion(const TiSuggestionList *suggestions, const char *contents) {
  for (int index = 0; index < suggestions->count; index++) {
    if (strcmp(suggestions->items[index].contents, contents) == 0)
      return 1;
  }

  return 0;
}

static TiSourceList
source_list_for(const char *source, TiSource *source_item) {
  source_item->source = source;
  source_item->source_byte_length = (int)strlen(source);

  TiSourceList sources = {
    .items = source_item,
    .count = 1,
  };

  return sources;
}

static int
find_hover(const char *source, int cursor_byte_offset, TiHoverInfo *hover_info) {
  TiSource source_item;
  TiSourceList sources = source_list_for(source, &source_item);

  return micropython_ti_find_hover_at_cursor(
    &sources,
    cursor_byte_offset,
    hover_info
  );
}

static TiSuggestionList
suggest_source(const char *source) {
  TiSuggestionList suggestions;
  int source_length = (int)strlen(source);
  TiSource source_item;
  TiSourceList sources = source_list_for(source, &source_item);

  micropython_ti_fill_suggestions_at_cursor(
    &sources,
    source_length,
    &suggestions
  );

  return suggestions;
}

static TiDiagnosticList
diagnose_source(const char *source) {
  TiDiagnosticList diagnostics;
  TiSource source_item;
  TiSourceList sources = source_list_for(source, &source_item);

  micropython_ti_fill_diagnostics(
    &sources,
    &diagnostics
  );

  return diagnostics;
}

static void
test_builtin_argument_type_diagnostic(void) {
  TiDiagnosticList diagnostics = diagnose_source("\"x\".find(1)");

  assert(diagnostics.count == 1);
  assert(strcmp(
    diagnostics.items[0].message,
    "type mismatch: expected str, but got int for str.find"
  ) == 0);
  assert(diagnostics.items[0].start_byte_offset == 9);
  assert(diagnostics.items[0].end_byte_offset == 10);

  diagnostics = diagnose_source("v = 1\nv = \"x\"\n\"y\".find(v)");
  assert(diagnostics.count == 0);

  diagnostics = diagnose_source("v = 1\nv = []\n\"y\".find(v)");
  assert(diagnostics.count == 1);
}

static void
test_unknown_argument_has_no_diagnostic(void) {
  TiDiagnosticList diagnostics = diagnose_source("\"x\".find(unknown)");

  assert(diagnostics.count == 0);
}

static void
test_list_and_dict_contents_have_no_diagnostic(void) {
  TiDiagnosticList list_diagnostics =
    diagnose_source("[1, \"x\"].index(1)");
  TiDiagnosticList dict_diagnostics =
    diagnose_source("{\"a\": 1}.get(1)");

  assert(list_diagnostics.count == 0);
  assert(dict_diagnostics.count == 0);
}

static void
test_builtin_argument_count_diagnostic(void) {
  TiDiagnosticList diagnostics =
    diagnose_source("(1).to_bytes(1, \"big\", 2)");

  assert(diagnostics.count == 1);
  assert(strcmp(
    diagnostics.items[0].message,
    "too many arguments for int.to_bytes"
  ) == 0);

  diagnostics = diagnose_source("\"x\".find()");
  assert(diagnostics.count == 1);
  assert(strcmp(
    diagnostics.items[0].message,
    "too few arguments for str.find"
  ) == 0);
}

static void
test_keyword_argument_diagnostic(void) {
  TiDiagnosticList diagnostics =
    diagnose_source("(1).to_bytes(1, \"big\", signed=True)");
  assert(diagnostics.count == 0);

  diagnostics = diagnose_source("(1).to_bytes(1, \"big\", signed=1)");
  assert(diagnostics.count == 1);
  assert(strcmp(
    diagnostics.items[0].message,
    "type mismatch: expected bool, but got int for int.to_bytes"
  ) == 0);

  diagnostics = diagnose_source("\"x\".find(sub=\"a\", unknown=1)");
  assert(diagnostics.count == 1);
  assert(strcmp(
    diagnostics.items[0].message,
    "too many arguments for str.find"
  ) == 0);
}

static void
test_splat_arguments_have_no_argument_count_diagnostic(void) {
  TiDiagnosticList diagnostics =
    diagnose_source("a = [\"a\"]\n\"x\".find(*a)");
  assert(diagnostics.count == 0);

  diagnostics = diagnose_source("a = {}\n\"x\".find(\"a\", **a)");
  assert(diagnostics.count == 0);
}

static void
test_user_defined_function_arguments_have_no_diagnostic(void) {
  TiDiagnosticList diagnostics =
    diagnose_source("def call(value):\n"
                    "    return value\n"
                    "call()\n"
                    "call(1, 2)");

  assert(diagnostics.count == 0);
}

static void
test_user_defined_method_return_type_is_scoped_by_class(void) {
  TiDiagnosticList diagnostics =
    diagnose_source("class Hoge:\n"
                    "    def test(self):\n"
                    "        return \"1\"\n"
                    "h = Hoge()\n"
                    "\"x\".find(h.test())\n"
                    "def test():\n"
                    "    return 1\n"
                    "\"x\".find(test())");

  assert(diagnostics.count == 1);
  assert(strcmp(
    diagnostics.items[0].message,
    "type mismatch: expected str, but got int for str.find"
  ) == 0);
}

static void
test_preload_source_has_no_diagnostic(void) {
  const char *preload_source = "\"x\".find(1)";
  const char *source = "\"x\".find(\"a\")";
  TiSource source_items[] = {
    {
      .source = preload_source,
      .source_byte_length = (int)strlen(preload_source),
    },
    {
      .source = source,
      .source_byte_length = (int)strlen(source),
    },
  };
  TiSourceList sources = {
    .items = source_items,
    .count = 2,
  };
  TiDiagnosticList diagnostics;

  micropython_ti_fill_diagnostics(&sources, &diagnostics);

  assert(diagnostics.count == 0);
}

static void
test_literal_bindings(void) {
  TiSuggestionList int_suggestions = suggest_source("a = 1\na.");
  assert(has_suggestion(&int_suggestions, "to_bytes"));

  TiSuggestionList str_suggestions = suggest_source("s = \"x\"\ns.sp");
  assert(has_suggestion(&str_suggestions, "split"));

  TiSuggestionList dict_suggestions = suggest_source("h = {}\nh.ke");
  assert(has_suggestion(&dict_suggestions, "keys"));

  TiSuggestionList bytes_suggestions = suggest_source("b = b\"x\"\nb.");
  assert(has_suggestion(&bytes_suggestions, "lower"));
}

static void
test_binding_lookup(void) {
  TiSuggestionList suggestions = suggest_source("a = 1\nb = a\nb.to");
  assert(has_suggestion(&suggestions, "to_bytes"));
}

static void
test_method_chain(void) {
  TiSuggestionList suggestions =
    suggest_source("s = \"abc\".replace(\"a\", \"b\")\ns.spl");
  assert(has_suggestion(&suggestions, "split"));
}

static void
test_list_element_type(void) {
  TiSuggestionList suggestions = suggest_source("a = [\"x\"]\nb = a[0]\nb.spl");
  assert(has_suggestion(&suggestions, "split"));
}

static void
test_list_comprehension_element_type(void) {
  TiSuggestionList suggestions =
    suggest_source("a = [\"x\" for value in [1]]\nb = a[0]\nb.spl");
  assert(has_suggestion(&suggestions, "split"));
}

static void
test_method_return_list_element_type(void) {
  TiSuggestionList suggestions =
    suggest_source("parts = \"a b\".split()\npart = parts[0]\npart.spl");
  assert(has_suggestion(&suggestions, "split"));
}

static void
test_definition_return(void) {
  TiSuggestionList suggestions =
    suggest_source("def plus_one(value):\n"
                   "    return 1\n"
                   "plus_one().to");
  assert(has_suggestion(&suggestions, "to_bytes"));
}

static void
test_definition_binding_return(void) {
  TiSuggestionList suggestions = suggest_source("def message():\n"
                                                "    value = \"x\"\n"
                                                "    return value\n"
                                                "message().sp");
  assert(has_suggestion(&suggestions, "split"));
}

static void
test_if_return(void) {
  TiSuggestionList suggestions = suggest_source("def mixed(condition):\n"
                                                "    if condition:\n"
                                                "        return 1\n"
                                                "    else:\n"
                                                "        return \"x\"\n"
                                                "mixed(True).");
  assert(has_suggestion(&suggestions, "to_bytes"));
  assert(has_suggestion(&suggestions, "find"));
}

static void
test_match_return(void) {
  TiSuggestionList suggestions = suggest_source("def mixed(value):\n"
                                                "    match value:\n"
                                                "        case 1:\n"
                                                "            return 1\n"
                                                "        case _:\n"
                                                "            return \"x\"\n"
                                                "mixed(1).");
  assert(has_suggestion(&suggestions, "to_bytes"));
  assert(has_suggestion(&suggestions, "find"));
}

static void
test_conditional_expression_binding(void) {
  TiSuggestionList suggestions =
    suggest_source("value = 1 if condition else \"x\"\nvalue.");
  assert(has_suggestion(&suggestions, "to_bytes"));
  assert(has_suggestion(&suggestions, "find"));
}

static void
test_explicit_return(void) {
  TiSuggestionList suggestions = suggest_source("def mixed():\n"
                                                "    return 1\n"
                                                "    return \"x\"\n"
                                                "mixed().");
  assert(has_suggestion(&suggestions, "to_bytes"));
  assert(has_suggestion(&suggestions, "find"));
}

static void
test_imported_module_method_return(void) {
  TiSuggestionList suggestions =
    suggest_source("import gc\ngc.collect().to");
  assert(has_suggestion(&suggestions, "to_bytes"));
}

static void
test_imported_function_return(void) {
  TiSuggestionList suggestions =
    suggest_source("from gc import mem_free\nmem_free().to");
  assert(has_suggestion(&suggestions, "to_bytes"));
}

static void
test_aliased_import_binding(void) {
  TiSuggestionList suggestions =
    suggest_source("import gc as collector\ncollector.co");
  assert(has_suggestion(&suggestions, "collect"));
}

static void
test_constructed_instance_binding(void) {
  TiSuggestionList suggestions =
    suggest_source("pin = GPIO(2, GPIO.OUT)\npin.re");
  assert(has_suggestion(&suggestions, "read"));
}

static void
test_type_at_cursor(void) {
  const char *source = "value = 1\nvalue\n";
  const char *target = strrchr(source, 'v');
  assert(target);

  TiHoverInfo hover_info;
  int found = find_hover(
    source,
    (int)(target - source),
    &hover_info
  );

  assert(found);
  assert(!hover_info.is_method);
  assert(strcmp(hover_info.variable_name, "value") == 0);
  assert(strcmp(hover_info.type_name, "int") == 0);
}

static void
test_union_type_at_cursor(void) {
  const char *source = "value = 1\nvalue = \"x\"\nvalue\n";
  const char *target = strrchr(source, 'v');
  assert(target);

  TiHoverInfo hover_info;
  assert(find_hover(
    source,
    (int)(target - source),
    &hover_info
  ));
  assert(strcmp(hover_info.type_name, "Union<int str>") == 0);
}

static void
test_list_type_at_cursor(void) {
  const char *source = "values = [1]\nvalues\n";
  const char *target = strrchr(source, 'v');
  TiHoverInfo hover_info;

  assert(target);
  assert(find_hover(
    source,
    (int)(target - source),
    &hover_info
  ));
  assert(strcmp(hover_info.type_name, "list<int>") == 0);
}

static void
test_builtin_method_at_cursor(void) {
  const char *source = "\"x\".find(\"a\")";
  const char *target = strstr(source, "find");
  TiHoverInfo hover_info;

  assert(target);
  assert(find_hover(
    source,
    (int)(target - source),
    &hover_info
  ));
  assert(hover_info.is_method);
  assert(hover_info.method_name_length == 4);
  assert(hover_info.method_document);
  assert(strncmp(hover_info.method_signature, "find(", 5) == 0);
}

static void
test_defined_function_at_cursor(void) {
  const char *source = "def answer(value):\n    return 1\nanswer(1)";
  const char *target = strrchr(source, 'a');
  TiHoverInfo hover_info;

  assert(target);
  assert(find_hover(
    source,
    (int)(target - source),
    &hover_info
  ));
  assert(hover_info.is_method);
  assert(hover_info.method_name_length == 6);
  assert(strcmp(hover_info.method_document, "") == 0);
  assert(strcmp(hover_info.method_signature, "answer(value) -> int") == 0);
}

static void
test_forward_definition(void) {
  TiSuggestionList suggestions =
    suggest_source("x = my_function()\n"
                   "def my_function():\n"
                   "    return \"x\"\n"
                   "x.sp");
  assert(has_suggestion(&suggestions, "split"));
}

static void
test_preload_source_definition(void) {
  const char *preload_source = "def answer():\n    return 1";
  const char *source = "answer().";
  TiSource source_items[] = {
    {
      .source = preload_source,
      .source_byte_length = (int)strlen(preload_source),
    },
    {
      .source = source,
      .source_byte_length = (int)strlen(source),
    },
  };
  TiSourceList sources = {
    .items = source_items,
    .count = 2,
  };
  TiSuggestionList suggestions;

  micropython_ti_fill_suggestions_at_cursor(
    &sources,
    source_items[1].source_byte_length,
    &suggestions
  );

  assert(has_suggestion(&suggestions, "to_bytes"));
}

static void
test_union_binding(void) {
  TiSuggestionList suggestions = suggest_source("v = 1\nv = \"s\"\nv.");
  assert(has_suggestion(&suggestions, "to_bytes"));
  assert(has_suggestion(&suggestions, "find"));
}

static void
test_union_capacity(void) {
  uint8_t class_identifiers[MICROPYTHON_TI_UNION_CAPACITY] = {
    MICROPYTHON_TI_CLASS_INT,
    MICROPYTHON_TI_CLASS_FLOAT,
    MICROPYTHON_TI_CLASS_STR,
    MICROPYTHON_TI_CLASS_BYTES,
    MICROPYTHON_TI_CLASS_BOOL,
  };

  ti_reset_arena();
  assert(micropython_ti_initialize_t());

  uint16_t union_t_node_index = 0;

  for (int index = 0; index < MICROPYTHON_TI_UNION_CAPACITY; index++) {
    uint16_t t_node_index =
      micropython_ti_new_t(class_identifiers[index], 0, 0);

    union_t_node_index =
      micropython_ti_make_union(union_t_node_index, t_node_index);
  }

  int union_member_count = 0;

  for (const MicroPythonTiT *union_t = micropython_ti_get_t(union_t_node_index);
       union_t;
       union_t = micropython_ti_get_t(union_t->union_next)) {
    union_member_count++;
  }

  assert(union_member_count == MICROPYTHON_TI_UNION_CAPACITY);

  uint16_t t_node_index =
    micropython_ti_new_t(MICROPYTHON_TI_CLASS_DICT, 0, 0);

  union_t_node_index =
    micropython_ti_make_union(union_t_node_index, t_node_index);

  const MicroPythonTiT *union_t = micropython_ti_get_t(union_t_node_index);
  assert(union_t);
  assert(union_t->object_class_id == MICROPYTHON_TI_CLASS_UNTYPED);
  assert(union_t->union_next == 0);
}

static void
test_unresolved_return_has_no_suggestion(void) {
  TiSuggestionList suggestions =
    suggest_source("def orphan(value):\n"
                   "    return value.foo()\n"
                   "orphan().");
  assert(suggestions.count == 0);
}

static void
test_while_body_binding(void) {
  TiSuggestionList suggestions = suggest_source("def run():\n"
                                                "    while True:\n"
                                                "        assigned = 1\n"
                                                "    return assigned\n"
                                                "run().");
  assert(has_suggestion(&suggestions, "to_bytes"));
}

static void
test_try_body_binding(void) {
  TiSuggestionList suggestions = suggest_source("def run():\n"
                                                "    try:\n"
                                                "        assigned = \"x\"\n"
                                                "    except Exception:\n"
                                                "        pass\n"
                                                "    return assigned\n"
                                                "run().");
  assert(has_suggestion(&suggestions, "find"));
}

static void
test_except_body_binding(void) {
  TiSuggestionList suggestions = suggest_source("def run():\n"
                                                "    try:\n"
                                                "        pass\n"
                                                "    except Exception:\n"
                                                "        assigned = 1\n"
                                                "    return assigned\n"
                                                "run().");
  assert(has_suggestion(&suggestions, "to_bytes"));
}

static void
test_for_body_binding(void) {
  TiSuggestionList suggestions = suggest_source("def run():\n"
                                                "    for item in [1]:\n"
                                                "        assigned = \"x\"\n"
                                                "    return assigned\n"
                                                "run().");
  assert(has_suggestion(&suggestions, "find"));
}

static void
test_with_body_binding(void) {
  TiSuggestionList suggestions = suggest_source("def run():\n"
                                                "    with handle:\n"
                                                "        assigned = 1\n"
                                                "    return assigned\n"
                                                "run().");
  assert(has_suggestion(&suggestions, "to_bytes"));
}

static void
test_nested_definition_in_definition(void) {
  TiSuggestionList suggestions = suggest_source("def outer():\n"
                                                "    def inner():\n"
                                                "        return 1\n"
                                                "    return inner()\n"
                                                "outer().");
  assert(has_suggestion(&suggestions, "to_bytes"));
}

static void
test_binding_overflow(void) {
  size_t capacity = 24000;
  char *source = malloc(capacity);
  assert(source);

  size_t offset = 0;
  for (int index = 0; index < 700; index++) {
    int written = snprintf(
      source + offset,
      capacity - offset,
      "value_%d = %d\n",
      index,
      index
    );
    assert(written > 0);
    offset += (size_t)written;
    assert(offset < capacity - 16);
  }

  memcpy(source + offset, "value_0.\0", 9);
  offset += 8;

  TiSuggestionList suggestions;
  TiSource source_item;
  TiSourceList sources = source_list_for(source, &source_item);
  int count = micropython_ti_fill_suggestions_at_cursor(
    &sources,
    (int)offset,
    &suggestions
  );
  assert(count == 0);

  free(source);
}

int
main(void) {
  test_builtin_argument_type_diagnostic();
  test_unknown_argument_has_no_diagnostic();
  test_list_and_dict_contents_have_no_diagnostic();
  test_builtin_argument_count_diagnostic();
  test_keyword_argument_diagnostic();
  test_splat_arguments_have_no_argument_count_diagnostic();
  test_user_defined_function_arguments_have_no_diagnostic();
  test_user_defined_method_return_type_is_scoped_by_class();
  test_preload_source_has_no_diagnostic();
  test_literal_bindings();
  test_binding_lookup();
  test_method_chain();
  test_list_element_type();
  test_list_comprehension_element_type();
  test_method_return_list_element_type();
  test_definition_return();
  test_definition_binding_return();
  test_if_return();
  test_match_return();
  test_conditional_expression_binding();
  test_explicit_return();
  test_imported_module_method_return();
  test_imported_function_return();
  test_aliased_import_binding();
  test_constructed_instance_binding();
  test_type_at_cursor();
  test_union_type_at_cursor();
  test_list_type_at_cursor();
  test_builtin_method_at_cursor();
  test_defined_function_at_cursor();
  test_forward_definition();
  test_preload_source_definition();
  test_union_binding();
  test_union_capacity();
  test_unresolved_return_has_no_suggestion();
  test_while_body_binding();
  test_try_body_binding();
  test_except_body_binding();
  test_for_body_binding();
  test_with_body_binding();
  test_nested_definition_in_definition();
  test_binding_overflow();

  return 0;
}
