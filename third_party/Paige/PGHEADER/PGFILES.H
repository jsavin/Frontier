/* Header file for pgFiles.h  */

#ifndef PGFILES_H
#define PGFILES_H

#include "Paige.h"


#define PAIGE_HEADER_SIG	0x77AA		/* Signature for file header */

#define KEY_REVISION1		0x00000008	/* Version where we started saving default tab spacing */
#define KEY_REVISION2		0x0000000A	/* Version where we added fields for selections */
#define KEY_REVISION3		0x00000100	/* Version where we added font fields */
#define KEY_REVISION4		0x00000101	/* Version where nbr styles increased */
#define KEY_REVISION5		0x00010000	/* Version where small caps added */
#define KEY_REVISION6		0x0001000B	/* Version where OITC changed styles etc. added */
#define KEY_REVISION7		0x0001000C	/* Version where we changed tabs to 32 element array */
#define KEY_REVISION8		0x0001000E	/* Version where we added alternate font name */
#define KEY_REVISION8A		0x0001000F	/* Version where Tom and I got out of sync, this fixes some compatibilities issues */
#define KEY_REVISION9		0x00010010	/* Version where we changed font_rec for RTF, WinNT, Win95  and other RTF enhancements */
#define KEY_REVISION10		0x00010011	/* Version where we changed style_info and par_info for RTF enhancements */
#define KEY_REVISION11		0x00010012	/* Version that saves pg_subsets */
#define KEY_REVISION12		0x00010013	/* Version that includes named stylesheets */
#define KEY_REVISION13		0x00010014	/* Version that displays virtual resolution */
#define KEY_REVISION14		0x00010016	/* Version that supports pure unicode */
#define KEY_REVISION15		0x00010017	/* Version that removed a field from paige_key */
#define KEY_REVISION16		0x00020000	/* Version that added unsupported RTF text to embeds. */
#define KEY_REVISION17		0x00020002	/* Version that added borders to par_info. */
#define KEY_REVISION18		0x00020003	/* Version that added tables to par_info. */
#define KEY_REVISION19		0x00020004	/* Version that adds subrefs forever. */
#define KEY_REVISION20		0x0002000F	/* Version that adds unique ID to hyperlinks */
#define KEY_REVISION21		0x00020010	/* Version that adds target ID to hyperlinks */
#define KEY_REVISION22		0x00020012	/* Version that adds html style to par info */
#define KEY_REVISION23		0x00020013	/* Version that extended table and border info */
#define KEY_REVISION30		0x00030001	/* Version that changes embed_ref border info */
#define KEY_REVISION31		0x00030002	/* Version that adds a value to hyperlinks */

#define OLD_MAX_STYLES		16			/* Previous number < KEY_REV 4 for styles */
#define REV8_STYLE_SIZE		190			/* sizeof(style_info) for revision 8 (solves out-sync) */
#define UNICODE_VERSION		KEY_REVISION14

#define CLR_NEW_FLAGS		((long) ~(DEACT_BIT | CARET_BIT | PRINT_MODE_BIT | NO_DEVICE_BIT))

#define KEY_HEADER_SIZE			4		/* Size of key header size (ASCII hex) */
#define DATA_HEADER_SIZE		8		/* Size of data header (ASCII hex) */
#define ELEMENT_HEADER_SIZE		8		/* Size of element (ASCII hex) */
#define PG_HEADER_SIZE			KEY_HEADER_SIZE + DATA_HEADER_SIZE + ELEMENT_HEADER_SIZE

#define DEFAULT_ELEMENT_INFO	0		/* First element_info for custom keys */

/* Flag settings for handler key "flags" field: */

#define HAS_STANDARD_WRITE_HANDLER		0x0001		/* Write handler is non-custom */
#define HAS_STANDARD_READ_HANDLER		0x0002		/* Read handler is non-custom */

typedef pg_bits8 pg_key_header[PG_HEADER_SIZE];

/* Element info for multiple pg_ref's in same doc (known items, otherwise custom): */

enum {
	section_pg,				/* Document section pg_ref */
	header_pg,				/* Header(s) pg_ref */
	footer_pg,				/* Footer(s) pg_ref */
	footnote_pg,			/* Footnote(s) pg_ref */
	endnote_pg,				/* Endnote(s) pg_ref */
	caption_pg,				/* Caption(s) pg_ref */
	rsrv_pg,				/* - reserved by DSI - */
	custom_pg				/* Custom by app */
};

#define AUX_PG_MASK			0x3FFF			/* Mask for *element_info for multiple pg's */
#define UNUSED_KEY_QTY		4				/* Number of keys not used */

/* Data types  */

enum {
	byte_data,				/* Next data byte count followed by bytes */
	short_data,				/* Next data is a short */
	long_data,				/* Next data is a long */
	terminator_data			/* Used as a terminator (no more bytes) */
};


/* Record used internally to do packing / unpacking  */

struct pack_walk {
	short		last_code;		/* Last code output */
	short		repeat_ctr;		/* Repeat counter for multiple like bytes */
	long		last_value;		/* Last byte, short or long transferred */
	memory_ref	data_ref;		/* Memory_ref used for storing / getting data */
	pg_bits8_ptr data;			/* Current data pointer */
	long		remaining_ctr;	/* Remaining count in data pointer */
	long		transfered;		/* # of bytes transfered */
	long		first_offset;	/* First offset packed or unpacked */
	long		version;		/* Version of the file we are handling */
	long		max_bytes;		/* Maximum bytes to read (for logical records) */
	long		max_bytes_ctr;	/* Count-up to max bytes */
};

