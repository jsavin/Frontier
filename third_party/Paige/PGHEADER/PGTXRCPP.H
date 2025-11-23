/* PAIGE Import/Export Class Definitions. Copyright 1995-96 DataPak Software, Inc.
Software by Gar.  CODEWARRIER NOTE: This header file can only be used with .cpp programs
and/or .c programs with Activate C++ Compiler option set. */

#ifndef PGTXRCPP_H
#define PGTXRCPP_H

#include "Paige.h"
#include "pgEmbed.h"
#include "pgTxr.h"
#include "pgFrame.h"

/* The file import class for C++ers: */

class PaigeImportFilter {

	protected:
		pg_filetype			file_type;			// Type of file being imported
		file_ref			filemap;			// The file reference
		long				file_os;			// Source OS where file came from
		long				import_mem_id;		// Import pg_ref memory ID
		memory_ref			memory_file;		// Optional OS handle (if importing from "memory")
		pg_ref				import_pg;			// The pg_ref we are importing to
		paige_rec_ptr		import_pg_rec;		// The <used> pointer to paige record
		paige_rec_ptr		original_pg_rec;	// Original target (or copy of import_pg_rec)
		file_io_proc		io_proc;			// The I/O function for reading the file
		pg_globals_ptr		paige_globals;		// PAIGE and memmgr globals
		long				text_position;		// Position into pg_ref we are importing
		long				bytes_imported;		// Total bytes imported so far
		long				bytes_read;			// Total bytes read from file

	// Basic member functions:

	public:

		PaigeImportFilter ();
		virtual ~PaigeImportFilter ();

	// Initialization (common to all, non-overridable):
	
		pg_error pgInitImportFile (pg_globals_ptr globals,
							pg_file_unit fileref,
							memory_ref memory_unit,
							file_io_proc read_proc,
							long first_position,
							long last_position);
		
		pg_error pgImportFile (pg_ref pg,
							long pg_position,
							long import_flags,
							pg_boolean keep_selection,
							short draw_mode);
		
		void ProcessImportData (void);

		void ApplyInputParagraph (par_info_ptr par, long applied_offset, pg_boolean unconditional_CR);
		void InsertErrorLog (short error_code, long begin, long end, pg_char_ptr msg, pg_boolean no_dups);

		pg_error			io_result;			// Error result of last operation

		long				import_bits;		// Data type(s) to import (can be set by app)
		long				max_buffer;			// Maximum buffer size, in bytes
		long				max_output_buffer;	// Current max on character output
		long				last_par_applied;	// Last par_info record number (from ApplyInputParagraph)
		pg_char_ptr			font_cross_table;	// Table of cross-fonts
		pg_char_ptr			character_table;	// Table of cross-chars
		memory_ref			image_spec;			// Holds path-specific info about the images
		memory_ref			error_log;			// Holds formatting errors (unsupported tags)
		short				target_link_style;	// Style, color to show target links (BK COLOR!)
		short				index_link_style;	// Style, color to show index target links (BK COLOR!)
		short				toc_link_style;		// Style, color to show TOC targets

	// Low-level I/O stuff:

	protected:
		long				feature_bits;	// What and how we are capable of importing
		pg_char_ptr			io_buffer;		// Low-level I/O buffer
		long				buffer_size;	// Size of buffer
		memory_ref			buffer_ref;		// Memory_ref that created buffer
		long				buffer_index;	// Index into buffer
		long				logical_pos;	// Logical file position (if importing serial stream)
		long				filepos;		// Current real file position
		long				file_begin;		// Beginning of file
		long				file_eof;		// End-of-file position
		pg_short_t			unicode_flag;	// Byte Order Mark if Unicode, otherwise zero

	// Translator information (can be mucked with):

	public:
		pg_translator		translator;			// File transfer information
		pg_char_ptr			output_ptr;			// Current pointer to read text buffer
		pg_ref				original_pg;		// The original pg_ref is pasting a temp
		memory_ref			parallel_buffer;	// If non-NULL imported bytes get inserted here also
		memory_ref			temp_buffer_ref;	// Used for seial stream in if non-zero
		pg_char_ptr			temp_buffer_ptr;	// Pointer to the chars in temp_buffer_ref
		long				temp_char_count;	// Remaining size of the chars in temp_buffer_ref
		long				temp_bytes_read;	// Number of temp bytes input from temp buffer
		long				last_physical_read;	// Number of physically read bytes prior to temp buffer
		pg_short_t			last_char_out;		// Last character sent to importer
		pg_short_t			last_char2_out;		// Last char before last one
		pg_short_t			last_char_in;		// Last character that came in from importing
		pg_short_t			this_char_in;		// Current character that came in from importing

