/* 2025-11-20 Codex: Grow Paige extractor handles via runtime APIs so Carbon handles stay untouched. */
/* 2025-11-20 Codex: Fix style_info parsing by skipping all reserved future longs before styles. */
#include "paige_text_extractor.h"

#include "memory.h"
#include "strings.h"

#include <limits.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define CODE_MASK 0x0F
#define REPEAT_LAST_VALUE 0x80
#define REPEAT_LAST_N_TIMES 0x40
#define PAIGE_KEY_HEADER_SIZE (4 + 8 + 8)

#define PAIGE_MAX_STYLES 32
#define PAIGE_FONT_NAME_MAX 64

/* File format revision constants (see third_party/Paige/PGHEADER/PGFILES.H). */
#define PAIGE_KEY_REVISION1  0x00000008L
#define PAIGE_KEY_REVISION3  0x00000100L
#define PAIGE_KEY_REVISION4  0x00000101L
#define PAIGE_KEY_REVISION5  0x00010000L
#define PAIGE_KEY_REVISION6  0x0001000BL
#define PAIGE_KEY_REVISION7  0x0001000CL
#define PAIGE_KEY_REVISION8  0x0001000EL
#define PAIGE_KEY_REVISION8A 0x0001000FL
#define PAIGE_KEY_REVISION9  0x00010010L
#define PAIGE_KEY_REVISION10 0x00010011L
#define PAIGE_KEY_REVISION12 0x00010013L
#define PAIGE_KEY_REVISION18 0x00020003L
#define PAIGE_KEY_REVISION22 0x00020012L
#define PAIGE_KEY_REVISION23 0x00020013L

static const uint16_t kMacRomanToUnicode[256] = {
0x0000,0x0001,0x0002,0x0003,0x0004,0x0005,0x0006,0x0007,0x0008,0x0009,0x000A,0x000B,0x000C,0x000D,0x000E,0x000F,
0x0010,0x0011,0x0012,0x0013,0x0014,0x0015,0x0016,0x0017,0x0018,0x0019,0x001A,0x001B,0x001C,0x001D,0x001E,0x001F,
0x0020,0x0021,0x0022,0x0023,0x0024,0x0025,0x0026,0x0027,0x0028,0x0029,0x002A,0x002B,0x002C,0x002D,0x002E,0x002F,
0x0030,0x0031,0x0032,0x0033,0x0034,0x0035,0x0036,0x0037,0x0038,0x0039,0x003A,0x003B,0x003C,0x003D,0x003E,0x003F,
0x0040,0x0041,0x0042,0x0043,0x0044,0x0045,0x0046,0x0047,0x0048,0x0049,0x004A,0x004B,0x004C,0x004D,0x004E,0x004F,
0x0050,0x0051,0x0052,0x0053,0x0054,0x0055,0x0056,0x0057,0x0058,0x0059,0x005A,0x005B,0x005C,0x005D,0x005E,0x005F,
0x0060,0x0061,0x0062,0x0063,0x0064,0x0065,0x0066,0x0067,0x0068,0x0069,0x006A,0x006B,0x006C,0x006D,0x006E,0x006F,
0x0070,0x0071,0x0072,0x0073,0x0074,0x0075,0x0076,0x0077,0x0078,0x0079,0x007A,0x007B,0x007C,0x007D,0x007E,0x007F,
0x00C4,0x00C5,0x00C7,0x00C9,0x00D1,0x00D6,0x00DC,0x00E1,0x00E0,0x00E2,0x00E4,0x00E3,0x00E5,0x00E7,0x00E9,0x00E8,
0x00EA,0x00EB,0x00ED,0x00EC,0x00EE,0x00EF,0x00F1,0x00F3,0x00F2,0x00F4,0x00F6,0x00F5,0x00FA,0x00F9,0x00FB,0x00FC,
0x2020,0x00B0,0x00A2,0x00A3,0x00A7,0x2022,0x00B6,0x00DF,0x00AE,0x00A9,0x2122,0x00B4,0x00A8,0x2260,0x00C6,0x00D8,
0x221E,0x00B1,0x2264,0x2265,0x00A5,0x00B5,0x2202,0x2211,0x220F,0x03C0,0x222B,0x00AA,0x00BA,0x03A9,0x00E6,0x00F8,
0x00BF,0x00A1,0x00AC,0x221A,0x0192,0x2248,0x2206,0x00AB,0x00BB,0x2026,0x00A0,0x00C0,0x00C3,0x00D5,0x0152,0x0153,
0x2013,0x2014,0x201C,0x201D,0x2018,0x2019,0x00F7,0x25CA,0x00FF,0x0178,0x2044,0x20AC,0x2039,0x203A,0xFB01,0xFB02,
0x2021,0x00B7,0x201A,0x201E,0x2030,0x00C2,0x00CA,0x00C1,0x00CB,0x00C8,0x00CD,0x00CE,0x00CF,0x00CC,0x00D3,0x00D4,
0xF8FF,0x00D2,0x00DA,0x00DB,0x00D9,0x0131,0x02C6,0x02DC,0x00AF,0x02D8,0x02D9,0x02DA,0x00B8,0x02DD,0x02DB,0x02C7
};

enum {
    paige_byte_data = 0,
    paige_short_data = 1,
    paige_long_data = 2,
    paige_terminator_data = 3
};

enum {
    paige_eof_key = -2,
    paige_signature_key = -1,
    paige_key = 0,
    paige_text_block_key,
    paige_text_key,
    paige_line_key,
    style_run_key,
    par_run_key,
    style_info_key,
    par_info_key,
    font_info_key
};

enum {
    paige_style_bold = 0,
    paige_style_italic,
    paige_style_underline,
    paige_style_outline,
    paige_style_shadow,
    paige_style_condense,
    paige_style_extend,
    paige_style_dbl_underline,
    paige_style_word_underline,
    paige_style_dotted_underline,
    paige_style_hidden_text,
    paige_style_strikeout,
    paige_style_superscript,
    paige_style_subscript,
    paige_style_rotation,
    paige_style_all_caps,
    paige_style_all_lower,
    paige_style_small_caps,
    paige_style_overline,
    paige_style_boxed,
    paige_style_relative_point,
    paige_style_super_impose,
    paige_style_revision,
    paige_style_nested_subset,
    paige_style_blink,
    paige_style_index,
    paige_style_toc,
    paige_style_dsi_custom = 27,
    paige_style_custom = 28
};

