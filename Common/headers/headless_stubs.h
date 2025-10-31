/*
 * headless_stubs.h - Declarations for Carbon-era APIs stubbed in headless builds.
 */

#ifndef FRONTIER_HEADLESS_STUBS_H
#define FRONTIER_HEADLESS_STUBS_H

#if defined(FRONTIER_HEADLESS)

#include <stddef.h>
#include "osincludes_portable.h"
#include <stdint.h>

#include "appleevent_portable.h"

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

#ifndef typeShortInteger
#define typeShortInteger (DescType)0x73686f72 /* 'shor' */
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

#if !defined(OS_PORTABLE_HAS_FSSPEC)
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

#if !defined(OS_PORTABLE_HAS_CFSTRING)
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

#if !defined(FRONTIER_USE_PORTABLE_HANDLES)
Handle NewHandle(long userSize);
void DisposeHandle(Handle h);
void HLock(Handle h);
void HUnlock(Handle h);
long GetHandleSize(Handle h);
OSErr SetHandleSize(Handle h, long userSize);
long MaxBlock(void);
OSErr MemError(void);
#endif
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

#if !defined(FRONTIER_USE_PORTABLE_HANDLES)
short FixRound(Fixed value);
Fixed FixRatio(long numer, long denom);
Fixed FixMul(Fixed a, Fixed b);
#endif
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
