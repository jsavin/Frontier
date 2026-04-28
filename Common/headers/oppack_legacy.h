
/*	$Id$    */

/*
    SPDX-License-Identifier: MIT

    Copyright (c) 1992-2004 UserLand Software, Inc.
    Copyright (c) 2025-present Frontier contributors

    Permission is hereby granted, free of charge, to any person obtaining a
    copy of this software and associated documentation files (the "Software"),
    to deal in the Software without restriction, including without limitation
    the rights to use, copy, modify, merge, publish, distribute, sublicense,
    and/or sell copies of the Software, and to permit persons to whom the
    Software is furnished to do so, subject to the following conditions:

    The above copyright notice and this permission notice shall be included in
    all copies or substantial portions of the Software.

    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
    IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
    FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
    AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
    LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
    FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
    DEALINGS IN THE SOFTWARE.
*/

/*
 * 2025-12-05: Legacy outline packer header for v2/v3 format (32-bit timestamps).
 * These functions handle READING old v6 database outline/script payloads.
 * V7 databases use oppack_v7.c with v4 portable header.
 * See planning/phase3/carbon_migration/outline_script_payload.md
 */

#ifndef oppacklegacyinclude
#define oppacklegacyinclude

#include "op.h"

/*
 * Legacy pack/unpack functions for v2/v3 outline format
 * Used by dispatch layer in opverbs.c to handle old v6 database payloads
 */

extern boolean oppack_legacy (Handle *hpackedoutline);

extern boolean oppackoutline_legacy (hdloutlinerecord houtline, Handle *hpackedoutline);

extern boolean opunpack_legacy (Handle hpackedoutline, long *ixload, hdloutlinerecord *houtline);

extern boolean opunpackoutline_legacy (Handle hpackedoutline, hdloutlinerecord *houtline);

#endif /* oppacklegacyinclude */