/* Style bit flags (mirroring Paige headers). */
#define X_BOLD_BIT             0x00000001L
#define X_ITALIC_BIT           0x00000002L
#define X_UNDERLINE_BIT        0x00000004L
#define X_OUTLINE_BIT          0x00000008L
#define X_SHADOW_BIT           0x00000010L
#define X_CONDENSE_BIT         0x00000020L
#define X_EXTEND_BIT           0x00000040L
#define X_DBL_UNDERLINE_BIT    0x00000080L
#define X_WORD_UNDERLINE_BIT   0x00000100L
#define X_DOTTED_UNDERLINE_BIT 0x00000200L
#define X_HIDDEN_TEXT_BIT      0x00000400L
#define X_STRIKEOUT_BIT        0x00000800L
#define X_SUPERSCRIPT_BIT      0x00001000L
#define X_SUBSCRIPT_BIT        0x00002000L
#define X_ROTATION_BIT         0x00004000L
#define X_ALL_CAPS_BIT         0x00008000L
#define X_ALL_LOWER_BIT        0x00010000L
#define X_SMALL_CAPS_BIT       0x00020000L
#define X_OVERLINE_BIT         0x00040000L
#define X_BOXED_BIT            0x00080000L
#define X_RELATIVE_POINT_BIT   0x00100000L
#define X_SUPERIMPOSE_BIT      0x00200000L
#define X_INDEX_BIT            0x02000000L
#define X_TOC_BIT              0x04000000L

enum {
    paige_justify_left = 0,
    paige_justify_center = 1,
    paige_justify_right = 2,
    paige_justify_full = 3
};

typedef struct {
	short last_code;
	short repeat_ctr;
	long last_value;
	const uint8_t *data;
	long remaining_ctr;
	long transfered;
	long max_bytes;
	long max_bytes_ctr;
} paige_pack_stream;

typedef struct {
	const uint8_t *cursor;
	long remaining;
} paige_blob_view;

typedef struct {
    long version;
    long flags2;
} paige_file_context;

typedef struct {
    short font_index;
    long point_fixed;
    uint32_t class_bits;
    short styles[PAIGE_MAX_STYLES];
} paige_style_record;

typedef struct {
    paige_style_record *records;
    size_t count;
    size_t capacity;
} paige_style_table;

typedef struct {
    size_t offset;
    short style_index;
} paige_style_run_entry;

typedef struct {
    paige_style_run_entry *runs;
    size_t count;
    size_t capacity;
} paige_style_run_table;

typedef struct {
    short justification;
} paige_par_record;

typedef struct {
    paige_par_record *records;
    size_t count;
    size_t capacity;
} paige_par_table;

typedef struct {
    size_t offset;
    short par_index;
} paige_par_run_entry;

typedef struct {
    paige_par_run_entry *runs;
    size_t count;
    size_t capacity;
} paige_par_run_table;

typedef struct {
    short font_id;
    uint8_t name_len;
    uint8_t name[PAIGE_FONT_NAME_MAX];
} paige_font_record;

typedef struct {
    paige_font_record *records;
    size_t count;
    size_t capacity;
} paige_font_table;

typedef struct {
    Handle mac_text;
    paige_style_table styles;
    paige_style_run_table style_runs;
    paige_par_table paragraphs;
    paige_par_run_table par_runs;
    paige_font_table fonts;
    paige_file_context file_ctx;
} paige_document;

static void paige_document_init(paige_document *doc);
static void paige_document_dispose(paige_document *doc);
static boolean paige_next_key(paige_blob_view *view, short *out_key, long *out_size,
        long *out_element, const uint8_t **out_payload);
static boolean paige_collect_text(const uint8_t *payload, long size, Handle hmac, paige_extract_stats *stats,
        char *errbuf, size_t errlen);
static boolean paige_append_chunk(Handle h, const uint8_t *chunk, long len);

static void paige_style_table_dispose(paige_style_table *table) {
    if (table == NULL)
        return;
    free(table->records);
    table->records = NULL;
    table->count = 0;
    table->capacity = 0;
}

static boolean paige_style_table_expand(paige_style_table *table, size_t needed) {
    if (table->count + needed <= table->capacity)
        return true;
    size_t new_cap = table->capacity ? table->capacity * 2 : 8;
    while (new_cap < table->count + needed)
        new_cap *= 2;
    paige_style_record *grown = realloc(table->records, new_cap * sizeof(paige_style_record));
    if (grown == NULL)
        return false;
    table->records = grown;
    table->capacity = new_cap;
    return true;
}

static void paige_style_run_table_dispose(paige_style_run_table *table) {
    if (table == NULL)
        return;
    free(table->runs);
    table->runs = NULL;
    table->count = 0;
    table->capacity = 0;
}

static boolean paige_style_run_table_expand(paige_style_run_table *table, size_t needed) {
    if (table->count + needed <= table->capacity)
        return true;
    size_t new_cap = table->capacity ? table->capacity * 2 : 16;
    while (new_cap < table->count + needed)
        new_cap *= 2;
    paige_style_run_entry *grown = realloc(table->runs, new_cap * sizeof(paige_style_run_entry));
    if (grown == NULL)
        return false;
    table->runs = grown;
    table->capacity = new_cap;
    return true;
}

static void paige_par_table_dispose(paige_par_table *table) {
    if (table == NULL)
        return;
    free(table->records);
    table->records = NULL;
    table->count = 0;
    table->capacity = 0;
}

static boolean paige_par_table_expand(paige_par_table *table, size_t needed) {
    if (table->count + needed <= table->capacity)
        return true;
    size_t new_cap = table->capacity ? table->capacity * 2 : 4;
    while (new_cap < table->count + needed)
        new_cap *= 2;
    paige_par_record *grown = realloc(table->records, new_cap * sizeof(paige_par_record));
    if (grown == NULL)
        return false;
    table->records = grown;
    table->capacity = new_cap;
    return true;
}

static void paige_par_run_table_dispose(paige_par_run_table *table) {
    if (table == NULL)
        return;
    free(table->runs);
    table->runs = NULL;
    table->count = 0;
    table->capacity = 0;
}

static boolean paige_par_run_table_expand(paige_par_run_table *table, size_t needed) {
    if (table->count + needed <= table->capacity)
        return true;
    size_t new_cap = table->capacity ? table->capacity * 2 : 8;
    while (new_cap < table->count + needed)
        new_cap *= 2;
    paige_par_run_entry *grown = realloc(table->runs, new_cap * sizeof(paige_par_run_entry));
    if (grown == NULL)
        return false;
    table->runs = grown;
    table->capacity = new_cap;
    return true;
}

static void paige_font_table_dispose(paige_font_table *table) {
    if (table == NULL)
        return;
    free(table->records);
    table->records = NULL;
    table->count = 0;
    table->capacity = 0;
}

static boolean paige_font_table_expand(paige_font_table *table, size_t needed) {
    if (table->count + needed <= table->capacity)
        return true;
    size_t new_cap = table->capacity ? table->capacity * 2 : 4;
    while (new_cap < table->count + needed)
        new_cap *= 2;
    paige_font_record *grown = realloc(table->records, new_cap * sizeof(paige_font_record));
    if (grown == NULL)
        return false;
    table->records = grown;
    table->capacity = new_cap;
    return true;
}

