/* 2025-11-26 Codex: Legacy reader fork for 32-bit headers (v6 and below). */

#include "frontier.h"
#include "standard.h"

#include <string.h>

#include "db_reader.h"
#include "db_format.h"

static dbaddress read_legacy_dbaddress32(const unsigned char *field) {
    return (dbaddress) db_format_read_be32(field);
}

boolean db_format_header_version(const unsigned char *rawheader, size_t raw_len, int *out_version) {
    if (rawheader == NULL || out_version == NULL || raw_len < 2)
        return false;
    *out_version = rawheader[1];
    return true;
}

boolean db_read_legacy(const unsigned char *rawheader, size_t raw_len, tydatabaserecord *out) {
    int header_version = 0;
    int i;
    size_t needed = sizeof (tydatabaserecord);

    if ((rawheader == NULL) || (out == NULL))
        return false;
    if (!db_format_header_version(rawheader, raw_len, &header_version))
        return false;
    if (header_version > 6)
        return false;
    if (raw_len < needed)
        return false;

    memset(out, 0, sizeof *out);
    memcpy(out, rawheader, sizeof *out);

    out->availlist = (dbaddress) read_legacy_dbaddress32(rawheader + 2);
    for (i = 0; i < ctviews; i++)
        out->views[i] = (dbaddress) read_legacy_dbaddress32(rawheader + 10 + (size_t) i * 4);

#ifdef SWAP_BYTE_ORDER
    {
    disktomemlong (out->u.extensions.availlistblock);
    disktomemshort (out->flags);
//  disktomemlong (out->fnumdatabase);
    disktomemlong (out->headerLength);
    disktomemshort (out->longversionMajor);
    disktomemshort (out->longversionMinor);
    }
#endif

    db_format_mode mode = {false, false, false};  /* 32-bit legacy format */
    db_format_mode_push(&mode);
    return true;
}
