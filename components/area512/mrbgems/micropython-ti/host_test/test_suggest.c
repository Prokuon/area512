#include "micropython_ti_suggest.h"
#include <assert.h>
#include <string.h>

static TiSuggestionList
suggest_source(const char *source) {
  TiSuggestionList suggestions;
  int source_byte_length = (int)strlen(source);
  TiSource source_item = {
    .source = source,
    .source_byte_length = source_byte_length,
  };
  TiSourceList sources = {
    .items = &source_item,
    .count = 1,
  };

  micropython_ti_fill_suggestions_at_cursor(
    &sources,
    source_byte_length,
    &suggestions
  );

  return suggestions;
}

static TiSuggestionList
suggest_with_preload_source(const char *preload_source, const char *source) {
  TiSuggestionList suggestions;
  int preload_source_byte_length = (int)strlen(preload_source);
  int source_byte_length = (int)strlen(source);
  TiSource source_items[] = {
    {
      .source = preload_source,
      .source_byte_length = preload_source_byte_length,
    },
    {
      .source = source,
      .source_byte_length = source_byte_length,
    },
  };
  TiSourceList sources = {
    .items = source_items,
    .count = 2,
  };

  micropython_ti_fill_suggestions_at_cursor(
    &sources,
    source_byte_length,
    &suggestions
  );

  return suggestions;
}

static const TiSuggestion *
find_suggestion(const TiSuggestionList *suggestions, const char *contents) {
  for (int index = 0; index < suggestions->count; index++) {
    if (strcmp(suggestions->items[index].contents, contents) == 0)
      return &suggestions->items[index];
  }

  return NULL;
}

static void
test_str_suggestion(void) {
  TiSuggestionList suggestions = suggest_source("s = \"abc\"\ns.");
  assert(suggestions.count > 0);

  for (int index = 1; index < suggestions.count; index++) {
    assert(
      strcmp(
        suggestions.items[index - 1].contents,
        suggestions.items[index].contents
      ) <= 0
    );
  }
}

static void
test_prefix_suggestion(void) {
  TiSuggestionList suggestions = suggest_source("s = \"abc\"\ns.spl");
  assert(suggestions.count == 1);
  assert(strcmp(suggestions.items[0].contents, "split") == 0);
}

static void
test_unknown_receiver(void) {
  TiSuggestionList suggestions = suggest_source("x.");
  assert(suggestions.count == 0);
}

static void
test_receiverless_suggestion(void) {
  TiSuggestionList suggestions = suggest_source("pri");

  assert(find_suggestion(&suggestions, "print"));
  assert(!find_suggestion(&suggestions, "input"));
}

static void
test_receiverless_top_level_function_suggestion(void) {
  TiSuggestionList suggestions =
    suggest_source("def hello(name: str) -> int:\n"
                   "    return 1\n"
                   "he");
  const TiSuggestion *suggestion = find_suggestion(&suggestions, "hello");

  assert(suggestion);
  assert(strcmp(suggestion->detail, "hello(name: str) -> int") == 0);
}

static void
test_rest_and_keyword_rest_parameter_suggestion(void) {
  TiSuggestionList suggestions =
    suggest_source("def test(x, *a, **b) -> int:\n"
                   "    return 1\n"
                   "te");
  const TiSuggestion *suggestion = find_suggestion(&suggestions, "test");

  assert(suggestion);
  assert(strcmp(
    suggestion->detail,
    "test(x: untyped, *a: untyped, **b: untyped) -> int"
  ) == 0);
}

static void
test_receiverless_user_class_suggestion(void) {
  TiSuggestionList suggestions = suggest_source("class User:\n"
                                                "    def hoge(self):\n"
                                                "        return 1\n"
                                                "    def fuga(self):\n"
                                                "        ho");

  assert(find_suggestion(&suggestions, "hoge"));
}

static void
test_receiverless_builtin_class_suggestion(void) {
  TiSuggestionList suggestions = suggest_source("Con");
  const TiSuggestion *suggestion = find_suggestion(&suggestions, "Console");

  assert(suggestion);
  assert(strcmp(suggestion->detail, "Console") == 0);
}

static void
test_receiverless_defined_class_suggestion(void) {
  TiSuggestionList suggestions = suggest_source("class User:\n"
                                                "    pass\n"
                                                "Us");
  const TiSuggestion *suggestion = find_suggestion(&suggestions, "User");

  assert(suggestion);
  assert(strcmp(suggestion->detail, "User") == 0);
}

