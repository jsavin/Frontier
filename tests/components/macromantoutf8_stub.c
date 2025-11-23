#include "../../Common/headers/strings.h"
#include <string.h>

boolean macromantoutf8(Handle h, Handle hresult) {
    if (h == NULL || hresult == NULL)
        return false;

    long len = gethandlesize(h);
    if (!sethandlesize(hresult, len))
        return false;

    if (len > 0)
        memcpy(*hresult, *h, (size_t)len);

    return true;
}
