/* This header file defines the "pack" data format for files and memory-based
structure saves.

The intention of this packing scheme is more for the purpose of platform
independence than anything else.  For example, numerical values (longs and
such) are output as ASCII "hex" values so it does not matter what order of
high/low words are used for memory addresses and long registers.   */

#ifndef PACKDEFS_H
#define PACKDEFS_H

/* High-order bits modifying values output:  */

#define REPEAT_LAST_VALUE	0x80		/* Repeat last value (even if different type) */
#define REPEAT_LAST_N_TIMES	0x40		/* Repeat last for immediate n-byte that follows */

#define CODE_MASK			0x0F		/* Mask for the real code */

#define MINIMUM_MULTI_PACK	3			/* Amount like bytes must be to do multi-repeats */
#define MAX_REPEAT_SIZE		127			/* Maximum byte repeat */

#define MAX_HEX_SIZE		10			/* Maximum size of hex output */
#define HEX_EXTEND_SIZE		(MAX_HEX_SIZE * 30)	/* Amount to extend hex output */

#define MINUS_SIGN			0x2D		/* Negative sign for minus hex numbers */

#endif
