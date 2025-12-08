/*
 * text_encoding_portable.c - Minimal TEC/TextEncoding bridge for headless builds
 *
 * 2025-10-31 Codex: Provide a small, table-driven implementation so the runtime
 * can enumerate a handful of encodings without linking Carbon. Converters are
 * pass-through (copy only) and only support identical encodings for now.
 */

#include "../Common/headers/frontier.h"

#include "portable_handles.h"
#include "text_encoding_portable.h"

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

typedef struct portable_encoding_entry {
    TextEncoding encoding;
    const char *iana;
    const char *display;
} portable_encoding_entry;

static const portable_encoding_entry g_encodings[] = {
    { kTextEncodingMacRoman,    "x-mac-roman",    "Macintosh Roman" },
    { kTextEncodingUTF8,        "utf-8",          "Unicode (UTF-8)" },
    { kTextEncodingWindowsLatin1, "windows-1252", "Windows Latin 1" },
};

typedef struct portable_tec_converter {
    TextEncoding in_encoding;
    TextEncoding out_encoding;
} portable_tec_converter;

static int portable_casecmp(const char *lhs, const char *rhs);

static size_t pascal_to_c(const unsigned char *src, char *dst, size_t dst_len) {
    size_t len = 0;
    if (src) {
        len = src[0];
        if (len + 1 > dst_len)
            len = dst_len - 1;
        if (len > 0)
            memcpy(dst, src + 1, len);
    }
    dst[len] = '\0';
    return len;
}

static void c_to_pascal(const char *src, unsigned char *dst) {
    size_t len = src ? strlen(src) : 0;
    if (len > 255)
        len = 255;
    dst[0] = (unsigned char) len;
    if (len > 0)
        memcpy(dst + 1, src, len);
}

static const portable_encoding_entry *find_encoding_by_value(TextEncoding enc) {
    for (size_t i = 0; i < sizeof(g_encodings) / sizeof(g_encodings[0]); ++i) {
        if (g_encodings[i].encoding == enc)
            return &g_encodings[i];
    }
    return NULL;
}

static const portable_encoding_entry *find_encoding_by_name(const char *name) {
    for (size_t i = 0; i < sizeof(g_encodings) / sizeof(g_encodings[0]); ++i) {
        const char *candidate = g_encodings[i].iana;
        if (candidate == NULL)
            continue;
        if (portable_casecmp(candidate, name) == 0)
            return &g_encodings[i];
    }
    return NULL;
}

OSStatus TECCountAvailableTextEncodings(ItemCount *count) {
    if (count)
        *count = (ItemCount)(sizeof(g_encodings) / sizeof(g_encodings[0]));
    return noErr;
}

OSStatus TECGetAvailableTextEncodings(TextEncoding encodings[], ItemCount maxCount, ItemCount *actualCount) {
    ItemCount total = (ItemCount)(sizeof(g_encodings) / sizeof(g_encodings[0]));
    if (actualCount)
        *actualCount = total;

    if (encodings && maxCount > 0) {
        ItemCount to_copy = total < maxCount ? total : maxCount;
        for (ItemCount i = 0; i < to_copy; ++i)
            encodings[i] = g_encodings[i].encoding;
        if (to_copy < total)
            return kTECPartialCharErr;
    }
    return noErr;
}

OSStatus TECGetTextEncodingInternetName(TextEncoding encoding, unsigned char *name) {
    const portable_encoding_entry *entry = find_encoding_by_value(encoding);
    if (!entry || !entry->iana)
        return kTextUnsupportedEncodingErr;
    if (name)
        c_to_pascal(entry->iana, name);
    return noErr;
}

OSStatus TECGetTextEncodingFromInternetName(TextEncoding *encoding, const unsigned char *name) {
    if (!encoding || !name)
        return paramErr;

    char buffer[260];
    pascal_to_c(name, buffer, sizeof buffer);

    const portable_encoding_entry *entry = find_encoding_by_name(buffer);
    if (!entry)
        return kTextUnsupportedEncodingErr;

    *encoding = entry->encoding;
    return noErr;
}

