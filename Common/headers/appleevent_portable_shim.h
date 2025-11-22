#ifndef APPLEEVENT_PORTABLE_SHIM_H
#define APPLEEVENT_PORTABLE_SHIM_H

/*
    2025-10-31 Codex: Made the AppleEvent shim tolerant of frontier.h include
    order. The shim now assumes frontier.h (and therefore frontier_compat.h)
    is included first, and only defines the classic error codes it relies on.

    2025-10-31 Codex: Inlined the minimal classic typedefs so the shim no longer
    depends on osincludes_portable.h include ordering.
*/

#include <stddef.h>
#include <stdint.h>

#if !defined(OSINCLUDES_PORTABLE_H) && !defined(APPLEEVENT_PORTABLE_BASE_TYPES_DEFINED)
#define APPLEEVENT_PORTABLE_BASE_TYPES_DEFINED 1

typedef unsigned char Boolean;
typedef uint16_t UInt16;
typedef uint32_t UInt32;
typedef unsigned char *Ptr;
typedef Ptr *Handle;
typedef int32_t OSErr;
typedef int32_t OSStatus;
typedef uint32_t OSType;
typedef OSType DescType;
typedef long Size;

typedef struct Point {
    int16_t v;
    int16_t h;
} Point;

typedef struct EventRecord {
    UInt16 what;
    UInt32 message;
    UInt32 when;
    Point where;
    UInt16 modifiers;
} EventRecord;

#endif /* base types */

#define APPLEEVENT_PORTABLE_PROVIDES_IMPLS 1

#ifndef noErr
#define noErr 0
#endif

#ifndef paramErr
#define paramErr (-50)
#endif

#ifndef errAEEventNotHandled
#define errAEEventNotHandled (-1708)
#endif

#ifndef errAEDescNotFound
#define errAEDescNotFound (-1711)
#endif

#ifndef errAECoercionFail
#define errAECoercionFail (-1700)
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

#ifndef AEAddressDesc
typedef AEDesc AEAddressDesc;
#endif

#ifndef AEEventID
typedef OSType AEEventID;
#endif

#ifndef AEEventClass
typedef OSType AEEventClass;
#endif

#ifndef AEKeyword
typedef OSType AEKeyword;
#endif

#ifndef AEReturnID
typedef short AEReturnID;
#endif

#ifndef AESendMode
typedef unsigned long AESendMode;
#endif

#ifndef AESendPriority
typedef unsigned long AESendPriority;
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

#ifndef AEDescList
typedef AEDesc AEDescList;
#endif

#ifndef typeAEList
#define typeAEList (DescType)0x6c697374 /* 'list' */
#endif

#ifndef typeAERecord
#define typeAERecord (DescType)0x7265636f /* 'reco' */
#endif

#ifndef typeAlias
#define typeAlias (DescType)0x616c6973 /* 'alis' */
#endif

#ifndef typeQDPoint
#define typeQDPoint (DescType)0x71647074 /* 'qdpt' */
#endif

#ifndef typeNull
#define typeNull (DescType)0
#endif

#ifndef typeChar
#define typeChar (DescType)0x54455854 /* 'TEXT' */
#endif

#ifndef cObjectSpecifier
#define cObjectSpecifier (DescType)0x6f626a20 /* 'obj ' */
#endif

#ifndef cApplication
#define cApplication (DescType)0x63617070 /* 'capp' */
#endif

#ifndef cFile
#define cFile (DescType)0x6366696c /* 'cfil' */
#endif

#ifndef cProperty
#define cProperty (DescType)0x70726f70 /* 'prop' */
#endif

#ifndef typeQDRectangle
#define typeQDRectangle (DescType)0x71647274 /* 'qdrt' */
#endif

#ifndef typeEnumerated
#define typeEnumerated (DescType)0x656e756d /* 'enum' */
#endif

#ifndef typeType
#define typeType (DescType)0x74797065 /* 'type' */
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

#ifndef typeInsertionLoc
#define typeInsertionLoc (DescType)0x696e736c /* 'insl' */
#endif

#ifndef typeObjectSpecifier
#define typeObjectSpecifier cObjectSpecifier
#endif

#ifndef typeProperty
#define typeProperty (DescType)0x70726f70 /* 'prop' */
#endif

#ifndef typeCurrentContainer
#define typeCurrentContainer (DescType)0x63636f6e /* 'ccon' */
#endif

