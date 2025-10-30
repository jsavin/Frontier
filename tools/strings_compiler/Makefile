CC ?= cc
CFLAGS ?= -Wall -Wextra -std=c99 -g -DYAML_VERSION_MAJOR=0 -DYAML_VERSION_MINOR=2 -DYAML_VERSION_PATCH=5 -DYAML_VERSION_STRING=\"0.2.5\"

YAML_OBJS = \
    libyaml_api.o \
    libyaml_dumper.o \
    libyaml_emitter.o \
    libyaml_loader.o \
    libyaml_parser.o \
    libyaml_reader.o \
    libyaml_scanner.o \
    libyaml_writer.o

OBJS = strings_compiler_main.o strings_compiler_runtime.o strings_yaml_loader.o $(YAML_OBJS)

LIBYAML_DIR = ../../third_party/libyaml
INCLUDES = -I$(LIBYAML_DIR)/include

all: strings_compiler

strings_compiler: $(OBJS)
	$(CC) $(CFLAGS) -o $@ $(OBJS)

strings_compiler_runtime.o: strings_compiler_runtime.c strings_compiler.h
	$(CC) $(CFLAGS) $(INCLUDES) -c strings_compiler_runtime.c

strings_compiler_main.o: strings_compiler_main.c strings_compiler.h
	$(CC) $(CFLAGS) $(INCLUDES) -c strings_compiler_main.c

strings_yaml_loader.o: strings_yaml_loader.c strings_compiler.h
	$(CC) $(CFLAGS) $(INCLUDES) -c strings_yaml_loader.c

libyaml_api.o: $(LIBYAML_DIR)/src/api.c
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

libyaml_dumper.o: $(LIBYAML_DIR)/src/dumper.c
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

libyaml_emitter.o: $(LIBYAML_DIR)/src/emitter.c
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

libyaml_loader.o: $(LIBYAML_DIR)/src/loader.c
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

libyaml_parser.o: $(LIBYAML_DIR)/src/parser.c
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

libyaml_reader.o: $(LIBYAML_DIR)/src/reader.c
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

libyaml_scanner.o: $(LIBYAML_DIR)/src/scanner.c
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

libyaml_writer.o: $(LIBYAML_DIR)/src/writer.c
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

clean:
	rm -f strings_compiler *.o $(YAML_OBJS)

.PHONY: all clean
