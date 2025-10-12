/*
 * headless_stubs.h - Declarations for Carbon-era APIs stubbed in headless builds.
 */

#ifndef FRONTIER_HEADLESS_STUBS_H
#define FRONTIER_HEADLESS_STUBS_H

#if defined(FRONTIER_HEADLESS)

#include <stddef.h>
#include "osincludes_portable.h"

#include <stdint.h>

#ifndef Component
typedef void *Component;
#endif

#ifndef ComponentInstance
typedef void *ComponentInstance;
#endif

#ifndef ByteCount
typedef size_t ByteCount;
#endif

#ifndef TextPtr
typedef unsigned char *TextPtr;
#endif

#ifndef ConstTextPtr
typedef const unsigned char *ConstTextPtr;
#endif

#ifndef DescType
typedef OSType DescType;
#endif

#ifndef AEDesc
typedef struct AEDesc {
    DescType descriptorType;
    Handle dataHandle;
} AEDesc;
#endif

#ifndef AppleEvent
typedef AEDesc AppleEvent;
#endif

#ifndef AEKeyword
typedef uint32_t AEKeyword;
#endif

#ifndef AEEventClass
typedef uint32_t AEEventClass;
#endif

#ifndef AEEventID
typedef OSType AEEventID;
#endif

#ifndef AEAddressDesc
typedef AEDesc AEAddressDesc;
#endif

#ifndef AESendMode
typedef uint32_t AESendMode;
#endif

#ifndef AESendPriority
typedef uint32_t AESendPriority;
#endif

#ifndef AEIdleUPP
typedef void *AEIdleUPP;
#endif

#ifndef AEFilterUPP
typedef void *AEFilterUPP;
#endif

#ifndef AEEventHandlerUPP
typedef OSErr (*AEEventHandlerUPP)(const AppleEvent *, AppleEvent *, long);
#endif

#ifndef ThreadSwitchUPP
typedef void *ThreadSwitchUPP;
#endif

#ifndef ThreadTerminationUPP
typedef void *ThreadTerminationUPP;
#endif

#ifndef ThreadEntryUPP
typedef void *ThreadEntryUPP;
#endif

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

#ifndef verUS
#define verUS 0U
#endif

#ifndef typeAEList
#define typeAEList (DescType)0x6c697374 /* 'list' */
#endif

#ifndef typeAERecord
#define typeAERecord (DescType)0x7265636f /* 'reco' */
#endif

#ifndef typeBoolean
#define typeBoolean (DescType)0x626f6f6c /* 'bool' */
#endif

#ifndef typeShortInteger
#define typeShortInteger (DescType)0x73686f72 /* 'shor' */
#endif

#ifndef typeLongInteger
#define typeLongInteger (DescType)0x6c6f6e67 /* 'long' */
#endif

#ifndef typeQDPoint
#define typeQDPoint (DescType)0x51447074 /* 'QDpt' */
#endif

#ifndef typeQDRectangle
#define typeQDRectangle (DescType)0x71647274 /* 'qdrt' */
#endif

#ifndef typeEnumerated
#define typeEnumerated (DescType)0x656e756d /* 'enum' */
#endif

#ifndef typeAlias
#define typeAlias (DescType)0x616c6973 /* 'alis' */
#endif

#ifndef typeShortFloat
#define typeShortFloat (DescType)0x73696e67 /* 'sing' */
#endif

#ifndef typeExtended
#define typeExtended (DescType)0x65787465 /* 'exte' */
#endif

#ifndef typeRGBColor
#define typeRGBColor (DescType)0x63475242 /* 'cRGB' */
#endif

#ifndef typeFixed
#define typeFixed (DescType)0x66697864 /* 'fixd' */
#endif

#ifndef typeFSS
#define typeFSS (DescType)0x66737320 /* 'fss ' */
#endif

#ifndef cObjectSpecifier
#define cObjectSpecifier (DescType)0x6f626a20 /* 'obj ' */
#endif

#ifndef typeNull
#define typeNull (DescType)0
#endif

#ifndef typeChar
#define typeChar (DescType)0x54455854 /* 'TEXT' */
#endif

#ifndef errAECoercionFail
#define errAECoercionFail (-1700)
#endif

#ifndef errOSAScriptError
#define errOSAScriptError (-2700)
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

#ifndef errAEDescNotFound
#define errAEDescNotFound (-1753)
#endif

#ifndef typeInsertionLoc
#define typeInsertionLoc (DescType)0x696E736C /* 'insl' */
#endif