OSStatus TECGetTextEncodingInfo(TextEncoding encoding, TextEncodingBase *base, TextEncodingVariant *variant, TextEncodingFormat *format) {
    (void)encoding;
    if (base)
        *base = 0;
    if (variant)
        *variant = 0;
    if (format)
        *format = 0;
    return noErr;
}

OSStatus GetTextEncodingName(TextEncoding encoding,
                             TextEncodingNameSelector selector,
                             RegionCode region,
                             TextEncoding referenceEncoding,
                             ItemCount maxLen,
                             unsigned long *actualLen,
                             RegionCode *outRegion,
                             TextEncoding *outEncoding,
                             unsigned char *outName) {
    (void)selector;
    (void)region;
    (void)referenceEncoding;
    (void)maxLen;

    const portable_encoding_entry *entry = find_encoding_by_value(encoding);
    if (!entry || !entry->display)
        return kTextUnsupportedEncodingErr;

    size_t len = strlen(entry->display);
    if (actualLen)
        *actualLen = (unsigned long) len;
    if (outRegion)
        *outRegion = verUS;
    if (outEncoding)
        *outEncoding = encoding;
    if (outName && maxLen > 0)
        c_to_pascal(entry->display, outName);
    return noErr;
}

OSStatus TECCreateConverter(TECObjectRef *converter, TextEncoding inputEncoding, TextEncoding outputEncoding) {
    if (!converter)
        return paramErr;

    *converter = NULL;

    if (!find_encoding_by_value(inputEncoding) || !find_encoding_by_value(outputEncoding))
        return kTECNoConversionPathErr;

    /* We only support identity passthrough; mismatched encodings would corrupt data. */
    if (inputEncoding != outputEncoding)
        return kTECNoConversionPathErr;

    portable_tec_converter *ctx = (portable_tec_converter *) malloc(sizeof(portable_tec_converter));
    if (!ctx)
        return memFullErr;

    ctx->in_encoding = inputEncoding;
    ctx->out_encoding = outputEncoding;
    *converter = ctx;
    return noErr;
}

OSStatus TECDisposeConverter(TECObjectRef converter) {
    portable_tec_converter *ctx = (portable_tec_converter *) converter;
    free(ctx);
    return noErr;
}

OSStatus TECConvertText(TECObjectRef converter,
                        ConstTextPtr inputBuf, ByteCount inputLen, ByteCount *inputRead,
                        TextPtr outputBuf, ByteCount outputLen, ByteCount *outputProduced) {
    (void)converter;
    if (inputRead)
        *inputRead = inputLen;
    if (!outputBuf || outputLen == 0 || !inputBuf) {
        if (outputProduced)
            *outputProduced = 0;
        return noErr;
    }

    ByteCount to_copy = inputLen < outputLen ? inputLen : outputLen;
    memcpy(outputBuf, inputBuf, to_copy);
    if (outputProduced)
        *outputProduced = to_copy;
    return (to_copy == inputLen) ? noErr : kTECPartialCharErr;
}

OSStatus TECFlushText(TECObjectRef converter, TextPtr outputBuf, ByteCount outputLen, ByteCount *outputProduced) {
    (void)converter;
    (void)outputBuf;
    (void)outputLen;
    if (outputProduced)
        *outputProduced = 0;
    return noErr;
}
static int portable_casecmp(const char *lhs, const char *rhs) {
    if (!lhs || !rhs)
        return lhs ? 1 : (rhs ? -1 : 0);
    while (*lhs && *rhs) {
        unsigned char a = (unsigned char) tolower((unsigned char)*lhs++);
        unsigned char b = (unsigned char) tolower((unsigned char)*rhs++);
        if (a != b)
            return (int)a - (int)b;
    }
    return (unsigned char)*lhs - (unsigned char)*rhs;
}
