#ifndef TABLEEXTERNAL_COMMON_H
#define TABLEEXTERNAL_COMMON_H

#include "tableverbs.h"

#ifndef TABLE_HEADER_RESERVED_BYTES
#define TABLE_HEADER_RESERVED_BYTES 1024
#endif

#ifndef TABLE_HEADER_RESERVED_VERSION
#define TABLE_HEADER_RESERVED_VERSION 4
#endif

boolean tableverbinmemory_common(hdlexternalvariable hvariable, hdlhashnode hnode);

#endif /* TABLEEXTERNAL_COMMON_H */
