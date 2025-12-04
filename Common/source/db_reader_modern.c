/* 2025-11-26 Codex: Modern reader fork for v7+ BE64 headers. */

#include "frontier.h"
#include "standard.h"

#include <string.h>

#include "db_reader.h"
#include "db_format.h"

static uint16_t read_be16(const unsigned char *field) {
    return db_format_read_be16(field);
}

static uint64_t read_be64(const unsigned char *field) {
    return db_format_read_be64(field);
}

boolean db_read_modern(const unsigned char *rawheader, size_t raw_len, tydatabaserecord *out) {
    int header_version = 0;
    int i;
    size_t needed = sizeof (tydatabaserecord_64);

    if ((rawheader == NULL) || (out == NULL))
        return false;
    if (!db_format_header_version(rawheader, raw_len, &header_version))
        return false;
    if (header_version < 7)
        return false;
    if (raw_len < needed)
        return false;

    memset(out, 0, sizeof *out);
    out->systemid = rawheader[0];
    out->versionnumber = rawheader[1];
    out->availlist = (dbaddress) read_be64(rawheader + offsetof(tydatabaserecord_64, availlist));
    out->oldfnumdatabase = (short) read_be16(rawheader + offsetof(tydatabaserecord_64, oldfnumdatabase));
    out->flags = (short) read_be16(rawheader + offsetof(tydatabaserecord_64, flags));

    for (i = 0; i < ctviews; i++) {
        size_t offset = offsetof(tydatabaserecord_64, views) + (size_t) i * sizeof(dbaddress);
        out->views[i] = (dbaddress) read_be64(rawheader + offset);
    }

    out->releasestack = nil;
    out->fnumdatabase = 0;
    out->headerLength = (long) db_format_read_be32(rawheader + offsetof(tydatabaserecord_64, headerLength));
    out->longversionMajor = (short) read_be16(rawheader + offsetof(tydatabaserecord_64, longversionMajor));
    out->longversionMinor = (short) read_be16(rawheader + offsetof(tydatabaserecord_64, longversionMinor));

    out->u.extensions.availlistblock = (dbaddress) db_format_read_be64(rawheader + offsetof(tydatabaserecord_64, u.extensions.availlistblock));
    out->u.extensions.flreadonly = rawheader[offsetof(tydatabaserecord_64, u.extensions.flreadonly)];

    db_format_mode mode = {true, false, false};  /* 64-bit modern format */
    db_format_mode_push(&mode);
    return true;
}
