#include "strings_compiler.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char *path_basename(const char *path) {
	if (!path)
		return NULL;
	const char *slash = strrchr(path, '/');
#ifdef _WIN32
	const char *backslash = strrchr(path, '\\');
	if (!slash || (backslash && backslash > slash))
		slash = backslash;
#endif
	return slash ? slash + 1 : path;
}

static void print_usage(const char *prog) {
	fprintf(stderr,
			"Usage: %s [--c-output <file>] [--h-output <file>] [--manifest <file>] [input.yaml]\n",
			prog);
}

int main(int argc, char **argv) {
	const char *input_path = NULL;
	const char *c_output = NULL;
	const char *h_output = NULL;
	const char *manifest = NULL;

	for (int i = 1; i < argc; ++i) {
		const char *arg = argv[i];
		if (strcmp(arg, "--c-output") == 0) {
			if (i + 1 >= argc) {
				fprintf(stderr, "strings_compiler: missing argument for --c-output\n");
				return EXIT_FAILURE;
			}
			c_output = argv[++i];
		} else if (strcmp(arg, "--h-output") == 0) {
			if (i + 1 >= argc) {
				fprintf(stderr, "strings_compiler: missing argument for --h-output\n");
				return EXIT_FAILURE;
			}
			h_output = argv[++i];
		} else if (strcmp(arg, "--manifest") == 0) {
			if (i + 1 >= argc) {
				fprintf(stderr, "strings_compiler: missing argument for --manifest\n");
				return EXIT_FAILURE;
			}
			manifest = argv[++i];
		} else if (strcmp(arg, "--help") == 0) {
			print_usage(argv[0]);
			return EXIT_SUCCESS;
		} else if (strcmp(arg, "--version") == 0) {
			printf("strings_compiler 0.1\n");
			return EXIT_SUCCESS;
		} else if (!input_path) {
			input_path = arg;
		} else {
			fprintf(stderr, "strings_compiler: unexpected argument '%s'\n", arg);
			return EXIT_FAILURE;
		}
	}

	if (!input_path)
		input_path = "-";

	FILE *input = NULL;
	if (strcmp(input_path, "-") == 0) {
		input = stdin;
	} else {
		input = fopen(input_path, "rb");
		if (!input) {
			fprintf(stderr, "strings_compiler: failed to open '%s'\n", input_path);
			return EXIT_FAILURE;
		}
	}

	strings_begin_document();

	int parse_result;
	if (input == stdin)
		parse_result = strings_load_yaml_stream(input, "stdin");
	else
		parse_result = strings_load_yaml_stream(input, input_path);

	if (input && input != stdin)
		fclose(input);

	if (parse_result != 0 || strings_has_errors()) {
		strings_document_free(&g_document);
		return EXIT_FAILURE;
	}

	strings_finish_document();

	const char *header_basename = h_output ? path_basename(h_output) : NULL;

	if (strings_emit_h(&g_document, h_output) != 0) {
		strings_document_free(&g_document);
		return EXIT_FAILURE;
	}

	if (strings_emit_c(&g_document, c_output, header_basename) != 0) {
		strings_document_free(&g_document);
		return EXIT_FAILURE;
	}

	if (strings_emit_manifest(&g_document, manifest) != 0) {
		strings_document_free(&g_document);
		return EXIT_FAILURE;
	}

	strings_document_free(&g_document);
	return EXIT_SUCCESS;
}