	// Low-level member functions (cannot override):

		pg_error pgGetImportByte (pg_bits8_ptr the_byte);	// Return next file byte
		pg_error pgGetImportChar (pg_char_ptr the_char);	// Returns char
		pg_error pgGetImportInt (pg_short_t PG_FAR *the_int); // Returns 16 bit int
		pg_error pgGetImport3ByteInt (long PG_FAR *the_int);
		pg_error pgGetImportLong (long PG_FAR *the_long);
		pg_error pgGetImportMemoryRef (memory_ref ref, long byte_size);
		pg_error pgGetImportNBytes (long PG_FAR *num_bytes, pg_bits8_ptr bytes);
		pg_error pgGetImportString (pg_char_ptr the_str);	// Returns a cstring
		pg_char pgNextImportChar (void);					// Returns next char without removing from buffer
		pg_boolean OutputCharacter (pg_char the_byte);
		pg_boolean OutputString (pg_char_ptr the_str);
		void InsertFrame (pg_frame_ptr frame);
		pg_boolean FindFontInList (font_info_ptr font);
		void TranslateCrLf (void);
		void PushOutputState (void);
		void PopOutputState (void);
		void ImportTemporaryBuffer (memory_ref ref);

	// Overridable member functions (higher level):
		
		virtual pg_error pgVerifySignature (void);
		virtual pg_error pgPrepareImport (void);
		virtual pg_boolean pgReadNextBlock (void);
		virtual void PG_FAR * pgProcessEmbedData (memory_ref ref, long embed_type);
		virtual pg_error pgImportDone ();
		virtual void pgImportCleanup ();
		virtual void pgMapFont (font_info_ptr font, long importing_os, long current_os);
		virtual void pgMapChars (pg_ref pg);

	private:
	
		void CallWait (void);

		pg_boolean	is_cached;
		pg_short_t	half_char_pending;		// Used for Unicode processing of bytes
		long		pushed_transferred;		// Bytes transferred preserved
		long		pushed_text_read;		// Bytes read preserved
		pg_short_t	pushed_char_out;		// Last character sent to importer
		pg_short_t	pushed_char2_out;
		pg_char		last_buffer_char;		// Last char in input buffer for LF-CR decode
};

// Native import class (derived from PaigeImportFile):

class PaigeNativeImportFilter : public PaigeImportFilter {

	public:
		PaigeNativeImportFilter();
		virtual ~PaigeNativeImportFilter();

	// Overridable member functions (higher level):
	
	public:
		virtual pg_error pgPrepareImport (void);
		virtual pg_error pgVerifySignature (void);
		virtual pg_boolean pgReadNextBlock (void);
		virtual pg_error pgImportDone (void);
};


// RTF import class (derived from PaigeImportFile):

class PaigeRTFImportFilter : public PaigeImportFilter {

	public:
		PaigeRTFImportFilter();
		virtual ~PaigeRTFImportFilter();

	// Overridable member functions (higher level):
	
	public:
		virtual pg_error pgPrepareImport (void);
		virtual pg_error pgVerifySignature (void);
		virtual pg_boolean pgReadNextBlock (void);
		virtual pg_error pgImportDone ();
		virtual void ProcessInfoCommand (short command, short parameter);
		virtual void ProcessIndexCommand (pg_char_ptr index_text, long length);
		virtual void UnsupportedCommand (pg_char_ptr command, short parameter);