static short __attribute__((unused)) paige_fixed_to_points(long value) {
    /* value is 16.16 fixed; round to nearest integer point. */
    long rounded = (value + 0x8000) >> 16;
    if (rounded > SHRT_MAX)
        return SHRT_MAX;
    if (rounded < SHRT_MIN)
        return SHRT_MIN;
    return (short)rounded;
}

static void paige_pack_setup(paige_pack_stream *stream, const uint8_t *data, long size) {
	if (stream == NULL)
		return;
	stream->last_code = 0;
	stream->repeat_ctr = 0;
	stream->last_value = 0;
	stream->data = data;
	stream->remaining_ctr = size;
	stream->transfered = 0;
	stream->max_bytes = 0;
	stream->max_bytes_ctr = 0;
}

static long paige_parse_hex(const uint8_t *data, long remaining, long *out_value, long *consumed) {
	if (data == NULL || remaining <= 0 || out_value == NULL || consumed == NULL)
		return -1;

	long idx = 0;
	long result = 0;
	boolean negative = false;

	if (remaining > 0 && data[idx] == '-') {
		negative = true;
		++idx;
		--remaining;
	}

	long digits = 0;
	while (remaining > 0) {
		uint8_t ch = data[idx];
		int nibble;
		if (ch >= '0' && ch <= '9')
			nibble = ch - '0';
		else if (ch >= 'A' && ch <= 'F')
			nibble = ch - 'A' + 10;
		else if (ch >= 'a' && ch <= 'f')
			nibble = ch - 'a' + 10;
		else
			break;

		result = (result << 4) | (uint32_t)nibble;
		++idx;
		--remaining;
		++digits;
	}

	if (digits == 0)
		return -1;

	if (negative)
		result = -result;

	*out_value = result;
	*consumed = idx;
	return digits;
}

static long paige_unpack_hex_value(const uint8_t *data, long remaining, long *out_value) {
	long consumed = 0;
	long digits = paige_parse_hex(data, remaining, out_value, &consumed);
	if (digits < 0)
		return -1;
	return consumed;
}

static int paige_hex_digit(uint8_t ch) {
	if (ch >= '0' && ch <= '9')
		return ch - '0';
	if (ch >= 'A' && ch <= 'F')
		return ch - 'A' + 10;
	if (ch >= 'a' && ch <= 'f')
		return ch - 'a' + 10;
	return -1;
}

static boolean paige_read_hex(paige_blob_view *view, int digits, long *out_value) {
	if (view == NULL || view->remaining < digits || out_value == NULL)
		return false;

	long accum = 0;
	for (int i = 0; i < digits; ++i) {
		int nibble = paige_hex_digit(view->cursor[i]);
		if (nibble < 0)
			return false;
		accum = (accum << 4) | (uint32_t)nibble;
	}

	view->cursor += digits;
	view->remaining -= digits;
	*out_value = accum;
	return true;
}

static void paige_log_error(char *errbuf, size_t len, const char *fmt, ...) {
	if (errbuf == NULL || len == 0 || fmt == NULL)
		return;

	va_list ap;
	va_start(ap, fmt);
	vsnprintf(errbuf, len, fmt, ap);
	va_end(ap);
}

static long paige_unpack_num(paige_pack_stream *stream) {
	if (stream == NULL || stream->remaining_ctr <= 0)
		return 0;

	const uint8_t *data = stream->data;
	long input_ctr = 0;
	long result = 0;
	uint8_t real_code = *data;
	uint8_t data_code = real_code & CODE_MASK;

        if (stream->repeat_ctr) {
            --stream->repeat_ctr;
            if (stream->max_bytes)
                stream->max_bytes_ctr += (stream->last_code == paige_short_data) ? sizeof(short) : sizeof(long);
            return stream->last_value;
        }

        if (data_code != paige_short_data && data_code != paige_long_data) {
		return 0;
	}

	++data;
	++input_ctr;

	if (real_code & REPEAT_LAST_VALUE) {
		result = stream->last_value;
		if (real_code & REPEAT_LAST_N_TIMES) {
			if (stream->remaining_ctr - input_ctr <= 0)
				return 0;
			stream->repeat_ctr = (*data - 1);
			++input_ctr;
			++data;
		}
	} else {
		long consumed = paige_unpack_hex_value(data, stream->remaining_ctr - input_ctr, &result);
		if (consumed < 0)
			return 0;
		data += consumed;
		input_ctr += consumed;
	}

	stream->last_code = data_code;
	stream->last_value = result;
	stream->data = data;
	stream->transfered += input_ctr;
	stream->remaining_ctr -= input_ctr;

	return result;
}

static boolean paige_unpack_bytes(paige_pack_stream *stream, long *out_len, const uint8_t **out_ptr) {
	if (stream == NULL || stream->remaining_ctr <= 0 || out_len == NULL)
		return false;

	const uint8_t *data = stream->data;
	long input_ctr = 0;

        if (*data != paige_byte_data)
            return false;

	++data;
	++input_ctr;

	long length = 0;
	long hex_consumed = paige_unpack_hex_value(data, stream->remaining_ctr - input_ctr, &length);
	if (hex_consumed < 0)
		return false;

	data += hex_consumed;
	input_ctr += hex_consumed;

	if (stream->remaining_ctr - input_ctr <= 0)
		return false;

	++data;
	++input_ctr;

	if (stream->remaining_ctr - input_ctr < length)
		return false;

	if (out_ptr)
		*out_ptr = data;

	data += length;
	input_ctr += length;

	stream->data = data;
	stream->transfered += input_ctr;
	stream->remaining_ctr -= input_ctr;

	*out_len = length;
	return true;
}

static boolean paige_skip_ptr_bytes(paige_pack_stream *stream) {
    long chunk_len = 0;
    return paige_unpack_bytes(stream, &chunk_len, NULL);
}

static boolean paige_skip_refcon(paige_pack_stream *stream, long version) {
    if (version == PAIGE_KEY_REVISION6)
        return paige_skip_ptr_bytes(stream);
    paige_unpack_num(stream);
    return true;
}

static boolean paige_parse_file_header(const uint8_t *payload, long size, paige_file_context *ctx,
        char *errbuf, size_t errlen) {
    if (payload == NULL || ctx == NULL)
        return false;

    paige_pack_stream stream;
    paige_pack_setup(&stream, payload, size);

    long version = paige_unpack_num(&stream);
    if (version <= 0) {
        paige_log_error(errbuf, errlen, "Invalid Paige version");
        return false;
    }

    ctx->version = version;

    /* Skip platform/flags we don't currently need. */
    paige_unpack_num(&stream); /* platform */
    paige_unpack_num(&stream); /* flags */

    long flags2 = 0;
    if (version >= PAIGE_KEY_REVISION6)
        flags2 = paige_unpack_num(&stream);
    ctx->flags2 = flags2;
    return true;
}

