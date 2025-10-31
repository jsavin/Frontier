#ifndef APPLEEVENT_PORTABLE_SHIM_H
#define APPLEEVENT_PORTABLE_SHIM_H

#include "osincludes_portable.h"

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

#ifndef AEKeyword
typedef OSType AEKeyword;
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

#ifndef typeAEList
#define typeAEList (DescType)0x6c697374 /* 'list' */
#endif

#ifndef typeAERecord
#define typeAERecord (DescType)0x7265636f /* 'reco' */
#endif

#ifndef typeAlias
#define typeAlias (DescType)0x616c6973 /* 'alis' */
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

#ifndef keyAEKeyForm
#define keyAEKeyForm (AEKeyword)0x666f726d /* 'form' */
#endif

#ifndef keyAEContainer
#define keyAEContainer (AEKeyword)0x66726f6d /* 'from' */
#endif

#ifndef keyAEKeyData
#define keyAEKeyData (AEKeyword)0x73656c64 /* 'seld' */
#endif

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

#endif /* APPLEEVENT_PORTABLE_SHIM_H */
#ifndef AEEventClass
typedef OSType AEEventClass;
#endif

#ifndef AEReturnID
typedef short AEReturnID;
#endif