typedef struct pack_walk pack_walk;
typedef pack_walk PG_FAR *pack_walk_ptr;

#ifdef __cplusplus
extern	"C" {
#endif


extern PG_PASCAL (pg_error) pgReadKey (file_io_proc io_proc, size_t PG_FAR *position,
	file_ref filemap, pg_file_key_ptr key, size_t PG_FAR *data_size,
	long PG_FAR *element_info);
extern PG_PASCAL (pg_error) pgWriteKeyData (pg_ref pg, pg_file_key key,
		void PG_FAR *data, long data_length, long element_info, file_io_proc io_proc,
		file_io_proc data_io_proc, size_t PG_FAR *file_position, file_ref filemap);
extern PG_PASCAL (void) pgPackData (memory_ref src_data, memory_ref target_data,
		pg_short_t data_type);
extern PG_PASCAL (void) pgUnpackData (memory_ref src_data, memory_ref target_data,
		long PG_FAR *starting_offset, pg_short_t data_type);
extern PG_PASCAL (pg_handler_ptr) pgFindHandlerFromKey (memory_ref handlers_list,
		pg_file_key key, pg_short_t PG_FAR *rec_location);
extern PG_PASCAL (void) pgInitOneHandler (pg_handler_ptr handler, pg_file_key key);
extern PG_PASCAL (memory_ref) pgBuildHandlerList (pg_globals_ptr globals, pg_file_key_ptr keys,
		pg_short_t num_keys);
extern PG_PASCAL (void) pgSetupPacker (pack_walk_ptr walker, memory_ref ref, long first_offset);
extern PG_PASCAL (pg_error) pgDoExceptionKey (paige_rec_ptr pg, memory_ref handlers,
	 short error_code, short for_write, memory_ref data);
extern PG_PASCAL (long) pgUnpackHex (pg_bits8_ptr data, long PG_FAR *value);
extern PG_PASCAL (pg_short_t) pgHexToByte (pg_bits8 hex_byte);
extern PG_PASCAL (long) pgUnpackNum (pack_walk_ptr in_data);
extern PG_PASCAL (void) pgUnpackNumbers (pack_walk_ptr out_data, void PG_FAR *ptr, short qty,
		short data_code);
extern PG_PASCAL (void) pgUnpackMemoryRef (pg_globals_ptr mem_globals, pack_walk_ptr walker,
		memory_ref PG_FAR *ref);
extern PG_PASCAL (long) pgUnpackBytes (pack_walk_ptr in_data, memory_ref out_data);
extern PG_PASCAL (void) pgUnpackPtrBytes (pack_walk_ptr in_data, pg_bits8_ptr out_ptr);
extern PG_PASCAL (long) pgGetUnpackedSize (pack_walk_ptr walker);
extern PG_PASCAL (pg_bits8_ptr) pgGetUnpackedPtr (pack_walk_ptr walker, long PG_FAR *bytesize);
extern PG_PASCAL (void) pgPackNum (pack_walk_ptr out_data, short code, long value);
extern PG_PASCAL (void) pgPackNumbers (pack_walk_ptr out_data, void PG_FAR *ptr, short qty, 
		short data_code);
extern PG_PASCAL (void) pgPackMemoryRef (pack_walk_ptr walker, memory_ref ref);
extern PG_PASCAL (void) pgPackBytes (pack_walk_ptr out_data, pg_bits8_ptr the_bytes, long length);
extern PG_PASCAL (memory_ref) pgFinishPack (pack_walk_ptr walker);
extern PG_PASCAL (void) pgPackRect (pack_walk_ptr walker, rectangle_ptr r);
extern PG_PASCAL (void) pgPackCoOrdinate (pack_walk_ptr walker, co_ordinate_ptr point);
extern PG_PASCAL (void) pgPackSelectPair (pack_walk_ptr walker, select_pair_ptr pair);
extern PG_PASCAL (void) pgPackColor (pack_walk_ptr walker, color_value PG_FAR *color);
extern PG_PASCAL (long) pgPackShape (pack_walk_ptr walker, shape_ref the_shape);
extern PG_PASCAL (void) pgUnpackCoOrdinate (pack_walk_ptr walker, co_ordinate_ptr point);
extern PG_PASCAL (void) pgUnpackRect (pack_walk_ptr walker, rectangle_ptr r);
extern PG_PASCAL (void) pgUnpackSelectPair (pack_walk_ptr walker, select_pair_ptr pair);
extern PG_PASCAL (void) pgUnpackColor (pack_walk_ptr walker, color_value PG_FAR *color);
extern PG_PASCAL (void) pgUnpackShape (pack_walk_ptr walker, shape_ref the_shape);
extern PG_PASCAL (long) pgPackStyleRun (pack_walk_ptr walker, memory_ref general_ref,
		long PG_FAR *unpacked_size);
extern PG_PASCAL (void) pgPackTextBlock (pack_walk_ptr walker, text_block_ptr block,
			pg_boolean include_text, pg_boolean no_line_save);
extern PG_PASCAL (void) pgUnpackTextBlock (paige_rec_ptr pg, pack_walk_ptr walker,
		text_block_ptr block, pg_boolean include_text, memory_ref home_subref);
extern PG_PASCAL (void) pgUnpackStyleRun (pack_walk_ptr walker, memory_ref ref, long elements);
extern PG_PASCAL (void) pgPackUnicodeBytes (pack_walk_ptr out_data, pg_bits8_ptr the_bytes,
		long length, pg_boolean unconditional);

#ifdef __cplusplus
	}
#endif

#endif