static void paige_style_record_init(paige_style_record *rec) {
    if (rec == NULL)
        return;
    memset(rec, 0, sizeof(*rec));
    rec->font_index = 0;
    rec->point_fixed = 0;
}

static boolean paige_parse_single_style(paige_pack_stream *stream, long version,
        paige_style_record *record) {
    if (stream == NULL || record == NULL)
        return false;

    paige_style_record_init(record);
    record->font_index = (short)paige_unpack_num(stream);

    /* Fields we skip but must consume to keep the pack stream aligned. */
    paige_unpack_num(stream); /* char_bytes */
    paige_unpack_num(stream); /* max_chars */
    paige_unpack_num(stream); /* ascent */
    paige_unpack_num(stream); /* descent */
    paige_unpack_num(stream); /* leading */
    paige_unpack_num(stream); /* shift_verb */
    record->class_bits = (uint32_t)paige_unpack_num(stream);
    paige_unpack_num(stream); /* style_sheet_id */

    /* Foreground/background colors (4 components each). */
    for (int i = 0; i < 4; ++i)
        paige_unpack_num(stream);
    for (int i = 0; i < 4; ++i)
        paige_unpack_num(stream);

    boolean legacy_v5 = (version < PAIGE_KEY_REVISION6);

    if (legacy_v5)
        paige_unpack_num(stream); /* machine_var */

    paige_unpack_num(stream); /* char_width */
    record->point_fixed = paige_unpack_num(stream);
    paige_unpack_num(stream); /* left_overhang */
    paige_unpack_num(stream); /* right_overhang */
    paige_unpack_num(stream); /* top_extra */
    paige_unpack_num(stream); /* bot_extra */
    paige_unpack_num(stream); /* space_extra */
    paige_unpack_num(stream); /* char_extra */
    paige_unpack_num(stream); /* user_id */
    paige_unpack_num(stream); /* user_data */
    paige_unpack_num(stream); /* user_data2 */

    if (legacy_v5)
        record->class_bits = (uint32_t)paige_unpack_num(stream);

    if (version >= PAIGE_KEY_REVISION8A)
        paige_unpack_num(stream); /* time_stamp */

    paige_skip_refcon(stream, version);
    paige_unpack_num(stream); /* used_ctr */

    /* future[8] */
    for (int i = 0; i < 8; ++i)
        paige_unpack_num(stream);

    int style_slots = (version < PAIGE_KEY_REVISION4) ? 16 : PAIGE_MAX_STYLES;
    for (int i = 0; i < style_slots; ++i)
        record->styles[i] = (short)paige_unpack_num(stream);
    for (int i = style_slots; i < PAIGE_MAX_STYLES; ++i)
        record->styles[i] = 0;

    if (version >= PAIGE_KEY_REVISION5)
        paige_unpack_num(stream); /* small_caps_index */

    if (version >= PAIGE_KEY_REVISION9) {
        paige_unpack_num(stream); /* key_equiv */
        paige_unpack_num(stream); /* style_num */
        paige_unpack_num(stream); /* style_basedon */
        paige_unpack_num(stream); /* rtf_reserved */
    }

    if (version >= PAIGE_KEY_REVISION12)
        paige_unpack_num(stream); /* named_style_index */

    return true;
}

static boolean paige_parse_style_records(const uint8_t *payload, long size, long element_info,
        const paige_file_context *ctx, paige_style_table *table,
        char *errbuf, size_t errlen) {
    if (payload == NULL || table == NULL || ctx == NULL)
        return false;

    long style_count = element_info;
    if (style_count <= 0)
        return true;

    if (!paige_style_table_expand(table, (size_t)style_count)) {
        paige_log_error(errbuf, errlen, "Unable to grow style table");
        return false;
    }

    paige_pack_stream stream;
    paige_pack_setup(&stream, payload, size);

    /* First long is insert_style; we do not use it but must consume it. */
    paige_unpack_num(&stream);

    for (long i = 0; i < style_count; ++i) {
        paige_style_record *record = &table->records[table->count];
        if (!paige_parse_single_style(&stream, ctx->version, record)) {
            paige_log_error(errbuf, errlen, "Unable to parse style record %ld", i);
            return false;
        }
        ++table->count;
    }

    return true;
}

static boolean paige_parse_style_runs(const uint8_t *payload, long size, long element_info,
        paige_style_run_table *runs, char *errbuf, size_t errlen) {
    if (payload == NULL || runs == NULL)
        return false;

    long run_count = element_info;
    if (run_count <= 0)
        return true;

    if (!paige_style_run_table_expand(runs, (size_t)run_count)) {
        paige_log_error(errbuf, errlen, "Unable to grow style run table");
        return false;
    }

    paige_pack_stream stream;
    paige_pack_setup(&stream, payload, size);

    for (long i = 0; i < run_count; ++i) {
        paige_style_run_entry *entry = &runs->runs[runs->count];
        entry->offset = (size_t)paige_unpack_num(&stream);
        entry->style_index = (short)paige_unpack_num(&stream);
        ++runs->count;
    }

    return true;
}

static boolean paige_parse_single_par(paige_pack_stream *stream, long version,
        paige_par_record *record) {
    if (stream == NULL || record == NULL)
        return false;

    memset(record, 0, sizeof(*record));
    record->justification = (short)paige_unpack_num(stream);
    paige_unpack_num(stream); /* direction */
    paige_unpack_num(stream); /* style_sheet_id */

    /* indents: left, right, first */
    paige_unpack_num(stream);
    paige_unpack_num(stream);
    paige_unpack_num(stream);

    paige_unpack_num(stream); /* spacing */
    paige_unpack_num(stream); /* leading_extra */
    paige_unpack_num(stream); /* leading_fixed */
    if (version >= PAIGE_KEY_REVISION6)
        paige_unpack_num(stream); /* leading_variable */
    paige_unpack_num(stream); /* top_extra */
    paige_unpack_num(stream); /* bot_extra */
    paige_unpack_num(stream); /* left_extra */
    paige_unpack_num(stream); /* right_extra */
    paige_unpack_num(stream); /* user_id */
    paige_unpack_num(stream); /* user_data */
    paige_unpack_num(stream); /* user_data2 */
    paige_unpack_num(stream); /* partial_just */
    if (version >= PAIGE_KEY_REVISION9)
        paige_unpack_num(stream); /* outline_level */

    paige_skip_refcon(stream, version);
    paige_unpack_num(stream); /* used_ctr */

    /* future[6] */
    for (int i = 0; i < 6; ++i)
        paige_unpack_num(stream);

    long tab_qty = paige_unpack_num(stream);
    if (tab_qty > 0) {
        for (long i = 0; i < tab_qty; ++i) {
            paige_unpack_num(stream); /* tab_type */
            paige_unpack_num(stream); /* position */
            paige_unpack_num(stream); /* leader */
            paige_skip_refcon(stream, version);
        }
    }

    if (version >= PAIGE_KEY_REVISION1)
        paige_unpack_num(stream); /* def_tab_space */

    if (version >= PAIGE_KEY_REVISION7)
        paige_unpack_num(stream); /* class_info */

    if (version >= PAIGE_KEY_REVISION9) {
        paige_unpack_num(stream); /* key_equiv */
        paige_unpack_num(stream); /* style_num */
        paige_unpack_num(stream); /* style_basedon */
        paige_unpack_num(stream); /* next_style */
    }

    if (version >= PAIGE_KEY_REVISION12)
        paige_unpack_num(stream); /* named_style_index */

    if (version >= PAIGE_KEY_REVISION18) {
        /* table info */
        for (int i = 0; i < 9; ++i)
            paige_unpack_num(stream);
    }

    if (version >= PAIGE_KEY_REVISION22)
        paige_unpack_num(stream); /* html_style */

    if (version >= PAIGE_KEY_REVISION23) {
        /* border colors */
        paige_unpack_num(stream);
        paige_unpack_num(stream);
        paige_unpack_num(stream);
        paige_unpack_num(stream);
    }

    return true;
}

