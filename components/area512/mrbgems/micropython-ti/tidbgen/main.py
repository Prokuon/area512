#!/usr/bin/env python3

import argparse
import ast
import re
from pathlib import Path


CORE_CLASSES = [
    "object", "int", "float", "str", "bytes", "bool", "tuple", "list",
    "dict", "range", "NoneType", "untyped", "type", "builtins",
]

# The stubs carry no alias declarations, so the typeshed protocol and alias
# names that stand for a concrete class are resolved from this table instead.
TYPE_ALIASES = {
    "Any": "untyped",
    "Incomplete": "untyped",
    "None": "NoneType",
    "LiteralString": "str",
    "SupportsIndex": "int",
    "SupportsBytes": "bytes",
    "ReadableBuffer": "bytes",
    "WriteableBuffer": "bytes",
}

# Only these keep their type arguments; every other generic resolves to untyped.
GENERIC_CLASSES = ("list", "tuple", "dict")

LITERAL_CLASSES = {
    bool: "bool",
    int: "int",
    float: "float",
    str: "str",
    bytes: "bytes",
    type(None): "NoneType",
}


def annotation_text(annotation):
    return "object" if annotation is None else ast.unparse(annotation)


def literal_names(subscript):
    elements = subscript.slice

    if isinstance(elements, ast.Tuple):
        elements = elements.elts
    else:
        elements = [elements]

    names = []

    for element in elements:
        if not isinstance(element, ast.Constant):
            return ["untyped"]

        names.append(LITERAL_CLASSES.get(type(element.value), "untyped"))

    return names or ["untyped"]


def type_names(annotation, owner):
    if annotation is None:
        return ["object"]

    if isinstance(annotation, ast.BinOp) and isinstance(annotation.op, ast.BitOr):
        return type_names(annotation.left, owner) + type_names(annotation.right, owner)

    if isinstance(annotation, ast.Constant) and annotation.value is None:
        return ["NoneType"]

    if isinstance(annotation, ast.Name):
        if annotation.id in ("Self", "_Self"):
            return [owner]

        return [TYPE_ALIASES.get(annotation.id, annotation.id)]

    if isinstance(annotation, ast.Attribute):
        return [annotation.attr]

    if isinstance(annotation, ast.Subscript):
        name = type_names(annotation.value, owner)[0]

        if name == "Literal":
            return literal_names(annotation)
        return [name] if name in GENERIC_CLASSES else ["untyped"]

    return ["untyped"]


def array_variant_names(annotation, owner):
    if not isinstance(annotation, ast.Subscript):
        return []

    name = type_names(annotation.value, owner)[0]

    if name not in ("list", "tuple"):
        return []

    element = annotation.slice

    if isinstance(element, ast.Tuple):
        element = element.elts[0]

    return type_names(element, owner)


def signature(function, drop_first):
    parameters = []
    positional = function.args.posonlyargs + function.args.args
    default_start = len(positional) - len(function.args.defaults)

    for index, parameter in enumerate(positional):
        if drop_first and index == 0:
            continue

        text = parameter.arg

        if parameter.annotation:
            text += ": " + annotation_text(parameter.annotation)

        if index >= default_start:
            text += " = " + ast.unparse(function.args.defaults[index - default_start])

        parameters.append(text)

    if function.args.vararg:
        text = "*" + function.args.vararg.arg

        if function.args.vararg.annotation:
            text += ": " + annotation_text(function.args.vararg.annotation)

        parameters.append(text)

    for parameter, default in zip(function.args.kwonlyargs, function.args.kw_defaults):
        text = parameter.arg

        if parameter.annotation:
            text += ": " + annotation_text(parameter.annotation)

        if default is not None:
            text += " = " + ast.unparse(default)

        parameters.append(text)

    if function.args.kwarg:
        text = "**" + function.args.kwarg.arg

        if function.args.kwarg.annotation:
            text += ": " + annotation_text(function.args.kwarg.annotation)

        parameters.append(text)

    return f"{function.name}({', '.join(parameters)}) -> {annotation_text(function.returns)}"


