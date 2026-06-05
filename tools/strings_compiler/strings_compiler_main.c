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
			"Usage: %s [--c-output <file>] [--h-output <file>] [--manifest <file>] [input.yaml ...]\n",
			prog);
}

int main(int argc, char **argv) {
	/*
	 * Positional inputs are collected into a small heap-allocated array
	 * and processed in argv order. strings_compiler_runtime accumulates
	 * tables across calls within a single strings_begin_document /
	 * strings_finish_document bracket (issue #681). Per-file invocations
	 * preserve source filenames for parse-error reporting -- the prior
	 * single-arg-or-stdin design forced callers to pipe `cat *.yaml` and
	 * lost filenames in the merged stream.
	 */
	const char **inputs = NULL;
	int input_count = 0;
	int input_cap = 0;
	const char *c_output = NULL;
	const char *h_output = NULL;
	const char *manifest = NULL;

	for (int i = 1; i < argc; ++i) {
		const char *arg = argv[i];
		if (strcmp(arg, "--c-output") == 0) {
			if (i + 1 >= argc) {
				fprintf(stderr, "strings_compiler: missing argument for --c-output\n");
				free(inputs);
				return EXIT_FAILURE;
			}
			c_output = argv[++i];
		} else if (strcmp(arg, "--h-output") == 0) {
			if (i + 1 >= argc) {
				fprintf(stderr, "strings_compiler: missing argument for --h-output\n");
				free(inputs);
				return EXIT_FAILURE;
			}
			h_output = argv[++i];
		} else if (strcmp(arg, "--manifest") == 0) {
			if (i + 1 >= argc) {
				fprintf(stderr, "strings_compiler: missing argument for --manifest\n");
				free(inputs);
				return EXIT_FAILURE;
			}
			manifest = argv[++i];
		} else if (strcmp(arg, "--help") == 0) {
			print_usage(argv[0]);
			free(inputs);
			return EXIT_SUCCESS;
		} else if (strcmp(arg, "--version") == 0) {
			printf("strings_compiler 0.1\n");
			free(inputs);
			return EXIT_SUCCESS;
		} else {
			/* Treat any non-option arg as a positional input file. */
			if (input_count == input_cap) {
				int new_cap = input_cap ? input_cap * 2 : 4;
				const char **resized = realloc(inputs, (size_t)new_cap * sizeof(*inputs));
				if (!resized) {
					fprintf(stderr, "strings_compiler: out of memory collecting inputs\n");
					free(inputs);
					return EXIT_FAILURE;
				}
				inputs = resized;
				input_cap = new_cap;
			}
			inputs[input_count++] = arg;
		}
	}

	/* No inputs => read stdin once, same backwards-compatible behavior
	 * as before. Callers can still pipe via `cat foo.yaml bar.yaml |
	 * strings_compiler`, though they lose source filenames in errors.
	 * Invariant: input_cap grows only when input_count grows, so
	 * input_count == 0 implies input_cap == 0 and inputs == NULL. */
	if (input_count == 0) {
		inputs = malloc(sizeof(*inputs));
		if (!inputs) {
			fprintf(stderr, "strings_compiler: out of memory\n");
			return EXIT_FAILURE;
		}
		inputs[0] = "-";
		input_count = 1;
	}

	strings_begin_document();

	int saw_error = 0;
	for (int i = 0; i < input_count; ++i) {
		const char *input_path = inputs[i];
		FILE *input = NULL;
		if (strcmp(input_path, "-") == 0) {
			input = stdin;
		} else {
			input = fopen(input_path, "rb");
			if (!input) {
				fprintf(stderr, "strings_compiler: failed to open '%s'\n", input_path);
				saw_error = 1;
				break;
			}
		}

		int parse_result;
		if (input == stdin)
			parse_result = strings_load_yaml_stream(input, "stdin");
		else
			parse_result = strings_load_yaml_stream(input, input_path);

		if (input != stdin)
			fclose(input);

		if (parse_result != 0 || strings_has_errors()) {
			saw_error = 1;
			break;
		}
	}

	free(inputs);

	if (saw_error) {
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