static boolean paige_parse_par_records(const uint8_t *payload, long size, long element_info,
        const paige_file_context *ctx, paige_par_table *table, char *errbuf, size_t errlen) {
    if (payload == NULL || table == NULL || ctx == NULL)
        return false;

    long par_count = element_info;
    if (par_count <= 0)
        return true;

    if (!paige_par_table_expand(table, (size_t)par_count)) {
        paige_log_error(errbuf, errlen, "Unable to grow paragraph table");
        return false;
    }

    paige_pack_stream stream;
    paige_pack_setup(&stream, payload, size);

    for (long i = 0; i < par_count; ++i) {
        paige_par_record *record = &table->records[table->count];
        if (!paige_parse_single_par(&stream, ctx->version, record)) {
            paige_log_error(errbuf, errlen, "Unable to parse paragraph record %ld", i);
            return false;
        }
        ++table->count;
    }

    return true;
}

static boolean paige_parse_par_runs(const uint8_t *payload, long size, long element_info,
        paige_par_run_table *runs, char *errbuf, size_t errlen) {
    if (payload == NULL || runs == NULL)
        return false;

    long run_count = element_info;
    if (run_count <= 0)
        return true;

    if (!paige_par_run_table_expand(runs, (size_t)run_count)) {
        paige_log_error(errbuf, errlen, "Unable to grow paragraph run table");
        return false;
    }

    paige_pack_stream stream;
    paige_pack_setup(&stream, payload, size);

    for (long i = 0; i < run_count; ++i) {
        paige_par_run_entry *entry = &runs->runs[runs->count];
        entry->offset = (size_t)paige_unpack_num(&stream);
        entry->par_index = (short)paige_unpack_num(&stream);
        ++runs->count;
    }

    return true;
}

static void paige_capture_font_name(const uint8_t *chunk, long chunk_len,
        paige_font_record *record) {
    if (record == NULL)
        return;
    record->name_len = 0;
    memset(record->name, 0, sizeof(record->name));
    if (chunk == NULL || chunk_len <= 0)
        return;
    uint8_t length = chunk[0];
    if (length > PAIGE_FONT_NAME_MAX)
        length = PAIGE_FONT_NAME_MAX;
    if (length > 0 && chunk_len - 1 >= length) {
        memcpy(record->name, chunk + 1, length);
        record->name_len = length;
    }
}

static boolean paige_parse_font_records(const uint8_t *payload, long size, long element_info,
        const paige_file_context *ctx, paige_font_table *table, char *errbuf, size_t errlen) {
    if (payload == NULL || table == NULL || ctx == NULL)
        return false;

    long font_count = element_info;
    if (font_count <= 0)
        return true;

    if (!paige_font_table_expand(table, (size_t)font_count)) {
        paige_log_error(errbuf, errlen, "Unable to grow font table");
        return false;
    }

    paige_pack_stream stream;
    paige_pack_setup(&stream, payload, size);

    for (long i = 0; i < font_count; ++i) {
        paige_font_record *record = &table->records[table->count];
        memset(record, 0, sizeof(*record));
        record->font_id = (short)i;

        long chunk_len = 0;
        const uint8_t *chunk = NULL;
        if (!paige_unpack_bytes(&stream, &chunk_len, &chunk)) {
            paige_log_error(errbuf, errlen, "Unable to read font name block");
            return false;
        }
        paige_capture_font_name(chunk, chunk_len, record);

        if (ctx->version >= PAIGE_KEY_REVISION8) {
            /* Alternate name (skip for now). */
            if (!paige_skip_ptr_bytes(&stream))
                return false;
        }

        paige_unpack_num(&stream); /* environs */
        paige_unpack_num(&stream); /* typeface */
        paige_unpack_num(&stream); /* family_id */
        paige_unpack_num(&stream); /* char_type */
        if (ctx->version >= PAIGE_KEY_REVISION9)
            paige_unpack_num(&stream); /* code_page */
        paige_unpack_num(&stream); /* language */
        paige_skip_refcon(&stream, ctx->version);

        for (int future = 0; future < 8; ++future)
            paige_unpack_num(&stream);

        if (ctx->version >= PAIGE_KEY_REVISION3) {
            paige_unpack_num(&stream); /* platform */
            paige_unpack_num(&stream); /* alternate_id */
        }

        ++table->count;
    }

    return true;
}

