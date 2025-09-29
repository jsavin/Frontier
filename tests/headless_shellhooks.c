#include "frontier.h"
#include "shellhooks.h"

boolean shellpushkeyboardhook(callback routine) { (void)routine; return true; }
boolean shellcallkeyboardhooks(void) { return true; }
boolean shellpushdirtyhook(callback routine) { (void)routine; return true; }
boolean shellcalldirtyhooks(void) { return true; }
boolean shellpushmenuhook(menuhookcallback routine) { (void)routine; return true; }
boolean shellcallmenuhooks(short a, short b) { (void)a; (void)b; return true; }
boolean shellpusheventhook(eventhookcallback routine) { (void)routine; return true; }
boolean shellpopeventhook(void) { return true; }
boolean shellcalleventhooks(EventRecord *e, WindowPtr w) { (void)e; (void)w; return true; }
boolean shellpusherrorhook(errorhookcallback routine) { (void)routine; return true; }
boolean shellpoperrorhook(void) { return true; }
boolean shellcallerrorhooks(bigstring bs) { (void)bs; return true; }
boolean shellpushscraphook(scraphookcallback routine) { (void)routine; return true; }
boolean shellcallscraphooks(Handle h) { (void)h; return true; }
boolean shellpushmemoryhook(memoryhookcallback routine) { (void)routine; return true; }
boolean shellcallmemoryhooks(long *value) { (void)value; return true; }
boolean shellpushfilehook(callback routine) { (void)routine; return true; }
boolean shellcallfilehooks(void) { return true; }
boolean shellpushwakeuphook(wakeuphookcallback routine) { (void)routine; return true; }
boolean shellcallwakeuphooks(hdlprocessthread thread) { (void)thread; return true; }
