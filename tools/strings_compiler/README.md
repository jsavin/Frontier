# strings_compiler

`strings_compiler` is the YAML-to-C generator for Frontier string tables. It
uses the vendored libyaml 0.2.5 parser to accept a full YAML 1.1 subset and
emit generated C tables, headers, and a manifest. Usage:

```
strings_compiler --c-output generated/strings_tables.c \
                 --h-output generated/strings_tables.h \
                 --manifest generated/strings_manifest.json \
                 resources/strings/langerrorlist.yaml
```

Omit any of the `--*-output` flags to skip generating that artifact. Passing
`-` as the input path reads from `stdin`.

Build the tool locally with:

```
make -C tools/strings_compiler
```

The tool currently expects a top-level mapping from table names to sequences of
entry mappings. Each entry may include `id`, `text`, and `index` keys; `index`
defaults to auto-increment if omitted. Thanks to libyaml we benefit from
standard YAML features (quoted strings, folded blocks, comments, etc.).
