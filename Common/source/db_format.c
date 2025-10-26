#include "frontier.h"
#include "standard.h"

#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <time.h>

#include "db_format.h"

boolean use_64bit_format = false;
static char last_backup_path[1024];

/* Utility helpers for big-endian encoding/decoding */
static uint16_t read_be16(const void *ptr) {
    const unsigned char *p = (const unsigned char *) ptr;
    return (uint16_t) ((p[0] << 8) | p[1]);
}

static uint32_t read_legacy_u32(const unsigned char *field) {
    return ((uint32_t) field[0] << 24) |
           ((uint32_t) field[1] << 16) |
           ((uint32_t) field[2] << 8)  |
            (uint32_t) field[3];
}

static dbaddress read_legacy_dbaddress32(const unsigned char *field) {
    return (dbaddress) read_legacy_u32(field);
}

static void write_be16(void *ptr, uint16_t value) {
    unsigned char *p = (unsigned char *) ptr;
    p[0] = (unsigned char)((value >> 8) & 0xFF);
    p[1] = (unsigned char)(value & 0xFF);
}

static void write_be32(void *ptr, uint32_t value) {
    unsigned char *p = (unsigned char *) ptr;
    p[0] = (unsigned char)((value >> 24) & 0xFF);
    p[1] = (unsigned char)((value >> 16) & 0xFF);
    p[2] = (unsigned char)((value >> 8) & 0xFF);
    p[3] = (unsigned char)(value & 0xFF);
}

static void write_dbaddress64(void *ptr, dbaddress value) {
    unsigned char *p = (unsigned char *) ptr;
    uint64_t v = (uint64_t) value;
    p[0] = (unsigned char)((v >> 56) & 0xFF);
    p[1] = (unsigned char)((v >> 48) & 0xFF);
    p[2] = (unsigned char)((v >> 40) & 0xFF);
    p[3] = (unsigned char)((v >> 32) & 0xFF);
    p[4] = (unsigned char)((v >> 24) & 0xFF);
    p[5] = (unsigned char)((v >> 16) & 0xFF);
    p[6] = (unsigned char)((v >> 8) & 0xFF);
    p[7] = (unsigned char)(v & 0xFF);
}

boolean detect_database_format(const tydatabaserecord *header) {
    if (header == NULL)
        return false;

    if (header->versionnumber <= 6) {
        use_64bit_format = false;
        return true;  /* Legacy 32-bit format */
    }

    if (header->versionnumber >= 7) {
        use_64bit_format = true;
        return true;  /* New 64-bit format */
    }

    return false;  /* Unsupported version */
}

boolean convert_32bit_header_to_64bit(const unsigned char *legacy_header, tydatabaserecord_64 *new_header) {
    if ((legacy_header == NULL) || (new_header == NULL))
        return false;

    memset(new_header, 0, sizeof *new_header);

    new_header->systemid = legacy_header[0];
    new_header->versionnumber = 7;
    new_header->availlist = read_legacy_dbaddress32(legacy_header + 2);
    new_header->oldfnumdatabase = (short) read_be16(legacy_header + 6);
    new_header->flags = (short) read_be16(legacy_header + 8);

    const size_t view_base = 10;
    const size_t view_stride = 4;
    for (int i = 0; i < ctviews; ++i)
        new_header->views[i] = read_legacy_dbaddress32(legacy_header + view_base + (size_t)i * view_stride);

    new_header->releasestack = 0; /* recalculated at runtime if needed */
    new_header->fnumdatabase = 0;
    uint32_t legacy_header_length = read_legacy_u32(legacy_header + 30);
    if (legacy_header_length == 0)
        legacy_header_length = (uint32_t) sizeof(tydatabaserecord_64);
    new_header->headerLength = (long) legacy_header_length;
    new_header->longversionMajor = (short) read_be16(legacy_header + 34);
    if (new_header->longversionMajor == 0)
        new_header->longversionMajor = 6;
    new_header->longversionMinor = (short) read_be16(legacy_header + 36);

    new_header->u.extensions.availlistblock = read_legacy_dbaddress32(legacy_header + 38);
    new_header->u.extensions.availlistshadow = nildbaddress;
    new_header->u.extensions.flreadonly = false;
    memset(new_header->u.extensions.reserved, 0, sizeof new_header->u.extensions.reserved);

    return true;
}