def arguments(function, drop_first, owner):
    result = []
    positional = function.args.posonlyargs + function.args.args
    default_start = len(positional) - len(function.args.defaults)

    for index, parameter in enumerate(positional):
        if drop_first and index == 0:
            continue

        kind = "OPTIONAL" if index >= default_start else "REQUIRED"

        result.append((parameter.arg, type_names(parameter.annotation, owner), kind))

    if function.args.vararg:
        result.append((function.args.vararg.arg, type_names(function.args.vararg.annotation, owner), "REST"))

    for parameter, default in zip(function.args.kwonlyargs, function.args.kw_defaults):
        kind = "OPTIONAL_KEYWORD" if default is not None else "REQUIRED_KEYWORD"
        result.append((parameter.arg, type_names(parameter.annotation, owner), kind))

    if function.args.kwarg:
        result.append((function.args.kwarg.arg, type_names(function.args.kwarg.annotation, owner), "REST_KEYWORD"))

    return result


def base_names(class_definition):
    names = []

    for base in class_definition.bases:
        if isinstance(base, ast.Subscript):
            base = base.value
        if isinstance(base, ast.Name):
            names.append(base.id)
        elif isinstance(base, ast.Attribute):
            names.append(base.attr)

    return names


def collect(paths):
    classes = list(CORE_CLASSES)
    bases = {}
    methods = []
    for path in paths:
        module_name = "builtins" if path.stem == "builtins" else path.stem

        if module_name not in classes:
            classes.append(module_name)

        module = ast.parse(path.read_text(encoding="utf-8"), filename=str(path))

        for statement in module.body:
            if isinstance(statement, ast.ClassDef) and statement.name not in classes:
                classes.append(statement.name)

        for statement in module.body:
            if isinstance(statement, (ast.FunctionDef, ast.AsyncFunctionDef)):
                methods.append(method_record(module_name, statement, True, False))
            elif isinstance(statement, ast.ClassDef):
                bases.setdefault(statement.name, []).extend(base_names(statement))

                for member in statement.body:
                    if not isinstance(member, (ast.FunctionDef, ast.AsyncFunctionDef)):
                        continue

                    decorators = {ast.unparse(item) for item in member.decorator_list}
                    is_static = "staticmethod" in decorators or "classmethod" in decorators
                    drop_first = "staticmethod" not in decorators

                    methods.append(method_record(statement.name, member, is_static, drop_first))

    known_bases = {
        name: [base for base in base_list if base in classes]
        for name, base_list in bases.items()
    }

    return classes, known_bases, methods


def method_tables(methods):
    tables = {}

    for method in methods:
        table = tables.setdefault(method["origin"], {"instance": {}, "static": {}})
        table["static" if method["is_static"] else "instance"].setdefault(
            method["name"], method
        )

    return tables


def collect_methods_from_class_and_bases(
    class_name, kind, tables, bases, resolved_methods, visited_class_names
):
    if class_name in visited_class_names:
        return

    visited_class_names.add(class_name)

    for name, method in tables.get(class_name, {}).get(kind, {}).items():
        resolved_methods.setdefault(name, method)

    for base_name in bases.get(class_name, []):
        collect_methods_from_class_and_bases(
            base_name, kind, tables, bases, resolved_methods, visited_class_names
        )


def resolve_methods(class_name, kind, tables, bases):
    resolved_methods = {}
    visited_class_names = set()

    collect_methods_from_class_and_bases(
        class_name, kind, tables, bases, resolved_methods, visited_class_names
    )

    if class_name != "object":
        collect_methods_from_class_and_bases(
            "object", kind, tables, bases, resolved_methods, visited_class_names
        )

    return [resolved_methods[name] for name in sorted(resolved_methods)]


def method_record(origin, function, is_static, drop_first):
    return {
        "origin": origin,
        "name": function.name,
        "signature": signature(function, drop_first),
        "document": ast.get_docstring(function, clean=False) or "",
        "arguments": arguments(function, drop_first, origin),
        "returns": type_names(function.returns, origin),
        "variants": array_variant_names(function.returns, origin),
        "is_static": is_static,
    }


def enum_name(name):
    return re.sub(r"[^A-Za-z0-9]+", "_", name).strip("_").upper()


class Pool:
    def __init__(self):
        self.data = "\0"
        self.offsets = {"": 0}

    def add(self, value):
        if value not in self.offsets:
            self.offsets[value] = len(self.data.encode("utf-8"))
            self.data += value + "\0"
        return self.offsets[value]