static boolean paige_parse_document(const uint8_t *bytes, long len, paige_document *doc,
        paige_extract_stats *stats, char *errbuf, size_t errbuflen) {
    if (bytes == NULL || len <= 0 || doc == NULL)
        return false;

    paige_document_init(doc);

    if (stats)
        memset(stats, 0, sizeof(*stats));

    if (!newclearhandle(0, &doc->mac_text)) {
        paige_log_error(errbuf, errbuflen, "Unable to allocate MacRoman handle");
        paige_document_dispose(doc);
        return false;
    }

    paige_blob_view view = { bytes, len };
    short key = 0;
    long data_size = 0;
    long element_info = 0;
    const uint8_t *payload = NULL;

    while (view.remaining > 0) {
        if (!paige_next_key(&view, &key, &data_size, &element_info, &payload)) {
            paige_log_error(errbuf, errbuflen, "Malformed Paige key header");
            paige_document_dispose(doc);
            return false;
        }

        if (key == paige_eof_key)
            break;

        if (key == paige_key) {
            if (!paige_parse_file_header(payload, data_size, &doc->file_ctx, errbuf, errbuflen)) {
                paige_document_dispose(doc);
                return false;
            }
        } else if (key == paige_text_key) {
            if (!paige_collect_text(payload, data_size, doc->mac_text, stats, errbuf, errbuflen)) {
                paige_document_dispose(doc);
                return false;
            }
        } else if (key == style_info_key) {
            if (doc->file_ctx.version == 0) {
                paige_log_error(errbuf, errbuflen, "Encountered style_info_key before paige_key");
                paige_document_dispose(doc);
                return false;
            }
            if (!paige_parse_style_records(payload, data_size, element_info, &doc->file_ctx,
                    &doc->styles, errbuf, errbuflen)) {
                paige_document_dispose(doc);
                return false;
            }
        } else if (key == style_run_key) {
            if (!paige_parse_style_runs(payload, data_size, element_info, &doc->style_runs, errbuf, errbuflen)) {
                paige_document_dispose(doc);
                return false;
            }
        } else if (key == par_info_key) {
            if (doc->file_ctx.version == 0) {
                paige_log_error(errbuf, errbuflen, "Encountered par_info_key before paige_key");
                paige_document_dispose(doc);
                return false;
            }
            if (!paige_parse_par_records(payload, data_size, element_info, &doc->file_ctx,
                    &doc->paragraphs, errbuf, errbuflen)) {
                paige_document_dispose(doc);
                return false;
            }
        } else if (key == par_run_key) {
            if (!paige_parse_par_runs(payload, data_size, element_info, &doc->par_runs, errbuf, errbuflen)) {
                paige_document_dispose(doc);
                return false;
            }
        } else if (key == font_info_key) {
            if (doc->file_ctx.version == 0) {
                paige_log_error(errbuf, errbuflen, "Encountered font_info_key before paige_key");
                paige_document_dispose(doc);
                return false;
            }
            if (!paige_parse_font_records(payload, data_size, element_info, &doc->file_ctx,
                    &doc->fonts, errbuf, errbuflen)) {
                paige_document_dispose(doc);
                return false;
            }
        }
    }

    return true;
}

static boolean paige_document_get_plaintext(paige_document *doc, Handle *hout_utf8,
        paige_extract_stats *stats, char *errbuf, size_t errbuflen) {
    if (doc == NULL || doc->mac_text == nil || hout_utf8 == NULL)
        return false;

    Handle hutf8 = nil;
    if (!newclearhandle(0, &hutf8)) {
        paige_log_error(errbuf, errbuflen, "Unable to allocate UTF-8 handle");
        return false;
    }

    if (!macromantoutf8(doc->mac_text, hutf8)) {
        disposehandle(hutf8);
        *hout_utf8 = doc->mac_text;
        if (stats)
            stats->returned_macroman = true;
        doc->mac_text = nil;
        return true;
    }

    *hout_utf8 = hutf8;
    return true;
}

static inline boolean paige_rtf_append_bytes(Handle h, const char *bytes, size_t len) {
    return paige_append_chunk(h, (const uint8_t *)bytes, (long)len);
}

static inline boolean paige_rtf_append_cstr(Handle h, const char *s) {
    return paige_rtf_append_bytes(h, s, strlen(s));
}

static inline boolean paige_rtf_append_char(Handle h, char c) {
    return paige_append_chunk(h, (const uint8_t *)&c, 1);
}

static inline uint16_t paige_macroman_to_unicode(uint8_t ch) {
    return kMacRomanToUnicode[ch];
}

typedef struct {
    boolean bold;
    boolean italic;
    boolean underline;
    boolean dbl_underline;
    boolean strike;
    boolean superscript;
    boolean subscript;
    boolean all_caps;
    boolean small_caps;
    boolean hidden;
    short font_index;
    short point_half_points;
} paige_rtf_style_state;

static void paige_rtf_style_state_init(paige_rtf_style_state *state) {
    if (state == NULL)
        return;
    memset(state, 0, sizeof(*state));
    state->point_half_points = 24; /* default 12pt */
}

static const paige_style_record *paige_style_for_index(const paige_document *doc, short idx) {
    if (doc == NULL || idx < 0 || (size_t)idx >= doc->styles.count)
        return NULL;
    return &doc->styles.records[idx];
}

static const paige_par_record *paige_par_for_index(const paige_document *doc, short idx) {
    if (doc == NULL || idx < 0 || (size_t)idx >= doc->paragraphs.count)
        return NULL;
    return &doc->paragraphs.records[idx];
}

static void paige_style_state_from_record(const paige_document *doc, short style_index,
        paige_rtf_style_state *state) {
    const paige_style_record *rec = paige_style_for_index(doc, style_index);
    if (state == NULL)
        return;
    if (rec == NULL) {
        paige_rtf_style_state_init(state);
        return;
    }
    uint32_t bits = rec->class_bits;
    state->bold = (bits & X_BOLD_BIT) != 0 || rec->styles[paige_style_bold] != 0;
    state->italic = (bits & X_ITALIC_BIT) != 0 || rec->styles[paige_style_italic] != 0;
    state->underline = (bits & (X_UNDERLINE_BIT | X_WORD_UNDERLINE_BIT)) != 0 ||
        rec->styles[paige_style_underline] != 0 || rec->styles[paige_style_word_underline] != 0;
    state->dbl_underline = (bits & X_DBL_UNDERLINE_BIT) != 0 || rec->styles[paige_style_dbl_underline] != 0;
    state->strike = (bits & X_STRIKEOUT_BIT) != 0 || rec->styles[paige_style_strikeout] != 0;
    state->superscript = (bits & X_SUPERSCRIPT_BIT) != 0 || rec->styles[paige_style_superscript] != 0;
    state->subscript = (bits & X_SUBSCRIPT_BIT) != 0 || rec->styles[paige_style_subscript] != 0;
    state->all_caps = (bits & X_ALL_CAPS_BIT) != 0 || rec->styles[paige_style_all_caps] != 0;
    state->small_caps = (bits & X_SMALL_CAPS_BIT) != 0 || rec->styles[paige_style_small_caps] != 0;
    state->hidden = (bits & X_HIDDEN_TEXT_BIT) != 0 || rec->styles[paige_style_hidden_text] != 0;
    short font_idx = rec->font_index;
    if (font_idx < 0)
        font_idx = 0;
    state->font_index = font_idx;
    short points = paige_fixed_to_points(rec->point_fixed);
    if (points <= 0)
        points = 12;
    state->point_half_points = (short)(points * 2);
}

