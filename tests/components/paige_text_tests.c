/* 2025-11-20 Codex: Add style assertions for hello_macroman RTF output. */
#include "../framework/test_framework.h"
#include "../../portable/paige_text_extractor.h"
#include "../../Common/headers/memory.h"
#include "../../Common/headers/logging.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char *const primary_fixture = "tests/fixtures/wptext/hello_macroman.bin";
static const char *const fallback_fixture = "fixtures/wptext/hello_macroman.bin";
static const char *const primary_fixture_text = "tests/fixtures/wptext/hello_macroman.txt";
static const char *const fallback_fixture_text = "fixtures/wptext/hello_macroman.txt";
static const char *const primary_examples_fixture = "tests/fixtures/wptext/examples_testText.bin";
static const char *const fallback_examples_fixture = "fixtures/wptext/examples_testText.bin";
static const char *const primary_examples_text = "tests/fixtures/wptext/examples_testText.txt";
static const char *const fallback_examples_text = "fixtures/wptext/examples_testText.txt";
static const char *const primary_rtf_fixture = "tests/fixtures/wptext/hello_macroman.bin";
static const char *const fallback_rtf_fixture = "fixtures/wptext/hello_macroman.bin";

static bool load_fixture(const char *primary, const char *fallback, uint8_t **out_bytes, long *out_size) {
    if (out_bytes == NULL || out_size == NULL)
        return false;
    FILE *fixture = fopen(primary, "rb");
    if (fixture == NULL)
        fixture = fopen(fallback, "rb");
    if (fixture == NULL)
        return false;
    fseek(fixture, 0, SEEK_END);
    long size = ftell(fixture);
    rewind(fixture);
    if (size <= 0) {
        fclose(fixture);
        return false;
    }
    uint8_t *buffer = (uint8_t *)malloc(size);
    if (buffer == NULL) {
        fclose(fixture);
        return false;
    }
    size_t read = fread(buffer, 1, size, fixture);
    fclose(fixture);
    if (read != (size_t)size) {
        free(buffer);
        return false;
    }
    *out_bytes = buffer;
    *out_size = size;
    return true;
}

static bool load_text_fixture(const char *primary, const char *fallback, char **out_text, long *out_size) {
    if (out_text == NULL || out_size == NULL)
        return false;
    FILE *fixture = fopen(primary, "rb");
    if (fixture == NULL)
        fixture = fopen(fallback, "rb");
    if (fixture == NULL)
        return false;
    fseek(fixture, 0, SEEK_END);
    long size = ftell(fixture);
    rewind(fixture);
    if (size < 0) {
        fclose(fixture);
        return false;
    }
    char *buffer = (char *)malloc((size_t)size + 1);
    if (buffer == NULL) {
        fclose(fixture);
        return false;
    }
    size_t read = fread(buffer, 1, (size_t)size, fixture);
    fclose(fixture);
    if (read != (size_t)size) {
        free(buffer);
        return false;
    }
    buffer[size] = '\0';
    *out_text = buffer;
    *out_size = size;
    return true;
}

bool test_paige_text_extraction(void) {
    g_test_stats.current_test_name = "Paige Text Extraction";

    uint8_t *buffer = NULL;
    long size = 0;
    TEST_ASSERT(load_fixture(primary_fixture, fallback_fixture, &buffer, &size), "load hello_macroman fixture");
    char *expected_text = NULL;
    long expected_len = 0;
    TEST_ASSERT(load_text_fixture(primary_fixture_text, fallback_fixture_text, &expected_text, &expected_len),
                "load hello_macroman text fixture");

    char errbuf[128] = {0};
    paige_extract_stats stats = {0};
    Handle hutf8 = nil;
    TEST_ASSERT(paige_extract_text_and_styles(buffer, size, &hutf8, &stats, errbuf, sizeof(errbuf)),
                "extractor should succeed (%s)", errbuf[0] ? errbuf : "no error");

    free(buffer);

    long utf8_len = GetHandleSize(hutf8);
    TEST_ASSERT(utf8_len == expected_len, "output length should match fixture (got %ld vs %ld)", utf8_len, expected_len);
    TEST_ASSERT(memcmp(*hutf8, expected_text, (size_t)expected_len) == 0, "extracted text must match fixture bytes");
    TEST_ASSERT(stats.text_key_count >= 1, "should decode at least one text_key");
    TEST_ASSERT(stats.total_text_bytes > 0, "stats should count bytes");
    TEST_ASSERT(!stats.returned_macroman, "should have converted to UTF-8");

    free(expected_text);
    disposehandle(hutf8);

    TEST_PASS("Paige text extractor returned expected string");
}

