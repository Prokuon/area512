set(TREE_SITTER_SOURCE_DIRECTORY ${COMPONENT_DIR}/tree-sitter)
set(TREE_SITTER_PYTHON_SOURCE_DIRECTORY ${COMPONENT_DIR}/tree-sitter-python)

idf_component_register(
  SRCS
    ${TREE_SITTER_SOURCE_DIRECTORY}/lib/src/lib.c
    ${TREE_SITTER_PYTHON_SOURCE_DIRECTORY}/src/parser.c
    ${TREE_SITTER_PYTHON_SOURCE_DIRECTORY}/src/scanner.c
  INCLUDE_DIRS
    ${TREE_SITTER_SOURCE_DIRECTORY}/lib/include
    ${TREE_SITTER_PYTHON_SOURCE_DIRECTORY}/bindings/c
  PRIV_INCLUDE_DIRS
    ${TREE_SITTER_SOURCE_DIRECTORY}/lib/src
    ${TREE_SITTER_PYTHON_SOURCE_DIRECTORY}/src
)

target_compile_definitions(
  ${COMPONENT_LIB}
  PRIVATE
    HAVE_ENDIAN_H=1
    TREE_SITTER_REUSE_ALLOCATOR
    _POSIX_C_SOURCE=200112L
    _DEFAULT_SOURCE
    _BSD_SOURCE
)

target_compile_options(
  ${COMPONENT_LIB}
  PRIVATE
    -Os
    -Wno-format
    -Wno-incompatible-pointer-types
    -Wno-unused-value
)