boolean create_root_backup(const char *original_path) {
    if (original_path == NULL)
        return false;

    last_backup_path[0] = '\0';

    char backup_path[1024];
    time_t now = time(NULL);
    struct tm *tm_info = localtime(&now);

    if (tm_info == NULL)
        return false;

    snprintf(backup_path, sizeof backup_path,
             "%s.%04d%02d%02d_%02d%02d%02d",
             original_path,
             tm_info->tm_year + 1900,
             tm_info->tm_mon + 1,
             tm_info->tm_mday,
             tm_info->tm_hour,
             tm_info->tm_min,
             tm_info->tm_sec);

    FILE *src = fopen(original_path, "rb");
    FILE *dst = fopen(backup_path, "wb");

    if (!src || !dst) {
        if (src)
            fclose(src);
        if (dst)
            fclose(dst);
        return false;
    }

    char buffer[4096];
    size_t bytes;
    while ((bytes = fread(buffer, 1, sizeof buffer, src)) > 0) {
        if (fwrite(buffer, 1, bytes, dst) != bytes) {
            fclose(src);
            fclose(dst);
            return false;
        }
    }

    fclose(src);
    fclose(dst);
    strncpy(last_backup_path, backup_path, sizeof last_backup_path);
    last_backup_path[sizeof last_backup_path - 1] = '\0';
    return true;
}