bool test_paige_examples_text(void) {
    g_test_stats.current_test_name = "Paige Examples Text";

    uint8_t *buffer = NULL;
    long size = 0;
    TEST_ASSERT(load_fixture(primary_examples_fixture, fallback_examples_fixture, &buffer, &size),
                "load examples_testText fixture");
    char *expected_text = NULL;
    long expected_len = 0;
    TEST_ASSERT(load_text_fixture(primary_examples_text, fallback_examples_text, &expected_text, &expected_len),
                "load examples_testText text fixture");

    char errbuf[128] = {0};
    paige_extract_stats stats = {0};
    Handle hutf8 = nil;
    TEST_ASSERT(paige_extract_text_and_styles(buffer, size, &hutf8, &stats, errbuf, sizeof(errbuf)),
                "extractor should succeed for examples (%s)", errbuf[0] ? errbuf : "no error");

    free(buffer);

    long utf8_len = GetHandleSize(hutf8);
    TEST_ASSERT(utf8_len == expected_len, "examples text length mismatch (%ld vs %ld)", utf8_len, expected_len);
    TEST_ASSERT(memcmp(*hutf8, expected_text, (size_t)expected_len) == 0, "examples text should match fixture");
    TEST_ASSERT(stats.text_key_count >= 1, "examples should decode at least one text_key");

    free(expected_text);
    disposehandle(hutf8);
    TEST_PASS("Paige text extractor handled examples.testText");
}

bool test_paige_emit_rtf(void) {
    g_test_stats.current_test_name = "Paige RTF Emission";

    uint8_t *buffer = NULL;
    long size = 0;
    TEST_ASSERT(load_fixture(primary_rtf_fixture, fallback_rtf_fixture, &buffer, &size),
                "load hello_macroman fixture for RTF");

    Handle hrtf = nil;
    long char_count = 0;
    paige_extract_stats stats = {0};
    char errbuf[128] = {0};
    TEST_ASSERT(wptext_emit_rtf_from_paige_blob(buffer, size, &hrtf, &char_count, &stats, errbuf, sizeof(errbuf)),
                "emit RTF (%s)", errbuf[0] ? errbuf : "no error");
    free(buffer);

    long rtf_len = GetHandleSize(hrtf);
    TEST_ASSERT(rtf_len > 0, "RTF handle should contain data");
    const char *rtf_bytes = (const char *)*hrtf;
    TEST_ASSERT(strncmp(rtf_bytes, "{\\rtf1", 6) == 0, "RTF header must start with {\\rtf1");

    char *rtf_copy = (char *)malloc((size_t)rtf_len + 1);
    TEST_ASSERT(rtf_copy != NULL, "unable to allocate RTF copy buffer");
    memcpy(rtf_copy, rtf_bytes, (size_t)rtf_len);
    rtf_copy[rtf_len] = '\0';

    TEST_ASSERT(strstr(rtf_copy, "Chess") != NULL, "RTF output should contain document text");
    TEST_ASSERT(strstr(rtf_copy, "\\b armies\\b0") != NULL, "bold span for 'armies' must be present");
    TEST_ASSERT(strstr(rtf_copy, "\\i millions") != NULL, "italic span for 'millions' must be present");
    TEST_ASSERT(strstr(rtf_copy, "\\caps") == NULL, "legacy small caps escape should not appear");

    free(rtf_copy);
    TEST_ASSERT(char_count > 0, "character count should be reported");

    disposehandle(hrtf);
    TEST_PASS("Paige RTF emission succeeded");
}

static test_case_t test_cases[] = {
    {"Paige Plain Text", test_paige_text_extraction},
    {"Paige Examples Text", test_paige_examples_text},
    {"Paige RTF Emission", test_paige_emit_rtf},
};

int main(void) {
    printf("=== Paige Text Extractor Tests ===\n");
    log_init();
    test_init();
    bool all_passed = run_test_suite(test_cases, sizeof(test_cases) / sizeof(test_cases[0]));
    test_summary();
    return all_passed ? 0 : 1;
}
