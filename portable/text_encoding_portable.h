/* text_encoding_portable.h - Portable shims for TEC/TextEncoding APIs */
#ifndef PORTABLE_TEXT_ENCODING_H
#define PORTABLE_TEXT_ENCODING_H

#include "standard_portable.h"
#include <stddef.h>

#ifndef kCFStringEncodingUTF8
#define kCFStringEncodingUTF8 0x08000100U
#endif

#ifndef kTextEncodingUnicodeDefault
#define kTextEncodingUnicodeDefault 0x0100U
#endif

#ifndef kTextEncodingWindowsLatin1
#define kTextEncodingWindowsLatin1 0x0500U
#endif

#ifndef kTextEncodingFullName
#define kTextEncodingFullName 0U
#endif

#ifndef kTextEncodingMacRoman
#define kTextEncodingMacRoman 0U
#endif

#ifndef kTextUnsupportedEncodingErr
#define kTextUnsupportedEncodingErr (-30874)
#endif

#ifndef kTextMalformedInputErr
#define kTextMalformedInputErr (-32768)
#endif

#ifndef kTextUndefinedElementErr
#define kTextUndefinedElementErr (-32767)
#endif

#ifndef kTECNoConversionPathErr
#define kTECNoConversionPathErr (-32766)
#endif

#ifndef kTECPartialCharErr
#define kTECPartialCharErr (-32765)
#endif

/* Stub TEC signatures */
typedef void *TECObjectRef;

static inline OSStatus TECCountAvailableTextEncodings(ItemCount *count) {
    if (count)
        *count = 0;
    return kTextUnsupportedEncodingErr;
}

static inline OSStatus TECGetAvailableTextEncodings(TextEncoding encodings[], ItemCount maxCount, ItemCount *actualCount) {
    if (actualCount)
        *actualCount = 0;
    (void)encodings; (void)maxCount;
    return kTextUnsupportedEncodingErr;
}

static inline OSStatus TECConvertText(TECObjectRef converter,
                                     ConstTextPtr inputBuf, ByteCount inputLen, ByteCount *inputRead,
                                     TextPtr outputBuf, ByteCount outputLen, ByteCount *outputProduced) {
    (void)converter; (void)inputBuf; (void)inputLen; (void)inputRead;
    (void)outputBuf; (void)outputLen; (void)outputProduced;
    return kTextUnsupportedEncodingErr;
}

static inline OSStatus TECCreateConverter(TECObjectRef *converter, TextEncoding inputEncoding, TextEncoding outputEncoding) {
    if (converter)
        *converter = NULL;
    (void)inputEncoding; (void)outputEncoding;
    return kTextUnsupportedEncodingErr;
}

static inline OSStatus TECDisposeConverter(TECObjectRef converter) {
    (void)converter;
    return kTextUnsupportedEncodingErr;
}

static inline OSStatus TECFlushText(TECObjectRef converter, TextPtr outputBuf, ByteCount outputLen, ByteCount *outputProduced) {
    (void)converter; (void)outputBuf; (void)outputLen; (void)outputProduced;
    return kTextUnsupportedEncodingErr;
}

#endif /* PORTABLE_TEXT_ENCODING_H */
