#include "strings_compiler.h"
#include <ctype.h>
#include <errno.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

strings_document g_document = {0};

static strings_table *g_current_table = NULL;
static strings_entry *g_current_entry = NULL;
static int g_error_count = 0;

static void *xrealloc(void *ptr, size_t size) {
    void *result = realloc(ptr, size);
    if (!result) {
        fprintf(stderr, "strings_compiler: out of memory (requested %zu bytes)\n", size);
        exit(EXIT_FAILURE);
    }
    return result;
}

static char *xstrdup(const char *s) {
    if (!s)
        return NULL;
    size_t len = strlen(s);
    char *copy = (char *)malloc(len + 1);
    if (!copy) {
        fprintf(stderr, "strings_compiler: out of memory (requested %zu bytes)\n", len + 1);
        exit(EXIT_FAILURE);
    }
    memcpy(copy, s, len + 1);
    return copy;
}

void strings_begin_document(void) {
    memset(&g_document, 0, sizeof(g_document));
    g_current_table = NULL;
    g_current_entry = NULL;
    g_error_count = 0;
}

void strings_finish_document(void) {
    /* no-op for now */
}

static void strings_free_entry(strings_entry *entry) {
    if (!entry)
        return;
    free(entry->id);
    free(entry->text);
}

void strings_document_free(strings_document *doc) {
    if (!doc)
        return;
    for (size_t i = 0; i < doc->table_count; ++i) {
        strings_table *table = &doc->tables[i];
        free(table->name);
        for (size_t j = 0; j < table->entry_count; ++j) {
            strings_free_entry(&table->entries[j]);
        }
        free(table->entries);
    }
    free(doc->tables);
    memset(doc, 0, sizeof(*doc));
    g_current_table = NULL;
    g_current_entry = NULL;
}

static strings_table *strings_document_add_table(strings_document *doc, const char *name) {
    for (size_t i = 0; i < doc->table_count; ++i) {
        if (strcmp(doc->tables[i].name, name) == 0) {
            strings_report_error("duplicate table '%s'", name);
            break;
        }
    }

    if (doc->table_count == doc->table_capacity) {
        size_t new_cap = doc->table_capacity ? doc->table_capacity * 2 : 4;
        doc->tables = (strings_table *)xrealloc(doc->tables, new_cap * sizeof(strings_table));
        doc->table_capacity = new_cap;
    }

    strings_table *table = &doc->tables[doc->table_count++];
    table->name = xstrdup(name);
    table->entries = NULL;
    table->entry_count = 0;
    table->entry_capacity = 0;
    table->next_auto_index = 0;
    return table;
}

static strings_entry *strings_table_add_entry(strings_table *table) {
    if (!table)
        return NULL;

    if (table->entry_count == table->entry_capacity) {
        size_t new_cap = table->entry_capacity ? table->entry_capacity * 2 : 8;
        table->entries = (strings_entry *)xrealloc(table->entries, new_cap * sizeof(strings_entry));
        table->entry_capacity = new_cap;
    }

    strings_entry *entry = &table->entries[table->entry_count++];
    entry->id = NULL;
    entry->index = -1;
    entry->text = NULL;
    entry->order = table->entry_count - 1;
    return entry;
}

void strings_begin_table(const char *name) {
    g_current_table = strings_document_add_table(&g_document, name);
}

void strings_end_table(void) {
    g_current_table = NULL;
}

void strings_begin_entry(void) {
    if (!g_current_table) {
        strings_report_error("entry declared outside of a table");
        return;
    }
    g_current_entry = strings_table_add_entry(g_current_table);
}

void strings_set_entry_field(const char *key, const parsed_value *value) {
    if (!g_current_entry) {
        strings_report_error("entry field declared before entry");
        return;
    }

    if (strcmp(key, "id") == 0) {
        if (value->kind != PVK_STRING) {
            strings_report_error("field 'id' must be a string");
            return;
        }
        if (g_current_entry->id) {
            strings_report_error("field 'id' specified more than once");
            return;
        }
        g_current_entry->id = xstrdup(value->string);
    } else if (strcmp(key, "text") == 0) {
        if (value->kind != PVK_STRING) {
            strings_report_error("field 'text' must be a string");
            return;
        }
        if (g_current_entry->text) {
            strings_report_error("field 'text' specified more than once");
            return;
        }
        g_current_entry->text = xstrdup(value->string);
    } else if (strcmp(key, "index") == 0) {
        if (value->kind != PVK_NUMBER) {
            strings_report_error("field 'index' must be numeric");
            return;
        }
        if (g_current_entry->index >= 0) {
            strings_report_error("field 'index' specified more than once");
            return;
        }
        g_current_entry->index = value->number;
    } else {
        strings_report_error("unknown field '%s'", key);
    }
}

