#ifndef STRINGS_COMPILER_H
#define STRINGS_COMPILER_H

#include <stddef.h>
#include <stdio.h>

typedef struct strings_entry {
	char *id;
	long index;
	char *text;
	size_t order;
} strings_entry;

typedef struct strings_table {
	char *name;
	strings_entry *entries;
	size_t entry_count;
	size_t entry_capacity;
	long next_auto_index;
} strings_table;

typedef struct strings_document {
	strings_table *tables;
	size_t table_count;
	size_t table_capacity;
} strings_document;

typedef enum parsed_value_kind {
	PVK_STRING,
	PVK_NUMBER
} parsed_value_kind;

typedef struct parsed_value {
	parsed_value_kind kind;
	char *string;
	long number;
} parsed_value;

extern strings_document g_document;

void strings_begin_document(void);
void strings_finish_document(void);
void strings_document_free(strings_document *doc);

void strings_begin_table(const char *name);
void strings_end_table(void);

void strings_begin_entry(void);
void strings_set_entry_field(const char *key, const parsed_value *value);
void strings_finish_entry(void);

parsed_value parsed_value_string(char *text);
parsed_value parsed_value_number(long number);
parsed_value parsed_value_ident(char *ident);
void parsed_value_dispose(parsed_value *value);

void strings_report_error(const char *fmt, ...);
int strings_has_errors(void);

int strings_emit_c(const strings_document *doc, const char *path, const char *header_basename);
int strings_emit_h(const strings_document *doc, const char *path);
int strings_emit_manifest(const strings_document *doc, const char *path);

char *strings_unescape_quoted(const char *input);

int strings_load_yaml_stream(FILE *stream, const char *source_name);
int strings_load_yaml_file(const char *path);

#endif /* STRINGS_COMPILER_H */
