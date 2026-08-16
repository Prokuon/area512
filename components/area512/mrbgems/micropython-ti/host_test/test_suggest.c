#include "micropython_ti_suggest.h"
#include <assert.h>
#include <string.h>

static TiSuggestionList
suggest_source(const char *source) {
  TiSuggestionList suggestions;
  int source_length = (int)strlen(source);
  TiSource source_item = {
    .source = source,
    .source_byte_length = source_length,
  };
  TiSourceList sources = {
    .items = &source_item,
    .count = 1,
  };

  micropython_ti_fill_suggestions_at_cursor(
    &sources,
    source_length,
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
    suggest_source("def hello(name):\n"
                   "    return 1\n"
                   "he");
  const TiSuggestion *suggestion = find_suggestion(&suggestions, "hello");

  assert(suggestion);
  assert(strcmp(suggestion->detail, "hello(name) -> int") == 0);
}

static void
test_rest_and_keyword_rest_parameter_suggestion(void) {
  TiSuggestionList suggestions =
    suggest_source("def test(x, *a, **b):\n"
                   "    return 1\n"
                   "te");
  const TiSuggestion *suggestion = find_suggestion(&suggestions, "test");

  assert(suggestion);
  assert(strcmp(suggestion->detail, "test(x, *a, **b) -> int") == 0);
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
  TiSuggestionList suggestions = suggest_source("v = 1\nv = \"s\"\nv.");
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
    suggest_source("v = b\"x\"\nv = \"s\"\nv.spl");
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
test_union_skips_user_class(void) {
  TiSuggestionList suggestions = suggest_source("class Foo:\n"
                                                "    def bar(self):\n"
                                                "        return 1\n"
                                                "value = Foo()\n"
                                                "value = 1\n"
                                                "value.");

  assert(find_suggestion(&suggestions, "to_bytes"));
  assert(!find_suggestion(&suggestions, "bar"));
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
  TiSuggestionList suggestions = suggest_source("class Foo:\n"
                                                "    def bar(self, value):\n"
                                                "        return 1\n"
                                                "foo = Foo()\n"
                                                "foo.ba");
  const TiSuggestion *suggestion = find_suggestion(&suggestions, "bar");

  assert(suggestion);
  assert(strcmp(suggestion->detail, "bar(value) -> int") == 0);
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
  TiSuggestionList suggestions = suggest_source("class Foo:\n"
                                                "    def value(self, foo):\n"
                                                "        return 1\n"
                                                "class Bar:\n"
                                                "    def value(self, bar):\n"
                                                "        return 1\n"
                                                "bar_instance = Bar()\n"
                                                "bar_instance.val");
  const TiSuggestion *suggestion = find_suggestion(&suggestions, "value");

  assert(suggestion);
  assert(strcmp(suggestion->detail, "value(bar) -> int") == 0);
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
  test_union_skips_user_class();
  test_static_method_suggestion();
  test_imported_module_suggestion();
  test_user_class_suggestion();
  test_user_class_only_suggests_its_methods();
  test_same_method_name_in_different_classes();
  test_nested_class_methods_have_separate_owners();

  return 0;
}