	private:
		memory_ref			style_stack;		// {bracketted state} style(s) list
		memory_ref			font_table;			// Table of fonts as given in RTF header
		memory_ref			color_table;		// Color table for text
		memory_ref			picture_ref;		// If non-NULL we are reading a picture
		embed_ref			previous_embed;		// The previous embed_ref inserted
		style_info			current_style;		// The current text format setting(s)
		par_info			current_par;		// The current paragraph formats
		font_info			current_font;		// The current font
		pg_doc_info			doc_info;			// Paper, page info
		style_table_entry	stylesheet;			// The stylesheet we are processing
		par_info			stylesheet_par_mask; // Mask for styles-based-on
		style_table_ptr		current_stylesheet;	// Non-null if processing stylesheets
		short				stylesheet_index;	// Index to stylesheet name
		short				stylesheet_level;	// Stack-nested level for stylesheet proceess
		short				default_font;		// Default font number (from RTF file)
		short				def_font_set;		// Default font that has been set in the doc
		short				embed_width;		// Width of embedded item, if any
		short				embed_height;		// Height of embedded item, if any
		long				embed_binary;		// Bytecount of incoming binary (of BIN type)
		long				embed_type;			// Type of embedded item, if any
		long				embed_modifier;		// The modifier value for NewEmbed()
		long				index_level;		// Level for index entry (in push, pop style stack)
		long				toc_level;			// Level for Table of Contents (in push, pop style stack)
		long				index_begin;		// Global offset for index beginning
		metafile_struct		metafile;			// Metafile input params
		pg_fixed			metascale_x;		// Scale-x factor of metafile
		pg_fixed			metascale_y;		// Scale-y factor of metafile
		memory_ref			unsupported_ref;	// Holds unsupported data
		memory_ref			index_text_ref;		// Holds text to index entry
		short				twips_width;		// Original width from rtf in TWIPS
		short				twips_height;		// Original height 
		short				twips_gwidth;		// Original picwgoal command width
		short				twips_gheight;		// Original pichgoal command
		pg_boolean			representing;		// TRUE if this pict is a representation of previous
		pg_boolean			had_actual_param;	// TRUE if there was a numeric parameter in command, keyword
		pg_boolean			double_borders;		// Double border thickness = ON if true

	// Table variables:

		par_info			par_table;			// Default table values
		
		short				table_level;		// Level of tables
		short				table_entry;		// Enter, exit table level
		long				next_table_id;		// Next unique table ID
		long				last_column_tab;	// Previous "tab" imported for cell width
		long				non_table_text;		// Amount of bytes imported NOT part of table
		long				last_border;		// Last border type imported

	// Local member functions to handle the import:

		long ReadCommand (short PG_FAR *param, pg_char_ptr terminator);
		pg_boolean PrepareRTFPicture (short pict_command_type);
		pg_boolean ReadRTFPicture (void);
		pg_boolean DoSpecialCharCommand (short command, short parameter);
		pg_boolean DoDestinationCommand (short command, short parameter);
		pg_boolean DoDocumentCommand (short command, short parameter);
		pg_boolean DoParagraphCommand (short command, short parameter);
		pg_boolean DoStyleCommand (short command, short parameter);
		pg_boolean DoTableCommand (short command, short parameter);

		void PushStyleStack (void);
		void PopStyleStack (void);
		void SkipCurrentLevel (void);
		void ProcessStylesheet (void);
};


// HTML import class (derived from PaigeImportFile):

class PaigeHTMLImportFilter : public PaigeImportFilter {

	public:
		PaigeHTMLImportFilter();
		virtual ~PaigeHTMLImportFilter();

	// Overridable member functions (higher level):
	
	public:
		virtual pg_error pgPrepareImport (void);
		virtual pg_error pgVerifySignature (void);
		virtual pg_boolean pgReadNextBlock (void);
		virtual pg_error pgImportDone ();
		virtual void pgImportCleanup ();

	// HTML-specific virtual functions:

		virtual pg_boolean PreProcessTag (short tag_type, short tag, tag_attribute_ptr params, long num_params);
		virtual pg_boolean UnimplementedTag (pg_char_ptr tag, pg_char_ptr paramlist, pg_boolean end_tag);

	// Other (non-virtual) functions:

		long ReadHTMLTag (pg_boolean PG_FAR *end_tag);
		void ProcessAttributes (pg_char_ptr attribute_table, short command_type);
		
		pg_boolean DoControlTag (short tag, tag_attribute_ptr params, long num_params);
		pg_boolean DoStyleTag (short tag, tag_attribute_ptr params, long num_params);
		pg_boolean DoDataTag (short tag, tag_attribute_ptr params, long num_params);
		pg_boolean DoTableTag (short tag, tag_attribute_ptr params, long num_params);
		pg_boolean DoPreTableTag (short tag, tag_attribute_ptr params, long num_params);
		