static void
test_receiverless_lowercase_prefix_skips_class_suggestions(void) {
  TiSuggestionList suggestions = suggest_source("class User:\n"
                                                "    pass\n"
                                                "us");

  assert(!find_suggestion(&suggestions, "User"));
}

static void
test_union_suggestion(void) {
  TiSuggestionList suggestions =
    suggest_source("v = 1 if condition else \"s\"\nv.");
  const TiSuggestion *int_suggestion =
    find_suggestion(&suggestions, "to_bytes");
  const TiSuggestion *str_suggestion = find_suggestion(&suggestions, "find");

  assert(int_suggestion);
  assert(str_suggestion);
  assert(strcmp(int_suggestion->class_name, "int") == 0);
  assert(strcmp(str_suggestion->class_name, "str") == 0);

  for (int first = 0; first < suggestions.count; first++) {
    for (int second = first + 1; second < suggestions.count; second++) {
      int same_name = strcmp(
                        suggestions.items[first].contents,
                        suggestions.items[second].contents
                      ) == 0;
      int same_detail = strcmp(
                          suggestions.items[first].detail,
                          suggestions.items[second].detail
                        ) == 0;

      assert(!(same_name && same_detail));
    }
  }
}

static void
test_union_prefix_suggestion(void) {
  TiSuggestionList suggestions =
    suggest_source("v = b\"x\" if condition else \"s\"\nv.spl");
  const TiSuggestion *bytes_suggestion =
    find_suggestion(&suggestions, "split");

  assert(bytes_suggestion);
  assert(suggestions.count == 2);
  assert(strcmp(suggestions.items[0].class_name, "bytes") == 0);
  assert(strcmp(suggestions.items[1].class_name, "str") == 0);

  for (int index = 0; index < suggestions.count; index++)
    assert(strcmp(suggestions.items[index].contents, "split") == 0);
}

static void
test_union_includes_user_class(void) {
  TiSuggestionList suggestions =
    suggest_source("class Foo:\n"
                   "    def bar(self):\n"
                   "        return 1\n"
                   "value = Foo() if condition else 1\n"
                   "value.");
  const TiSuggestion *suggestion = find_suggestion(&suggestions, "bar");

  assert(find_suggestion(&suggestions, "to_bytes"));
  assert(suggestion);
  assert(strcmp(suggestion->class_name, "Foo") == 0);
}

static void
test_instance_attribute_suggestion(void) {
  TiSuggestionList suggestions =
    suggest_source("class Holder:\n"
                   "    def __init__(self):\n"
                   "        self.name = \"x\"\n"
                   "holder = Holder()\n"
                   "holder.");
  const TiSuggestion *suggestion = find_suggestion(&suggestions, "name");

  assert(suggestion);
  assert(strcmp(suggestion->detail, "name: str") == 0);
}

static void
test_self_attribute_suggestion(void) {
  TiSuggestionList suggestions =
    suggest_source("class Holder:\n"
                   "    def __init__(self):\n"
                   "        self.count = 1\n"
                   "    def run(self):\n"
                   "        self.");

  assert(find_suggestion(&suggestions, "count"));
  assert(find_suggestion(&suggestions, "run"));
}

static void
test_static_method_suggestion(void) {
  TiSuggestionList suggestions = suggest_source("Console.re");
  assert(find_suggestion(&suggestions, "reset"));
}

static void
test_imported_module_suggestion(void) {
  TiSuggestionList suggestions = suggest_source("import gc\ngc.co");
  assert(find_suggestion(&suggestions, "collect"));
}

static void
test_user_class_suggestion(void) {
  TiSuggestionList suggestions =
    suggest_source("class Foo:\n"
                   "    def bar(self, value: int) -> int:\n"
                   "        return 1\n"
                   "foo = Foo()\n"
                   "foo.ba");
  const TiSuggestion *suggestion = find_suggestion(&suggestions, "bar");

  assert(suggestion);
  assert(strcmp(suggestion->detail, "bar(value: int) -> int") == 0);
}

static void
test_user_class_only_suggests_its_methods(void) {
  TiSuggestionList suggestions = suggest_source("class Foo:\n"
                                                "    def foo_method(self):\n"
                                                "        return 1\n"
                                                "class Bar:\n"
                                                "    def bar_method(self):\n"
                                                "        return 1\n"
                                                "foo = Foo()\n"
                                                "foo.");

  assert(find_suggestion(&suggestions, "foo_method"));
  assert(!find_suggestion(&suggestions, "bar_method"));
}

