PICORUBY_TI_ROOT := ../../picoruby-ti
PRISM_ROOT := $(PICORUBY_TI_ROOT)/lib/prism
PRISM_HEADER := $(PRISM_ROOT)/include/prism/ast.h
TREE_SITTER_ROOT := ../../../../area512_tree_sitter/tree-sitter
TREE_SITTER_PYTHON_ROOT := ../../../../area512_tree_sitter/tree-sitter-python
SOURCE_ROOT := ../src
SOURCE_DIRS := base builtin context diagnostic eval eval/method_evaluator hover suggest
ENGINE_SOURCES := \
  $(foreach source_dir,$(SOURCE_DIRS),$(wildcard $(SOURCE_ROOT)/$(source_dir)/*.c))
GENERATED_SOURCE := $(SOURCE_ROOT)/generated/micropython_ti_builtin_database.c
PARSER_SOURCES := \
  $(TREE_SITTER_ROOT)/lib/src/lib.c \
  $(TREE_SITTER_PYTHON_ROOT)/src/parser.c \
  $(TREE_SITTER_PYTHON_ROOT)/src/scanner.c
ARENA_SOURCE := $(PICORUBY_TI_ROOT)/src/base/picoruby_ti_arena.c
OBJECT_ROOT := build
ENGINE_OBJECTS := \
  $(patsubst $(SOURCE_ROOT)/%.c,$(OBJECT_ROOT)/%.o,$(ENGINE_SOURCES))
GENERATED_OBJECT := \
  $(patsubst $(SOURCE_ROOT)/%.c,$(OBJECT_ROOT)/%.o,$(GENERATED_SOURCE))
PARSER_OBJECTS := \
  $(OBJECT_ROOT)/parser/lib.o \
  $(OBJECT_ROOT)/parser/parser.o \
  $(OBJECT_ROOT)/parser/scanner.o
ARENA_OBJECT := $(OBJECT_ROOT)/arena/picoruby_ti_arena.o
TEST_OBJECTS := \
  $(OBJECT_ROOT)/test_builtin.o \
  $(OBJECT_ROOT)/test_eval.o \
  $(OBJECT_ROOT)/test_suggest.o
DEPENDENCIES := \
  $(ENGINE_OBJECTS:.o=.d) \
  $(GENERATED_OBJECT:.o=.d) \
  $(PARSER_OBJECTS:.o=.d) \
  $(ARENA_OBJECT:.o=.d) \
  $(TEST_OBJECTS:.o=.d)
INCLUDE_FLAGS := \
  $(foreach source_dir,$(SOURCE_DIRS),-I$(SOURCE_ROOT)/$(source_dir)) \
  -I../include -I$(SOURCE_ROOT)/generated \
  -I$(PICORUBY_TI_ROOT)/include -I$(PICORUBY_TI_ROOT)/src/base \
  -I$(PRISM_ROOT)/include \
  -I$(TREE_SITTER_ROOT)/lib/include \
  -I$(TREE_SITTER_PYTHON_ROOT)/bindings/c
PARSER_INCLUDE_FLAGS := \
  -I$(TREE_SITTER_ROOT)/lib/include \
  -I$(TREE_SITTER_ROOT)/lib/src \
  -I$(TREE_SITTER_PYTHON_ROOT)/src
CFLAGS := -std=c11 -Wall -Wextra -Werror -MMD -MP $(INCLUDE_FLAGS)
PARSER_DEFINE_FLAGS := \
  -DHAVE_ENDIAN_H=1 -D_POSIX_C_SOURCE=200112L -D_DEFAULT_SOURCE -D_BSD_SOURCE
PARSER_CFLAGS := \
  -std=c11 -MMD -MP $(PARSER_DEFINE_FLAGS) $(PARSER_INCLUDE_FLAGS)

.PHONY: all test clean

all: test_builtin test_eval test_suggest

test:
	$(MAKE) all
	./test_builtin
	./test_eval
	./test_suggest

$(PRISM_HEADER):
	cd $(PRISM_ROOT) && ruby templates/template.rb

$(ENGINE_OBJECTS) $(TEST_OBJECTS): $(PRISM_HEADER)

$(OBJECT_ROOT)/%.o: $(SOURCE_ROOT)/%.c
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJECT_ROOT)/%.o: %.c
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJECT_ROOT)/parser/lib.o: $(TREE_SITTER_ROOT)/lib/src/lib.c
	@mkdir -p $(@D)
	$(CC) $(PARSER_CFLAGS) -c $< -o $@

$(OBJECT_ROOT)/parser/parser.o: $(TREE_SITTER_PYTHON_ROOT)/src/parser.c
	@mkdir -p $(@D)
	$(CC) $(PARSER_CFLAGS) -c $< -o $@

$(OBJECT_ROOT)/parser/scanner.o: $(TREE_SITTER_PYTHON_ROOT)/src/scanner.c
	@mkdir -p $(@D)
	$(CC) $(PARSER_CFLAGS) -c $< -o $@

$(ARENA_OBJECT): $(ARENA_SOURCE)
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) -c $< -o $@

test_builtin: \
  $(OBJECT_ROOT)/test_builtin.o \
  $(OBJECT_ROOT)/builtin/micropython_ti_builtin.o \
  $(GENERATED_OBJECT)
	$(CC) $^ -o $@

test_eval: \
  $(OBJECT_ROOT)/test_eval.o \
  $(ENGINE_OBJECTS) \
  $(GENERATED_OBJECT) \
  $(PARSER_OBJECTS) \
  $(ARENA_OBJECT)
	$(CC) $^ -o $@

test_suggest: \
  $(OBJECT_ROOT)/test_suggest.o \
  $(ENGINE_OBJECTS) \
  $(GENERATED_OBJECT) \
  $(PARSER_OBJECTS) \
  $(ARENA_OBJECT)
	$(CC) $^ -o $@

clean:
	$(RM) -r $(OBJECT_ROOT) test_builtin test_eval test_suggest

-include $(DEPENDENCIES)
