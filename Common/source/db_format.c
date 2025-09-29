#include "frontier.h"
#include "standard.h"

#include <stdio.h>
#include <time.h>

#include "db_format.h"

boolean use_64bit_format = false;

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

boolean convert_32bit_header_to_64bit(const tydatabaserecord *old_header, tydatabaserecord_64 *new_header) {
    if ((old_header == NULL) || (new_header == NULL))
        return false;

    new_header->systemid = old_header->systemid;
    new_header->versionnumber = 7;
    new_header->availlist = (dbaddress)old_header->availlist;
    new_header->oldfnumdatabase = old_header->oldfnumdatabase;
    new_header->flags = old_header->flags;

    for (int i = 0; i < ctviews; ++i)
        new_header->views[i] = (dbaddress)old_header->views[i];

    new_header->releasestack = old_header->releasestack;
    new_header->fnumdatabase = (long)old_header->fnumdatabase;
    new_header->headerLength = (long)old_header->headerLength;
    new_header->longversionMajor = old_header->longversionMajor;
    new_header->longversionMinor = old_header->longversionMinor;

    new_header->u.extensions.availlistblock = (dbaddress)old_header->u.extensions.availlistblock;
    new_header->u.extensions.availlistshadow = old_header->u.extensions.availlistshadow;
    new_header->u.extensions.flreadonly = old_header->u.extensions.flreadonly;

    return true;
}

boolean create_root_backup(const char *original_path) {
    if (original_path == NULL)
        return false;

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
    return true;
}

boolean migrate_32bit_to_64bit(const char *db_path) {
    if (db_path == NULL)
        return false;

    if (!create_root_backup(db_path))
        return false;

    FILE *src = fopen(db_path, "rb");
    if (!src)
        return false;

    tydatabaserecord old_header;
    if (fread(&old_header, sizeof old_header, 1, src) != 1) {
        fclose(src);
        return false;
    }

    tydatabaserecord_64 new_header;
    if (!convert_32bit_header_to_64bit(&old_header, &new_header)) {
        fclose(src);
        return false;
    }

    char temp_path[1024];
    snprintf(temp_path, sizeof temp_path, "%s.tmp", db_path);

    FILE *dst = fopen(temp_path, "wb");
    if (!dst) {
        fclose(src);
        return false;
    }

    const size_t written = fwrite(&new_header, sizeof new_header, 1, dst);
    fclose(src);
    fclose(dst);
    return (written == 1);
}