static void
test_same_method_name_in_different_classes(void) {
  TiSuggestionList suggestions =
    suggest_source("class Foo:\n"
                   "    def value(self, foo):\n"
                   "        return 1\n"
                   "class Bar:\n"
                   "    def value(self, bar) -> int:\n"
                   "        return 1\n"
                   "bar_instance = Bar()\n"
                   "bar_instance.val");
  const TiSuggestion *suggestion = find_suggestion(&suggestions, "value");

  assert(suggestion);
  assert(strcmp(suggestion->detail, "value(bar: untyped) -> int") == 0);
}

static void
test_nested_class_methods_have_separate_owners(void) {
  TiSuggestionList suggestions = suggest_source("class Outer:\n"
                                                "    def outer_method(self):\n"
                                                "        return 1\n"
                                                "    class Inner:\n"
                                                "        def inner_method(self):\n"
                                                "            return 1\n"
                                                "outer = Outer()\n"
                                                "outer.");

  assert(find_suggestion(&suggestions, "outer_method"));
  assert(!find_suggestion(&suggestions, "inner_method"));
}

static void
test_signature_shows_argument_types(void) {
  TiSuggestionList suggestions =
    suggest_source("def run(first: int, second) -> str:\n"
                   "    return \"x\"\n"
                   "ru");
  const TiSuggestion *suggestion = find_suggestion(&suggestions, "run");

  assert(suggestion);
  assert(strcmp(
    suggestion->detail,
    "run(first: int, second: untyped) -> str"
  ) == 0);
}

static void
test_unsupported_type_hint_is_untyped(void) {
  TiSuggestionList suggestions =
    suggest_source("def run(value: list[int]) -> int | None:\n"
                   "    return 1\n"
                   "ru");
  const TiSuggestion *suggestion = find_suggestion(&suggestions, "run");

  assert(suggestion);
  assert(strcmp(
    suggestion->detail,
    "run(value: untyped) -> Union<int NoneType>"
  ) == 0);
}

static void
test_preload_initializer_attribute_is_suggested(void) {
  TiSuggestionList suggestions =
    suggest_with_preload_source(
      "class Holder:\n"
      "    def __init__(self):\n"
      "        self.value = 1\n",
      "Holder()."
    );

  assert(find_suggestion(&suggestions, "value"));
}

static void
test_preload_non_initializer_attribute_is_not_suggested(void) {
  TiSuggestionList suggestions =
    suggest_with_preload_source(
      "class Holder:\n"
      "    def setup(self):\n"
      "        self.value = 1\n",
      "Holder()."
    );

  assert(!find_suggestion(&suggestions, "value"));
}

static void
test_preload_top_level_binding_is_suggested(void) {
  TiSuggestionList suggestions =
    suggest_with_preload_source("answer = 1\n", "answer.to_by");

  assert(find_suggestion(&suggestions, "to_bytes"));
}

static void
test_preload_class_variable_is_suggested(void) {
  TiSuggestionList suggestions =
    suggest_with_preload_source(
      "class Holder:\n"
      "    limit = 1\n",
      "Holder()."
    );

  assert(find_suggestion(&suggestions, "limit"));
}

static void
test_preload_aliased_import_is_not_suggested(void) {
  TiSuggestionList suggestions =
    suggest_with_preload_source("import gc as collector\n", "collector.co");

  assert(!find_suggestion(&suggestions, "collect"));
}

int
main(void) {
  test_str_suggestion();
  test_prefix_suggestion();
  test_unknown_receiver();
  test_receiverless_suggestion();
  test_receiverless_top_level_function_suggestion();
  test_rest_and_keyword_rest_parameter_suggestion();
  test_receiverless_user_class_suggestion();
  test_receiverless_builtin_class_suggestion();
  test_receiverless_defined_class_suggestion();
  test_receiverless_lowercase_prefix_skips_class_suggestions();
  test_union_suggestion();
  test_union_prefix_suggestion();
  test_union_includes_user_class();
  test_instance_attribute_suggestion();
  test_self_attribute_suggestion();
  test_static_method_suggestion();
  test_imported_module_suggestion();
  test_user_class_suggestion();
  test_user_class_only_suggests_its_methods();
  test_same_method_name_in_different_classes();
  test_nested_class_methods_have_separate_owners();
  test_signature_shows_argument_types();
  test_unsupported_type_hint_is_untyped();
  test_preload_initializer_attribute_is_suggested();
  test_preload_non_initializer_attribute_is_not_suggested();
  test_preload_top_level_binding_is_suggested();
  test_preload_class_variable_is_suggested();
  test_preload_aliased_import_is_not_suggested();

  return 0;
}