		void ProcessPreTable (tag_attribute_ptr params, long num_params);
		void ProcessHyperlink (void);
		void ProcessEmbed (void);
		void SetDefaultTable (pg_table_ptr table);
		void DumpPendingText (void);

		pg_boolean BreakLine (pg_boolean unconditional, pg_boolean large_break);
		long SetTableAttributes (tag_attribute_ptr params, long num_params, pg_table_ptr attributes);
		void EndTableRow (void);
		long TranslateAlignment (pg_char_ptr param);
//QUALCOMM Begin - Kusuma (mimic 3.14 release)
		//long TranslateColor (pg_char_ptr param, color_value_ptr color);
		long TranslateColor (pg_char_ptr param, color_value_ptr color, pg_boolean black_OK = TRUE);
//QUALCOMM Begin - Kusuma (mimic 3.14 release)
		void TranslateFont (pg_char_ptr param);
		pg_char TranslateCharacter (pg_char_ptr param);
		void InsertTokenChar (pg_char the_char, pg_boolean parse_spaces);
		pg_char_ptr GetNextToken (pg_short_t PG_FAR *token_size);
		void SetColumnFormat (void);
		void InsertFontError (pg_char_ptr facename, long offset);
//QUALCOMM Begin - Kusuma (mimic 3.14 release)
		//void GetStylesheet (long style_id);
		void GetStylesheet (long style_id, pg_boolean set_whole_style = TRUE);
		void PushStyle (void);
//QUALCOMM Begin - Beckley (fix bug with </font> restoring too much)
		//void PopStyle (void);
		void PopStyle (pg_boolean bFontTag = false);
//QUALCOMM End - Beckley (fix bug with </font> restoring too much)
//QUALCOMM End - Kusuma (mimic 3.14 release)
	// Public variables:

		long				html_styles[NUM_HTML_STYLES];		// Default HTML styles
		long				list_indent_value;		// Value for par indent defining listing level(s)
		color_value			bk_color;				// Document's default background color
		font_info			default_font;			// Default font
		pg_fixed			point_levels[7];		// Levels for point sizes ("normal" = element 2)
		pg_hyperlink		default_source;			// Default source link
		pg_hyperlink		default_target;			// Default target link
		long				base_font_index;		// Default base font index
		long				current_point_level;
		long				column_ctr;			// Counts columns during table passes
		embed_callback		image_callback;		// The embed_ref callback function (mostly for images)
		color_value			def_text_color;		// Default text color
		color_value			cur_text_color;		// Current text color
		style_info			current_style;		// The current text format setting(s)
		font_info			current_font;		// The current font
		par_info			current_par;		// The current paragraph formats
		pg_table			master_table;		// Table settings
		select_pair			link_range;			// Defines link range for source or target link
		memory_ref			param_ref;			// Holds parsed tag attribute records
		memory_ref			anchor_ref;			// Makes a copy of a-tag attributes
		memory_ref			attribute_ref;		// Holds attribute line
		memory_ref			table_text;			// Holds text for feeding first-pass cells
		pg_char_ptr			attribute_ptr;		// Holds string with attributes
		long				attribute_ref_size;	// Current size of attribute ref
		long				attribute_index;	// Index to attribute line
		long				deflist_level;		// Definition list is active
		long				list_level;			// Unordered list is active
		short				cr_enable;			// TRUE if processing "pre" tag
		short				table_def_align;	// Default table align (tab type)
		pg_boolean			embed_last_action;	// Last main action was an embed_ref
		
	private:
		long ImportedOffset (void);
//QUALCOMM Begin - Kusuma (mimic 3.14 version)
		memory_ref			style_stack_ref;
//QUALCOMM End - Kusuma (mimic 3.14 version)

	protected:
		
		memory_ref			tables_ref;			// The table offset(s) just imported
		pg_char_ptr			title_ptr;			// Non-null if title enabled
		long				table_unique_id;	// Next unique ID for tables
		long				page_width;			// Max width for page
		long				span_flag;			// Column span if non-zero
		long				row_begin;			// Absolute offset of where row begins
		long				saved_point_index;	// Saves pointsize index
		style_info			saved_style;			// Saved style from "font" tag
		font_info			saved_font;				// Saved font from "font" tag
		pg_char				linebreak_char;		// Character to use for linebreaking (CR or LF)
		short				doing_table;		// Table input mode (level counter)
		pg_boolean			doing_row;			// Table row input mode
		pg_boolean			code_listing;		// Code listing enabled
		pg_boolean			font_tag_enable;	// Font tag is active
		//QUALCOMM Begin == Kusuma
		pg_char				base_url_string[1020];
		//QUALCOMM End == Kusuma
};


