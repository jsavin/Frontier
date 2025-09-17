#ifndef OSINCLUDES_APPLE_H
#define OSINCLUDES_APPLE_H

#define OTDEBUG 1

#ifdef FRONTIER_FLAT_HEADERS
#include <Carbon.h>
#include <ApplicationServices.h>
#else
#include <Carbon/Carbon.h>
#include <ApplicationServices/ApplicationServices.h>
#endif

#define ELASTERRNO ELAST

struct StandardFileReply {
  Boolean             sfGood;
  Boolean             sfReplacing;
  OSType              sfType;
  FSSpec              sfFile;
  ScriptCode          sfScript;
  short               sfFlags;
  Boolean             sfIsFolder;
  Boolean             sfIsVolume;
  long                sfReserved1;
  short               sfReserved2;
};
typedef struct StandardFileReply StandardFileReply;

struct EntityName {
  Str32Field          objStr;
  Str32Field          typeStr;
  Str32Field          zoneStr;
};
typedef struct EntityName EntityName;

typedef SInt16 PPCLocationKind;
typedef SInt16 PPCPortKinds;
enum { ppcByCreatorAndType = 1, ppcByString = 2 };
typedef SInt16 PPCXTIAddressType;
struct PPCXTIAddress {
  PPCXTIAddressType   fAddressType;
  UInt8               fAddress[96];
};
typedef struct PPCXTIAddress            PPCXTIAddress;
typedef PPCXTIAddress *                 PPCXTIAddressPtr;
struct PPCAddrRec {
  UInt8               Reserved[3];
  UInt8               xtiAddrLen;
  PPCXTIAddress       xtiAddr;
};
typedef struct PPCAddrRec PPCAddrRec;
typedef PPCAddrRec *    PPCAddrRecPtr;
struct LocationNameRec {
  PPCLocationKind     locationKindSelector;
  union { EntityName nbpEntity; Str32 nbpType; PPCAddrRec xtiType; } u;
};
typedef struct LocationNameRec LocationNameRec;
typedef LocationNameRec * LocationNamePtr;
struct PPCPortRec {
  ScriptCode          nameScript;
  Str32Field          name;
  PPCPortKinds        portKindSelector;
  union {
    Str32 portTypeStr;
    struct { OSType portCreator; OSType portType; } port;
  } u;
};
typedef struct PPCPortRec PPCPortRec;
typedef PPCPortRec * PPCPortPtr;
struct PortInfoRec {
  SInt8    filler1;
  Boolean  authRequired;
  PPCPortRec name;
};
typedef struct PortInfoRec PortInfoRec;
typedef PortInfoRec * PortInfoPtr;
struct TargetID {
  long          sessionID;
  PPCPortRec    name;
  LocationNameRec location;
  PPCPortRec    recvrName;
};
typedef struct TargetID TargetID;
typedef TargetID * TargetIDPtr;
typedef TargetIDPtr * TargetIDHandle;
typedef TargetIDHandle TargetIDHdl;
typedef TargetID SenderID;
typedef SenderID * SenderIDPtr;

#define topLeft(r)  (((Point *) &(r))[0])
#define botRight(r) (((Point *) &(r))[1])

#endif /* OSINCLUDES_APPLE_H */