def c_string(value):
    escaped = value.replace("\\", "\\\\").replace('"', '\\"').replace("\n", "\\n").replace("\r", "\\r").replace("\0", "\\000")
    return f'"{escaped}"'


def write_database(output, classes, bases, methods):
    output.mkdir(parents=True, exist_ok=True)

    class_ids = {name: index + 1 for index, name in enumerate(classes)}
    class_ids["None"] = class_ids["NoneType"]
    class_ids["Any"] = class_ids["untyped"]

    name_pool, signature_pool, document_pool = Pool(), Pool(), Pool()

    unions = [(0, 0, 0, 0)]
    union_indexes = {unions[0]: 0}

    def resolved(names):
        identifiers = []

        for name in names:
            identifier = class_ids.get(name, class_ids["untyped"])

            if identifier not in identifiers:
                identifiers.append(identifier)

        return identifiers[:4]

    def type_fields(names):
        identifiers = resolved(names)

        if len(identifiers) <= 1:
            return (identifiers[0] if identifiers else 0, 0)

        key = tuple(identifiers + [0] * (4 - len(identifiers)))

        if key not in union_indexes:
            union_indexes[key] = len(unions)
            unions.append(key)

        return 0, union_indexes[key]

    tables = method_tables(methods)
    ordered_methods = []
    class_rows = [(0, 0, 0, 0, 0)]

    for class_name in classes:
        instance = resolve_methods(class_name, "instance", tables, bases)
        static = resolve_methods(class_name, "static", tables, bases)

        instance_start = len(ordered_methods)
        ordered_methods.extend(instance)

        static_start = len(ordered_methods)
        ordered_methods.extend(static)

        class_rows.append((name_pool.add(class_name), instance_start, len(instance), static_start, len(static)))

    argument_rows = []
    method_rows = []

    for method in ordered_methods:
        argument_start = len(argument_rows)

        for name, names, kind in method["arguments"]:
            class_id, union_index = type_fields(names)
            argument_rows.append((name_pool.add(name), class_id, union_index, kind))

        return_class_id, return_union_index = type_fields(method["returns"])
        variant_class_id, variant_union_index = type_fields(method["variants"])

        method_rows.append((
            name_pool.add(method["name"]), signature_pool.add(method["signature"]),
            document_pool.add(method["document"]), argument_start,
            len(method["arguments"]), return_class_id, variant_class_id,
            return_union_index, variant_union_index, 0,
            class_ids[method["origin"]],
        ))

    header = [
        "#ifndef MICROPYTHON_TI_BUILTIN_DATABASE_H",
        "#define MICROPYTHON_TI_BUILTIN_DATABASE_H", "", "#include <stdint.h>", "",
        "typedef enum {", "  MICROPYTHON_TI_CLASS_NONE = 0,",
    ]

    for name in classes:
        header.append(f"  MICROPYTHON_TI_CLASS_{enum_name(name)},")

    header.extend([
        "  MICROPYTHON_TI_CLASS_BUILTIN_COUNT,",
        "  MICROPYTHON_TI_CLASS_USER_BASE = MICROPYTHON_TI_CLASS_BUILTIN_COUNT",
        "} MicroPythonTiClassIdentifier;", "",
        "typedef struct { uint16_t name_offset; uint16_t signature_offset; uint16_t document_offset; uint16_t argument_start_index; uint8_t argument_count; uint8_t return_class_identifier; uint8_t return_array_variant_class_identifier; uint16_t return_union_index; uint16_t array_variant_union_index; uint8_t block_parameter_class_identifier; uint8_t origin_class_identifier; } MicroPythonTiBuiltinMethod;",
        "typedef enum { MICROPYTHON_TI_BUILTIN_ARGUMENT_REQUIRED = 0, MICROPYTHON_TI_BUILTIN_ARGUMENT_OPTIONAL, MICROPYTHON_TI_BUILTIN_ARGUMENT_REST, MICROPYTHON_TI_BUILTIN_ARGUMENT_REQUIRED_KEYWORD, MICROPYTHON_TI_BUILTIN_ARGUMENT_OPTIONAL_KEYWORD, MICROPYTHON_TI_BUILTIN_ARGUMENT_REST_KEYWORD } MicroPythonTiBuiltinArgumentKind;",
        "typedef struct { uint16_t name_offset; uint8_t class_identifier; uint16_t union_index; uint8_t kind; } MicroPythonTiBuiltinArgument;",
        "typedef struct { uint16_t name_offset; uint16_t instance_method_start_index; uint16_t instance_method_count; uint16_t static_method_start_index; uint16_t static_method_count; } MicroPythonTiBuiltinClass;",
        "typedef struct { uint8_t member_class_identifiers[4]; } MicroPythonTiBuiltinUnion;", "",
        "extern const MicroPythonTiBuiltinClass micropython_ti_builtin_classes[];",
        "extern const MicroPythonTiBuiltinMethod micropython_ti_builtin_methods[];",
        "extern const MicroPythonTiBuiltinArgument micropython_ti_builtin_arguments[];",
        "extern const MicroPythonTiBuiltinUnion micropython_ti_builtin_unions[];",
        "extern const char micropython_ti_builtin_name_pool[];",
        "extern const char micropython_ti_builtin_signature_pool[];",
        "extern const char micropython_ti_builtin_document_pool[];",
        "extern const uint16_t micropython_ti_builtin_class_count;",
        "extern const uint16_t micropython_ti_builtin_method_count;",
        "extern const uint16_t micropython_ti_builtin_argument_count;",
        "extern const uint16_t micropython_ti_builtin_union_count;",
        "extern const uint16_t micropython_ti_builtin_name_pool_size;",
        "extern const uint16_t micropython_ti_builtin_signature_pool_size;",
        "extern const uint16_t micropython_ti_builtin_document_pool_size;", "", "#endif", "",
    ])
    (output / "micropython_ti_builtin_database.h").write_text("\n".join(header), encoding="utf-8")

    source = ['#include "micropython_ti_builtin_database.h"', ""]
    source.append("const MicroPythonTiBuiltinClass micropython_ti_builtin_classes[] = {")
    source.extend(f"  {{{', '.join(map(str, row))}}}," for row in class_rows)
    source.append("};\n")
    source.append("const MicroPythonTiBuiltinMethod micropython_ti_builtin_methods[] = {")
    source.extend(f"  {{{', '.join(map(str, row))}}}," for row in method_rows)
    source.append("};\n")
    source.append("const MicroPythonTiBuiltinArgument micropython_ti_builtin_arguments[] = {")
    source.extend(f"  {{{row[0]}, {row[1]}, {row[2]}, MICROPYTHON_TI_BUILTIN_ARGUMENT_{row[3]}}}," for row in argument_rows)
    source.append("};\n")
    source.append("const MicroPythonTiBuiltinUnion micropython_ti_builtin_unions[] = {")
    source.extend(f"  {{{{{', '.join(map(str, row))}}}}}," for row in unions)
    source.append("};\n")

    for name, pool in (("name", name_pool), ("signature", signature_pool), ("document", document_pool)):
        source.append(f"const char micropython_ti_builtin_{name}_pool[] = {c_string(pool.data)};")

    source.extend([
        f"const uint16_t micropython_ti_builtin_class_count = {len(class_rows)};",
        f"const uint16_t micropython_ti_builtin_method_count = {len(method_rows)};",
        f"const uint16_t micropython_ti_builtin_argument_count = {len(argument_rows)};",
        f"const uint16_t micropython_ti_builtin_union_count = {len(unions)};",
        f"const uint16_t micropython_ti_builtin_name_pool_size = {len(name_pool.data.encode('utf-8'))};",
        f"const uint16_t micropython_ti_builtin_signature_pool_size = {len(signature_pool.data.encode('utf-8'))};",
        f"const uint16_t micropython_ti_builtin_document_pool_size = {len(document_pool.data.encode('utf-8'))};", "",
    ])

    (output / "micropython_ti_builtin_database.c").write_text("\n".join(source), encoding="utf-8")


def main():
    parser = argparse.ArgumentParser()

    parser.add_argument("--sig-dir", required=True, type=Path)
    parser.add_argument("--out", required=True, type=Path)

    arguments = parser.parse_args()
    paths = sorted(arguments.sig_dir.glob("*.pyi"))

    if not paths:
        parser.error(f"signature directory contains no PYI files: {arguments.sig_dir}")

    write_database(arguments.out, *collect(paths))


if __name__ == "__main__":
    main()
