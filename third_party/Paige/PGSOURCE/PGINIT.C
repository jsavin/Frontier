/* This small file initializes all the default function pointers. On Macintosh,
if this code exists in its own segment, you will be able to do UnloadSeg on
any PAIGE segment. This is because all addresses will be taken from A5(function)
instead of using <some> function defined within the same segment.
Copyright 1994 by DataPak Software, Inc.  All rights reserved. Software by GAR. */

#include "Paige.h"
#include "machine.h"   
#include "DefProcs.h"
#include "pgFiles.h"
#include "pgSubRef.h"
#include "pgExceps.h"

#ifdef MAC_PLATFORM
#pragma segment initpg
#endif


/* pgInit initializes Paige globals and anything required for the machine.
The mem_globals is a pointer to memory globals which must have been previously
initialized with pgMemStartup!   */

extern PG_PASCAL (void) pgInit (pg_globals_ptr globals, const pgm_globals_ptr mem_globals)
{
	short		table_index;

	pgFillBlock(globals, sizeof(pg_globals), 0);
	
	globals->mem_globals = mem_globals;	
	globals->pg_extend = pgExtendProc;
	pgSetStandardProcs(globals);
	mem_globals->free_memory = pgCacheFree;
	pgMachineInit(globals);

/* 2.0 feature, initialize width tables: */
	
	PG_TRY (mem_globals) {
	
		for (table_index = 0; table_index < WIDTH_QTY; ++table_index) {
			
			globals->width_tables[table_index].positions = MemoryAlloc(mem_globals, sizeof(long), 0, 16);
			globals->width_tables[table_index].types = MemoryAlloc(mem_globals, sizeof(short), 0, 0);
			SetMemoryPurge(globals->width_tables[table_index].positions, 0x00E0, FALSE);
			SetMemoryPurge(globals->width_tables[table_index].types, 0x00E0, FALSE);
			globals->width_tables[table_index].offset = -1;
			globals->width_tables[table_index].mem_id = 0;
			globals->width_tables[table_index].used_ctr = 0;
		}
	};
	
	PG_CATCH {
		pgFailure(mem_globals, mem_globals->last_error, 0);
	};
	
	PG_ENDTRY;
}



/* pgShutdown de-allocates anything it created at startup time */

PG_PASCAL (void) pgShutdown (const pg_globals_ptr globals)
{
	pgMachineShutdown(globals);
}

/* pgSetStandardProcs initialize all the standard functions which it normally
sets during pgInit. Only specialized features will need to call this. */