static boolean paige_emit_style_transition(Handle hrtf, paige_rtf_style_state *current,
        const paige_rtf_style_state *target, short *font_map, size_t font_count) {
    if (hrtf == nil || current == NULL || target == NULL || font_map == NULL)
        return false;
    char buf[32];
    if (current->bold != target->bold)
        paige_rtf_append_cstr(hrtf, target->bold ? "\\b " : "\\b0 ");
    if (current->italic != target->italic)
        paige_rtf_append_cstr(hrtf, target->italic ? "\\i " : "\\i0 ");
    if (current->underline != target->underline)
        paige_rtf_append_cstr(hrtf, target->underline ? "\\ul " : "\\ul0 ");
    if (current->dbl_underline != target->dbl_underline)
        paige_rtf_append_cstr(hrtf, target->dbl_underline ? "\\uldb " : "\\ulnone ");
    if (current->strike != target->strike)
        paige_rtf_append_cstr(hrtf, target->strike ? "\\strike " : "\\strike0 ");
    if (current->superscript != target->superscript)
        paige_rtf_append_cstr(hrtf, target->superscript ? "\\super " : "\\nosupersub ");
    if (current->subscript != target->subscript)
        paige_rtf_append_cstr(hrtf, target->subscript ? "\\sub " : "\\nosupersub ");
    if (current->all_caps != target->all_caps)
        paige_rtf_append_cstr(hrtf, target->all_caps ? "\\caps " : "\\caps0 ");
    if (current->small_caps != target->small_caps)
        paige_rtf_append_cstr(hrtf, target->small_caps ? "\\scaps " : "\\scaps0 ");
    if (current->hidden != target->hidden)
        paige_rtf_append_cstr(hrtf, target->hidden ? "\\v " : "\\v0 ");

    short target_font = target->font_index;
    if (target_font < 0 || (size_t)target_font >= font_count)
        target_font = 0;
    if (target_font != current->font_index) {
        snprintf(buf, sizeof(buf), "\\f%d ", font_map[target_font]);
        paige_rtf_append_cstr(hrtf, buf);
    }

    if (current->point_half_points != target->point_half_points) {
        snprintf(buf, sizeof(buf), "\\fs%d ", target->point_half_points);
        paige_rtf_append_cstr(hrtf, buf);
    }

    *current = *target;
    return true;
}

static boolean paige_emit_paragraph_control(Handle hrtf, short justification) {
    const char *control = "\\ql ";
    switch (justification) {
        case paige_justify_center: control = "\\qc "; break;
        case paige_justify_right: control = "\\qr "; break;
        case paige_justify_full: control = "\\qj "; break;
        default: control = "\\ql "; break;
    }
    return paige_rtf_append_cstr(hrtf, control);
}

static boolean paige_build_font_map(const paige_document *doc, short **out_map,
        size_t *out_count, Handle hrtf, char *errbuf, size_t errlen) {
    (void)errbuf;
    (void)errlen;
    size_t count = doc->fonts.count;
    if (count == 0)
        count = 1;
    short *map = (short *)calloc(count, sizeof(short));
    if (map == NULL)
        return false;

    paige_rtf_append_cstr(hrtf, "{\\fonttbl");
    if (doc->fonts.count == 0) {
        map[0] = 0;
        paige_rtf_append_cstr(hrtf, "{\\f0\\fcharset77 Geneva;}");
    } else {
        for (size_t i = 0; i < doc->fonts.count; ++i) {
            const paige_font_record *font = &doc->fonts.records[i];
            map[i] = (short)i;
            char header[32];
            snprintf(header, sizeof(header), "{\\f%d\\fcharset77 ", (int)i);
            paige_rtf_append_cstr(hrtf, header);
            for (uint8_t idx = 0; idx < font->name_len; ++idx) {
                uint8_t mac = font->name[idx];
                if (mac == '\\' || mac == '{' || mac == '}') {
                    paige_rtf_append_char(hrtf, '\\');
                    paige_rtf_append_char(hrtf, (char)mac);
                } else if (mac < 0x80 && mac >= 0x20) {
                    paige_rtf_append_char(hrtf, (char)mac);
                } else {
                    char esc[6];
                    snprintf(esc, sizeof(esc), "\\'%02x", mac);
                    paige_rtf_append_cstr(hrtf, esc);
                }
            }
            paige_rtf_append_cstr(hrtf, ";}");
        }
    }
    paige_rtf_append_cstr(hrtf, "}");
    *out_map = map;
    *out_count = count;
    return true;
}

static boolean wptext_emit_rtf_from_document(const paige_document *doc, Handle *hout_rtf,
        char *errbuf, size_t errlen) {
    if (doc == NULL || doc->mac_text == nil || hout_rtf == NULL)
        return false;

    Handle hrtf = nil;
    if (!newclearhandle(0, &hrtf)) {
        paige_log_error(errbuf, errlen, "Unable to allocate RTF handle");
        return false;
    }

    paige_rtf_append_cstr(hrtf, "{\\rtf1\\ansi\\ansicpg10000\\deff0");

    short *font_map = NULL;
    size_t font_count = 0;
    if (!paige_build_font_map(doc, &font_map, &font_count, hrtf, errbuf, errlen)) {
        disposehandle(hrtf);
        return false;
    }
    paige_rtf_append_cstr(hrtf, "\n\\viewkind4\\uc1\n");

    paige_rtf_style_state current_style;
    paige_rtf_style_state target_style;
    paige_rtf_style_state_init(&current_style);
    paige_style_state_from_record(doc, 0, &target_style);
    paige_emit_style_transition(hrtf, &current_style, &target_style, font_map, font_count);

    short current_par = paige_justify_left;
    const paige_par_record *default_par = paige_par_for_index(doc, 0);
    if (default_par != NULL)
        current_par = default_par->justification;

    size_t text_len = gethandlesize(doc->mac_text);
    const uint8_t *text_bytes = (const uint8_t *)*doc->mac_text;

    size_t style_run_idx = 0;
    size_t par_run_idx = 0;
    boolean pending_paragraph = true;

    while (style_run_idx < doc->style_runs.count &&
           doc->style_runs.runs[style_run_idx].offset == 0) {
        paige_style_state_from_record(doc, doc->style_runs.runs[style_run_idx].style_index, &target_style);
        paige_emit_style_transition(hrtf, &current_style, &target_style, font_map, font_count);
        ++style_run_idx;
    }

    while (par_run_idx < doc->par_runs.count &&
           doc->par_runs.runs[par_run_idx].offset == 0) {
        const paige_par_record *par = paige_par_for_index(doc, doc->par_runs.runs[par_run_idx].par_index);
        if (par != NULL)
            current_par = par->justification;
        ++par_run_idx;
    }

    for (size_t offset = 0; offset < text_len; ++offset) {
        while (style_run_idx < doc->style_runs.count &&
               doc->style_runs.runs[style_run_idx].offset == offset) {
            paige_style_state_from_record(doc, doc->style_runs.runs[style_run_idx].style_index, &target_style);
            paige_emit_style_transition(hrtf, &current_style, &target_style, font_map, font_count);
            ++style_run_idx;
        }

        while (par_run_idx < doc->par_runs.count &&
               doc->par_runs.runs[par_run_idx].offset == offset) {
            const paige_par_record *par = paige_par_for_index(doc, doc->par_runs.runs[par_run_idx].par_index);
            if (par != NULL)
                current_par = par->justification;
            ++par_run_idx;
            paige_emit_paragraph_control(hrtf, current_par);
        }

        if (pending_paragraph) {
            paige_rtf_append_cstr(hrtf, "\\pard ");
            paige_emit_paragraph_control(hrtf, current_par);
            pending_paragraph = false;
        }

        uint8_t mac = text_bytes[offset];
        if (mac == '\r') {
            paige_rtf_append_cstr(hrtf, "\\par\n");
            pending_paragraph = true;
            continue;
        }
        if (mac == '\n')
            continue;
        if (mac == '\t') {
            paige_rtf_append_cstr(hrtf, "\\tab ");
            continue;
        }

        uint16_t unicode = paige_macroman_to_unicode(mac);
        if (unicode == '\\' || unicode == '{' || unicode == '}') {
            paige_rtf_append_char(hrtf, '\\');
            paige_rtf_append_char(hrtf, (char)unicode);
        } else if (unicode >= 0x20 && unicode < 0x80) {
            paige_rtf_append_char(hrtf, (char)unicode);
        } else {
            char buf[32];
            snprintf(buf, sizeof(buf), "\\u%d?", (int16_t)unicode);
            paige_rtf_append_cstr(hrtf, buf);
        }
    }

    paige_rtf_append_cstr(hrtf, "}");
    free(font_map);
    *hout_rtf = hrtf;
    return true;
}

