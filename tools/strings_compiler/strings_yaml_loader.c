#include "strings_compiler.h"

#include <yaml.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>

static char *dup_yaml_string(const yaml_node_t *node) {
    if (!node || node->type != YAML_SCALAR_NODE)
        return NULL;
    size_t len = node->data.scalar.length;
    char *copy = (char *)malloc(len + 1);
    if (!copy)
        return NULL;
    memcpy(copy, node->data.scalar.value, len);
    copy[len] = '\0';
    return copy;
}

static long parse_long(const yaml_node_t *node, int *ok) {
    if (!node || node->type != YAML_SCALAR_NODE) {
        if (ok)
            *ok = 0;
        return 0;
    }
    const char *text = (const char *)node->data.scalar.value;
    char *endptr = NULL;
    long value = strtol(text, &endptr, 10);
    if (ok)
        *ok = (endptr && *endptr == '\0');
    return value;
}

static int node_is_sequence(const yaml_node_t *node) {
    return node && node->type == YAML_SEQUENCE_NODE;
}

static int node_is_mapping(const yaml_node_t *node) {
    return node && node->type == YAML_MAPPING_NODE;
}

static const yaml_node_t *document_get_node(yaml_document_t *doc, int index) {
    if (index <= 0)
        return NULL;
    return yaml_document_get_node(doc, index);
}

int strings_load_yaml_stream(FILE *stream, const char *source_name) {
    yaml_parser_t parser;
    yaml_document_t document;

    if (!yaml_parser_initialize(&parser)) {
        strings_report_error("failed to initialise libyaml parser");
        return -1;
    }

    yaml_parser_set_input_file(&parser, stream);

    if (!yaml_parser_load(&parser, &document)) {
        strings_report_error("libyaml parser error: %s", parser.problem ? parser.problem : "unknown");
        yaml_parser_delete(&parser);
        return -1;
    }

    yaml_parser_delete(&parser);

    yaml_node_t *root = yaml_document_get_root_node(&document);
    if (!root) {
        yaml_document_delete(&document);
        strings_report_error("%s: empty YAML document", source_name ? source_name : "input");
        return -1;
    }

    if (!node_is_mapping(root)) {
        yaml_document_delete(&document);
        strings_report_error("%s: expected top-level mapping", source_name ? source_name : "input");
        return -1;
    }

    for (yaml_node_pair_t *pair = root->data.mapping.pairs.start;
         pair < root->data.mapping.pairs.top; ++pair) {
        const yaml_node_t *key_node = document_get_node(&document, pair->key);
        const yaml_node_t *value_node = document_get_node(&document, pair->value);

        if (!key_node || key_node->type != YAML_SCALAR_NODE) {
            strings_report_error("%s: table name must be a scalar", source_name ? source_name : "input");
            continue;
        }

        char *table_name = dup_yaml_string(key_node);
        if (!table_name) {
            strings_report_error("%s: out of memory duplicating table name", source_name ? source_name : "input");
            continue;
        }

        strings_begin_table(table_name);
        free(table_name);

        if (!node_is_sequence(value_node)) {
            strings_report_error("%s: table '%s' must be a sequence", source_name ? source_name : "input", key_node->data.scalar.value);
            strings_end_table();
            continue;
        }

        size_t entry_index = 0;
        for (yaml_node_item_t *item = value_node->data.sequence.items.start;
             item < value_node->data.sequence.items.top; ++item, ++entry_index) {
            const yaml_node_t *entry_node = document_get_node(&document, *item);
            if (!node_is_mapping(entry_node)) {
                strings_report_error("%s: entry %zu in table '%s' must be a mapping",
                                     source_name ? source_name : "input",
                                     entry_index,
                                     key_node->data.scalar.value);
                continue;
            }

            strings_begin_entry();

            for (yaml_node_pair_t *entry_pair = entry_node->data.mapping.pairs.start;
                 entry_pair < entry_node->data.mapping.pairs.top; ++entry_pair) {
                const yaml_node_t *entry_key = document_get_node(&document, entry_pair->key);
                const yaml_node_t *entry_value = document_get_node(&document, entry_pair->value);
                if (!entry_key || entry_key->type != YAML_SCALAR_NODE) {
                    strings_report_error("%s: entry key must be a scalar", source_name ? source_name : "input");
                    continue;
                }

                char *field_name = dup_yaml_string(entry_key);
                if (!field_name) {
                    strings_report_error("%s: out of memory duplicating field name", source_name ? source_name : "input");
                    continue;
                }

                parsed_value pv;
                if (entry_value && entry_value->type == YAML_SCALAR_NODE) {
                    if (strcmp(field_name, "index") == 0) {
                        int ok_number = 0;
                        long number = parse_long(entry_value, &ok_number);
                        if (!ok_number) {
                            strings_report_error("%s: field 'index' must be numeric", source_name ? source_name : "input");
                            free(field_name);
                            continue;
                        }
                        pv = parsed_value_number(number);
                    } else {
                        char *value_copy = dup_yaml_string(entry_value);
                        if (!value_copy) {
                            free(field_name);
                            strings_report_error("%s: out of memory duplicating field value", source_name ? source_name : "input");
                            continue;
                        }
                        pv = parsed_value_string(value_copy);
                    }
                } else {
                    strings_report_error("%s: unsupported YAML node type in field '%s'", source_name ? source_name : "input", field_name);
                    free(field_name);
                    continue;
                }

                strings_set_entry_field(field_name, &pv);
                parsed_value_dispose(&pv);
                free(field_name);
            }

            strings_finish_entry();
        }

        strings_end_table();
    }

    yaml_document_delete(&document);
    return 0;
}

int strings_load_yaml_file(const char *path) {
    FILE *file = fopen(path, "rb");
    if (!file) {
        strings_report_error("failed to open '%s': %s", path, strerror(errno));
        return -1;
    }

    int result = strings_load_yaml_stream(file, path);
    fclose(file);
    return result;
}