/* Generic EXPORT (file write) class: */

class PaigeExportFilter {

	protected:
		pg_filetype			file_type;			// Type of file being exported
		long				file_version;		// Version of file (example - 0x00060000)
		file_ref			filemap;			// The file reference
		long				file_os;			// The target platform
		memory_ref			memory_file;		// Optional OS handle (if importing from "memory")
		pg_ref				export_pg;			// The pg_ref we are exporting from
		paige_rec_ptr		export_pg_rec;		// The <used> pointer to paige record
		file_io_proc		io_proc;			// The I/O function for reading the file
		pg_globals_ptr		paige_globals;		// PAIGE and memmgr globals
		select_pair			export_range;		// Text selection we are importing
		text_block_ptr		block;				// The current text_block
		style_info_ptr		next_style;			// Shows next style (what will come next text block)
		par_info_ptr		next_par;			// Shows next par style
		font_info_ptr		next_font;			// Shows next font
		long				bytes_exported;		// Total bytes exported so far
		long				local_begin;		// Offset into text_block for this transfer

	// Basic member functions:

	public:

		PaigeExportFilter ();
		virtual ~PaigeExportFilter ();

		pg_char				file_kind[KIND_STR_SIZE];	// Recommended filetype (set by constructor)

	// Initialization (common to all, non-overridable):
	
		pg_error pgInitExportFile (pg_globals_ptr globals,
							pg_file_unit fileref,
							memory_ref memory_unit,
							file_io_proc write_proc,
							long first_position);
		
		pg_error pgExportFile (pg_ref pg,
							select_pair_ptr range,
							long export_flags,
							pg_boolean selection_only);

		void InsertErrorLog (short error_code, long begin, long end, pg_char_ptr msg, pg_boolean no_dups);

		long				feature_bits;		// What and how we are capable of exporting
		long				export_bits;		// Data type(s) we are exporting
		long				max_buffer;			// Maximum holding buffer, in bytes
		long				filepos;			// Current real file position
		long				error_refcon;		// This goes into the error log's refcon
		pg_error			io_result;			// Error result of last operation

		memory_ref			error_log_ref;		// The error log
		
		style_info			previous_style;		// Shows previous style (last WriteNextBlock)
		par_info			previous_par;		// Shows previous par style

	// Low-level I/O stuff:

	protected:
	
		pg_char_ptr			io_buffer;		// Low-level I/O buffer
		long				buffer_size;	// Size of buffer
		memory_ref			buffer_ref;		// Memory_ref that created buffer
		memory_ref			unicode_ref;	// Memory_ref to convert from Unicode
		memory_ref			cr_lf_ref;		// Memory_ref to add LFs for Windows
		long				buffer_index;	// Index into buffer
		long				file_begin;		// Beginning of file
		long				total_out;		// Total bytes out (written to file or not)
		pg_short_t			last_char_out;	// Last character type out
		pg_short_t			half_char_pending; // If Unicode, 1/2 the char is pending as a byte

		pg_char				last_buffer_char;	// Last char seen for CR-LF conversion

	// Translator information (can be mucked with):

	public:
		pg_translator		translator;			// File transfer information

	// Low-level functions, non-overridable
		
		void pgWriteByte (pg_bits8 the_byte);	// Send byte to holding buffer
		void pgWriteChar (pg_char the_char);	// Send char (same as above if not Unicode)
		void pgWriteNBytes (pg_char_ptr bytes, long num_bytes); // Send byte(s) to holding buffer
		void pgWriteDecimal (short decimal_value);
		void pgWriteHexByte (pg_char the_byte);
		void pgWriteString (pg_char_ptr the_str, pg_char prefix, pg_char suffix);
		void pgWriteBytes (pg_bits8_ptr the_str, pg_bits8 prefix, pg_bits8 suffix);
		void pgWriteCString (pg_char_ptr the_str);
		void pgOutputBDC (short PG_FAR *value, short subtractor, pg_boolean PG_FAR *zero_suppress);
		void pgWriteInt (pg_short_t the_int);
		void pgWrite3ByteInt (long the_int);
		void pgWriteLong (long the_long);
		void pgWriteMemoryRef (memory_ref ref);
		void pgSetSerialPos (long the_position);
		void pgFlushOutputBuffer (void);  // Dump pending bytes in hold buffer
		long InsertLFChars (void);
		long MapExportFont (font_info_ptr font, pg_char_ptr output);

