#include "frontier.h"
#include "langipc.h"

#ifdef FRONTIER_HEADLESS

/*
 * Minimal headless replacement for the classic IPC layer. We keep the
 * exported surface so runtime callers link, but the routines simply report
 * that cross-process automation is unavailable in headless mode.
 */

typrocessid langipcself = {0, 0};

static boolean __attribute__((unused)) unsupported(boolean defaultValue) {
	(void)defaultValue;
	return false;
}

boolean langipcerrorroutine(bigstring bs, ptrvoid refcon) {
	(void)bs;
	(void)refcon;
	return false;
}

boolean setdescriptorvalue(AEDesc desc, tyvaluerecord *val) {
	(void)desc;
	(void)val;
	return false;
}

boolean valuetodescriptor(tyvaluerecord *val, AEDesc *desc) {
	(void)val;
	if (desc) {
		desc->descriptorType = typeNull;
		desc->dataHandle = NULL;
	}
	return false;
}

boolean langipcfindapptable(OSType id, boolean flcreate, hdlhashtable *htable, bigstring bsname) {
	(void)id;
	(void)flcreate;
	if (htable)
		*htable = NULL;
	if (bsname)
		setstringlength(bsname, 0);
	return false;
}

boolean langipcbrowsenetwork(hdltreenode hp1, tyvaluerecord *vres) {
	(void)hp1;
	if (vres)
		initvalue(vres, novaluetype);
	return false;
}

boolean langipcsettimeout(hdltreenode hp1, tyvaluerecord *vres) {
	(void)hp1;
	if (vres)
		initvalue(vres, novaluetype);
	return false;
}

boolean langipcsettransactionid(hdltreenode hp1, tyvaluerecord *vres) {
	(void)hp1;
	if (vres)
		initvalue(vres, novaluetype);
	return false;
}

boolean langipcsetinteractionlevel(hdltreenode hp1, tyvaluerecord *vres) {
	(void)hp1;
	if (vres)
		initvalue(vres, novaluetype);
	return false;
}

boolean langipcgeteventattr(hdltreenode hp1, tyvaluerecord *vres) {
	(void)hp1;
	if (vres)
		initvalue(vres, novaluetype);
	return false;
}

boolean langipccoerceappleitem(hdltreenode hp1, tyvaluerecord *vres) {
	(void)hp1;
	if (vres)
		initvalue(vres, novaluetype);
	return false;
}

boolean langipcapprunning(hdltreenode hp1, tyvaluerecord *vres) {
	(void)hp1;
	if (vres)
		setbooleanvalue(false, vres);
	return true;
}

boolean langipcgetaddressvalue(hdltreenode hp1, tyvaluerecord *vres) {
	(void)hp1;
	if (vres)
		initvalue(vres, novaluetype);
	return false;
}

void binarytodesc(Handle h, AEDesc *desc) {
	(void)h;
	if (desc) {
		desc->descriptorType = typeNull;
		desc->dataHandle = NULL;
	}
}

boolean langipcconvertoplist(const tyvaluerecord *vlist, AEDesc *desc) {
	(void)vlist;
	if (desc) {
		desc->descriptorType = typeNull;
		desc->dataHandle = NULL;
	}
	return false;
}

boolean langipcconvertaelist(const AEDesc *list, tyvaluerecord *vlist) {
	(void)list;
	if (vlist)
		initvalue(vlist, novaluetype);
	return false;
}

boolean langipcputlistitem(hdltreenode hp1, tyvaluerecord *vres) {
	(void)hp1;
	if (vres)
		initvalue(vres, novaluetype);
	return false;
}

boolean langipcgetlistitem(hdltreenode hp1, tyvaluerecord *vres) {
	(void)hp1;
	if (vres)
		initvalue(vres, novaluetype);
	return false;
}

boolean langipccountlistitems(hdltreenode hp1, tyvaluerecord *vres) {
	(void)hp1;
	if (vres)
		setlongvalue(0, vres);
	return true;
}

boolean newselfaddressedevent(AEEventID id, AppleEvent *event) {
	(void)id;
	if (event) {
		event->descriptorType = typeNull;
		event->dataHandle = NULL;
	}
	return false;
}

boolean langipcmessage(hdltreenode hp1, tyipcmessageflags flags, tyvaluerecord *vres) {
	(void)hp1;
	(void)flags;
	if (vres)
		initvalue(vres, novaluetype);
	return false;
}

boolean langipccomplexmessage(hdltreenode hp1, tyvaluerecord *vres) {
	return langipcmessage(hp1, normalmsg, vres);
}

boolean langipctablemessage(hdltreenode hp1, tyvaluerecord *vres) {
	return langipcmessage(hp1, normalmsg, vres);
}

boolean langipcbuildsubroutineevent(AppleEvent *event, bigstring bs, hdltreenode hp1) {
	(void)event;
	(void)bs;
	(void)hp1;
	return false;
}

boolean langipchandlercall(hdltreenode htree, bigstring bsverb, hdltreenode hparam1, tyvaluerecord *vres) {
	(void)htree;
	(void)bsverb;
	(void)hparam1;
	if (vres)
		initvalue(vres, novaluetype);
	return false;
}

boolean langipckernelfunction(hdlhashtable htable, bigstring bsverb, hdltreenode hparam1, tyvaluerecord *vres) {
	(void)htable;
	(void)bsverb;
	(void)hparam1;
	if (vres)
		initvalue(vres, novaluetype);
	return false;
}

boolean langipcshowmenunode(long hnode) {
	(void)hnode;
	return false;
}

boolean langipcnoop(void) {
	return true;
}

boolean langipcstart(void) {
	return false;
}

void langipcshutdown(void) {
}

boolean langipcinit(void) {
	return true;
}

/* Menu/desktop helpers referenced from langipcmenus */
boolean langipcgetmenuhandle(OSType id, short ix, Handle *hmenu) {
	(void)id;
	(void)ix;
	if (hmenu)
		*hmenu = NULL;
	return false;
}

boolean langipcgetitemlangtext(long hmenu, short item, short part, Handle *htext, long *typecode) {
	(void)hmenu;
	(void)item;
	(void)part;
	if (htext)
		*htext = NULL;
	if (typecode)
		*typecode = 0;
	return false;
}

boolean langipccheckformulas(long hmenu) {
	(void)hmenu;
	return false;
}

void langipcdisposemenuarray(long hmenu, Handle harray) {
	(void)hmenu;
	(void)harray;
}

boolean langipcrunitem(long hmenu, short item, short part, long *result) {
	(void)hmenu;
	(void)item;
	(void)part;
	if (result)
		*result = 0;
	return false;
}

boolean langipckillscript(long id) {
	(void)id;
	return false;
}

boolean langipcgetmenuarray(long hmenu, short depth, boolean fllocal, Handle *harray) {
	(void)hmenu;
	(void)depth;
	(void)fllocal;
	if (harray)
		*harray = NULL;
	return false;
}

boolean langipcmenustartup(void) {
	return false;
}

boolean langipcmenushutdown(void) {
	return false;
}

boolean langipcsymbolchanged(hdlhashtable htable, const bigstring bs, boolean flvalue) {
	(void)htable;
	(void)bs;
	(void)flvalue;
	return false;
}

boolean langipcsymbolinserted(hdlhashtable htable, const bigstring bs) {
	(void)htable;
	(void)bs;
	return false;
}

boolean langipcsymboldeleted(hdlhashtable htable, const bigstring bs) {
	(void)htable;
	(void)bs;
	return false;
}

boolean langipcmenuinit(void) {
	return true;
}

#endif /* FRONTIER_HEADLESS */
