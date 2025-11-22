/* 2025-10-31 Codex: Provide portable AppleEvent stubs so headless builds can
 *  link without Carbon. Semantics mirror the legacy fallbacks: most calls
 *  return errAEEventNotHandled, with AEProcessAppleEvent/AEDisposeDesc keeping
 *  their historical success responses. */

#include "../Common/headers/frontier.h"

#include <string.h>

static void appleevent_clear_desc(AEDesc *desc) {
    if (!desc)
        return;
    desc->descriptorType = typeNull;
    desc->dataHandle = NULL;
}

OSErr AECreateDesc(DescType typeCode, const void *dataPtr, Size dataSize, AEDesc *result) {
    (void)dataPtr;
    (void)dataSize;
    if (!result)
        return paramErr;
    result->descriptorType = typeCode;
    result->dataHandle = NULL;
    return errAEEventNotHandled;
}

OSErr AEDuplicateDesc(const AEDesc *src, AEDesc *dst) {
    if (!src || !dst)
        return paramErr;
    *dst = *src;
    return noErr;
}

OSStatus AECoerceDesc(const AEDesc *desc, DescType typeCode, AEDesc *result) {
    if (!result)
        return paramErr;
    result->descriptorType = typeCode;
    result->dataHandle = desc ? desc->dataHandle : NULL;
    return noErr;
}

OSStatus AEProcessAppleEvent(const EventRecord *event) {
    (void)event;
    return noErr;
}

OSErr AECreateList(const void *factoringPtr, Size elementSize, Boolean isRecord, AEDesc *resultList) {
    (void)factoringPtr;
    (void)elementSize;
    (void)isRecord;
    if (!resultList)
        return paramErr;
    appleevent_clear_desc(resultList);
    resultList->descriptorType = isRecord ? typeAERecord : typeAEList;
    return errAEEventNotHandled;
}

OSErr AECreateAppleEvent(AEEventClass theAEEventClass, AEEventID theAEEventID, const AEAddressDesc *target, AEReturnID returnID, AESendMode transaction, AppleEvent *result) {
    (void)theAEEventClass;
    (void)theAEEventID;
    (void)target;
    (void)returnID;
    (void)transaction;
    if (!result)
        return paramErr;
    appleevent_clear_desc(result);
    return errAEEventNotHandled;
}

OSErr AEPutDesc(AEDesc *theAERecord, long index, const AEDesc *theAEDesc) {
    (void)theAERecord;
    (void)index;
    (void)theAEDesc;
    return errAEEventNotHandled;
}

OSErr AEGetNthDesc(const AEDescList *list, long index, DescType desiredType, AEKeyword *theKeyword, AEDesc *result) {
    (void)list;
    (void)index;
    (void)desiredType;
    if (theKeyword)
        *theKeyword = typeNull;
    appleevent_clear_desc(result);
    return errAEEventNotHandled;
}

OSErr AEGetKeyPtr(const AppleEvent *event, AEKeyword keyword, DescType desiredType, AEKeyword *actualType, void *dataPtr, Size maximumSize, Size *actualSize) {
    (void)event;
    (void)keyword;
    (void)desiredType;
    (void)dataPtr;
    (void)maximumSize;
    if (actualType)
        *actualType = typeNull;
    if (actualSize)
        *actualSize = 0;
    return errAEDescNotFound;
}

OSErr AEGetKeyDesc(const AppleEvent *event, AEKeyword keyword, DescType desiredType, AEDesc *result) {
    (void)event;
    (void)keyword;
    (void)desiredType;
    appleevent_clear_desc(result);
    return errAEEventNotHandled;
}

OSErr AEGetParamDesc(const AppleEvent *event, AEKeyword keyword, DescType desiredType, AEDesc *result) {
    return AEGetKeyDesc(event, keyword, desiredType, result);
}

OSErr AEInstallEventHandler(AEEventClass theAEEventClass, AEEventID theAEEventID, AEEventHandlerUPP handler, long handlerRefcon, Boolean isSysHandler) {
    (void)theAEEventClass;
    (void)theAEEventID;
    (void)handler;
    (void)handlerRefcon;
    (void)isSysHandler;
    return errAEEventNotHandled;
}

OSErr AESend(const AppleEvent *event, AppleEvent *reply, AESendMode sendMode, AESendPriority sendPriority, long timeoutInTicks, AEIdleUPP idleProc, AEFilterUPP filterProc) {
    (void)event;
    (void)reply;
    (void)sendMode;
    (void)sendPriority;
    (void)timeoutInTicks;
    (void)idleProc;
    (void)filterProc;
    return errAEEventNotHandled;
}

OSStatus AEDisposeDesc(AEDesc *desc) {
    appleevent_clear_desc(desc);
    return noErr;
}

OSErr CreateObjSpecifier(DescType desiredClass, const AEDesc *container, DescType keyForm, const AEDesc *keyData, Boolean createIfNeeded, AEDesc *result) {
    (void)desiredClass;
    (void)container;
    (void)keyForm;
    (void)keyData;
    (void)createIfNeeded;
    if (!result)
        return paramErr;
    result->descriptorType = cObjectSpecifier;
    result->dataHandle = NULL;
    return errAEEventNotHandled;
}

OSErr CreateCompDescriptor(DescType operatorKeyword, const AEDesc *object1, const AEDesc *object2, Boolean disposeInputs, AEDesc *result) {
    (void)operatorKeyword;
    (void)object1;
    (void)object2;
    (void)disposeInputs;
    appleevent_clear_desc(result);
    return errAEEventNotHandled;
}

OSErr CreateLogicalDescriptor(const AEDescList *theList, DescType operatorKeyword, Boolean disposeInputs, AEDesc *result) {
    (void)theList;
    (void)operatorKeyword;
    (void)disposeInputs;
    appleevent_clear_desc(result);
    return errAEEventNotHandled;
}

OSErr CreateRangeDescriptor(const AEDesc *startDescriptor, const AEDesc *stopDescriptor, Boolean disposeInputs, AEDesc *result) {
    (void)startDescriptor;
    (void)stopDescriptor;
    (void)disposeInputs;
    appleevent_clear_desc(result);
    return errAEEventNotHandled;
}

OSErr AECountItems(const AEDescList *list, long *count) {
    (void)list;
    if (count)
        *count = 0;
    return errAEEventNotHandled;
}