#ifndef typeType
#define typeType (DescType)0x74797065 /* 'type' */
#endif

#ifndef typeWildCard
#define typeWildCard (DescType)0x2a2a2a2a /* '****' */
#endif

#ifndef typeObjectSpecifier
#define typeObjectSpecifier (DescType)0x6F626A20 /* 'obj ' */
#endif

#ifndef typeCurrentContainer
#define typeCurrentContainer (DescType)0x636E746E /* 'cntn' */
#endif

#ifndef formName
#define formName (DescType)0x6e616d65 /* 'name' */
#endif

#ifndef keyAEDesiredClass
#define keyAEDesiredClass (AEKeyword)0x77616e74 /* 'want' */
#endif

#ifndef keyAEKeyForm
#define keyAEKeyForm (AEKeyword)0x666f726d /* 'form' */
#endif

#ifndef keyAEContainer
#define keyAEContainer (AEKeyword)0x66726f6d /* 'from' */
#endif

#ifndef keyAEKeyData
#define keyAEKeyData (AEKeyword)0x73656c64 /* 'seld' */
#endif

#ifndef cFile
#define cFile (DescType)0x66696c65 /* 'file' */
#endif

#ifndef cApplication
#define cApplication (DescType)0x63617070 /* 'capp' */
#endif

#ifndef cCell
#define cCell (DescType)0x63656c6c /* 'cell' */
#endif

#ifndef kAEEquals
#define kAEEquals (DescType)0x3d3d3d3d /* placeholder */
#endif

#ifndef kAEGreaterThan
#define kAEGreaterThan (DescType)0x3e3e3e3e /* '>>>>' placeholder */
#endif

#ifndef kAELessThan
#define kAELessThan (DescType)0x3c3c3c3c /* '<<<<' placeholder */
#endif

#ifndef kAEGreaterThanEquals
#define kAEGreaterThanEquals (DescType)0x3e3d3e3d /* placeholder */
#endif

#ifndef kAELessThanEquals
#define kAELessThanEquals (DescType)0x3c3d3c3d /* placeholder */
#endif

#ifndef kAEBeginsWith
#define kAEBeginsWith (DescType)0x62676e73 /* 'bgns' */
#endif

#ifndef kAEEndsWith
#define kAEEndsWith (DescType)0x656e6473 /* 'ends' */
#endif

#ifndef kAEContains
#define kAEContains (DescType)0x636f6e74 /* 'cont' */
#endif

#ifndef kAENOT
#define kAENOT (DescType)0x6e6f7420 /* 'not ' */
#endif

#ifndef kAEAND
#define kAEAND (DescType)0x616e6420 /* 'and ' */
#endif

#ifndef kAEOR
#define kAEOR (DescType)0x6f722020 /* 'or  ' */
#endif

#ifndef typeLogicalDescriptor
#define typeLogicalDescriptor (DescType)0x6c6f6764 /* 'logd' */
#endif

#ifndef typeCompDescriptor
#define typeCompDescriptor (DescType)0x636f6d70 /* 'comp' */
#endif

#ifndef typeRangeDescriptor
#define typeRangeDescriptor (DescType)0x726e6764 /* 'rngd' */
#endif

#ifndef keyAELogicalOperator
#define keyAELogicalOperator (AEKeyword)0x6c6f676f /* 'logo' */
#endif

#ifndef keyAELogicalTerms
#define keyAELogicalTerms (AEKeyword)0x7465726d /* 'term' */
#endif

#ifndef keyAECompOperator
#define keyAECompOperator (AEKeyword)0x636f6d70 /* 'comp' */
#endif

#ifndef keyAEObject1
#define keyAEObject1 (AEKeyword)0x6f626a31 /* 'obj1' */
#endif

#ifndef keyAEObject2
#define keyAEObject2 (AEKeyword)0x6f626a32 /* 'obj2' */
#endif

#ifndef keyAERangeStart
#define keyAERangeStart (AEKeyword)0x73746172 /* 'star' */
#endif

#ifndef keyAERangeStop
#define keyAERangeStop (AEKeyword)0x73746f70 /* 'stop' */
#endif

#ifndef typeObjectBeingExamined
#define typeObjectBeingExamined (DescType)0x6f626a65 /* 'obje' */
#endif

#ifndef formTest
#define formTest (DescType)0x74657374 /* 'test' */
#endif

#ifndef formRange
#define formRange (DescType)0x726e6765 /* 'rnge' */
#endif

#ifndef formPropertyID
#define formPropertyID (DescType)0x70726f70 /* 'prop' */
#endif