#ifndef AERecord
typedef AppleEvent AERecord;
#endif

#ifndef keyAEKeyForm
#define keyAEKeyForm (AEKeyword)0x666f726d /* 'form' */
#endif

#ifndef keyAERequestedType
#define keyAERequestedType (AEKeyword)0x72747970 /* 'rtyp' */
#endif

#ifndef keyAEDesiredClass
#define keyAEDesiredClass (AEKeyword)0x77616e74 /* 'want' */
#endif

#ifndef keyAEObjectClass
#define keyAEObjectClass (AEKeyword)0x6f626a63 /* 'objc' */
#endif

#ifndef keyAEPosition
#define keyAEPosition (AEKeyword)0x706f736e /* 'posn' */
#endif

#ifndef keyAEProperty
#define keyAEProperty (AEKeyword)0x70726f70 /* 'prop' */
#endif

#ifndef keyAEContainer
#define keyAEContainer (AEKeyword)0x66726f6d /* 'from' */
#endif

#ifndef keyAEKeyData
#define keyAEKeyData (AEKeyword)0x73656c64 /* 'seld' */
#endif

#ifndef formName
#define formName (AEKeyword)0x6e616d65 /* 'name' */
#endif

#ifndef formAbsolutePosition
#define formAbsolutePosition (AEKeyword)0x696e6478 /* 'indx' */
#endif

#ifndef formRelativePosition
#define formRelativePosition (AEKeyword)0x72656c65 /* 'rele' */
#endif

#ifndef formTest
#define formTest (AEKeyword)0x74657374 /* 'test' */
#endif

#ifndef formRange
#define formRange (AEKeyword)0x72616e67 /* 'rang' */
#endif

#ifndef formPropertyID
#define formPropertyID (AEKeyword)0x70726f70 /* 'prop' */
#endif

#ifndef formUserPropertyID
#define formUserPropertyID (AEKeyword)0x75736572 /* 'user' */
#endif

#ifdef __cplusplus
extern "C" {
#endif

/* Portable function prototypes implemented in portable/appleevent_portable.c. */
OSStatus AEProcessAppleEvent(const EventRecord *event);
OSErr AECreateDesc(DescType typeCode, const void *dataPtr, Size dataSize, AEDesc *result);
OSErr AECreateList(const void *factoringPtr, Size elementSize, Boolean isRecord, AEDesc *resultList);
OSErr AECreateAppleEvent(AEEventClass theAEEventClass, AEEventID theAEEventID, const AEAddressDesc *target, AEReturnID returnID, AESendMode transaction, AppleEvent *result);
OSErr AEPutDesc(AEDesc *theAERecord, AEKeyword theAEKeyword, const AEDesc *theAEDesc);
OSErr AEGetKeyDesc(const AppleEvent *event, AEKeyword theAEKeyword, DescType desiredType, AEDesc *result);
OSStatus AECoerceDesc(const AEDesc *desc, DescType typeCode, AEDesc *result);
OSStatus AEDisposeDesc(AEDesc *desc);
OSStatus AEGetKeyPtr(const AppleEvent *event, AEKeyword keyword, DescType desiredType, AEKeyword *actualType, void *dataPtr, Size maximumSize, Size *actualSize);
OSErr AEGetNthDesc(const AEDescList *theAEDescList, long index, DescType desiredType, AEKeyword *theAEKeyword, AEDesc *result);
OSErr AEGetParamDesc(const AppleEvent *event, AEKeyword keyword, DescType desiredType, AEDesc *result);
OSErr AEInstallEventHandler(AEEventClass theAEEventClass, AEEventID theAEEventID, AEEventHandlerUPP handler, long handlerRefcon, Boolean isSysHandler);
OSErr AESend(const AppleEvent *event, AppleEvent *reply, AESendMode sendMode, AESendPriority sendPriority, long timeoutInTicks, AEIdleUPP idleProc, AEFilterUPP filterProc);

#ifdef __cplusplus
} /* extern "C" */
#endif

#if !defined(APPLEEVENT_PORTABLE_PROVIDES_IMPLS)

/* Stubs */
static inline OSStatus AEProcessAppleEvent(const EventRecord *event) {
    (void)event;
    return noErr;
}