boolean migrate_32bit_to_64bit(const char *db_path) {
    if (db_path == NULL)
        return false;

    /* No longer create backup - we'll write to a new file instead */

    FILE *src = fopen(db_path, "rb");
    if (!src)
        return false;

    unsigned char legacy_header[LEGACY_DB_HEADER_BYTES];
    if (fread(legacy_header, 1, sizeof legacy_header, src) != sizeof legacy_header) {
        fclose(src);
        return false;
    }

    tydatabaserecord_64 new_header;
    if (!convert_32bit_header_to_64bit(legacy_header, &new_header)) {
        fclose(src);
        return false;
    }

    /* Create output path: <base>-v7.root */
    char output_path[1024];
    const char *ext = strrchr(db_path, '.');
    if (ext && strcmp(ext, ".root") == 0) {
        size_t base_len = ext - db_path;
        snprintf(output_path, sizeof output_path, "%.*s-v7.root", (int)base_len, db_path);
    } else {
        /* If no .root extension, just append -v7 */
        snprintf(output_path, sizeof output_path, "%s-v7", db_path);
    }

    /* Store output path for caller to retrieve */
    strncpy(last_backup_path, output_path, sizeof last_backup_path);
    if (sizeof last_backup_path > 0)
        last_backup_path[sizeof last_backup_path - 1] = '\0';

    char temp_path[1024];
    snprintf(temp_path, sizeof temp_path, "%s.tmp", output_path);

    FILE *dst = fopen(temp_path, "wb");
    if (!dst) {
        fclose(src);
        return false;
    }

    /* Encode header fields in on-disk byte order */
    tydatabaserecord_64 disk_header;
    memset(&disk_header, 0, sizeof disk_header);

    disk_header.systemid = new_header.systemid;
    disk_header.versionnumber = new_header.versionnumber;

    write_dbaddress64(&disk_header.availlist, new_header.availlist);
    write_be16(&disk_header.oldfnumdatabase, (uint16_t)new_header.oldfnumdatabase);
    write_be16(&disk_header.flags, (uint16_t)new_header.flags);

    for (int i = 0; i < ctviews; ++i) {
        write_dbaddress64(&disk_header.views[i], new_header.views[i]);
#if 0
        {
            const unsigned char *enc = (const unsigned char *) &disk_header.views[i];
            fprintf(stderr, "encoded view[%d]=0x%llx -> %02x %02x %02x %02x %02x %02x %02x %02x\n",
                    i,
                    (unsigned long long) new_header.views[i],
                    enc[0], enc[1], enc[2], enc[3],
                    enc[4], enc[5], enc[6], enc[7]);
        }
#endif
    }

    /* The runtime builds these at load; leave zeroed */
    disk_header.releasestack = 0;
    write_be32(&disk_header.fnumdatabase, 0);

    write_be32(&disk_header.headerLength, (uint32_t)new_header.headerLength);
    write_be16(&disk_header.longversionMajor, (uint16_t)new_header.longversionMajor);
    write_be16(&disk_header.longversionMinor, (uint16_t)new_header.longversionMinor);

    write_dbaddress64(&disk_header.u.extensions.availlistblock, new_header.u.extensions.availlistblock);
    disk_header.u.extensions.availlistshadow = 0;
    disk_header.u.extensions.flreadonly = false;
    memset(disk_header.u.extensions.reserved, 0, sizeof disk_header.u.extensions.reserved);

    /* Write new header */
    if (fwrite(&disk_header, sizeof disk_header, 1, dst) != 1) {
        fclose(src);
        fclose(dst);
        remove(temp_path);
        return false;
    }

    /* Copy remainder of file (skip old header) */
    if (fseek(src, (long)sizeof legacy_header, SEEK_SET) != 0) {
        fclose(src);
        fclose(dst);
        remove(temp_path);
        return false;
    }
    char buffer[64 * 1024];
    size_t bytes;
    while ((bytes = fread(buffer, 1, sizeof buffer, src)) > 0) {
        if (fwrite(buffer, 1, bytes, dst) != bytes) {
            fclose(src);
            fclose(dst);
            remove(temp_path);
            return false;
        }
    }

    fclose(src);
    if (fflush(dst) != 0) { fclose(dst); remove(temp_path); return false; }
    if (fclose(dst) != 0) { remove(temp_path); return false; }

    /* Move temp file to final output path (original remains untouched) */
    if (rename(temp_path, output_path) != 0) {
        remove(temp_path);
        return false;
    }

    return true;
}

boolean ensure_database_modern(const char *db_path, boolean *migrated, char *output_path, size_t output_path_size) {
    if (migrated)
        *migrated = false;
    if (db_path == NULL || db_path[0] == '\0')
        return false;

    FILE *fp = fopen(db_path, "rb");
    if (!fp)
        return false;

    tydatabaserecord header;
    boolean ok = fread(&header, sizeof header, 1, fp) == 1;
    fclose(fp);
    if (!ok)
        return false;

    if (!detect_database_format(&header))
        return false;

    if (use_64bit_format) {
        /* Already modern - return original path */
        if (output_path && output_path_size > 0) {
            strncpy(output_path, db_path, output_path_size);
            if (output_path_size > 0)
                output_path[output_path_size - 1] = '\0';
        }
        return true;
    }

    if (!migrate_32bit_to_64bit(db_path))
        return false;

    /* Migration succeeded; return path to new v7 file */
    if (output_path && output_path_size > 0) {
        if (!db_format_last_backup_path(output_path, output_path_size))
            return false;
    }

    /* Future reads should treat file as modern. */
    use_64bit_format = true;

    if (migrated)
        *migrated = true;
    return true;
}

boolean db_format_last_backup_path(char *buffer, size_t length) {
    if (buffer == NULL || length == 0)
        return false;
    if (last_backup_path[0] == '\0') {
        buffer[0] = '\0';
        return false;
    }
    strncpy(buffer, last_backup_path, length);
    if (length > 0)
        buffer[length - 1] = '\0';
    return true;
}

void db_format_clear_last_backup_path(void) {
    last_backup_path[0] = '\0';
}
