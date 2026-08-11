#pragma once

int area512_micropython_compile_python_source_file(
  const char *python_source_path,
  const char *python_bytecode_path
);

int area512_micropython_run_python_bytecode_file(
  const char *python_bytecode_path
);

int area512_micropython_run_python_manifest(
  const char *directory_path,
  const char *manifest_path
);