static inline OSErr AECreateDesc(DescType typeCode, const void *dataPtr, Size dataSize, AEDesc *result) {
    (void)typeCode; (void)dataPtr; (void)dataSize;
    if (!result)
        return paramErr;
    result->descriptorType = typeNull;
    result->dataHandle = NULL;
    return errAEEventNotHandled;
}

static inline OSErr AECreateList(const void *factoringPtr, Size elementSize, Boolean isRecord, AEDesc *resultList) {
    (void)factoringPtr; (void)elementSize; (void)isRecord;
    if (!resultList)
        return paramErr;
    resultList->descriptorType = typeNull;
    resultList->dataHandle = NULL;
    return errAEEventNotHandled;
}

static inline OSErr AECreateAppleEvent(AEEventClass theAEEventClass, AEEventID theAEEventID, const AEAddressDesc *target, AEReturnID returnID, AESendMode transaction, AppleEvent *result) {
    (void)theAEEventClass; (void)theAEEventID; (void)target; (void)returnID; (void)transaction;
    if (!result)
        return paramErr;
    result->descriptorType = typeNull;
    result->dataHandle = NULL;
    return errAEEventNotHandled;
}

static inline OSErr AEPutDesc(AEDesc *theAERecord, AEKeyword theAEKeyword, const AEDesc *theAEDesc) {
    (void)theAERecord; (void)theAEKeyword; (void)theAEDesc;
    return errAEEventNotHandled;
}

static inline OSErr AEGetKeyDesc(const AppleEvent *event, AEKeyword theAEKeyword, DescType desiredType, AEDesc *result) {
    (void)event; (void)theAEKeyword; (void)desiredType;
    if (!result)
        return paramErr;
    result->descriptorType = typeNull;
    result->dataHandle = NULL;
    return errAEEventNotHandled;
}

static inline OSStatus AECoerceDesc(const AEDesc *desc, DescType typeCode, AEDesc *result) {
    if (!result)
        return paramErr;
    result->descriptorType = typeCode;
    result->dataHandle = (desc) ? desc->dataHandle : NULL;
    return noErr;
}

static inline OSStatus AEDisposeDesc(AEDesc *desc) {
    if (!desc)
        return paramErr;
    desc->descriptorType = typeNull;
    desc->dataHandle = NULL;
    return noErr;
}

static inline OSStatus AEGetKeyPtr(const AppleEvent *event, AEKeyword keyword, DescType desiredType, AEKeyword *actualType, void *dataPtr, Size maximumSize, Size *actualSize) {
    (void)event; (void)keyword; (void)desiredType; (void)dataPtr; (void)maximumSize;
    if (actualType)
        *actualType = typeNull;
    if (actualSize)
        *actualSize = 0;
    return errAEDescNotFound;
}

static inline OSErr AEGetNthDesc(const AEDescList *theAEDescList, long index, DescType desiredType, AEKeyword *theAEKeyword, AEDesc *result) {
    (void)theAEDescList; (void)index; (void)desiredType; (void)theAEKeyword;
    if (!result)
        return paramErr;
    result->descriptorType = typeNull;
    result->dataHandle = NULL;
    return errAEEventNotHandled;
}

static inline OSErr AEGetParamDesc(const AppleEvent *event, AEKeyword keyword, DescType desiredType, AEDesc *result) {
    (void)event; (void)keyword; (void)desiredType;
    if (!result)
        return paramErr;
    result->descriptorType = typeNull;
    result->dataHandle = NULL;
    return errAEEventNotHandled;
}

static inline OSErr AEInstallEventHandler(AEEventClass theAEEventClass, AEEventID theAEEventID, AEEventHandlerUPP handler, long handlerRefcon, Boolean isSysHandler) {
    (void)theAEEventClass; (void)theAEEventID; (void)handler; (void)handlerRefcon; (void)isSysHandler;
    return errAEEventNotHandled;
}

static inline OSErr AESend(const AppleEvent *event, AppleEvent *reply, AESendMode sendMode, AESendPriority sendPriority, long timeoutInTicks, AEIdleUPP idleProc, AEFilterUPP filterProc) {
    (void)event; (void)reply; (void)sendMode; (void)sendPriority; (void)timeoutInTicks; (void)idleProc; (void)filterProc;
    return errAEEventNotHandled;
}

#endif /* !APPLEEVENT_PORTABLE_PROVIDES_IMPLS */

#endif /* APPLEEVENT_PORTABLE_SHIM_H */
