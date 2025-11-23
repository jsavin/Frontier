/* text_encoding_portable.h - Portable shims for TEC/TextEncoding APIs */
#ifndef PORTABLE_TEXT_ENCODING_H
#define PORTABLE_TEXT_ENCODING_H

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

#ifndef kTextEncodingUTF8
#define kTextEncodingUTF8 kCFStringEncodingUTF8
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

#ifndef verUS
#define verUS 0
#endif

OSStatus TECCountAvailableTextEncodings(ItemCount *count);
OSStatus TECGetAvailableTextEncodings(TextEncoding encodings[], ItemCount maxCount, ItemCount *actualCount);
OSStatus TECGetTextEncodingInternetName(TextEncoding encoding, unsigned char *name);
OSStatus TECGetTextEncodingFromInternetName(TextEncoding *encoding, const unsigned char *name);
OSStatus TECGetTextEncodingInfo(TextEncoding encoding, TextEncodingBase *base, TextEncodingVariant *variant, TextEncodingFormat *format);
OSStatus GetTextEncodingName(TextEncoding encoding, TextEncodingNameSelector selector, RegionCode region, TextEncoding referenceEncoding, ItemCount maxLen, unsigned long *actualLen, RegionCode *outRegion, TextEncoding *outEncoding, unsigned char *outName);
OSStatus TECCreateConverter(TECObjectRef *converter, TextEncoding inputEncoding, TextEncoding outputEncoding);
OSStatus TECDisposeConverter(TECObjectRef converter);
OSStatus TECConvertText(TECObjectRef converter,
                        ConstTextPtr inputBuf, ByteCount inputLen, ByteCount *inputRead,
                        TextPtr outputBuf, ByteCount outputLen, ByteCount *outputProduced);
OSStatus TECFlushText(TECObjectRef converter, TextPtr outputBuf, ByteCount outputLen, ByteCount *outputProduced);

#endif /* PORTABLE_TEXT_ENCODING_H */