	// Overridable member functions (higher level)

		virtual pg_error pgPrepareExport (void);
		virtual pg_boolean pgWriteNextBlock (void);
		virtual pg_error pgExportDone ();

		virtual pg_bits8_ptr pgPrepareEmbedData (embed_ref ref, long PG_FAR *byte_count,
										long PG_FAR *local_storage);
		virtual void pgReleaseEmbedData (embed_ref ref, long local_storage);
};



// Native export class (derived from PaigeImportFile):

class PaigeNativeExportFilter : public PaigeExportFilter {

	public:
		PaigeNativeExportFilter();
		virtual ~PaigeNativeExportFilter();

	// Overridable member functions (higher level):
	
	public:
		virtual pg_error pgPrepareExport (void);
		virtual pg_boolean pgWriteNextBlock (void);
		virtual pg_error pgExportDone (void);
	
	private:
		pg_ref			copied_ref;
};



// RTF export class (derived from PaigeExportFilter):

class PaigeRTFExportFilter : public PaigeExportFilter {

	public:
		PaigeRTFExportFilter();
		virtual ~PaigeRTFExportFilter();

	// Overridable member functions (higher level):
	
	public:
		virtual pg_error pgPrepareExport (void);
		virtual pg_boolean pgWriteNextBlock (void);
		virtual pg_error pgExportDone ();
		virtual pg_error OutputHeaders ();
		virtual pg_error OutputFooters ();
		virtual pg_error OutputEmbed ();
		virtual pg_error OutputCustomParams();

		pg_char			def_stylename[FONT_SIZE + BOM_HEADER]; // Default "normal" stylesheet name

	private:
	// Local member functions to handle the import:

		memory_ref			style_stack;		// {bracketted state} style(s) list
		memory_ref			font_table;			// Font table (pre-built)
		memory_ref			color_table;		// Color table (pre-built)
		style_info			def_style;			// Default style_info
		par_info			def_par;			// Default par_info
		font_info			def_font;			// Default font_info
		long				count_last_cr;		// Byte counter since last CR
		long				def_state_level;	// Default style-state level
		pg_boolean			last_text_par;		// Last output was end of paragraph
		pg_boolean			doing_row;			// If TRUE we are outputing a row of cells

		void WriteCommand (pg_char_ptr rtf_table,
								short table_entry,
								short PG_FAR *parameter,
								pg_boolean delimeter);
		void OutputFontTable ();
		void OutputColorTable ();
		void OutputColorEntry (color_value_ptr color);
		void OutputStylesheets ();
		void OutputNamedStyle (named_stylesheet_ptr style, short named_index);
		void OutputPageInfo ();
		void OutputStyleInfo (style_info_ptr style, style_info_ptr mask);
		void OutputParInfo (par_info_ptr par, par_info_ptr mask);
		void WriteBorderCommand (long border_info, pg_char_ptr command_table, short command);
		void PushStyleStack (void);
		void PopStyleStack (void);
		pg_char_ptr GetFontType (font_info_ptr font);
		void OutputCR (pg_boolean unconditional);
		short PointConversion (short value, pg_boolean convert_resolution, pg_boolean x10);
		void OutputRowInfo (par_info_ptr par);
		void EnterTable ();
};


// RTF export class (derived from PaigeExportFilter):

class PaigeHTMLExportFilter : public PaigeExportFilter {

	public:
		PaigeHTMLExportFilter();
		virtual ~PaigeHTMLExportFilter();

	// Overridable member functions (higher level):
	
	public:
		virtual pg_error pgPrepareExport (void);
		virtual pg_boolean pgWriteNextBlock (void);
		virtual pg_error pgExportDone ();
		
