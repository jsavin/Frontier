#include "../../Common/headers/memory.h"
#include "portable_handles.h"
#include <string.h>

boolean newclearhandle(long size, Handle *out_handle) {
    if (out_handle == NULL)
        return false;
    if (size < 0)
        size = 0;
    Handle h = frontierAlloc(size);
    if (h == NULL)
        return false;
    if (size > 0)
        memset(*h, 0, (size_t)size);
    *out_handle = h;
    return true;
}
