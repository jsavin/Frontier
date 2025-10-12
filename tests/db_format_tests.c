#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "frontier.h"
#include "db_format.h"

static void reset_use_64bit_format(void) {
    use_64bit_format = false;
}

static void test_detect_legacy_database(void) {
    reset_use_64bit_format();

    tydatabaserecord legacy = {0};
    legacy.versionnumber = 6;

    assert(detect_database_format(&legacy));
    assert(!use_64bit_format);
}

static void test_detect_modern_database(void) {
    reset_use_64bit_format();

    tydatabaserecord modern = {0};
    modern.versionnumber = 7;

    assert(detect_database_format(&modern));
    assert(use_64bit_format);
}

static void test_convert_header(void) {
    tydatabaserecord old_header = {0};
    old_header.systemid = 1;
    old_header.versionnumber = 6;
    old_header.availlist = 0x11223344;
    old_header.oldfnumdatabase = 42;
    old_header.flags = 0x55AA;
    old_header.views[0] = 0x01020304;
    old_header.views[1] = 0x05060708;
    old_header.views[2] = 0x090A0B0C;
    old_header.releasestack = (Handle)0x1234;
    old_header.fnumdatabase = 99;
    old_header.headerLength = 256;
    old_header.longversionMajor = 6;
    old_header.longversionMinor = 1;
    old_header.u.extensions.availlistblock = 0x0BADF00D;
    old_header.u.extensions.availlistshadow.data = (Handle)0xBEEF;
    old_header.u.extensions.availlistshadow.pos = 12;
    old_header.u.extensions.availlistshadow.eof = 34;
    old_header.u.extensions.availlistshadow.size = 56;
    old_header.u.extensions.flreadonly = true;

    tydatabaserecord_64 new_header;
    memset(&new_header, 0, sizeof new_header);

    assert(convert_32bit_header_to_64bit(&old_header, &new_header));

    assert(new_header.systemid == old_header.systemid);
    assert(new_header.versionnumber == 7);
    assert(new_header.availlist == (dbaddress)old_header.availlist);
    assert(new_header.oldfnumdatabase == old_header.oldfnumdatabase);
    assert(new_header.flags == old_header.flags);
    assert(new_header.views[0] == (dbaddress)old_header.views[0]);
    assert(new_header.views[1] == (dbaddress)old_header.views[1]);
    assert(new_header.views[2] == (dbaddress)old_header.views[2]);
    assert(new_header.releasestack == old_header.releasestack);
    assert(new_header.fnumdatabase == (long)old_header.fnumdatabase);
    assert(new_header.headerLength == (long)old_header.headerLength);
    assert(new_header.longversionMajor == old_header.longversionMajor);
    assert(new_header.longversionMinor == old_header.longversionMinor);
    assert(new_header.u.extensions.availlistblock == (dbaddress)old_header.u.extensions.availlistblock);
    assert(new_header.u.extensions.availlistshadow.data == old_header.u.extensions.availlistshadow.data);
    assert(new_header.u.extensions.availlistshadow.pos == old_header.u.extensions.availlistshadow.pos);
    assert(new_header.u.extensions.availlistshadow.eof == old_header.u.extensions.availlistshadow.eof);
    assert(new_header.u.extensions.availlistshadow.size == old_header.u.extensions.availlistshadow.size);
    assert(new_header.u.extensions.flreadonly == old_header.u.extensions.flreadonly);
}

int main(void) {
    test_detect_legacy_database();
    test_detect_modern_database();
    test_convert_header();
    printf("db_format_tests: all checks passed\n");
    return 0;
}