void strings_finish_entry(void) {
    if (!g_current_table || !g_current_entry)
        return;

    strings_entry *entry = g_current_entry;

    if (!entry->text) {
        strings_report_error("entry in table '%s' missing 'text' field", g_current_table->name);
    }

    if (entry->index < 0) {
        entry->index = g_current_table->next_auto_index++;
    } else {
        if (entry->index >= g_current_table->next_auto_index)
            g_current_table->next_auto_index = entry->index + 1;
    }

    for (size_t i = 0; i + 1 < g_current_table->entry_count; ++i) {
        strings_entry *other = &g_current_table->entries[i];
        if (other == entry)
            continue;
        if (other->index == entry->index) {
            strings_report_error("duplicate index %ld in table '%s'", entry->index, g_current_table->name);
            break;
        }
        if (entry->id && other->id && strcmp(entry->id, other->id) == 0) {
            strings_report_error("duplicate id '%s' in table '%s'", entry->id, g_current_table->name);
            break;
        }
    }

    g_current_entry = NULL;
}

parsed_value parsed_value_string(char *text) {
    parsed_value value;
    value.kind = PVK_STRING;
    value.string = text;
    value.number = 0;
    return value;
}

parsed_value parsed_value_number(long number) {
    parsed_value value;
    value.kind = PVK_NUMBER;
    value.number = number;
    value.string = NULL;
    return value;
}

parsed_value parsed_value_ident(char *ident) {
    return parsed_value_string(ident);
}

void parsed_value_dispose(parsed_value *value) {
    if (!value)
        return;
    if (value->kind == PVK_STRING) {
        free(value->string);
        value->string = NULL;
    }
}

void strings_report_error(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    fprintf(stderr, "strings_compiler: ");
    vfprintf(stderr, fmt, args);
    fprintf(stderr, "\n");
    va_end(args);
    g_error_count++;
}

int strings_has_errors(void) {
    return g_error_count;
}

static void emit_c_string(FILE *out, const char *text) {
    fputc('"', out);
    for (const unsigned char *p = (const unsigned char *)text; *p; ++p) {
        unsigned char c = *p;
        switch (c) {
            case '\\': fputs("\\\\", out); break;
            case '\"': fputs("\\\"", out); break;
            case '\n': fputs("\\n", out); break;
            case '\r': fputs("\\r", out); break;
            case '\t': fputs("\\t", out); break;
            default:
                if (isprint(c))
                    fputc((int)c, out);
                else
                    fprintf(out, "\\x%02x", c);
                break;
        }
    }
    fputc('"', out);
}

static void sanitize_symbol(const char *name, char *buffer, size_t buffer_size) {
    size_t j = 0;
    for (size_t i = 0; name[i] && j + 1 < buffer_size; ++i) {
        char c = name[i];
        if (isalnum((unsigned char)c) || c == '_') {
            buffer[j++] = c;
        } else {
            buffer[j++] = '_';
        }
    }
    buffer[j] = '\0';
    if (j == 0 && buffer_size > 1) {
        buffer[0] = '_';
        buffer[1] = '\0';
    }
}

int strings_emit_c(const strings_document *doc, const char *path, const char *header_basename) {
    if (!path)
        return 0;

    FILE *out = fopen(path, "w");
    if (!out) {
        fprintf(stderr, "strings_compiler: failed to open '%s' for writing: %s\n", path, strerror(errno));
        return -1;
    }

    fprintf(out, "/* Auto-generated by strings_compiler; do not edit. */\n");
    fprintf(out, "#include <stddef.h>\n");
    fprintf(out, "#include <string.h>\n");
    if (header_basename)
        fprintf(out, "#include \"%s\"\n\n", header_basename);
    else
        fputc('\n', out);

    for (size_t i = 0; i < doc->table_count; ++i) {
        const strings_table *table = &doc->tables[i];
        char symbol[128];
        sanitize_symbol(table->name, symbol, sizeof(symbol));
        fprintf(out, "static const strings_entry_record %s_entries[] = {\n", symbol);
        for (size_t j = 0; j < table->entry_count; ++j) {
            const strings_entry *entry = &table->entries[j];
            fprintf(out, "    { %ld, ", entry->index);
            if (entry->id)
                emit_c_string(out, entry->id);
            else
                fputs("NULL", out);
            fputs(", ", out);
            if (entry->text)
                emit_c_string(out, entry->text);
            else
                fputs("NULL", out);
            fputs(" },\n", out);
        }
        fprintf(out, "};\n\n");
    }

    fprintf(out, "const strings_table_record strings_tables[] = {\n");
    for (size_t i = 0; i < doc->table_count; ++i) {
        const strings_table *table = &doc->tables[i];
        char symbol[128];
        sanitize_symbol(table->name, symbol, sizeof(symbol));
        fprintf(out, "    { ");
        emit_c_string(out, table->name);
        fprintf(out, ", %s_entries, sizeof(%s_entries) / sizeof(%s_entries[0]) },\n", symbol, symbol, symbol);
    }
    fprintf(out, "};\n\n");

    fprintf(out, "const size_t strings_tables_count = sizeof(strings_tables) / sizeof(strings_tables[0]);\n\n");

    fprintf(out, "const strings_table_record *strings_find_table(const char *name) {\n");
    fprintf(out, "    if (!name) return NULL;\n");
    fprintf(out, "    for (size_t i = 0; i < strings_tables_count; ++i) {\n");
    fprintf(out, "        if (strcmp(strings_tables[i].name, name) == 0)\n");
    fprintf(out, "            return &strings_tables[i];\n");
    fprintf(out, "    }\n    return NULL;\n}\n");

    if (fclose(out) != 0) {
        fprintf(stderr, "strings_compiler: failed to close '%s': %s\n", path, strerror(errno));
        return -1;
    }

    return 0;
}

