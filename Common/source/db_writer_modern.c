/* 2025-11-26 Codex: Modern (v7+) BE64 writer helpers split from db_format.c. */

#include "frontier.h"
#include "standard.h"

#include <string.h>

#include "db_writer_modern.h"
#include "db_format.h"

boolean db_write_modern_header(const tydatabaserecord *in, unsigned char *outbuf, size_t out_len) {
    tydatabaserecord_64 diskrec64;
    if (in == NULL || outbuf == NULL)
        return false;
    if (out_len < sizeof(tydatabaserecord_64))
        return false;

    memset(&diskrec64, 0, sizeof diskrec64);

    diskrec64.systemid = in->systemid; /* legacy compatibility; not used for platform gating */
    diskrec64.versionnumber = dbversionnumber; /* force canonical v7 */
    diskrec64.availlist = in->availlist;
    diskrec64.oldfnumdatabase = in->oldfnumdatabase;
    diskrec64.flags = in->flags;

    /*
     * Copy views from in-memory structure.
     * dbaddress is typedef'd as long long (64-bit).
     */
    for (int i = 0; i < ctviews; ++i)
        diskrec64.views[i] = in->views[i];

    diskrec64.releasestack = nil;
    diskrec64.fnumdatabase = 0;
    diskrec64.headerLength = (long) sizeof(tydatabaserecord_64); /* force modern size */
    diskrec64.longversionMajor = (in->longversionMajor == 0) ? 7 : in->longversionMajor;
    diskrec64.longversionMinor = (in->longversionMinor == 0) ? 0 : in->longversionMinor;
    diskrec64.u.extensions.availlistblock = in->u.extensions.availlistblock;
    diskrec64.u.extensions.availlistshadow = nildbaddress;
    diskrec64.u.extensions.flreadonly = in->u.extensions.flreadonly;
    memset(diskrec64.u.extensions.reserved, 0, sizeof diskrec64.u.extensions.reserved);

    return db_format_write_header64(&diskrec64, outbuf, out_len);
}

boolean db_write_modern_block_header(dbaddress adr, boolean flfree, long ctbytes, tyvariance variance) {
    uint64_t raw_size = (uint64_t) ctbytes;
    tyheader64 header;

    if (flfree)
        raw_size |= 0x8000000000000000ULL;

    memset(&header, 0, sizeof header);
    db_format_write_be64(&header.sizefreeword.size, raw_size);
    db_format_write_be32(&header.variance, (uint32_t) variance);

    return dbwrite(adr, sizeheader_v7, &header);
}

boolean db_write_modern_block_trailer(dbaddress adr, boolean flfree, long ctbytes) {
    uint64_t raw_size = (uint64_t) ctbytes;
    tytrailer64 trailer;

    if (flfree)
        raw_size |= 0x8000000000000000ULL;

    memset(&trailer, 0, sizeof trailer);
    db_format_write_be64(&trailer.sizefreeword.size, raw_size);

    return dbwrite(adr, sizetrailer_v7, &trailer);
}