#ifndef formAbsolutePosition
#define formAbsolutePosition (DescType)0x706f736e /* 'posn' */
#endif

#ifndef formRelativePosition
#define formRelativePosition (DescType)0x72656c20 /* 'rel ' */
#endif

#ifndef cProperty
#define cProperty (DescType)0x70727072 /* 'prpr' */
#endif

#ifndef typeEnumeration
#define typeEnumeration (DescType)0x656e756d /* 'enum' */
#endif

#ifndef typeAbsoluteOrdinal
#define typeAbsoluteOrdinal (DescType)0x6162736f /* 'abso' */
#endif

#ifndef kAENext
#define kAENext (DescType)0x6e657874 /* 'next' */
#endif

#ifndef kAEPrevious
#define kAEPrevious (DescType)0x70726576 /* 'prev' */
#endif

#ifndef noErr
#define noErr 0
#endif

#ifndef userCanceledErr
#define userCanceledErr (-128)
#endif

#ifndef memFullErr
#define memFullErr (-108)
#endif

#ifndef paramErr
#define paramErr (-50)
#endif

#ifndef errAENoSuchObject
#define errAENoSuchObject (-1728)
#endif

#ifndef errAEEventNotHandled
#define errAEEventNotHandled (-1708)
#endif

#ifndef fnfErr
#define fnfErr (-43)
#endif

#ifndef gestaltAliasMgrAttr
#define gestaltAliasMgrAttr ((OSType)0x616c6973) /* 'alis' */
#endif

#ifndef AliasInfoType
typedef uint32_t AliasInfoType;
#endif

#ifndef asiAliasName
#define asiAliasName 0U
#endif

#ifndef asiVolumeName
#define asiVolumeName 1U
#endif

#ifndef kARMNoUI
#define kARMNoUI 0U
#endif

#ifndef UnsignedWide
typedef struct UnsignedWide {
    uint32_t hi;
    uint32_t lo;
} UnsignedWide;
#endif

#ifndef FSSpec
typedef struct FSSpec {
    short vRefNum;
    long parID;
    Str255 name;
} FSSpec;
#endif

#ifndef Size
typedef long Size;
#endif

#ifndef AERecord
typedef AEDesc AERecord;
#endif

#ifndef TargetID
typedef struct TargetID {
    ProcessSerialNumber psn;
} TargetID;
#endif

#ifndef AEDescList
typedef AEDesc AEDescList;
#endif

#ifndef FSAliasInfoBitmap
typedef UInt32 FSAliasInfoBitmap;
#endif

#ifndef kFSAliasInfoNone
#define kFSAliasInfoNone 0U
#endif

#ifndef CFStringRef
typedef void *CFStringRef;
#endif

#ifndef CFBundleRef
typedef void *CFBundleRef;
#endif

OSStatus TECCountAvailableTextEncodings(ItemCount *count);
OSStatus TECGetAvailableTextEncodings(TextEncoding encodings[], ItemCount maxCount, ItemCount *actualCount);
OSStatus TECGetTextEncodingFromInternetName(TextEncoding *outEncoding, const unsigned char *name);
OSStatus TECGetTextEncodingInfo(TextEncoding encoding, TextEncodingBase *base, TextEncodingVariant *variant, TextEncodingFormat *format);
OSStatus TECGetTextEncodingInternetName(TextEncoding encoding, unsigned char *name);
OSStatus GetTextEncodingName(TextEncoding encoding, TextEncodingNameSelector selector, RegionCode region, TextEncoding referenceEncoding, ItemCount maxLen, unsigned long *actualLen, RegionCode *outRegion, TextEncoding *outEncoding, unsigned char *name);
OSStatus TECCreateConverter(TECObjectRef *converter, TextEncoding inputEncoding, TextEncoding outputEncoding);
OSStatus TECDisposeConverter(TECObjectRef converter);
OSStatus TECConvertText(TECObjectRef converter, ConstTextPtr inputBuf, ByteCount inputLen, ByteCount *inputRead, TextPtr outputBuf, ByteCount outputLen, ByteCount *outputProduced);
OSStatus TECFlushText(TECObjectRef converter, TextPtr outputBuf, ByteCount outputLen, ByteCount *outputProduced);

void SysBeep(short duration);

void NumToString(long value, Str255 result);
void StringToNum(ConstStr255Param str, long *value);