		void WriteTag (pg_char_ptr tag_table,
					short table_entry, pg_boolean terminate_tag,
					pg_boolean send_CR);
        void WriteBreak (pg_boolean paragraph);
        void OutputCR (void);
        void OutputParam (pg_char_ptr table_ptr, short table_entry, pg_boolean leading_space);
        void InsertParameter(short code, pg_char_ptr table_used,
        			pg_char_ptr param, short param_size);
        void InsertDecimalParameter(short code, pg_char_ptr table_used, long value);
        void InsertColorParameter(short code, pg_char_ptr table_used,
        			color_value_ptr color, long long_color);
        void InsertAlignmentParam (short alignment);
		void OutputSpecialChar (pg_char the_char);
		
		pg_char				file_extension[8];		// This gets appended as ".xxxx" to URLs
		pg_fixed			point_levels[7];		// Levels for point sizes ("normal" = element 2)
		pg_hyperlink		default_source;			// Default source link
		pg_hyperlink		default_target;			// Default target link
		style_info			current_style;			// Current style info
		style_info			default_style;			// The default ("body")
		par_info			current_par;			// Current paragraph
		short				pretag_font;			// Font index for <pre> tag
		short				pretag_pointsize;		// Pointsize for <pre> tag
		short				codetag_font;			// Font index for <code> tag
		short				codetag_pointsize;		// Pointsize for <code> tag
		long				image_ctr;				// Counts images as they are sent
		long				list_indent_value;		// Value for par indent defining listing level(s)
		long				listing_level;			// Current listing level
		long				indent_level;			// Indent level (!= listing_level if definition list)
		long				listing_code;			// Current listing code

    protected:

    	void OutputImage (memory_ref embed, embed_callback callback, pg_char_ptr alt_text,
    				long alt_text_size);
 		void OutputStyleTags (pg_char_ptr next_char, long remaining_chars);
 		void OutputStyleEndTags (void);
  		void OutputParChanges (void);
 		void StyleExpected (style_info_ptr changes, style_info_ptr previous_style, pg_boolean for_endtags);
		void EndFontTag (void);
        void TerminateList (short code);
		long GetPointLevel (style_info_ptr style, long PG_FAR *converted_point);
		void WriteListingTag (void);
		// QUALCOMM Begin == Kusuma
		// Added virtual keyword for EmulateTab function
		virtual void EmulateTab (void);
		// QUALCOMM End == Kusuma
		long ValidCR (pg_char_ptr text, long remaining_chars);
		pg_boolean IsPretagStyle (style_info_ptr style);
		pg_boolean ComputeListingCodes (par_info_ptr old_format,
				par_info_ptr new_format,
				long PG_FAR *old_indent, long PG_FAR *new_indent,
				long PG_FAR *old_code, long PG_FAR *new_code);
		void InsertFontError (pg_char_ptr fontface, long size_of_run);
		void InsertPointsizeError (long point, long size_of_run);
		void InsertCharacterError (pg_char the_char);
		void OutputTableTag (void);
		void OutputTableCell (void);

    	memory_ref			params_ref;
    	memory_ref			converter;
    	long				real_text_out;
    	long				font_tag_enable;
    	long				total_chars_in;
    	long				column_ctr;			// Counter of current column when doing tables
    	long				row_char_count;		// Number of chars output this row.
    	long				chars_since_cr;		// Number of chars since last CR
    	pg_boolean			new_par;			// TRUE if text-out entry is a new paragraph
    	pg_boolean			doing_table;		// Processing table
    	pg_boolean			styles_output;		// TRUE if we already processed styles changes
    	pg_boolean			pretag_enable;		// Pretag is ON
    	short				cell_attribute;		// Which cell type is being output.
    	short				space_width;		// Space width character
    	short				space_last_font;	// Last font used to calculate space width
    	short				space_last_point;	// Last pointsize used to calculate space width

		// QUALCOMM Begin - Wenger
		bool				m_bIsForClipboard;	// Whether or not export is being prepared for the clipboard
		// QUALCOMM End - Wenger
    
    public:

		virtual void ConvertURL (pg_char_ptr input, pg_char_ptr output);
		virtual void PreProcessStyleChange (style_info_ptr old_style, style_info_ptr new_style,
				long doc_position);
		virtual void PreProcessParChange (par_info_ptr old_style, par_info_ptr new_style,
				long doc_position);
};



typedef PaigeImportFilter PG_FAR *PaigeImportObject;
typedef PaigeExportFilter PG_FAR *PaigeExportObject;


#endif