int strings_emit_h(const strings_document *doc, const char *path) {
    if (!path)
        return 0;

    FILE *out = fopen(path, "w");
    if (!out) {
        fprintf(stderr, "strings_compiler: failed to open '%s' for writing: %s\n", path, strerror(errno));
        return -1;
    }

    fprintf(out, "/* Auto-generated by strings_compiler; do not edit. */\n");
    fprintf(out, "#ifndef GENERATED_STRINGS_TABLES_H\n");
    fprintf(out, "#define GENERATED_STRINGS_TABLES_H\n\n");
    fprintf(out, "#include <stddef.h>\n\n");
    fprintf(out, "typedef struct strings_entry_record {\n");
    fprintf(out, "    long index;\n    const char *id;\n    const char *text;\n} strings_entry_record;\n\n");
    fprintf(out, "typedef struct strings_table_record {\n");
    fprintf(out, "    const char *name;\n    const strings_entry_record *entries;\n    size_t count;\n} strings_table_record;\n\n");
    fprintf(out, "extern const strings_table_record strings_tables[];\n");
    fprintf(out, "extern const size_t strings_tables_count;\n\n");
    fprintf(out, "const strings_table_record *strings_find_table(const char *name);\n\n");
    fprintf(out, "#endif /* GENERATED_STRINGS_TABLES_H */\n");

    if (fclose(out) != 0) {
        fprintf(stderr, "strings_compiler: failed to close '%s': %s\n", path, strerror(errno));
        return -1;
    }

    (void)doc;
    return 0;
}

int strings_emit_manifest(const strings_document *doc, const char *path) {
    if (!path)
        return 0;

    FILE *out = fopen(path, "w");
    if (!out) {
        fprintf(stderr, "strings_compiler: failed to open '%s' for writing: %s\n", path, strerror(errno));
        return -1;
    }

    fprintf(out, "{\n  \"tables\": [\n");
    for (size_t i = 0; i < doc->table_count; ++i) {
        const strings_table *table = &doc->tables[i];
        fprintf(out, "    { \"name\": ");
        emit_c_string(out, table->name);
        fprintf(out, ", \"count\": %zu }", table->entry_count);
        if (i + 1 < doc->table_count)
            fputs(",", out);
        fputc('\n', out);
    }
    fprintf(out, "  ]\n}\n");

    if (fclose(out) != 0) {
        fprintf(stderr, "strings_compiler: failed to close '%s': %s\n", path, strerror(errno));
        return -1;
    }

    return 0;
}

char *strings_unescape_quoted(const char *input) {
    size_t len = strlen(input);
    if (len < 2 || input[0] != '"' || input[len - 1] != '"')
        return xstrdup("");

    char *out = (char *)malloc(len);
    if (!out) {
        fprintf(stderr, "strings_compiler: out of memory (requested %zu bytes)\n", len);
        exit(EXIT_FAILURE);
    }

    size_t oi = 0;
    for (size_t i = 1; i + 1 < len; ++i) {
        unsigned char c = (unsigned char)input[i];
        if (c == '\\' && i + 1 < len - 1) {
            unsigned char next = (unsigned char)input[++i];
            switch (next) {
                case 'n': out[oi++] = '\n'; break;
                case 'r': out[oi++] = '\r'; break;
                case 't': out[oi++] = '\t'; break;
                case '\\': out[oi++] = '\\'; break;
                case '"': out[oi++] = '"'; break;
                default:
                    out[oi++] = (char)next;
                    break;
            }
        } else {
            out[oi++] = (char)c;
        }
    }
    out[oi] = '\0';
    return out;
}