Handle NewHandle(long userSize);
void DisposeHandle(Handle h);
void HLock(Handle h);
void HUnlock(Handle h);
long GetHandleSize(Handle h);
OSErr SetHandleSize(Handle h, long userSize);
long MaxBlock(void);
OSErr MemError(void);
Handle GetString(short resID);
OSStatus FSNewAlias(const void *fromFile, const FSRef *target, AliasHandle *result);
OSStatus FSNewAliasMinimal(const FSRef *target, AliasHandle *result);
OSStatus FSNewAliasMinimalUnicode(const FSRef *target, UniCharCount nameLength, const UniChar *name, AliasHandle *result, const FSRef *base);
OSStatus FSNewAliasUnicode(const void *fromFile, const FSRef *target, UniCharCount nameLength, const UniChar *name, AliasHandle *result, const FSRef *base);
OSStatus NewAliasMinimalFromFullPath(long fullPathLength, const void *fullPath, const void *zone, const void *hints, AliasHandle *alias);
OSStatus FSFollowFinderAlias(const void *fromFile, AliasHandle alias, Boolean logon, FSRef *target, Boolean *changed);
OSStatus FSUpdateAlias(const void *fromFile, const FSRef *target, AliasHandle alias, Boolean *changed);
OSStatus FSResolveAliasWithMountFlags(const void *fromFile, AliasHandle alias, FSRef *target, Boolean *changed, uint32_t mountFlags);
OSStatus FSCopyAliasInfo(AliasHandle alias, HFSUniStr255 *name, HFSUniStr255 *volumeName, void *info1, FSAliasInfoBitmap *whichInfo, void *info2);
OSErr GetAliasInfo(AliasHandle alias, AliasInfoType index, Str255 info);
OSStatus FSGetResourceForkName(HFSUniStr255 *name);
OSStatus FSOpenFork(const FSRef *ref, UniCharCount nameLength, const UniChar *name, SInt8 permissions, SInt16 *forkRef);

OSStatus AEProcessAppleEvent(const EventRecord *event);
OSStatus AECoerceDesc(const AEDesc *desc, DescType typeCode, AEDesc *result);
OSStatus AEDisposeDesc(AEDesc *desc);
OSErr AEGetKeyPtr(const AppleEvent *event, AEKeyword keyword, DescType desiredType, AEKeyword *actualType, void *dataPtr, Size maximumSize, Size *actualSize);
OSErr AEGetKeyDesc(const AppleEvent *event, AEKeyword keyword, DescType desiredType, AEDesc *result);
OSErr AEDuplicateDesc(const AEDesc *src, AEDesc *dst);
OSErr AECreateList(const void *factoringPtr, Size elementSize, Boolean isRecord, AEDesc *resultList);
OSErr AEPutDesc(AEDesc *theAERecord, long index, const AEDesc *theAEDesc);
OSErr CreateCompDescriptor(DescType operatorKeyword, const AEDesc *object1, const AEDesc *object2, Boolean disposeInputs, AEDesc *result);
OSErr CreateLogicalDescriptor(const AEDescList *theList, DescType operatorKeyword, Boolean disposeInputs, AEDesc *result);
OSErr CreateRangeDescriptor(const AEDesc *startDescriptor, const AEDesc *stopDescriptor, Boolean disposeInputs, AEDesc *result);
OSErr AECreateDesc(DescType typeCode, const void *dataPtr, Size dataSize, AEDesc *result);
OSErr CreateObjSpecifier(DescType desiredClass, const AEDesc *container, DescType keyForm, const AEDesc *keyData, Boolean createIfNeeded, AEDesc *result);
OSErr AECountItems(const AEDescList *list, long *count);
OSErr AEGetNthDesc(const AEDescList *list, long index, DescType desiredType, AEKeyword *theKeyword, AEDesc *result);

void dtox80(const double *value, extended80 *out);
double x80tod(const extended80 *value);

short FixRound(Fixed value);
Fixed FixRatio(long numer, long denom);
Fixed FixMul(Fixed a, Fixed b);
void DebugStr(const unsigned char *pascalString);
void Debugger(void);
void Microseconds(UnsignedWide *result);
UInt32 TickCount(void);
long FreeMem(void);
CGrafPtr GetWindowPort(WindowPtr window);

#ifndef kCStackBased
#define kCStackBased 0U
#endif

#ifndef RESULT_SIZE
#define RESULT_SIZE(x) 0U
#endif

#ifndef SIZE_CODE
#define SIZE_CODE(x) 0U
#endif

#ifndef STACK_ROUTINE_PARAMETER
#define STACK_ROUTINE_PARAMETER(index, size) 0U
#endif

#endif /* FRONTIER_HEADLESS */
#endif /* FRONTIER_HEADLESS_STUBS_H */