PG_PASCAL (void) pgSetStandardProcs (pg_globals_ptr globals)
{
	register pg_globals_ptr	global_vars;
	
	global_vars = globals;		/* "register" forces much less code! */

	global_vars->def_hooks.line_init = pgInitLineProc;
	global_vars->def_hooks.adjust_proc = pgLineAdjustProc;
	global_vars->def_hooks.validate_line = pgLineValidate;
	global_vars->def_hooks.parse_line = pgLineParse;
	global_vars->def_hooks.hyphenate = pgHyphenateProc;
	global_vars->def_hooks.boundary_proc = pgParBoundaryProc;
	global_vars->def_hooks.hilite_rgn = pgHiliteProc;
	global_vars->def_hooks.hilite_draw = pgDrawHiliteProc;
	global_vars->def_hooks.cursor_proc = pgDrawCursorProc;
	global_vars->def_hooks.idle_proc = pgIdleProc;
	global_vars->def_hooks.load_proc = pgTextLoadProc;
	global_vars->def_hooks.break_proc = pgTextBreakProc;
	global_vars->def_hooks.offset_proc = pgPt2OffsetProc;
	global_vars->def_hooks.smart_quotes = pgSmartQuotesProc;
	global_vars->def_hooks.font_proc = pgInitFont;
	global_vars->def_hooks.special_proc = pgSpecialCharProc;
	global_vars->def_hooks.auto_scroll = pgAutoScrollProc;
	global_vars->def_hooks.adjust_scroll = pgScrollAdjustProc;
	global_vars->def_hooks.draw_scroll = pgDrawScrollProc;
	global_vars->def_hooks.page_proc = pgDrawPageProc;
	global_vars->def_hooks.undo_enhance = pgEnhanceUndo;
	global_vars->def_hooks.bitmap_proc = pgBitmapModifyProc;
	global_vars->def_hooks.wait_proc = pgWaitProc;
	global_vars->def_hooks.container_proc = pgModifyContainerProc;
	global_vars->def_hooks.paginate_proc = pgPostPaginateProc;
	global_vars->def_hooks.text_increment = pgTextIncrementProc;
	global_vars->def_hooks.click_proc = pgExamineClickProc;
	global_vars->def_hooks.set_device = pgSetGrafDevice;
	global_vars->def_hooks.page_modify = pgPageModify;
	global_vars->def_hooks.wordbreak_proc = pgBreakInfoProc;
	global_vars->def_hooks.charclass_proc = pgCharClassProc;
	global_vars->def_hooks.insert_query = pgInsertQuery;
	global_vars->def_hooks.subset_glitter = pgSubsetGlitter;
	global_vars->def_hooks.background_image = pgBkImageProc;

/* Default style functions: */

	global_vars->def_style.procs.init = pgStyleInitProc;
	global_vars->def_style.procs.install = pgInstallFont;
	global_vars->def_style.procs.measure = pgMeasureProc;
	global_vars->def_style.procs.merge = pgMergeProc;
	global_vars->def_style.procs.char_info = pgCharInfoProc;
	global_vars->def_style.procs.draw = pgDrawProc;
	global_vars->def_style.procs.duplicate = pgDupStyleProc;
	global_vars->def_style.procs.delete_style = pgDeleteStyleProc;
	global_vars->def_style.procs.alter_style = pgAlterStyleProc;
	global_vars->def_style.procs.save_style = pgSaveStyleProc;
	global_vars->def_style.procs.copy_text = pgCopyTextProc;
	global_vars->def_style.procs.delete_text = pgDeleteTextProc;
	global_vars->def_style.procs.insert_proc = pgSetupInsertProc;
	global_vars->def_style.procs.track_ctl = pgTrackCtlProc;
	global_vars->def_style.procs.activate_proc = pgActivateStyleProc;
	global_vars->def_style.procs.bytes_to_unicode = pgBytesToUnicode;
	global_vars->def_style.procs.unicode_to_bytes = pgUnicodeToBytes;

/* Default paragraph functions: */


	global_vars->def_par.procs.line_proc = pgLineMeasureProc;
	global_vars->def_par.procs.line_glitter = pgLineGlitterProc;
	global_vars->def_par.procs.tab_width = pgTabMeasureProc;
	global_vars->def_par.procs.tab_draw = pgTabDrawProc;
	global_vars->def_par.procs.duplicate = pgDupParProc;
	global_vars->def_par.procs.delete_par = pgDeleteParProc;
	global_vars->def_par.procs.alter_par = pgAlterParProc;


/* Misc. defaults */

	global_vars->pg_extend(global_vars, pg_std_procs);
}



/* pgInitStandardHandlers sets all the default pg_handler's for file read/write.
If handlers currently exist, only the "defaults" are replaced (i.e., handlers >=
CUSTOM_HANDLER_KEY are not affected). 
Update 12/13/93, no longer use "line_key" as a standard to save memory space
(which is why we set handler array to PLATFORM_SPECIFIC_KEY - 1).  */
/* 6 jan 95 - embedded_item_key is not standard and made so as not 
to be so memory compute dependant. Also this is safer - TRS/OITC */

PG_PASCAL (void) pgInitStandardHandlers (pg_globals_ptr globals)
{
	register pg_handler_ptr		handlers;
	register pg_file_key		key_ctr;

	if (!globals->file_handlers)
		globals->file_handlers = MemoryAlloc(globals->mem_globals, sizeof(pg_handler),
			0, PLATFORM_SPECIFIC_KEY - UNUSED_KEY_QTY);
	else
		SetMemorySize(globals->file_handlers, 0);
	
	for (key_ctr = paige_key; key_ctr < PLATFORM_SPECIFIC_KEY; ++key_ctr) {
		
		if (key_ctr != line_key && key_ctr != embedded_item_key && key_ctr != format_init_key) {
		
			handlers = (pg_handler_ptr) AppendMemory (globals->file_handlers, 1, FALSE);
		
			pgInitOneHandler(handlers, key_ctr);
			
			UnuseMemory(globals->file_handlers);
		}
	}

// Add hyperlink handlers:

	handlers = (pg_handler_ptr) AppendMemory (globals->file_handlers, 2, FALSE);
	pgInitOneHandler(handlers, hyperlink_key);
	pgInitOneHandler(&handlers[1], hyperlink_target_key);
	UnuseMemory(globals->file_handlers);
}


/* pgInitOneHandler initializes a handler record to its defaults. */

PG_PASCAL (void) pgInitOneHandler (pg_handler_ptr handler, pg_file_key key)
{
	handler->key = key;
	handler->flags = HAS_STANDARD_WRITE_HANDLER | HAS_STANDARD_READ_HANDLER;
	handler->read_handler = pgReadHandlerProc;
	handler->write_handler = pgWriteHandlerProc;
	handler->read_data_proc = pgStandardReadProc;
	handler->write_data_proc = pgStandardWriteProc;
}