boolean wptext_emit_rtf_from_paige_blob(const uint8_t *bytes, long len, Handle *hout_rtf,
        long *out_char_count, paige_extract_stats *stats, char *errbuf, size_t errlen) {
    if (hout_rtf == NULL)
        return false;

    paige_document doc;
    if (!paige_parse_document(bytes, len, &doc, stats, errbuf, errlen)) {
        paige_document_dispose(&doc);
        return false;
    }

    boolean ok = wptext_emit_rtf_from_document(&doc, hout_rtf, errbuf, errlen);
    if (ok && out_char_count != NULL)
        *out_char_count = gethandlesize(doc.mac_text);
    paige_document_dispose(&doc);
    return ok;
}

static boolean paige_append_chunk(Handle h, const uint8_t *chunk, long len) {
    if (h == NULL || chunk == NULL || len <= 0)
        return true;

    long old_size = gethandlesize(h);
    if (old_size < 0)
        old_size = 0;

    if (len > LONG_MAX - old_size)
        return false;

    long new_size = old_size + len;
    if (!sethandlesize(h, new_size)) {
#if defined(FRONTIER_HEADLESS)
        fprintf(stderr, "[paige] append fail old=%ld chunk=%ld\n", old_size, len);
#endif
        return false;
    }

    unsigned char *dest = *h;
    if (dest == NULL)
        return false;

    memcpy(dest + old_size, chunk, (size_t)len);
    return true;
}

static void paige_document_init(paige_document *doc) {
    if (doc == NULL)
        return;
    memset(doc, 0, sizeof(*doc));
}

static void paige_document_dispose(paige_document *doc) {
    if (doc == NULL)
        return;
    if (doc->mac_text != nil) {
        disposehandle(doc->mac_text);
        doc->mac_text = nil;
    }
    paige_style_table_dispose(&doc->styles);
    paige_style_run_table_dispose(&doc->style_runs);
    paige_par_table_dispose(&doc->paragraphs);
    paige_par_run_table_dispose(&doc->par_runs);
    paige_font_table_dispose(&doc->fonts);
    doc->file_ctx.version = 0;
    doc->file_ctx.flags2 = 0;
}

static boolean paige_collect_text(const uint8_t *payload, long size, Handle hmac, paige_extract_stats *stats,
        char *errbuf, size_t errlen) {
    if (payload == NULL || size <= 0 || hmac == NULL)
        return false;

    paige_pack_stream stream;
    paige_pack_setup(&stream, payload, size);

    while (stream.remaining_ctr > 0) {
		uint8_t code = *stream.data;
		uint8_t type = code & CODE_MASK;

            if (type == paige_byte_data) {
			long chunk_len = 0;
			const uint8_t *chunk = NULL;
			if (!paige_unpack_bytes(&stream, &chunk_len, &chunk)) {
				paige_log_error(errbuf, errlen, "Malformed byte_data block");
				return false;
			}
			if (chunk_len > 0 && !paige_append_chunk(hmac, chunk, chunk_len)) {
				paige_log_error(errbuf, errlen, "Failed to append chunk");
				return false;
			}
            if (stats)
                stats->total_text_bytes += chunk_len;
        } else if (type == paige_short_data || type == paige_long_data) {
            paige_unpack_num(&stream);
        } else if (type == paige_terminator_data) {
			++stream.data;
			--stream.remaining_ctr;
			break;
		} else {
			++stream.data;
			--stream.remaining_ctr;
			if (stats)
				stats->dropped_controls = true;
		}
	}

    if (stats)
        stats->text_key_count += 1;

	return true;
}

static boolean paige_next_key(paige_blob_view *view, short *out_key, long *out_size,
		long *out_element, const uint8_t **out_payload) {
	if (view == NULL || view->remaining < PAIGE_KEY_HEADER_SIZE ||
		out_key == NULL || out_size == NULL || out_element == NULL || out_payload == NULL)
		return false;

	long key = 0;
	if (!paige_read_hex(view, 4, &key))
		return false;

	long data_size = 0;
	if (!paige_read_hex(view, 8, &data_size))
		return false;

	long element_info = 0;
	if (!paige_read_hex(view, 8, &element_info))
		return false;

	if (data_size < 0 || view->remaining < data_size)
		return false;

	*out_key = (short)key;
	*out_size = data_size;
	*out_element = element_info;
	*out_payload = view->cursor;
	view->cursor += data_size;
	view->remaining -= data_size;
	return true;
}

boolean paige_extract_text_and_styles(const uint8_t *bytes, long len, Handle *hout_utf8,
	paige_extract_stats *stats, char *errbuf, size_t errbuflen) {
    if (bytes == NULL || len <= 0 || hout_utf8 == NULL)
        return false;

    fprintf(stderr, "[paige] extract len=%ld\n", len);

    char local_errbuf[256] = {0};
    if (errbuf == NULL) {
        errbuf = local_errbuf;
        errbuflen = sizeof(local_errbuf);
    }

    paige_document doc;
    if (!paige_parse_document(bytes, len, &doc, stats, errbuf, errbuflen)) {
        paige_document_dispose(&doc);
        return false;
    }

    Handle hutf8 = nil;
    boolean ok = paige_document_get_plaintext(&doc, &hutf8, stats, errbuf, errbuflen);
    if (ok) {
        fprintf(stderr, "[paige] extract ok bytes=%ld\n", gethandlesize(hutf8));
        *hout_utf8 = hutf8;
    }

    paige_document_dispose(&doc);
    return ok;
}
