/* This file handles memory management at a high enough level to be portable.
Actual machine-specific code has handled in "machinememory.c," so if you
are porting this code, change that file (not this one). 

Update 12/12/93 Exception handling implemented. Note that all errors at this
level call pgFailure to propogate upwards (as opposed to returning NULL or some
other error). This is so upper level code can detect all memory errors with a 
single "PG_CATCH". */

/* Updated by TR Shaw, OITC, Inc. 25 Feb/20 Apr 1994 for C/C++/Pascal linkage and PPC control
	and for inlines, size and speed */

/* Updated by TR Shaw, OITC, Inc. 30 Apr 1995 for cross platform I/O */

/* 2025-11-16 Codex: Add FRONTIER_TESTS handle tracing for doc-info debugging. */

#include "pgMemMgr.h"
#include "pgMTraps.h"
#include "pgExceps.h"
#include "pgErrors.h"
#include "pgIO.h"
#include "pgUtils.h"
#include "pgOSUtl.h"
#if defined(FRONTIER_TESTS)
#include <stdlib.h>
#include <stdio.h>
extern memory_ref frontier_docinfo_subject_ref;
#endif
#if defined(FRONTIER_TESTS)
#include <stdio.h>
#include <dlfcn.h>
#include <execinfo.h>
#include <string.h>
#if defined(__has_builtin)
#if __has_builtin(__builtin_return_address)
#define PG_TRACE_CALLER() __builtin_return_address(0)
#else
#define PG_TRACE_CALLER() NULL
#endif
#else
#define PG_TRACE_CALLER() NULL
#endif
#endif

#ifdef MAC_PLATFORM

#pragma segment pgbasic4

#include <Files.h>

#endif

#define AVAILABLE_LIST_TERMINATOR		0x7FFFFFFF	/* File available list terminator */
#define PAIR_SIZE						(sizeof(long) * 2)	/* Size of available pair */

// Local record used in Windows version for VM:

typedef struct {
	mem_rec			record;
	long			swap_file_offsets[2];
} purged_mem_rec, PG_FAR *purged_mem_ptr;


#if defined(FRONTIER_TESTS)
/* 2025-11-16 Codex: Trace selected handles (doc-info) to debug corrupt frees. */
#define PG_HANDLE_TRACE_LIMIT 64
typedef struct pg_handle_trace_entry {
	memory_ref ref;
	const char *tag;
} pg_handle_trace_entry;

static pg_handle_trace_entry pg_handle_trace_table[PG_HANDLE_TRACE_LIMIT];

static const char *pg_trace_handle_find(memory_ref ref)
{
	if (!ref)
		return NULL;
	for (int i = 0; i < PG_HANDLE_TRACE_LIMIT; ++i)
		if (pg_handle_trace_table[i].ref == ref)
			return pg_handle_trace_table[i].tag;
	return NULL;
}

void pg_trace_handle_forget(memory_ref ref)
{
	if (!ref)
		return;
	for (int i = 0; i < PG_HANDLE_TRACE_LIMIT; ++i)
		if (pg_handle_trace_table[i].ref == ref) {
			pg_handle_trace_table[i].ref = MEM_NULL;
			pg_handle_trace_table[i].tag = NULL;
			return;
		}
}

#if defined(__cplusplus)
extern "C" {
#endif
void pg_trace_handle_watch(memory_ref ref, const char *tag)
{
	if (!ref || tag == NULL)
		return;
	for (int i = 0; i < PG_HANDLE_TRACE_LIMIT; ++i)
		if (pg_handle_trace_table[i].ref == ref) {
			pg_handle_trace_table[i].tag = tag;
			return;
		}
	for (int i = 0; i < PG_HANDLE_TRACE_LIMIT; ++i)
		if (pg_handle_trace_table[i].ref == MEM_NULL) {
			pg_handle_trace_table[i].ref = ref;
			pg_handle_trace_table[i].tag = tag;
			fprintf(stdout, "[pg-handle] watch ref=%p tag=%s\n", (void *)ref, tag);
			fflush(stdout);
			return;
		}
}
#if defined(__cplusplus)
}
#endif

const char *pg_trace_handle_tag(memory_ref ref)
{
	return pg_trace_handle_find(ref);
}

long pg_trace_handle_access(memory_ref ref)
{
	if (!ref)
		return -999;
	mem_rec_ptr data = pgMemoryPtr(ref);
	long access = data ? (long)data->access : -999;
	pgFreePtr(ref);
	return access;
}

static void pg_trace_handle_pre_event(const char *event, memory_ref ref, void *caller)
{
	if (!event || !ref)
		return;
	const char *tag = pg_trace_handle_find(ref);
	if (!tag)
		return;
#if defined(UNIX_PLATFORM)
	pg_unix_slot slot = *((pg_unix_slot *)ref);
	void *slot_ptr = (void *)slot;
	void *payload = slot ? (void *)pg_unix_payload(slot) : NULL;
	size_t size = slot ? pg_unix_handle_size(slot) : 0;
#else
	void *slot_ptr = NULL;
	void *payload = NULL;
	size_t size = 0;
#endif
	fprintf(stdout,
	        "[pg-handle] %s ref=%p tag=%s slot=%p payload=%p size=%zu caller=%p\n",
	        event,
	        (void *)ref,
	        tag,
	        slot_ptr,
	        payload,
	        (size_t)size,
	        caller);
	fflush(stdout);
#if defined(FRONTIER_TESTS)
	if (slot_ptr) {
		mem_rec_ptr trace_hdr = pgMemoryPtr(ref);
		if (trace_hdr) {
			fprintf(stdout,
			        "[pg-handle] %s-header ref=%p tag=%s globals=%p master_index=%p rec_size=%d num_recs=%zu access=%ld qty=%ld purge=0x%02x\n",
			        event,
			        (void *)ref,
			        tag,
			        (void *)trace_hdr->globals,
			        (void *)trace_hdr->master_index,
			        (int)trace_hdr->rec_size,
			        (size_t)trace_hdr->num_recs,
			        (long)trace_hdr->access,
			        (long)trace_hdr->qty_used,
			        (int)trace_hdr->purge);
			fflush(stdout);
			pgFreePtr(ref);
		} else {
			fprintf(stdout,
			        "[pg-handle] %s-header ref=%p tag=%s header=INVALID\n",
			        event,
			        (void *)ref,
			        tag);
			fflush(stdout);
		}
	}
#endif
}

static void pg_trace_handle_event(const char *event, memory_ref ref, mem_rec_ptr data, void *caller)
{
	const char *tag = pg_trace_handle_find(ref);
	if (!tag || !event)
		return;
	const char *symbol = NULL;
	long offset = 0;
	if (caller) {
		Dl_info info;
		if (dladdr(caller, &info) && info.dli_sname) {
			symbol = info.dli_sname;
			offset = (long)((char *)caller - (char *)info.dli_saddr);
		}
	}
	fprintf(stdout,
		"[pg-handle] %s ref=%p tag=%s access=%ld size=%ld purge=0x%02x rec_size=%d qty=%ld caller=%p",
		event,
		(void *)ref,
		tag,
		data ? (long)data->access : 0L,
		data ? (long)pgMemorySize(ref) : 0L,
		data ? (int)data->purge : 0,
		data ? (int)data->rec_size : 0,
		data ? (long)data->qty_used : 0L,
		caller);
	if (symbol)
		fprintf(stdout, " (%s+0x%lx)", symbol, offset);
	fprintf(stdout, "\n");
	fflush(stdout);
	if (!strcmp(event, "unuse-underflow")) {
		void *stack[16];
		int depth = backtrace(stack, 16);
		backtrace_symbols_fd(stack, depth, fileno(stdout));
		fflush(stdout);
	}
}
#endif

typedef enum {
	access_dont_care,
	access_positive,
	access_zero,
	access_one,
	access_one_only,
	access_nonzero
} access_verb;

typedef void PG_FAR *mem_proc;

static void get_memory_record (memory_ref ref, mem_rec_ptr record);
static memory_ref suggest_purge (pgm_globals_ptr globals, short purge_priority,
		memory_ref mask_ref);
static void purge_excess_memory (pgm_globals_ptr globals, memory_ref mask_ref);
static void reload_ref (memory_ref ref);
static short get_access_ctr (memory_ref ref);
static size_t find_available_space (pgm_globals_ptr globals, memory_ref avail_ref,
		size_t wanted_space);
static memory_ref free_available_space (pgm_globals_ptr globals, memory_ref the_avail_ref,
		size_t saved_position, size_t end_position);
static memory_ref cleanup_available_list (pgm_globals_ptr globals, memory_ref the_avail_ref);
static short two_available_pairs_alike (memory_ref avail_ref, size_t PG_FAR *offset);
static pg_boolean quick_ref_check (memory_ref ref);
static void extend_master_list (pgm_globals_ptr mem_globals);
static void unuse_all_memory (pgm_globals_ptr mem_globals, long memory_id, pg_boolean failed_unuse);
static void dispose_all_memory (pgm_globals_ptr mem_globals, long memory_id, pg_boolean dispose_failed);
static void resize_purged_memory (memory_ref ref);

#if defined(UNIX_PLATFORM)
typedef struct pg_unix_master_table {
	size_t		capacity;
	pg_handle		*entries;
} pg_unix_master_table;

static pg_unix_master_table *pg_unix_master_from_handle(pg_handle handle)
{
	return (pg_unix_master_table *)handle;
}

static master_list_ptr pg_unix_master_list(pg_handle handle)
{
	pg_unix_master_table *table = pg_unix_master_from_handle(handle);
	return table ? (master_list_ptr)table->entries : NULL;
}

static size_t pg_unix_master_capacity(pg_handle handle)
{
	pg_unix_master_table *table = pg_unix_master_from_handle(handle);
	return table ? table->capacity : 0;
}

static pg_handle pg_unix_create_master_table(size_t capacity)
{
	pg_unix_master_table *table = (pg_unix_master_table *)malloc(sizeof(*table));
	if (!table)
		return NULL;
	table->entries = (pg_handle *)calloc(capacity, sizeof(pg_handle));
	if (!table->entries) {
		free(table);
		return NULL;
	}
	table->capacity = capacity;
	return (pg_handle)table;
}

static void pg_unix_destroy_master_table(pg_handle handle)
{
	pg_unix_master_table *table = pg_unix_master_from_handle(handle);
	if (!table)
		return;
	free(table->entries);
	free(table);
}
#endif


#ifdef PG_DEBUG

#ifdef PG_DEBUG_STATICS

pgm_globals_ptr	debug_globals;

#endif

static pg_boolean check_bad_ref (memory_ref ref, short expected_access);
static pg_boolean check_range (memory_ref ref, long rec_access, long num_recs);
static void append_checksum (memory_ref ref);
static short validate_address (void PG_FAR *address);
static pg_error validate_ref (memory_ref ref);

#endif




/* pgMemStartup must initialize pgm_globals, including the master block buffer,
and whatever else is necessary for the working platform */

PG_PASCAL (void) pgMemStartup (pgm_globals_ptr mem_globals, long max_memory)
{
	pgFillBlock(mem_globals, sizeof(pgm_globals), 0);

	mem_globals->signature = CHECKSUM_SIG;
	mem_globals->debug_proc = pgDebugProc;
	
#ifdef PG_DEBUG
#ifdef PG_DEBUG_STATICS
	debug_globals = mem_globals;
#endif
#endif

	PG_TRY(mem_globals) {
	
		mem_globals->spare_tire = pgAllocMemory(SPARE_TIRE_SIZE);
#if defined(UNIX_PLATFORM)
		mem_globals->master_handle = pg_unix_create_master_table(DEF_MASTER_QTY);
		pgFailZero(mem_globals, PG_LONGWORD(generic_var)mem_globals->master_handle);
		mem_globals->master_list = pg_unix_master_list(mem_globals->master_handle);
#else
		mem_globals->master_handle = pgAllocMemoryClear(DEF_MASTER_QTY * MASTER_ENTRY_SIZE);
		pgFailZero(mem_globals, PG_LONGWORD(generic_var)mem_globals->master_handle);
#ifdef WINDOWS_PLATFORM
// Keep the master list locked because these entries ARE the memory_ref(s):
		mem_globals->master_list = (master_list_ptr)GlobalLock(mem_globals->master_handle);
#endif
#endif
		mem_globals->debug_flags = DEF_DEBUG_BITS;
		mem_globals->next_mem_id = mem_globals->current_id = MINIMUM_ID;
		mem_globals->purge_threshold = PURGE_THRESHOLD;
	
		mem_globals->total_unpurged = (DEF_MASTER_QTY * MASTER_ENTRY_SIZE) + SPARE_TIRE_SIZE;
		
		if (!(mem_globals->max_memory = max_memory))
			mem_globals->max_memory = 0x7FFFFFFF;
	}
	
	PG_CATCH {
		
		pgMemShutdown(mem_globals);
		pgFailure(mem_globals, NO_MEMORY_ERR, 0);
	}
	
	PG_ENDTRY;
}


/* pgMemShutdown de-allocates anything it allocated during the course of things.
This includes de-allocating every ref in the master list.  */

PG_PASCAL (void) pgMemShutdown (pgm_globals_ptr mem_globals)
{
	master_list_ptr		master_list;
	long				master_qty;

#ifdef PG_DEBUG
		mem_globals->debug_flags &= (~ALL_MEMORY_CHECK);
#endif

	if (mem_globals->purge_info)
		DisposeMemory(mem_globals->purge_info);

#ifdef MAC_PLATFORM

	if (mem_globals->spare_tire)
		DisposeHandle(mem_globals->spare_tire);
	
	if (mem_globals->master_handle) {

		master_qty = GetHandleSize(mem_globals->master_handle) / sizeof(memory_ref);
		HLock(mem_globals->master_handle);
		master_list = (master_list_ptr)*(mem_globals->master_handle);
		
		while (master_qty) {
			
			if (*master_list) {
				
		#ifdef PG_DEBUG
				
				check_bad_ref((memory_ref)*master_list, access_zero);
		#endif
				DisposeHandle(*master_list);
			}
			
			++master_list;
			--master_qty;
		}
	
		DisposeHandle(mem_globals->master_handle);
	}

#endif

#ifdef UNIX_PLATFORM

	if (mem_globals->spare_tire)
		pgFreeMemory(mem_globals->spare_tire);
	
	if (mem_globals->master_handle) {

		master_list = pg_unix_master_list(mem_globals->master_handle);
		master_qty = (long)pg_unix_master_capacity(mem_globals->master_handle);
	
		while (master_qty) {
			if (*master_list) {
#ifdef PG_DEBUG
				check_bad_ref((memory_ref)master_list, access_zero);
#endif
				DisposeMemory((memory_ref)master_list);
			}
			++master_list;
			--master_qty;
		}
	
		pg_unix_destroy_master_table(mem_globals->master_handle);
		mem_globals->master_handle = NULL;
		mem_globals->master_list = NULL;
	}

#endif

#ifdef WINDOWS_PLATFORM

	if (mem_globals->spare_tire)
		GlobalFree(mem_globals->spare_tire);
	
	if (mem_globals->master_handle) {
		HANDLE			next_master, this_master;
		
		next_master = mem_globals->master_handle;
		
		while (next_master) {
			
			master_list = (master_list_ptr)GlobalLock(next_master);
			GlobalUnlock(next_master);
			master_qty = MAX_HANDLE_ENTRIES;
			
			for (master_qty = MAX_HANDLE_ENTRIES; master_qty; ++master_list, --master_qty) {

				if (*master_list) {
					
			#ifdef PG_DEBUG
					check_bad_ref((memory_ref)master_list, access_zero);
			#endif
					GlobalFree(*master_list);
				}
			}
            
            this_master = next_master;
			next_master = *master_list;
		
		// Reduce the lock counter to zero (we started it at one during conception):
		    
	#ifndef PG_DEBUG
	
			GlobalUnlock(this_master);
			GlobalFree(this_master);
	#endif
	
		}
	#ifdef PG_DEBUG
	    
	    next_master = mem_globals->master_handle;
		
		while (next_master) {
			
			master_list = (master_list_ptr)GlobalLock(next_master);
			GlobalUnlock(next_master);
			
			this_master = next_master;
			next_master = master_list[MAX_HANDLE_ENTRIES];
		
			GlobalUnlock(this_master);
			GlobalFree(this_master);
		}
		
	#endif
	}
	
#endif
}



/* MemoryDuplicate makes a copy of a memory_ref (a new memory_ref but identical
size and contents) */

PG_PASCAL (memory_ref) MemoryDuplicate (memory_ref src_ref)
{
	memory_ref		dup_result;
	void PG_FAR		*target_data, *src_data;
	mem_rec			src_rec;
	size_t			byte_size;

	#ifdef PG_DEBUG
	if (check_bad_ref(src_ref, access_dont_care))
		return	MEM_NULL;

	#endif
	
	get_memory_record(src_ref, &src_rec);
	byte_size = pgGetByteSize(src_rec.rec_size, src_rec.num_recs);
	
	dup_result = MemoryAlloc(src_rec.globals, src_rec.rec_size, src_rec.num_recs,
			src_rec.extend_size);

	if (byte_size) {
		
		src_data = UseMemory(src_ref);
		target_data = UseMemory(dup_result);
		pgBlockMove(src_data, target_data, byte_size);
		
		UnuseMemory(src_ref);
		UnuseMemory(dup_result);
	}

	return	dup_result;
}


/* MemoryDuplicateID is identical to MemoryDuplicate except the mem_id value is forced is the
memory ID. */

PG_PASCAL (memory_ref) MemoryDuplicateID (memory_ref src_ref, long mem_id)
{
	memory_ref		dup_result;
	mem_rec_ptr		data;
	long			use_id;
	
	if ((use_id = mem_id) == 0) {
		
		data = pgMemoryPtr(src_ref);
		use_id = data->mem_id;
		pgFreePtr(src_ref);
	}
	
	dup_result = MemoryDuplicate(src_ref);
	data = pgMemoryPtr(dup_result);
	data->mem_id = use_id;
	pgFreePtr(dup_result);

	return	dup_result;
}


/* MemoryCopy copies the contents from one memory ref to another (the target's
size and record size will become identical) */

PG_PASCAL (void) MemoryCopy (memory_ref src_ref, memory_ref target_ref)
{
	mem_rec				src_rec;
	mem_rec_ptr			target_ptr;
	size_t				src_size;
	
	#ifdef PG_DEBUG
	if (check_bad_ref(src_ref, access_positive))
		return;
		
	if (check_bad_ref(target_ref, access_zero))
		return;

	#endif
	
	get_memory_record(src_ref, &src_rec);
	target_ptr = pgMemoryPtr(target_ref);
	target_ptr->rec_size = src_rec.rec_size;
	pgFreePtr(target_ref);

	SetMemorySize(target_ref, src_rec.num_recs);

	if ((src_size = pgGetByteSize(src_rec.rec_size, src_rec.num_recs)) != 0) {
	
		pgBlockMove(UseMemory(src_ref), UseMemory(target_ref), src_size);

		UnuseMemory(src_ref);
		UnuseMemory(target_ref);
	}
}


/* UseMemory returns an address to a data block and sets the block to be non-
relocatable. This can be nested since we maintain an "access counter."  */

PG_PASCAL (void PG_FAR*) UseMemory (memory_ref ref)
{
	register mem_rec_ptr	data;
#ifdef PG_DEBUG
	pgm_globals_ptr			globals;

	if (check_bad_ref(ref, access_positive))
		return	NULL;

	data = pgMemoryPtr(ref);
	globals = data->globals;
	
	if (globals->debug_check)
		if (globals->debug_check == ref)
			if ((data->access + 1) == globals->debug_access)
				globals->debug_proc(USER_BREAK_ERR, ref);
	pgFreePtr(ref);
#endif
	
	reload_ref(ref);
	pgLockMemory(ref);
	data = pgMemoryPtr(ref);

	++data->access;

#ifdef PG_DEBUG
	if (data->purge & PURGED_FLAG)
		data->globals->debug_proc(PURGED_MEMORY_ERR, ref);
	if ((long)pgMemorySize(ref) < (long)(pgGetByteSize(data->rec_size, data->real_num_recs) + sizeof(mem_rec)))
        data->globals->debug_proc(PURGED_MEMORY_ERR, ref);
#endif
	++data->qty_used;
	++data;
#if defined(FRONTIER_TESTS)
	pg_trace_handle_event("use", ref, data - 1, PG_TRACE_CALLER());
#endif
	
	return	(void PG_FAR *) data;
}


/* UseForLongTime is same as UseMemory except the machine equiv to "MoveHi" is
invoked before locking the first time. */

PG_PASCAL (void PG_FAR*) UseForLongTime(memory_ref ref)
{
	#ifdef PG_DEBUG
	if (check_bad_ref(ref, access_positive))
		return	NULL;

	#endif
	
	if (!get_access_ctr(ref))
		pgLockMemoryHi(ref);

	return	UseMemory(ref);
}


/* UseMemoryRecord is similar to UseMemory except a pointer to a specific
record in the memory ref is used. ** Version 0.01 just does a MemUse and
increments **. If first_use = TRUE, the access counter is incremented
(otherwise it is not). The seq_recs_used param indicates how many sequential
records, beginning with wanted_rec, will be accessed:  if zero, only wanted_rec
is guaranteed, otherwise only wanted_rec through wanted_rec + seq_recs_used
are guaranteed. If seq_recs_used is -1, all records are guaranteed. (The purpose
if this is to allow partial loads from purged refs).  */

PG_PASCAL (void PG_FAR*) UseMemoryRecord (memory_ref ref, size_t wanted_rec,
			size_t seq_recs_used, pg_boolean first_use)
{
	register mem_rec_ptr	data;
	register pg_bits8_ptr	result;

#ifdef MAC_PLATFORM
#pragma unused (seq_recs_used)
#endif

	if (first_use)
		UseMemory(ref);

	#ifdef PG_DEBUG
	else
		if (check_bad_ref(ref, access_nonzero))
			return	NULL;
	#endif

	data = pgMemoryPtr(ref);
	pgFreePtr(ref);	/* Windows note: should still be "in use" */
	
	#ifdef PG_DEBUG
		check_range(ref, wanted_rec, 1);	
	#endif
	
	result = (pg_bits8_ptr) data;
	result += sizeof(mem_rec);

	if (wanted_rec)
		result += pgGetByteSize(data->rec_size, wanted_rec);

	return	(void PG_FAR*) result;
}


/* GetMemoryRecord fills in a record #n from a ref without changing the access
counter. Note that a pointer to the record is provided by the app and it must
contain enough space. */

PG_PASCAL (void) GetMemoryRecord (memory_ref ref, long wanted_rec,
			void PG_FAR *record)
{
	register mem_rec_ptr	data;
	pg_bits8_ptr			bytes;

	#ifdef PG_DEBUG
	
	if (check_bad_ref(ref, access_dont_care))
		return;
	if (check_range(ref, wanted_rec, 1))
		return;

	#endif
	
	reload_ref(ref);
	data = pgMemoryPtr(ref);
	bytes = (pg_bits8_ptr) data;
	bytes += sizeof(mem_rec);
 
	if (wanted_rec)
		bytes += pgGetByteSize(data->rec_size, wanted_rec);

	pgBlockMove(bytes, record, data->rec_size);
	
	pgFreePtr(ref);
}


/* SetMemorySize changes the size of a memory_ref. */

PG_PASCAL (void) SetMemorySize (memory_ref ref_to_size, size_t wanted_size)
{
	register mem_rec_ptr		data;
	volatile pgm_globals_ptr	globals;
	volatile memory_ref			ref;
	size_t						added_size, extra_size, required_purge;
	pg_error					err;
	
	ref = ref_to_size;

	#ifdef PG_DEBUG

	if (check_bad_ref(ref, access_zero))
		return;

	#endif
	
	data = pgMemoryPtr(ref);
	globals = data->globals;
	globals->last_error = NO_ERROR;

	if (data->num_recs == wanted_size) {
		
		pgFreePtr(ref);
		return;
	}

	if (data->purge & PURGED_FLAG) {
		
		data->num_recs = data->real_num_recs = wanted_size;
		pgFreePtr(ref);
		return;
	}
	
	if (wanted_size > data->real_num_recs) {
		
		extra_size = pgGetByteSize(data->rec_size, data->real_num_recs) + NON_APP_SIZE;
		data->real_num_recs = wanted_size + data->extend_size;
		added_size = pgGetByteSize(data->rec_size, data->real_num_recs) + NON_APP_SIZE;
		extra_size = added_size - extra_size;
		required_purge = extra_size + data->globals->purge_threshold;
		
		pgFreePtr(ref);
		
		PG_TRY(globals) {

			for (;;) {

	#ifdef PG_DEBUG
				
				if (added_size < sizeof(mem_rec))
					globals->debug_proc(BAD_LINK_ERR, ref);
	#endif
				pgResizeMemory(globals, ref, added_size);
	
				if (pgMemorySize(ref) >= added_size)
					break;
					
				pgFailNIL(globals, (void PG_FAR*) globals->spare_tire);
		
				pgFreeMemory(globals->spare_tire);
				globals->spare_tire = MEM_NULL;
				globals->total_unpurged -= SPARE_TIRE_SIZE;
		
				err = MemoryPurge(globals, required_purge, ref);
				pgFailError(globals, err);
				
				globals->last_error = NO_ERROR;
			}

			globals->total_unpurged += extra_size;
			
			if (!globals->spare_tire)
				globals->spare_tire = pgAllocMemory(SPARE_TIRE_SIZE);
	
			purge_excess_memory(globals, ref);
		}
		
		PG_CATCH {
			
			pgFailure(globals, globals->last_error, (long) ref);
		}
		
		PG_ENDTRY;
		
		data = pgMemoryPtr(ref);
	}

	data->num_recs = wanted_size;
	pgFreePtr(ref);

	#ifdef PG_DEBUG
		
	append_checksum(ref);

	#endif
}

PG_PASCAL (void) SetMemoryExtendSize (memory_ref ref, short extend_size)
{
	mem_rec_ptr		data;

	#ifdef PG_DEBUG

	if (check_bad_ref(ref, access_dont_care))
		return;

	#endif
	
	data = pgMemoryPtr(ref);
	data->extend_size = extend_size;		
	pgFreePtr(ref);
}

PG_PASCAL (short) GetMemoryExtendSize (memory_ref ref)
{
	mem_rec_ptr		data;
	short			result;
	
	#ifdef PG_DEBUG

	if (check_bad_ref(ref, access_dont_care))
		return 0;

	#endif
	
	data = pgMemoryPtr(ref);
	result = data->extend_size;		
	pgFreePtr(ref);
	
	return	result;
}

/* DetachMemory removes the memory_ref from the master list and returns its machine-specific
memory allocation (Mac Handle or Windows HANDLE). If it is locked (in use) it is unlocked
before returning. */

PG_PASCAL (pg_handle) DetachMemory (memory_ref ref)
{
	register master_list_ptr		ref_storage;
	register pgm_globals_ptr		globals;
	register size_t					storage_index;
	mem_rec_ptr						data;
	short							access;
	pg_handle						result;
	long							real_bytesize;

	#ifdef PG_DEBUG
	if (check_bad_ref(ref, access_dont_care))
		return MEM_NULL;
	#endif
	
	data = pgMemoryPtr(ref);
	globals = data->globals;
	storage_index = data->master_index;
	access = data->access;

	if (data->purge & PURGED_FLAG)
		globals->purge(ref, globals, dispose_purge);

	pgFreePtr(ref);
	
	while (access) {
		
		UnuseMemory(ref);
		--access;
	}

	real_bytesize = pgMemorySize(ref);
	globals->total_unpurged -= real_bytesize;

#ifdef MAC_PLATFORM
	ref_storage = (master_list_ptr) *(globals->master_handle);
	ref_storage += storage_index;
#endif

#ifdef WINDOWS_PLATFORM
	ref_storage = (master_list_ptr)storage_index;
#endif

#ifdef UNIX_PLATFORM
	ref_storage = (master_list_ptr)storage_index;
#endif

	result = *ref_storage;
	*ref_storage = NULL;
	
	return	result;
}


/* DisposeMemory disposes the memory_ref allocation */

PG_PASCAL (memory_ref) DisposeMemory (memory_ref ref)
{
	register master_list_ptr		ref_storage;
	register pgm_globals_ptr		globals;
	register size_t					storage_index;
	mem_rec_ptr						data;
	long							disposed_size;

	#ifdef PG_DEBUG
	if (check_bad_ref(ref, access_zero))
		return MEM_NULL;
	#endif

#if defined(FRONTIER_TESTS)
	pg_trace_handle_pre_event("dispose-pre", ref, PG_TRACE_CALLER());
#endif
	data = pgMemoryPtr(ref);
	globals = data->globals;
	storage_index = data->master_index;
#if defined(FRONTIER_TESTS)
	pg_trace_handle_event("dispose", ref, data, PG_TRACE_CALLER());
	pg_trace_handle_forget(ref);
#endif
	
	if (data->purge & PURGED_FLAG)
		globals->purge(ref, globals, dispose_purge);

	pgFreePtr(ref);
	disposed_size = pgMemorySize(ref);

#ifdef PG_DEBUG
	
	if (globals->dispose_check)
		if (globals->dispose_check == ref)
			globals->debug_proc(USER_BREAK_ERR, ref);

	if (globals->debug_flags & (FILL_ONES_ENABLE | ALL_ONES_ENABLE)) {
		long			fill_size;
		
		if (globals->debug_flags & ALL_ONES_ENABLE)
			fill_size = disposed_size;
		else
			fill_size = sizeof(mem_rec);

		data = pgMemoryPtr(ref);
		pgFillBlock(data, fill_size, -1);
		pgFreePtr(ref);
	}

#endif

	pgFreeMemory(ref);
	globals->total_unpurged -= disposed_size;

#ifdef MAC_PLATFORM
	ref_storage = (master_list_ptr) *(globals->master_handle);
#ifdef PG_DEBUG
	if (ref_storage[storage_index] != (Handle)ref)
		globals->debug_proc(BAD_LINK_ERR, ref);
#endif
	ref_storage[storage_index] = NULL;
	
	if ((storage_index < globals->next_master) || (globals->next_master == -1))
		globals->next_master = storage_index;

#endif

#ifdef MAC_PLATFORM
#ifdef PG_DEBUG
	if (validate_ref(ref) == NO_ERROR)
		globals->debug_proc(BAD_LINK_ERR, ref);
#endif
#endif

#ifdef WINDOWS_PLATFORM
	ref_storage = (master_list_ptr) storage_index;
	*ref_storage = NULL;
#endif

#ifdef UNIX_PLATFORM
	ref_storage = (master_list_ptr) storage_index;
	*ref_storage = NULL;
#endif

#ifdef PG_DEBUG
	if (globals->debug_flags & ALL_MEMORY_CHECK)
		pgCheckAllMemoryRefs();
#endif
	return MEM_NULL;
}


/* DisposeNonNilMemory disposes ref if it is non-zero. */

PG_PASCAL (memory_ref) DisposeNonNilMemory (memory_ref ref)
{
	if (ref)
		DisposeMemory(ref);
	return MEM_NULL;
}

PG_PASCAL (void) DisposeUsedMemory (memory_ref ref)
{
	#ifdef PG_DEBUG

	if (check_bad_ref(ref, access_one))
		return;

	#endif

	while (get_access_ctr(ref))
		UnuseMemory(ref);

	DisposeMemory(ref);
}

/* InsertMemory inserts a specific amount of space and returns a used pointer
to that new spot. NOTE: In this case, you may have the access counter at ONE
or less, but a debug error will get flagged if access > 1. */

PG_PASCAL (void PG_FAR*) InsertMemory (memory_ref ref, size_t offset, size_t insert_size)
{
	mem_rec_ptr				record;
	pg_bits8_ptr			data;
	size_t					old_size, byte_offset, byte_size;
	short					access, rec_size;

	if (GetMemorySize(ref) == offset)
		return AppendMemory(ref, insert_size, FALSE);	
	
	#ifdef PG_DEBUG
	
	if (check_bad_ref(ref, access_one))
		return	NULL;
	if (check_range(ref, offset, 0))
		return	NULL;
	
	#endif
	
	record = pgMemoryPtr(ref);
	access = record->access;
	rec_size = record->rec_size;
	old_size = record->num_recs;
	pgFreePtr(ref);

	if (access)
		UnuseMemory(ref);

	byte_offset = pgGetByteSize(rec_size, offset);
	byte_size = pgGetByteSize(rec_size, insert_size);

	SetMemorySize(ref, old_size + insert_size);

	data = (pg_bits8_ptr) UseMemory(ref);
	pgMemInsert(ref, byte_offset + sizeof(mem_rec), byte_size,
		pgGetByteSize(rec_size, old_size) + sizeof(mem_rec));

	data += byte_offset;
	
	return	(void PG_FAR*) data;
}


/* AppendMemory adds a specified amount of space and returns a pointer to the
new location. NOTE: In this case, you may have the access counter at ONE
or less, but a debug error will get flagged if access > 1. */

extern PG_PASCAL (void PG_FAR*) AppendMemory (memory_ref ref, size_t append_size,
		pg_boolean zero_fill)
{
	size_t					old_size;
	short					rec_size, access;
	mem_rec_ptr				record;
	register pg_bits8_ptr	data;

	#ifdef PG_DEBUG

	if (check_bad_ref(ref, access_one))
		return	NULL;

	#endif
	
	record = pgMemoryPtr(ref);
	access = record->access;
	old_size = record->num_recs;
	rec_size = record->rec_size;
	pgFreePtr(ref);

	if (access)
		UnuseMemory(ref);

	SetMemorySize(ref, old_size + append_size);
	
	data = (pg_bits8_ptr) UseMemoryRecord(ref, old_size, append_size - 1, TRUE);
	
	if (zero_fill)
		pgFillBlock(data, pgGetByteSize(rec_size, append_size), 0);
	
	return	(void PG_FAR*) data;
}


/* DeleteMemory removes a certain amount of space from a specified offset */

PG_PASCAL (void) DeleteMemory (memory_ref ref, size_t offset, size_t delete_size)
{
	mem_rec_ptr				record;
	pg_short_t				rec_size;
	size_t					old_size, byte_offset, byte_size;

	if (!delete_size)
		return;

	#ifdef PG_DEBUG

	if (check_bad_ref(ref, access_zero))
		return;
	if (check_range(ref, offset, delete_size))
		return;

	#endif

	reload_ref(ref);
	
	record = pgMemoryPtr(ref);
	rec_size = record->rec_size;
	old_size = record->num_recs;
	pgFreePtr(ref);

	byte_size = pgGetByteSize(rec_size, delete_size);
	byte_offset = pgGetByteSize(rec_size, offset);
	
	byte_offset += sizeof(mem_rec);
	pgMemDelete(ref, byte_offset, byte_size, pgGetByteSize(rec_size, old_size)
			+ NON_APP_SIZE);
			
	record = pgMemoryPtr(ref);
	record->num_recs = old_size - delete_size;
	pgFreePtr(ref);
}


/* SetMemoryPurge sets the purge priority and yes/no to save data when
purged for a memory_ref.  The purge status is unaffected if purge_priority
== PURGE_NO_CHANGE.  */

PG_PASCAL (void) SetMemoryPurge (memory_ref ref, short purge_priority,
		pg_boolean no_data_save)
{
	mem_rec_ptr			data;

	#ifdef PG_DEBUG
	if (check_bad_ref(ref, access_dont_care))
		return;
	#endif

	data = pgMemoryPtr(ref);

	if (purge_priority != PURGE_NO_CHANGE) {

		data->purge &= (~PURGE_VALUE_MASK);
		data->purge |= (purge_priority & PURGE_VALUE_MASK);
	}

	if (no_data_save)
		data->purge |= NO_DATA_SAVE_FLAG;
	else
		data->purge &= (~NO_DATA_SAVE_FLAG);
	
	pgFreePtr(ref);
}


/* GetMemoryPurge returns the purge flags status of ref. */

PG_PASCAL (pg_short_t) GetMemoryPurge (memory_ref ref)
{
	mem_rec_ptr			data;
	pg_short_t			result;

	#ifdef PG_DEBUG
	if (check_bad_ref(ref, access_dont_care))
		return	0;
	#endif

	data = pgMemoryPtr(ref);
	result = data->purge;
	pgFreePtr(ref);
	
	return		result;
}


/* DisposeAllMemory disposes all memory_refs whose internal ID is memory_id, or,
if memory_id == 0 then ALL refs are disposed.  */

PG_PASCAL (void) DisposeAllMemory (pgm_globals_ptr mem_globals, long memory_id)
{
	dispose_all_memory(mem_globals, memory_id, FALSE);
}

PG_PASCAL (void) DisposeAllFailedMemory (pgm_globals_ptr mem_globals, long memory_id)
{
	dispose_all_memory(mem_globals, memory_id, FALSE);
}



/* UnuseAllMemory unuses all refs created with memory_id. This is usually
used only after a big failure and I want to "unlock" all refs with a particular
ID code.  */

PG_PASCAL (void) UnuseAllMemory (pgm_globals_ptr mem_globals, long memory_id)
{
	unuse_all_memory(mem_globals, memory_id, FALSE);
}

PG_PASCAL (void) UnuseAllFailedMemory (pgm_globals_ptr mem_globals, long memory_id)
{
	unuse_all_memory(mem_globals, memory_id, TRUE);
}


/* GetAllMemorySize returns the total aggregate size of memory under memory_id.
If return_byte_size is TRUE, physical byte size is returned, otherwise the total
record size(s) are added. */

PG_PASCAL (size_t) GetAllMemorySize (pgm_globals_ptr mem_globals, long memory_id,
			pg_boolean return_byte_size)
{
	register master_list_ptr	storage_list;
	register pgm_globals_ptr	globals = mem_globals;
	register long 				index;
	size_t						mem_id, num_recs, master_list_size;
	memory_ref					ref_in_question;
	size_t						result = 0;
	short						rec_size;
	mem_rec_ptr					data;
	
#ifdef MAC_PLATFORM
	
	master_list_size = GetHandleSize(globals->master_handle) / sizeof(Handle);

	for (index = 0; index < master_list_size; ++index) {

		storage_list = (master_list_ptr) *(globals->master_handle);
		ref_in_question = (memory_ref)storage_list[index];

		if (ref_in_question) {
			
			data = (mem_rec_ptr) *((Handle)ref_in_question);
			mem_id = data->mem_id;
			rec_size = data->rec_size;
			num_recs = data->num_recs;

			if ((!memory_id) || (mem_id == memory_id)) {
			
				if (return_byte_size)
					result += pgGetByteSize(rec_size, num_recs);
				else
					result += num_recs;
			}
		}
	}
#endif

#ifdef WINDOWS_PLATFORM
	{
		HANDLE			next_master;
		
		next_master = globals->master_handle;
		
		while (next_master) {
			
			storage_list = (master_list_ptr)GlobalLock(next_master);
			GlobalUnlock(next_master);
			master_list_size = MAX_HANDLE_ENTRIES;

			for (index = 0; index < master_list_size; ++storage_list, ++index) {

				if (*storage_list) {
					
					ref_in_question = (memory_ref)storage_list;
					data = (mem_rec_ptr) GlobalLock(*storage_list);
					mem_id = data->mem_id;
					rec_size = data->rec_size;
					num_recs = data->num_recs;
					GlobalUnlock(*storage_list);

					if ((!memory_id) || (mem_id == memory_id)) {
					
						if (return_byte_size)
							result += pgGetByteSize(rec_size, num_recs);
						else
							result += num_recs;
					}
				}
			}

			next_master = *storage_list;
		}
	}
#endif

	return	result;
}



/* PurgeAllMemory forces all memory of memory_id to be purged to the scratch file */

PG_PASCAL (void) PurgeAllMemory (pgm_globals_ptr mem_globals, long memory_id)
{
	register master_list_ptr	storage_list;
	register pgm_globals_ptr	globals = mem_globals;
	register long 				index;
	memory_ref					ref_in_question;
	long						master_list_size;
	mem_rec						data_rec;
	
#ifdef MAC_PLATFORM
	
	master_list_size = GetHandleSize(globals->master_handle) / sizeof(Handle);

	for (index = 0; index < master_list_size; ++index) {

		storage_list = (master_list_ptr) *(globals->master_handle);
		ref_in_question = (memory_ref)storage_list[index];

		if (ref_in_question) {

			get_memory_record(ref_in_question, &data_rec);

			if (data_rec.mem_id == memory_id)
				if ((pgGetByteSize(data_rec.rec_size, data_rec.real_num_recs) >= PURGE_WORTH_IT)
					 && (!(data_rec.purge & PURGED_FLAG)) && (!data_rec.access))
					if (globals->purge(ref_in_question, globals, purge_memory))
						break;
		}
	}
#endif

#ifdef WINDOWS_PLATFORM
	{
		HANDLE			next_master;
		
		next_master = globals->master_handle;
		
		while (next_master) {
			
			storage_list = (master_list_ptr)GlobalLock(next_master);
			GlobalUnlock(next_master);
			master_list_size = MAX_HANDLE_ENTRIES;

			for (index = 0; index < master_list_size; ++storage_list, ++index) {

				if (*storage_list) {
					
					ref_in_question = (memory_ref)storage_list;
					get_memory_record(ref_in_question, &data_rec);
		
					if (data_rec.mem_id == memory_id)
						if ((pgGetByteSize(data_rec.rec_size, data_rec.real_num_recs) >= PURGE_WORTH_IT)
							 && (!(data_rec.purge & PURGED_FLAG)) && (!data_rec.access))
							if (globals->purge((memory_ref)storage_list, globals, purge_memory))
								return;
				}
			}

			next_master = *storage_list;
		}
	}
#endif
}


/* InitMemoryRef adds a newly created reference to the "master list." (The reason
we have a separate, external function for this is to allow machine-specific
conversions elsewhere in Paige. For example, a Macintosh memory_ref is actually a Handle,
while a Windows memory_ref is a far pointer to a Handle.  The function returns the
actual memory_ref;  for Mac this will be the same as base_ref.  */

PG_PASCAL (memory_ref) InitMemoryRef (pgm_globals_ptr mem_globals, pg_handle base_ref)
{
	register pgm_globals_ptr		globals;
	register size_t					storage_index;
	size_t							remaining_qty;
	master_list_ptr					storage_list;
	memory_ref						new_ref;
	mem_rec_ptr						data;
	
	globals = mem_globals;
	
	if (globals->next_master < 0)  /* Master list requires extension */
		extend_master_list(globals);

	storage_index = globals->next_master;
	
#ifdef MAC_PLATFORM
	HNoPurge((Handle)base_ref);
	remaining_qty = (GetHandleSize(globals->master_handle) / sizeof(Handle)) - storage_index - 1;

	storage_list = (master_list_ptr) *(globals->master_handle);
	storage_list += storage_index;
	*storage_list = base_ref;
	new_ref = (memory_ref)base_ref;
#elif defined(UNIX_PLATFORM)
	remaining_qty = pg_unix_master_capacity(globals->master_handle) - storage_index - 1;
	storage_list = pg_unix_master_list(globals->master_handle);
	storage_list += storage_index;
	*storage_list = base_ref;
	new_ref = (memory_ref)storage_list;
#endif

#ifdef WINDOWS_PLATFORM
	remaining_qty = MAX_HANDLE_ENTRIES - storage_index - 1;

	storage_list = (master_list_ptr) globals->master_list;
	storage_list += storage_index;
	*storage_list = base_ref;
	new_ref = (memory_ref)storage_list;
	storage_index = (size_t)storage_list;
#endif
#if defined(UNIX_PLATFORM)
	storage_index = (size_t)storage_list;
#endif
	
	data = pgMemoryPtr(new_ref);
	data->master_index = storage_index;
	pgFreePtr(new_ref);

	while (remaining_qty) {
		
		++globals->next_master;
		++storage_list;
		
		if (*storage_list == NULL)
			break;
		
		--remaining_qty;
	}
	
	if (remaining_qty == 0)
		globals->next_master = -1;

	return		new_ref;
}


/* InitVirtualMemory initializes the purge function (which also enables
"virtual memory").  If purge_function is NULL the standard function is used.
The ref_con must be whatever the machine expects to use for purging files
(such as a file reference for an opened file).  */

PG_PASCAL (void) InitVirtualMemory (pgm_globals_ptr globals, purge_proc purge_function,
		long ref_con)
{
	if (!(globals->purge = purge_function))
		globals->purge = (purge_proc) (mem_proc) pgStandardPurgeProc;
	
	globals->purge_ref_con = ref_con;
	globals->purge(MEM_NULL, globals, purge_init);
}


/* MemoryPurge forces minimum_amount of memory to be purged. If successful,
the function returns NO_ERROR, otherwise some sort of error is returned. The
mask_ref, if non-NULL, is skipped (not considered for purging).  */

PG_PASCAL (pg_error) MemoryPurge (pgm_globals_ptr globals, size_t minimum_amount,
		memory_ref mask_ref)
{
	register size_t			amount_purged;
	register short			purge_priority_ctr;
	memory_ref				ref_to_purge;
	size_t					prior_unpurged;
	short					error;

	if (globals->free_memory) {
	
		amount_purged = globals->free_memory(globals, mask_ref, minimum_amount);
		globals->total_unpurged -= amount_purged;

		if (amount_purged >= minimum_amount)
			return	NO_ERROR;
	}
	else
		amount_purged = 0;

	if (!globals->purge) {
	
		if (amount_purged == 0)
			return	NO_ERROR;
		else
			return	NOT_ENOUGH_PURGED_ERR;
	}

	error = NO_ERROR;

	for (purge_priority_ctr = 0; purge_priority_ctr < NO_PURGING_STATUS;
			purge_priority_ctr += 32) {
		
		while (ref_to_purge = suggest_purge(globals, purge_priority_ctr, mask_ref)) {
			
			prior_unpurged = globals->total_unpurged;

			if (error = globals->purge(ref_to_purge, globals, purge_memory))
				break;

			amount_purged += (prior_unpurged - globals->total_unpurged);
			
			if (amount_purged >= minimum_amount)
				break;
		}
		
		if (error || (amount_purged >= minimum_amount))
			break;
	}

	if (error)
		return	error;

	if (amount_purged < minimum_amount)
		return	NOT_ENOUGH_PURGED_ERR;

	return	NO_ERROR;
}


/* ForceMinimumMemorySize removes excess extensions from ref (most refs are
created with some extra space to maximize re-sizing speed).  The function
returns the same memory_ref (** WHICH CAN BE DIFFERENT IN WINDOWS VERSION
IF +-64K boundary has changed! **) .  */

PG_PASCAL (memory_ref) ForceMinimumMemorySize (memory_ref ref_to_size)
{
	mem_rec			data_rec;
	memory_ref		ref;
	size_t			amount_reduced;

	ref = ref_to_size;

	#ifdef PG_DEBUG
	if (check_bad_ref(ref, access_zero))
		return		ref;
	#endif
	
	amount_reduced = 0;
	get_memory_record(ref, &data_rec);

	if ((data_rec.real_num_recs > data_rec.num_recs) && (!(data_rec.purge & PURGED_FLAG))) {
		mem_rec_ptr		data;
		pgm_globals_ptr	globals;
		size_t			reduced_size;
		
		amount_reduced = pgGetByteSize(data_rec.rec_size, data_rec.real_num_recs - data_rec.num_recs);
		
		data = pgMemoryPtr(ref);
		data->real_num_recs = data_rec.num_recs;
		globals = data->globals;
		pgFreePtr(ref);
		
		reduced_size = pgGetByteSize(data_rec.rec_size, data_rec.num_recs) + NON_APP_SIZE;

	#ifdef PG_DEBUG
				
		if (reduced_size < sizeof(mem_rec))
			globals->debug_proc(BAD_LINK_ERR, ref);
	#endif

		pgResizeMemory(globals, ref, reduced_size);
		if (pgMemorySize(ref) < reduced_size)
			pgFailure(globals, NO_MEMORY_ERR, 0);
		globals->total_unpurged -= amount_reduced;
	}

	return	ref;
}



/* pgMemInsert inserts byte_size memory at offset location in the block. The
logical block size is given in block_size. Note: The block is already
sufficient size.  */

PG_PASCAL (void) pgMemInsert (memory_ref block, size_t offset, size_t byte_size,
		size_t block_size)
{
	register pg_bits8_ptr		src, dest;
	size_t						h_size, diff;

	h_size = block_size;
	diff = h_size - offset;
	h_size += byte_size;
	
	if (diff) {
		
		dest = (pg_bits8_ptr) pgMemoryPtr(block);

		dest += h_size;
		src = dest - byte_size;
		
		pgBlockMove(src, dest, -(ptrdiff_t)diff);
		
		pgFreePtr(block);
	}
}


/* pgMemDelete deletes byte_size amount from block at offset. The logical block
size is passed in block_size.  NOTE: THE MEMORY BLOCK IS NOT RESIZED.  */

PG_PASCAL (void) pgMemDelete (memory_ref block, size_t offset, size_t byte_size,
		size_t block_size)
{
	size_t old_size, new_size;
	
	old_size = block_size;
	new_size = old_size - byte_size;

	if ((offset + byte_size) < old_size) {
		unsigned char PG_FAR	*src;
		unsigned char PG_FAR	*dest;

		dest = (unsigned char PG_FAR *) pgMemoryPtr(block);
		dest += offset;
		src = dest + byte_size;
		pgBlockMove(src, dest, old_size - offset - byte_size);
		
		pgFreePtr(block);
	}
}


/* pgBlockMove -- copies memory block *src to memory block *dest for block_size bytes.
HOWEVER, if block_size is negative, the buffers are copied backwards
(decrementing pointers) for absolute value of block_size. */

PG_PASCAL (void) pgBlockMove (const void PG_FAR *src, void PG_FAR *dest, ptrdiff_t block_size)
{
	register size_t byte_count;
	
	if (!block_size)
		return;

	if (block_size > 0)
#ifdef MAC_PLATFORM
		BlockMoveData(src, dest, block_size);
#else
      BlockMove(src, dest, block_size);
#endif
	else {
		byte_count = -block_size;
		
		if ((byte_count >= sizeof(size_t)) && (!((size_t)src & 1)) && (!((size_t)dest & 1))) {
			register size_t PG_FAR	*src_bytes;
			register size_t PG_FAR	*dest_bytes;
			register size_t			long_count;
			
			long_count = byte_count / sizeof(size_t);
			byte_count -= (long_count * sizeof(size_t));
			
			for (src_bytes = (size_t PG_FAR*)src, dest_bytes = (size_t PG_FAR*)dest; long_count; --long_count)
				*(--dest_bytes) = *(--src_bytes);
			
			src = src_bytes;
			dest = dest_bytes;
		}
		
		if (byte_count) {
			register unsigned char PG_FAR	*src_bytes;
			register unsigned char PG_FAR	*dest_bytes;

			for (src_bytes = (unsigned char PG_FAR*)src, dest_bytes = (unsigned char PG_FAR*)dest; byte_count; --byte_count)
				*(--dest_bytes) = *(--src_bytes);
		}
	}
}


#ifdef PG_DEBUG

/* pgCheckAllMemoryRefs checks every reference that exists. If any error, function returns TRUE. */

PG_PASCAL (pg_boolean) pgCheckAllMemoryRefs (void)
{
#ifdef PG_DEBUG_STATICS
	master_list_ptr  	storage_list;
	pg_boolean			result = FALSE;
	long 				i, master_list_size;
 	
#ifdef MAC_PLATFORM
	
	if (debug_globals->debug_flags & SCRAMBLE_CHECK)
		PurgeMem(5000000);

	master_list_size = GetHandleSize(debug_globals->master_handle) / sizeof(Handle);

    for (i = 0; i < master_list_size; i++) {
 
	   storage_list = (master_list_ptr) *(debug_globals->master_handle);

	   if (storage_list[i]) {
	   		memory_ref		ref_to_test;

	   		ref_to_test = (memory_ref)storage_list[i];
	   		
	   		if (result = quick_ref_check(ref_to_test))
	   			break;
	   	}
   }

#endif

#ifdef WINDOWS_PLATFORM
	HANDLE			next_master;
	pg_short_t		old_debug_flags;
	
	next_master = debug_globals->master_handle;
	old_debug_flags = debug_globals->debug_flags;
	debug_globals->debug_flags &= (~ALL_MEMORY_CHECK);
	
	while (next_master) {
		
		storage_list = GlobalLock(next_master);
		GlobalUnlock(next_master);
		master_list_size = MAX_HANDLE_ENTRIES;

    	for (i = 0; i < master_list_size; ++storage_list, ++i) {
 
	    	if (*storage_list)
	   			if (result = check_bad_ref ((memory_ref)storage_list, access_dont_care))
	   				break;
	   	}
	   	
	   	next_master = *storage_list;
   }
   
   debug_globals->debug_flags = old_debug_flags;

#endif

	return	result;

#endif
}

#endif
 
 
/* Get a unique ID that will be used for all subsequent memory request (until
some else changes globals->mem_globals.current_id)  */

PG_PASCAL (long) pgUniqueMemoryID (pgm_globals_ptr globals)
{
   ++globals->next_mem_id;
   globals->current_id = globals->next_mem_id;
   return (globals->current_id);
}


/* Allocates memory with a specific id */

PG_PASCAL (memory_ref) MemoryAllocID (pgm_globals_ptr globals,
	pg_short_t rec_size, long num_recs, short extend_size, long mem_id)
{
   mem_rec_ptr			data;
   memory_ref 			ref;
 
   ref = MemoryAlloc (globals, rec_size,  num_recs, extend_size);
   data = pgMemoryPtr(ref);
   data->mem_id = mem_id;
   pgFreePtr(ref);

   return (ref);
}
 
/* Allocates and clears memory with a specific id */

PG_PASCAL (memory_ref) MemoryAllocClearID (pgm_globals_ptr globals,
	pg_short_t rec_size, long num_recs, short extend_size, long mem_id)
{
   mem_rec_ptr			data;
   memory_ref 			ref;
 
   ref = MemoryAllocClear (globals, rec_size,  num_recs, extend_size);
   
   data = pgMemoryPtr(ref);
   data->mem_id = mem_id;
   pgFreePtr(ref);

   return (ref);
}
 

/* Added to make life easier */

PG_PASCAL (memory_ref) MemoryRecover (void PG_FAR *ptr)
{
	mem_rec_ptr  p = (mem_rec_ptr) ptr;
	return PG_LONGWORD(memory_ref) (pgRecoverMemory(--p));
}


PG_PASCAL (void) ChangeAllMemoryID(pgm_globals_ptr mem_globals, long orig_id, long new_id)
{
	register master_list_ptr 	storage_list;
	register long   			index;
 	mem_rec_ptr					data;
 
 #ifdef MAC_PLATFORM
 	long						master_list_size;
	
 	master_list_size = GetHandleSize(mem_globals->master_handle) / sizeof(Handle);
	storage_list = (master_list_ptr) *(mem_globals->master_handle);
 
	for (index = master_list_size - 1; index <= 0; --index) {
 		register memory_ref  		ref_in_question;
 
		if (ref_in_question = (memory_ref)*storage_list++) {
			
			data = pgMemoryPtr(ref_in_question);
			
			if (data->mem_id == orig_id)
				data->mem_id = new_id;
		}
	}

#endif

#ifdef WINDOWS_PLATFORM
	HANDLE				next_master;
	
	next_master = mem_globals->master_handle;
	
	while (next_master) {
	
		storage_list = (master_list_ptr) GlobalLock(next_master);
	 	GlobalUnlock(next_master);
		
		for (index = 0;  index < MAX_HANDLE_ENTRIES; ++storage_list, ++index) {
	
			if (*storage_list) {
				
				data = (mem_rec_ptr)GlobalLock((HANDLE)*storage_list);
				
				if (data->mem_id == orig_id)
					data->mem_id = new_id;
				
				GlobalUnlock(*storage_list);
			}
		}
		
		next_master = *storage_list;
	}

#endif
}



/* pgGetByteSize returns a byte size based on the record size and number of
records. (The reason I do it this way is to avoid excessive multiplication for
many memory_refs that have 1 byte record sizes).  */

PG_PASCAL (size_t) pgGetByteSize (pg_short_t rec_size, size_t num_recs)
{
	switch (rec_size) {
		
		case 1:
			return	num_recs;
			
		case 2:
			return	num_recs << 1;
			
		case 4:
			return	num_recs << 2;

		case 8:
			return	num_recs << 3;

		case 16:
			return	num_recs << 4;
		
			break;
	}
	
	return	num_recs * rec_size;
}


/* SetMemoryRecSize changes the record size in ref to new_rec_size. When the record size
is changed, the allocation is set to ZERO SIZE. */

PG_PASCAL (void) SetMemoryRecSize (memory_ref ref, pg_short_t new_rec_size, short extend_size)
{
	mem_rec_ptr			data;
	pgm_globals_ptr		globals;
	mem_rec				data_rec;
	size_t				new_size;

	#ifdef PG_DEBUG
	if (check_bad_ref(ref, access_zero))
		return;
	#endif
	
	get_memory_record(ref, &data_rec);

	globals = data_rec.globals;
	
	if (data_rec.purge & PURGED_FLAG)
		globals->purge(ref, globals, dispose_purge);

	new_size = pgGetByteSize(new_rec_size, extend_size) + NON_APP_SIZE;

	data = pgMemoryPtr(ref);
	data->rec_size = new_rec_size;
	data->num_recs = 0;
	data->real_num_recs = extend_size;
	data->extend_size = extend_size;		
	pgFreePtr(ref);

	#ifdef PG_DEBUG
				
		if (new_size < sizeof(mem_rec))
			globals->debug_proc(BAD_LINK_ERR, ref);
	#endif

	pgResizeMemory(globals, ref, new_size);

#ifdef PG_DEBUG
	append_checksum(ref);
#endif
}


/* Returns a newly created memory ref with all fields set up */

#ifdef PG_DEBUG
PG_PASCAL (memory_ref) pgAllocateNewRef (pgm_globals_ptr mem_globals, pg_short_t rec_size,
		size_t num_recs, short extend_size, short zero_fill, char *file,int line)
#else
PG_PASCAL (memory_ref) pgAllocateNewRef (pgm_globals_ptr mem_globals, pg_short_t rec_size,
		size_t num_recs, short extend_size, short zero_fill)
#endif
{
	register mem_rec_ptr			data;
	pg_handle						base_ref;
	pg_error						err;
	size_t							total_size;
	memory_ref						new_ref = MEM_NULL;

#ifdef PG_DEBUG
	if (mem_globals->debug_flags & ALL_MEMORY_CHECK)
		if (pgCheckAllMemoryRefs())
			return	MEM_NULL;
#endif

	total_size = pgGetByteSize(rec_size, num_recs + extend_size) + NON_APP_SIZE;

	PG_TRY (mem_globals) {
		
		for (;;) {

			if (zero_fill)
				base_ref = (pg_handle)pgAllocMemoryClear(total_size);
			else
				base_ref = (pg_handle)pgAllocMemory(total_size);
			
			if (base_ref)
				break;

			pgFailNIL(mem_globals, (void PG_FAR *) mem_globals->spare_tire);

			pgFreeMemory(mem_globals->spare_tire);
			mem_globals->spare_tire = MEM_NULL;
			mem_globals->total_unpurged -= SPARE_TIRE_SIZE;
			err = MemoryPurge(mem_globals, total_size + mem_globals->purge_threshold, MEM_NULL);
			
			pgFailError(mem_globals, err);

			mem_globals->last_error = NO_ERROR;
		}

		mem_globals->total_unpurged += total_size;
		new_ref = InitMemoryRef(mem_globals, base_ref);

		data = pgMemoryPtr(new_ref);
		
		data->rec_size = rec_size;
		data->num_recs = data->real_num_recs = num_recs;
		data->real_num_recs += extend_size;
		data->extend_size = extend_size;
		data->access = 0;
		data->qty_used = 0;
		data->purge = 0x0080;
		data->globals = mem_globals;
		data->mem_id = mem_globals->current_id;
#if defined(FRONTIER_TESTS)
	pg_trace_handle_event(zero_fill ? "alloc-clear" : "alloc", new_ref, data, PG_TRACE_CALLER());
#endif
#ifdef PG_DEBUG
#ifdef MAC_PLATFORM
		BlockMoveData(file,data->file,DEBUG_INFO_SIZE);
#else
      BlockMove(file, data->file, DEBUG_INFO_SIZE);
#endif
		data->file[DEBUG_INFO_SIZE - 1] = 0;
		data->line = line;
#endif
		pgFreePtr(new_ref);

		#ifdef PG_DEBUG
			append_checksum(new_ref);
		#endif
	}
	
	PG_CATCH {
		
		pgFailure(mem_globals, mem_globals->last_error, 0);
	}
	
	PG_ENDTRY;

	if (!mem_globals->spare_tire)
		mem_globals->spare_tire = pgAllocMemory(SPARE_TIRE_SIZE);

	purge_excess_memory(mem_globals, new_ref);

	return	new_ref;
}



#ifdef NO_C_INLINE			/* Begin non-inline for C code */

#ifdef PG_DEBUG

PG_PASCAL (memory_ref) MemoryAlloc (pgm_globals_ptr globals, pg_short_t rec_size,
		long num_recs, short extend_size, char *src_file, char *line_number)
{
	return  pgAllocateNewRef(globals, rec_size, num_recs, extend_size, FALSE, src_file, line_number);
}

/* MemoryAllocClear is identical to MemoryAlloc except the block is filled with zeros */
PG_PASCAL (memory_ref) MemoryAllocClear (pgm_globals_ptr globals, pg_short_t rec_size,
		long num_recs, short extend_size, char *src_file, char *line_number)
{
	return  pgAllocateNewRef(globals, rec_size, num_recs, extend_size, TRUE, src_file, line_number);
}

#else
/* MemoryAlloc allocates a new memory_ref */

PG_PASCAL (memory_ref) MemoryAlloc (pgm_globals_ptr globals, pg_short_t rec_size,
		long num_recs, short extend_size)
{
	return  pgAllocateNewRef(globals, rec_size, num_recs, extend_size, FALSE);
}

/* MemoryAllocClear is identical to MemoryAlloc except the block is filled with zeros */
PG_PASCAL (memory_ref) MemoryAllocClear (pgm_globals_ptr globals, pg_short_t rec_size,
		long num_recs, short extend_size)
{
	return  pgAllocateNewRef(globals, rec_size, num_recs, extend_size, TRUE);
}

#endif		/* End no-inline c code definition */
#endif

/* UnuseMemory must be called once the memory_ref is no longer to be used.
The result is the block is marked re-locatable if the access counter decrements
to zero */

PG_PASCAL (void) UnuseMemory (memory_ref ref)
{
	register mem_rec_ptr			data;
	short							new_access;
#ifdef PG_DEBUG
	pgm_globals_ptr					globals;

	if (check_bad_ref(ref, access_nonzero))
		return;

	data = pgMemoryPtr(ref);

	if (data->purge & PURGED_FLAG) {
	
		data->globals->debug_proc(PURGED_MEMORY_ERR, ref);
		return;
	}

	globals = data->globals;
	
	if (globals->debug_check)
		if (globals->debug_check == ref)
			if (data->access == (globals->debug_access + 1))
				globals->debug_proc(USER_BREAK_ERR, ref);

	pgFreePtr(ref);

#endif
	
	data = pgMemoryPtr(ref);
	data->access -= 1;
	new_access = data->access;
	pgFreePtr(ref);
#if defined(FRONTIER_TESTS)
	const char *trace_event = (new_access < 0) ? "unuse-underflow" : "unuse";
	pg_trace_handle_event(trace_event, ref, data, PG_TRACE_CALLER());
	if (new_access < 0) {
		fprintf(stdout, "[pg-handle-debug] unuse-underflow ref=%p watch=%p access=%d\n", (void *)ref, (void *)frontier_docinfo_subject_ref, (int)new_access);
		fflush(stdout);
		new_access = 0;
		data->access = 0;
	}
#endif
	
	if (!new_access)
		pgUnlockMemory(ref);
	
	pgFreePtr(ref);	/* Mostly required for Windows */
}


/* GetMemorySize returns a size of a memory_ref */

PG_PASCAL (size_t) GetMemorySize (memory_ref ref)
{
	size_t			num_recs;
	mem_rec_ptr		data;
	
	#ifdef PG_DEBUG
	if (check_bad_ref(ref, access_positive))
		return	0;
	#endif
	
	data = pgMemoryPtr(ref);
	num_recs = data->num_recs;
	pgFreePtr(ref);

	return	num_recs;
}


/* GetMemoryRecSize returns the record size of a memory_ref */

PG_PASCAL (short) GetMemoryRecSize (memory_ref ref)
{
	short			rec_size;
	mem_rec_ptr		data;
	
	#ifdef PG_DEBUG
	if (check_bad_ref(ref, access_positive))
		return	0;
	#endif
	
	data = pgMemoryPtr(ref);
	rec_size = data->rec_size;
	pgFreePtr(ref);

	return	rec_size;
}



/* GetByteSize returns the total memory_ref size, in bytes (instead of record numbers) */

PG_PASCAL (size_t) GetByteSize (memory_ref ref)
{
	short			rec_size;
	size_t			num_recs;
	mem_rec_ptr		data;
	
	#ifdef PG_DEBUG
	if (check_bad_ref(ref, access_positive))
		return	0;
	#endif
	
	data = pgMemoryPtr(ref);
	rec_size = data->rec_size;
	num_recs = data->num_recs;
	pgFreePtr(ref);

	return pgGetByteSize(rec_size, num_recs);
}


/* GetGlobalsFromRef returns a pgm_globals_ptr found from the given ref. */

PG_PASCAL (pgm_globals_ptr) GetGlobalsFromRef (memory_ref ref)
{
	pgm_globals_ptr		globals;
	mem_rec_ptr			data;
	
	#ifdef PG_DEBUG
	if (check_bad_ref(ref, access_dont_care))
		return	NULL;
	#endif
	
	data = pgMemoryPtr(ref);
	globals = data->globals;
	pgFreePtr(ref);

	return	globals;
}


/* UnuseAndDispose will allow an access counter of 1 but dispose the memory_ref.
Note: In debug mode, the access counter must be exactly 1 or a debug break will
result. This is purely for code discipline to make sure you only had the ref
in use once.  */

PG_PASCAL (void) UnuseAndDispose (memory_ref ref)
{
	#ifdef PG_DEBUG
	if (check_bad_ref(ref, access_one_only))
		return;
	#endif
	
	UnuseMemory(ref);
	DisposeMemory(ref);
}

/* Gets a memory ref's specific id */

PG_PASCAL (long) GetMemoryRefID(memory_ref ref)
{
   mem_rec_ptr		data;
   long				mem_id;
   
   data = pgMemoryPtr(ref);
   mem_id = data->mem_id;
   pgFreePtr(ref);

   return mem_id;
}
 
/* Sets a memory ref with a specific id */

PG_PASCAL (void) SetMemoryRefID(memory_ref ref, long mem_id)
{
   mem_rec_ptr		data;
   
   data = pgMemoryPtr(ref);
   data->mem_id = mem_id;
   pgFreePtr(ref);
}

/* GetAccessCtr returns the current access counter in ref. */

PG_PASCAL (short) GetAccessCtr (memory_ref ref)
{
	return	get_access_ctr(ref);
}



/* Unuse memory regardless */
PG_PASCAL (void) UnuseFailedMemory (memory_ref ref)
{
	while (get_access_ctr(ref))
		UnuseMemory(ref);
}


/* DisposeFailedMemory disposes the memory_ref allocation from a failure
handler during Debug */
 
PG_PASCAL (void) DisposeFailedMemory (memory_ref ref)
{
   UnuseFailedMemory(ref);
   DisposeMemory (ref);
}


#ifndef C_LIBRARY

/* pgFillBlock -- fills a memory block of block_size byte size with byte value
in pg_char parameter */

PG_PASCAL (void) pgFillBlock (void PG_FAR *block, long block_size, char value)
{
	register unsigned char PG_FAR *byte_target;
	register long		   PG_FAR *long_target;
	register long				   counter, long_value;
	short						   ctr;

	counter = block_size;

	if (!((long) block & 1)) {
		
		if (long_value = value)
			for (ctr = sizeof(long) - 1, long_value = value; ctr; --ctr,
					long_value <<= 8, long_value |= value) ;

		for (long_target = (long *) block; counter >= sizeof(long); counter -= sizeof(long))
			*long_target++ = long_value;
		
		byte_target = (unsigned char PG_FAR *) long_target;
	}
	else
		byte_target = (unsigned char *) block;
	
	while (counter) {
		*byte_target++ = value;
		--counter;
	}
}

#endif


/* pgStandardPurgeProc is the standard purging function */

PG_PASCAL (pg_error) pgStandardPurgeProc (memory_ref ref, pgm_globals_ptr mem_globals, short verb)
{
	register available_ptr			available_list;
	register short					ref_num;

	ref_num = (short)mem_globals->purge_ref_con;

	if (verb == purge_init) {

		PG_TRY(mem_globals) {

			if (ref_num) {
				
				if (!mem_globals->purge_info)
					mem_globals->purge_info = MemoryAlloc(mem_globals, 1, PAIR_SIZE, sizeof(long) * 16);
				
				SetMemoryPurge(mem_globals->purge_info, NO_PURGING_STATUS, FALSE);
				available_list = (available_ptr) UseMemory(mem_globals->purge_info);
				
				*available_list++ = 0;
				*available_list = AVAILABLE_LIST_TERMINATOR;
				
				UnuseMemory(mem_globals->purge_info);
			}
		}
		
		PG_CATCH {
			
			return	mem_globals->last_error;
		}
		
		PG_ENDTRY;
	}
	else {
		mem_rec_ptr		data;
		available_ptr	saved_offsets_ptr;
		size_t			starting_offset, ending_offset, data_size;
		size_t			original_size, new_physical_size;
		short			purge_flags;
		pg_error		error;
				
		if (!ref_num)
			return	NO_PURGE_FILE_ERR;
		
/* Purge / Unpurge goes here.  The way I do a "purge" is the block gets set
to sizeof(mem_rec) + 2 longs;  the 2 longs hold the starting/ending offset where
I have saved the data to a file.	*/
		
		PG_TRY(mem_globals) {
		
			original_size = pgMemorySize(ref);
	
			if (verb == purge_memory) {
			
				data_size = GetByteSize(ref);
				data = pgMemoryPtr(ref);
				purge_flags = data->purge;
				pgFreePtr(ref);

				if (!(purge_flags & NO_DATA_SAVE_FLAG) && (data_size > 0)) {
				
					starting_offset = find_available_space(mem_globals, mem_globals->purge_info, data_size);
					error = pgSetFilePos(ref_num, starting_offset);	//ее TRS/OITC

					pgFailError(mem_globals, pgProcessError(error));
		
					pgLockMemory(ref);
					data = pgMemoryPtr(ref);
					data->real_num_recs = data->num_recs;	/* There won't be any more extension */		
					++data;					
					error = pgWriteFileData(ref_num, data_size, data);
					pgFailError(mem_globals, pgProcessError(error));
					
					saved_offsets_ptr = (size_t PG_FAR *) data;
					*saved_offsets_ptr++ = starting_offset;
					*saved_offsets_ptr = starting_offset + data_size;
			
					pgUnlockMemory(ref);
					pgFreePtr(ref);
		            
		            resize_purged_memory(ref);		            
					pgFailError(mem_globals, pgMemoryError(mem_globals));
				}
	            else
	            	resize_purged_memory(ref);

				data = pgMemoryPtr(ref);
				data->purge |= PURGED_FLAG;
				pgFreePtr(ref);
			}
			else {	/* Re-load (unpurge) or dispose_purge */
	
				data = pgMemoryPtr(ref);
	
				if ((data->purge & NO_DATA_SAVE_FLAG) || (data->num_recs == 0)) {
					
					if (verb == unpurge_memory) {
				
						new_physical_size = GetByteSize(ref) + NON_APP_SIZE;

						data->real_num_recs = data->num_recs;
						data->purge &= CLEAR_PURGED_FLAG;
						pgFreePtr(ref);
	
						pgResizeMemory(mem_globals, ref, new_physical_size);
						
						if ((error = pgMemoryError(mem_globals)) == NO_MEMORY_ERR)
							return	error;		// Do not throw exception if out of memory
			
						if (pgMemorySize(ref) < new_physical_size)
							return	NO_MEMORY_ERR;

						pgFailError(mem_globals, error);
						
						#ifdef PG_DEBUG
						
						append_checksum(ref);
					
						#endif
					}
					else
						pgFreePtr(ref);
				}
				else {
				
					++data;
					saved_offsets_ptr = (size_t PG_FAR *) data;
		
					starting_offset = *saved_offsets_ptr++;
					ending_offset = *saved_offsets_ptr;
					
					pgFreePtr(ref);

					if (verb == unpurge_memory) {
					
						data_size = ending_offset - starting_offset;
						
						new_physical_size = GetByteSize(ref);
						
						if (data_size > new_physical_size)
							data_size = new_physical_size;
						
						new_physical_size += NON_APP_SIZE;
						pgResizeMemory(mem_globals, ref, new_physical_size);
						
						if ((error = pgMemoryError(mem_globals)) == NO_MEMORY_ERR)
							return	error;		// Do not throw exception if out of memory
						if (pgMemorySize(ref) < new_physical_size)
							return	NO_MEMORY_ERR;

						pgFailError(mem_globals, error);

						pgLockMemory(ref);
						data = pgMemoryPtr(ref);
						
						data->purge &= CLEAR_PURGED_FLAG;
						data->real_num_recs = data->num_recs;

						++data;
		
						error = pgSetFilePos(ref_num, starting_offset);
						pgFailError(mem_globals, pgProcessError(error));
						error = pgReadFileData(ref_num, data_size, data);
						pgFailError(mem_globals, pgProcessError(error));

						pgUnlockMemory(ref);
						pgFreePtr(ref);

						#ifdef PG_DEBUG
						
						append_checksum(ref);
					
						#endif
					}
		
					free_available_space(mem_globals, mem_globals->purge_info, starting_offset, ending_offset);
				}
			}

			mem_globals->total_unpurged += pgMemorySize(ref) - original_size;
		}
		
		PG_CATCH {
			
			pgUnlockMemory(ref);
			return	mem_globals->last_error;
		}
		
		PG_ENDTRY;
	}

	return	NO_ERROR;
}


/* pgStandardFreeProc gets called to free memory that might be application-specific. Upon entry,
desired_free is the preferred amount to free up. The dont_free param is a memory_ref to NOT
free. (This standard function does nothing. Its intended use is for applications and extensions). */

PG_PASCAL (long) pgStandardFreeProc (pgm_globals_ptr mem_globals, memory_ref dont_free, long desired_free)
{
#ifdef MAC_PLATFORM
#pragma unused(mem_globals, dont_free, desired_free)
#endif
	return	0;
}


//е TRS/OITC

/* MemoryToCStr converts a normal memory ref to a C string memory ref. */

PG_PASCAL (size_t) MemoryToCStr(memory_ref ref)
{
	size_t	result;
	
	result = GetMemorySize(ref);
	AppendMemory (ref, 1, TRUE);
	UnuseMemory(ref);

	return result;
}


/* MemoryToPStr converts a normal memory ref to a pascal string memory ref. */

PG_PASCAL (size_t) MemoryToPStr(memory_ref ref)
{
	size_t	result;
	
	result = GetMemorySize(ref);
	
	if (GetMemoryRecSize(ref) == sizeof(pg_char))
		if (result < 255)
		{
			*((pg_char_ptr)InsertMemory (ref, 0, 1)) = (pg_char)result;
			UnuseMemory(ref);
		}

	return result;
}


/* CStrToMemory converts a C string memory ref to a normal memory ref. */

PG_PASCAL (size_t) CStrToMemory(memory_ref ref)
{
	size_t	result;
	
	result = GetMemorySize(ref);
	DeleteMemory (ref, --result, 1);
	return result;
}


/* PStrToMemory converts a pascal string memory ref to a normal memory ref. */

PG_PASCAL (size_t) PStrToMemory(memory_ref ref)
{
	size_t	result;
	
	result = GetMemorySize(ref) - 1;
	DeleteMemory (ref, 0, 1);
	return result;
}


/* UseMemoryToCStr converts a memory ref to a C string locks and returns pointer. */

PG_PASCAL (char PG_FAR *) UseMemoryToCStr(memory_ref ref)
{
	MemoryToCStr(ref);
	return (char PG_FAR *) UseMemory(ref);
}


/* UseMemoryToPStr converts a memory ref to a pascal string locks and returns pointer. */

PG_PASCAL (pg_char_ptr) UseMemoryToPStr(memory_ref ref)
{
	if (MemoryToPStr(ref) > 255)
		pgFailure(GetGlobalsFromRef(ref), PG_PSTRING_TOO_BIG_ERR, 0);
	return (pg_char_ptr) UseMemory(ref);
}


/* UnuseMemoryFromCStr unuses ref and remove C terminating 0. */

PG_PASCAL (void) UnuseMemoryFromCStr(memory_ref ref)
{
	UnuseMemory(ref);
	CStrToMemory(ref);
}


/* UnuseMemoryFromPStr unuses ref and remove pascal length byte. */

PG_PASCAL (void) UnuseMemoryFromPStr(memory_ref ref)
{
	UnuseMemory(ref);
	PStrToMemory(ref);
}


/* UnuseAndZero unuses ref and sets siz to zero. */

PG_PASCAL (void) UnuseAndZero(memory_ref ref)
{
	UnuseMemory(ref);
	SetMemorySize(ref, 0);
}


/* DisposeNonNilMemory disposes ref if it is non-zero. */

PG_PASCAL (memory_ref) DisposeNonNilFailedMemory (memory_ref ref)
{
	if (ref)
		DisposeFailedMemory(ref);
	return MEM_NULL;
}


/* DuplicateNonNilMemory disposes ref if it is non-zero. */

PG_PASCAL (memory_ref) DuplicateNonNilMemory (memory_ref ref)
{
	if (ref)
		return MemoryDuplicate(ref);
	return MEM_NULL;
}


/* EqualMemory compares two memory structures for equality. */

PG_PASCAL (pg_boolean) EqualMemory (memory_ref ref1, memory_ref ref2)
{
	size_t		len;
	pg_boolean	result = FALSE;
	
	if ((len = GetByteSize(ref1)) == GetByteSize(ref2))
	{
		result = pgEqualStruct (UseMemory(ref1), UseMemory(ref2), len);
		UnuseMemory(ref1);
		UnuseMemory(ref2);
	}
	return result;
}


#ifdef NO_C_INLINE

/* PString2Memory creates a memory ref containing the pascal string. */

PG_PASCAL (memory_ref) PString2Memory (pgm_globals_ptr mem_globals, const pg_p_string_ptr string)
{
	return Bytes2Memory (mem_globals, (pg_bits8_ptr)string, sizeof(pg_char), string[0] + 1);
}


/* CString2Memory creates a memory ref containing the c string. */

PG_PASCAL (memory_ref) CString2Memory (pgm_globals_ptr mem_globals, const pg_c_string_ptr string)
{
	return Bytes2Memory (mem_globals, (pg_char_ptr)string, sizeof(pg_char), pgCStrLength(string) + 1);
}

#endif


/* Bytes2Memory creates a memory ref containing the bytes. */

PG_PASCAL (memory_ref) Bytes2Memory (pgm_globals_ptr mem_globals, const pg_bits8_ptr data, short char_size, long length)
{
	memory_ref	ref;
	
	ref = MemoryAlloc (mem_globals, char_size, length, 0);
	pgBlockMove(data, UseMemory(ref), length * char_size);
	UnuseMemory(ref);
	return ref;
}


#ifdef PG_DEBUG

#ifdef PG_DEBUG_STATICS

/* The following is available only in "debug mode" and only if #PG_DEBUG_STATICS
This returns the total amount of memory allocations Paige is using.  */

PG_PASCAL (long) TotalPaigeMemory (void)
{
	register master_list_ptr	storage_list;
	register pgm_globals_ptr	globals;
	register long 				total = 0;
	long						index, master_list_size;

#ifdef MAC_PLATFORM
	globals = debug_globals;
	total = 0;
	master_list_size = GetHandleSize(globals->master_handle) / sizeof(Handle);
	storage_list = (master_list_ptr) *(globals->master_handle);

	for (index = 0; index < master_list_size; ++index, ++storage_list) {

		if (*storage_list)
			total += pgMemorySize(*storage_list);
	}

#endif

#ifdef WINDOWS_PLATFORM
	HANDLE		next_master;
	
	globals = debug_globals;
	next_master = globals->master_handle;
	
	while (next_master) {
	
		master_list_size = MAX_HANDLE_ENTRIES;
		storage_list = (master_list_ptr) GlobalLock(next_master);
		GlobalUnlock(next_master);

		for (index = 0; index < master_list_size; ++index, ++storage_list) {
	
			if (*storage_list)
				total += GlobalSize(*storage_list);
		}
		
		next_master = *storage_list;
	}

#endif

	return	total;
}

#endif

/* pgCheckReference -- checks to see if "ref" is a valid allocation (for Mac it
checks to see if it is a reasonable address). If OK, return NO_ERROR, otherwise
return the appropriate error code per pgErrors.h */

PG_PASCAL (short) pgCheckReference (memory_ref ref)
{
	short			result;
	mem_rec_ptr		block_data;
	pgm_globals_ptr	globals;
	
	if (result = validate_address((void PG_FAR *) ref))
		return	result;

	if (result = validate_ref(ref))
		return	result;

	block_data = pgMemoryPtr(ref);

	if (result = validate_address((void PG_FAR *) block_data))
		return  result;
	
	globals = block_data->globals;

	if (pgMemorySize(ref) < sizeof(mem_rec))
		return	BAD_REF_ERR;

	if (result = validate_address(globals))
		return  result;
	
	if (globals->signature != CHECKSUM_SIG)
		return  BAD_LINK_ERR;
	
	pgFreePtr(ref);

	return NO_ERROR;
}

#endif


/******************************  LOCAL FUNCTIONS  ***************************/


/* get_memory_record returns basic info from inside the memory_ref. */

static void get_memory_record (memory_ref ref, mem_rec_ptr record)
{
	mem_rec_ptr			data;
	
	data = pgMemoryPtr(ref);
	*record = *data;
	pgFreePtr(ref);
}


/* get_access_ctr returns the access counter of a memory_ref. */

static short get_access_ctr (memory_ref ref)
{
	mem_rec_ptr	data_ptr;
	short		result;
	
	data_ptr = pgMemoryPtr(ref);
	result = data_ptr->access;
	pgFreePtr(ref);
	
	return	result;
}



/* The function suggest_purge searches for the lowest # of uses among all refs
with a purge priority of purge_priority, not already purged and not in use. If
none are found the function returns NULL.  The mask_ref parameter contains a
reference, however, to not consider (which is typically the one about to be
set to a used state).  */

static memory_ref suggest_purge (pgm_globals_ptr globals, short purge_priority,
		memory_ref mask_ref)
{
	register master_list_ptr		ref_storage;
	register long					num_refs;
	long							master_list_size;
	memory_ref						answer;
	mem_rec							data_rec;
	
#ifdef MAC_PLATFORM
	
	master_list_size = GetHandleSize(globals->master_handle) / sizeof(Handle);
	answer = MEM_NULL;
	ref_storage = (master_list_ptr)*(globals->master_handle);

	for (num_refs = master_list_size; num_refs; ++ref_storage, --num_refs) {
		
		if ((*ref_storage)) {
			
			if ((memory_ref)*ref_storage != mask_ref) {
				
				get_memory_record((memory_ref)*ref_storage, &data_rec);
				
				if ((pgGetByteSize(data_rec.rec_size, data_rec.real_num_recs) >= PURGE_WORTH_IT)
					 && (!(data_rec.purge & (PURGED_FLAG | PURGE_LOCK_FLAGS))) && (!data_rec.access)
						&& ((data_rec.purge & PURGE_VALUE_MASK) <= purge_priority)) {
						
						answer = (memory_ref)*ref_storage;
						break;
					}
			}
		}
	}

#endif

#ifdef WINDOWS_PLATFORM
	HANDLE			next_master;
	
	master_list_size = MAX_HANDLE_ENTRIES;
	next_master = globals->master_handle;

	while (next_master) {
	
		ref_storage = (master_list_ptr) GlobalLock(next_master);
		GlobalUnlock(next_master);
		answer = MEM_NULL;

		for (num_refs = master_list_size; num_refs; ++ref_storage, --num_refs) {
			
			if (*ref_storage) {
				
				if ((memory_ref)ref_storage != mask_ref) {
					
					get_memory_record((memory_ref)ref_storage, &data_rec);
					
					if ((pgGetByteSize(data_rec.rec_size, data_rec.real_num_recs) >= PURGE_WORTH_IT)
						 && (!(data_rec.purge & (PURGED_FLAG | PURGE_LOCK_FLAGS))) && (!data_rec.access)
							&& ((short)(data_rec.purge & PURGE_VALUE_MASK) <= purge_priority))
								return	(memory_ref)ref_storage;
				}
			}
		}
		
		next_master = *ref_storage;
	}
	
#endif

	return	answer;
}


/* This function checks if the total unpurged amount is greater than the maximum
designated by app and, if so, purges some memory to equalize the situation. */

static void purge_excess_memory (pgm_globals_ptr globals, memory_ref mask_ref)
{
	if (globals->total_unpurged > globals->max_memory)
		MemoryPurge(globals, globals->total_unpurged - globals->max_memory
				+ globals->purge_threshold, mask_ref);
}


static void resize_purged_memory (memory_ref ref)
{
#ifdef WINDOWS_PLATFORM
    mem_rec_ptr			data;
	purged_mem_rec		temp_copy;
	HANDLE PG_FAR		*global_handle;
		            	
	global_handle = (HANDLE PG_FAR *)ref;
	data = (mem_rec_ptr) GlobalLock(*global_handle);
	pgBlockMove(data, &temp_copy, sizeof(purged_mem_rec));
	GlobalUnlock(*global_handle);
	GlobalFree(*global_handle);		            	
	*global_handle = GlobalAlloc(GMEM_MOVEABLE, sizeof(purged_mem_rec));
	data = (mem_rec_ptr) GlobalLock(*global_handle);
	pgBlockMove(&temp_copy, data, sizeof(purged_mem_rec));
	GlobalUnlock(*global_handle);
#else
	pgm_globals_ptr	mem_globals;

	mem_globals = GetGlobalsFromRef(ref);
	if (mem_globals)
		pgResizeMemory(mem_globals, ref, sizeof(mem_rec) + PAIR_SIZE);
#endif
}


/* This function checks to see if the memory_ref is purged and, if so, re-loads
it.  This gets called when the ref is about to be set to a used state.  */

static void reload_ref (memory_ref ref)
{
	register mem_rec_ptr		data;
	volatile pgm_globals_ptr	globals;
	size_t						unpurged_size;
	volatile pg_error			error;

	data = pgMemoryPtr(ref);

#ifdef PG_DEBUG
	globals = data->globals;
	if (globals->debug_check == 0xFFFFFFFF)
		pgFailure(globals, -1, 0);
#endif

	if (!(data->purge & PURGED_FLAG)) {
		
		pgFreePtr(ref);

		return;
	}
	
	globals = data->globals;
	
	unpurged_size = pgGetByteSize(data->rec_size, data->num_recs);
	data->purge += PURGE_LOCK_CTR;
	pgFreePtr(ref);

	error = 0;

	PG_TRY (globals) {
	
		for (;;) {
			
			if ((error = globals->purge(ref, globals, unpurge_memory)) == NO_ERROR)
				break;

			pgFailNotError(globals, NO_MEMORY_ERR, error);
			pgFailNIL(globals, (void PG_FAR *)globals->spare_tire);

			pgFreeMemory(globals->spare_tire);
			globals->spare_tire = MEM_NULL;
			globals->total_unpurged += SPARE_TIRE_SIZE;

			error = MemoryPurge(globals, unpurged_size + globals->purge_threshold, ref);
			pgFailError(globals, error);
			pgFailError(globals, NO_MEMORY_ERR);

			globals->last_error = NO_ERROR;
		}
	}

	PG_CATCH {
		
		pgFailure(globals, error, (long) ref);
	}
	
	PG_ENDTRY;

	if (!globals->spare_tire)
		globals->spare_tire = pgAllocMemory(SPARE_TIRE_SIZE);

	purge_excess_memory(globals, ref);

	data = pgMemoryPtr(ref);
	data->purge -= PURGE_LOCK_CTR;
	pgFreePtr(ref);
}




/* This function locates the first available space in the available list
that can hold wanted_space. The file offset is returned as the function result
and the available list is updated.  */

static size_t find_available_space (pgm_globals_ptr globals, memory_ref avail_ref,
		size_t wanted_space)
{
	register available_ptr	available_list;
	mem_rec_ptr				temp_ptr;
	register size_t			space_available;

	temp_ptr = pgMemoryPtr(avail_ref);
	++temp_ptr;
	available_list = (available_ptr)temp_ptr;

	for (;;) {
		
		space_available = available_list[1] - *available_list;

		if (space_available >= wanted_space)
			break;
		
		available_list += 2;
	}

	space_available = *available_list;
	*available_list += wanted_space;

	pgFreePtr(avail_ref);

	avail_ref = cleanup_available_list(globals, avail_ref);

	return	space_available;
}


/* This returns the (previously saved) file space to the available list.
The original saved file offset is in saved_position and ending position in
end_position.  The function returns avail_ref (because in certain platforms this
can be a different ref when re-sized). */

static memory_ref free_available_space (pgm_globals_ptr globals, memory_ref the_avail_ref,
		size_t saved_position, size_t end_position)
{
	register available_ptr		available_list;
	register long				offset_ctr;
	memory_ref					avail_ref;
	size_t						block_size;
	
	avail_ref = the_avail_ref;
	available_list = (available_ptr) UseMemory(avail_ref);
	offset_ctr = 0;

	while (saved_position > *available_list) {

		available_list += 2;
		offset_ctr += 2;
	}
	
	UnuseMemory(avail_ref);

	block_size = GetMemorySize(avail_ref);
	SetMemorySize(avail_ref, block_size + PAIR_SIZE);

	available_list = (available_ptr) InsertMemory(avail_ref, offset_ctr * sizeof(long), PAIR_SIZE);
	*available_list++ = saved_position;
	*available_list = end_position;
	UnuseMemory(avail_ref);

	cleanup_available_list(globals, avail_ref);
	
	return	avail_ref;
}


/* This function locates and deletes all available list pairs that do not
belong in the list.  For example, available pairs of  0, 2, 2, 4 can be changed
to 0, 4. The function returns avail_ref (because some platforms might change
the reference). */

static memory_ref cleanup_available_list (pgm_globals_ptr globals, memory_ref the_avail_ref)
{
	size_t			list_size, offset;
	memory_ref		avail_ref;
	
	avail_ref = the_avail_ref;

	while (two_available_pairs_alike(avail_ref, &offset)) {

		list_size = GetMemorySize(avail_ref);
		DeleteMemory(avail_ref, offset, PAIR_SIZE);
	}
	
	return	avail_ref;
}


/* This gets called by cleanup_available_list.  The available list is scanned
for any two entries that match and returns the offset of the first entry in
*offset and TRUE as the function result if one is found.  */

static short two_available_pairs_alike (memory_ref avail_ref, size_t PG_FAR *offset)
{
	register available_ptr		available_list;
	register size_t				num_entries, offset_ctr;

	num_entries = GetMemorySize(avail_ref) / sizeof(long);
	available_list = (available_ptr) UseMemory(avail_ref);

	for (--num_entries, offset_ctr = 0;  num_entries;
			++available_list, offset_ctr += sizeof(long), --num_entries)
		if (*available_list == available_list[1]) {
			
			*offset = offset_ctr;
			UnuseMemory(avail_ref);

			return	TRUE;
		}
	
	UnuseMemory(avail_ref);

	return	FALSE;
}


/* extend_master_list extends the master list of memory records by DEF_MASTER_QTY and initializes
the necessary fields in mem_globals to allow for another entry. */

static void extend_master_list (pgm_globals_ptr mem_globals)
{
	long				extend_size = (MASTER_ENTRY_SIZE * DEF_MASTER_QTY);
	size_t				required_purge;
	pg_error			err;
#ifdef MAC_PLATFORM
	unsigned char		*zeros_ptr;
	long			     old_size;
	
	old_size = GetHandleSize(mem_globals->master_handle);

	for (;;) {
		
		required_purge = mem_globals->purge_threshold + extend_size;
		SetHandleSize(mem_globals->master_handle, old_size + extend_size);
		if ((err = MemError()) == NO_ERROR)
			break;
		
		pgFailNotError(mem_globals, NO_MEMORY_ERR, err);
		pgFailNIL(mem_globals, (void PG_FAR*) mem_globals->spare_tire);

		DisposeHandle(mem_globals->spare_tire);
		mem_globals->spare_tire = NULL;
		mem_globals->total_unpurged -= SPARE_TIRE_SIZE;

		err = MemoryPurge(mem_globals, required_purge, MEM_NULL);
		pgFailError(mem_globals, err);
		mem_globals->last_error = NO_ERROR;
	}

	mem_globals->next_master = old_size / MASTER_ENTRY_SIZE;
	zeros_ptr = (unsigned char *)*(mem_globals->master_handle);
	pgFillBlock(&zeros_ptr[old_size], extend_size, 0);

#endif

#ifdef WINDOWS_PLATFORM

	HANDLE			next_master;
	master_list_ptr	linked_master_list;
	long			master_qty;

// Before attempting to expand the list, first see if there is an open entry:

	next_master = mem_globals->master_handle;
	
	while (next_master) {
		
		linked_master_list = (master_list_ptr) GlobalLock(next_master);
		GlobalUnlock(next_master);  // (Pointer still locked since initial creation locked it)

		mem_globals->master_list = linked_master_list;
		mem_globals->next_master = 0;

		for (master_qty = MAX_HANDLE_ENTRIES; master_qty; ++linked_master_list, --master_qty) {
		
			if (*linked_master_list == NULL)
				return;
			
			mem_globals->next_master += 1;
		}
		
		next_master = *linked_master_list;
	}

	for (;;) {
		
		required_purge = mem_globals->purge_threshold + extend_size;
		
		if ((next_master = GlobalAlloc(GMEM_MOVEABLE | GMEM_ZEROINIT, extend_size)) != NULL)
			break;

		pgFailNIL(mem_globals, (void PG_FAR*) mem_globals->spare_tire);
		GlobalFree(mem_globals->spare_tire);
		mem_globals->spare_tire = NULL;
		mem_globals->total_unpurged -= SPARE_TIRE_SIZE;

		err = MemoryPurge(mem_globals, required_purge, MEM_NULL);
		pgFailError(mem_globals, err);
		mem_globals->last_error = NO_ERROR;
	}

	mem_globals->master_list[MAX_HANDLE_ENTRIES] = next_master;
	
// Set locked counter to one so this master list remains unmovable:

	mem_globals->master_list = (master_list_ptr)GlobalLock(next_master);
	mem_globals->next_master = 0;

#endif

#ifdef UNIX_PLATFORM
	pg_unix_master_table *table = pg_unix_master_from_handle(mem_globals->master_handle);
	size_t old_capacity = table ? table->capacity : 0;
	size_t new_capacity = old_capacity + DEF_MASTER_QTY;
	pg_handle *new_entries;

	for (;;) {
		new_entries = (pg_handle *)realloc(table->entries, new_capacity * sizeof(pg_handle));
		if (new_entries)
			break;
		pgFailNIL(mem_globals, (void PG_FAR*) mem_globals->spare_tire);
		pgFreeMemory(mem_globals->spare_tire);
		mem_globals->spare_tire = NULL;
		mem_globals->total_unpurged -= SPARE_TIRE_SIZE;
		err = MemoryPurge(mem_globals, mem_globals->purge_threshold + extend_size, MEM_NULL);
		pgFailError(mem_globals, err);
		mem_globals->last_error = NO_ERROR;
	}

	pgFillBlock(&new_entries[old_capacity], DEF_MASTER_QTY * sizeof(pg_handle), 0);
	table->entries = new_entries;
	table->capacity = new_capacity;
	mem_globals->master_list = new_entries;
	mem_globals->next_master = old_capacity;
#endif

	mem_globals->total_unpurged += (DEF_MASTER_QTY * MASTER_ENTRY_SIZE);

	if (!mem_globals->spare_tire)
		mem_globals->spare_tire = pgAllocMemory(SPARE_TIRE_SIZE);

#ifdef PG_DEBUG
	if (mem_globals->debug_flags & ALL_MEMORY_CHECK)
		pgCheckAllMemoryRefs();
#endif
}


static void dispose_all_memory (pgm_globals_ptr mem_globals, long memory_id, pg_boolean dispose_failed)
{
	register master_list_ptr	storage_list;
	register pgm_globals_ptr	globals;
	register long 				index;
	memory_ref					ref_in_question;
	mem_rec_ptr					data;
	long						mem_id, master_list_size;

	globals = mem_globals;

#ifdef MAC_PLATFORM
	
	master_list_size = GetHandleSize(globals->master_handle) / sizeof(Handle);

	for (index = 0; index < master_list_size; ++index) {

		storage_list = (master_list_ptr) *(globals->master_handle);
		ref_in_question = (memory_ref)storage_list[index];

		if (ref_in_question) {
			
			data = (mem_rec_ptr) *((Handle)ref_in_question);
			mem_id = data->mem_id;

			if ((!memory_id) || (mem_id == memory_id)) {
				
				if (dispose_failed)
					DisposeMemory(ref_in_question);
				else
					DisposeFailedMemory(ref_in_question);
			}
		}
	}
#endif

#ifdef WINDOWS_PLATFORM
	{
		HANDLE			next_master;
		
		next_master = globals->master_handle;
		
		while (next_master) {
			
			storage_list = (master_list_ptr)GlobalLock(next_master);
			GlobalUnlock(next_master);

			master_list_size = MAX_HANDLE_ENTRIES;

			for (index = 0; index < master_list_size; ++storage_list, ++index) {

				if (*storage_list) {
					
					ref_in_question = (memory_ref)storage_list;
					data = (mem_rec_ptr) GlobalLock(*storage_list);
					mem_id = data->mem_id;
					GlobalUnlock(*storage_list);

					if ((!memory_id) || (mem_id == memory_id)) {
						
						if (dispose_failed)
							DisposeMemory(ref_in_question);
						else
							DisposeFailedMemory(ref_in_question);
					}
				}
			}

			next_master = *storage_list;
		}
	}
#endif
}


/* unuse_all_memory gets called by UnuseAllMemory and UnuseAllFailedMemory. */

static void unuse_all_memory (pgm_globals_ptr mem_globals, long memory_id, pg_boolean failed_unuse)
{
	register master_list_ptr	storage_list;
	register pgm_globals_ptr	globals;
	register long 				index;
	mem_rec_ptr					data;
	memory_ref					ref_in_question;
	long						mem_id, master_list_size;
	
	globals = mem_globals;

#ifdef MAC_PLATFORM
	
	master_list_size = GetHandleSize(globals->master_handle) / sizeof(Handle);

	for (index = 0; index < master_list_size; ++index) {

		storage_list = (master_list_ptr) *(globals->master_handle);
		ref_in_question = (memory_ref)storage_list[index];

		if (ref_in_question) {
			
			data = (mem_rec_ptr) *((Handle)ref_in_question);
			mem_id = data->mem_id;

			if ((!memory_id) || (mem_id == memory_id)) {
				
				if (failed_unuse)
					UnuseFailedMemory(ref_in_question);
				else
					UnuseMemory(ref_in_question);
			}
		}
	}
#endif

#ifdef WINDOWS_PLATFORM
	{
		HANDLE			next_master;
		
		next_master = globals->master_handle;
		
		while (next_master) {
			
			storage_list = (master_list_ptr)GlobalLock(next_master);
			GlobalUnlock(next_master);
			master_list_size = MAX_HANDLE_ENTRIES;

			for (index = 0; index < master_list_size; ++storage_list, ++index) {

				if (*storage_list) {
					
					ref_in_question = (memory_ref)storage_list;
					data = (mem_rec_ptr) GlobalLock(*storage_list);
					mem_id = data->mem_id;
					GlobalUnlock(*storage_list);

					if ((!memory_id) || (mem_id == memory_id)) {

						if (failed_unuse)
							UnuseFailedMemory(ref_in_question);
						else
							UnuseMemory(ref_in_question);
					}
				}
			}

			next_master = *storage_list;
		}
	}
#endif
}


#ifdef PG_DEBUG

/* Checks a memory_ref (debug mode) and returns TRUE if it failed. The debug proc
also gets called if anything failed.  */
static pg_boolean check_bad_ref (memory_ref ref, short expected_access)
{
	register pg_bits8_ptr	checksum;
	mem_rec					data_rec;
	short					err, debug_flags;
	long					master_index, rec_check;
	pgm_globals_ptr			globals;
	
	if (err = pgCheckReference(ref)) {

#ifdef PG_DEBUG_STATICS
		debug_globals->debug_proc(err, ref);
#else
		pgDebugProc(err, ref);
#endif
		return	TRUE;
	}
	
	get_memory_record(ref, &data_rec);

	rec_check = pgGetByteSize(data_rec.rec_size, data_rec.num_recs);

	if ((rec_check < 0) || (rec_check > REASONABLE_REC_SIZE)) {

		data_rec.globals->debug_proc(BAD_LINK_ERR, ref);
		return	TRUE;
	}

	globals = data_rec.globals;
	master_index = data_rec.master_index;
	debug_flags = globals->debug_flags;

#ifdef MAC_PLATFORM

	if ((master_index < 0) || (master_index >= ( GetHandleSize(globals->master_handle) / sizeof(Handle)))) {
	
		globals->debug_proc(BAD_LINK_ERR, ref);
		return	TRUE;
	}

	if (debug_flags & NOT_DISPOSED_CHECK) {
		memory_ref_ptr		storage;
		memory_ref			ref_compare;
		
		storage = (memory_ref_ptr) *(globals->master_handle);
		ref_compare = storage[master_index];

		if ((!ref_compare) || (ref_compare != ref)) {
		
			globals->debug_proc(REF_DISPOSED_ERR, ref);
			return	TRUE;
		}
	}

#endif

#ifdef WINDOWS_PLATFORM
if (master_index == 0) {
		globals->debug_proc(BAD_LINK_ERR, ref);
		return	TRUE;
	}
#endif

	if (debug_flags & OVERWRITE_CHECK) {
		long			data_size;
		
		if (!(data_rec.purge & PURGED_FLAG)) {

			data_size = pgGetByteSize(data_rec.rec_size, data_rec.num_recs) + NON_APP_SIZE;
			checksum = (pg_bits8_ptr) pgMemoryPtr(ref);
			checksum += (data_size - CHECKSUM_SIZE);
			
#ifdef PG_BIG_CHECKSUMS
			{
				long			big_check_ctr;

				for (big_check_ctr = CHECKSUM_SIZE; big_check_ctr; --big_check_ctr)
					if (*checksum++ != BIG_CHECKSUM_BYTE) {
					
						globals->debug_proc(CHECKSUM_ERR, ref);
						return	TRUE;
					}
			}
#else
			if (*checksum != CHECKSUM_BYTE1 || *(checksum + 1) != CHECKSUM_BYTE2
				|| *(checksum + 2) != CHECKSUM_BYTE3 || *(checksum + 3) != CHECKSUM_BYTE4) {
				
					globals->debug_proc(CHECKSUM_ERR, ref);
					return	TRUE;
				}
#endif
			
			pgFreePtr(ref);
		}
	} 
	
	if ((debug_flags & ACCESS_CTR_CHECK) && expected_access) {
		switch (expected_access) {

			case access_dont_care:
			case access_positive:

				if (data_rec.access < 0) {
				
					globals->debug_proc(ACCESS_ERR, ref);
					return	TRUE;
				}
				
				break;

			case access_zero:
				if (data_rec.access) {
					globals->debug_proc(ACCESS_ERR, ref);
					return	TRUE;
				}

				break;
			
			case access_one:
				if (data_rec.access > 1) {
					globals->debug_proc(ACCESS_ERR, ref);
					return	TRUE;
				}

				break;
			
			case access_one_only:
				if (data_rec.access != 1) {
					globals->debug_proc(ACCESS_ERR, ref);
					return	TRUE;
				}

				break;

			case access_nonzero:
				if (data_rec.access <= 0) {
					globals->debug_proc(ACCESS_ERR, ref);
					return	TRUE;
				}

				break;

		}
	}
	
	if (debug_flags & ALL_MEMORY_CHECK) 
		return	pgCheckAllMemoryRefs();

	return	FALSE;
}

/* quick_ref_check gets called to check the memory_ref for trouble as fast as possible. The
access counter is not checked, nor is the referenced searched for in the master list (because
pgCheckAllMemory() calls this function so we already know it is "valid"). */

static pg_boolean quick_ref_check (memory_ref ref)
{
	register pg_bits8_ptr	checksum;
	mem_rec_ptr				block_data;
	mem_rec					data_rec;
	short					err, debug_flags;
	long					master_index, size_check;
#ifdef WINDOWS_PLATFORM
	long					rec_check;
#endif
	pgm_globals_ptr			globals;

	err = NO_ERROR;

	for (;;) {
		
		if (err = validate_address((void PG_FAR *) ref))
			break;

		block_data = pgMemoryPtr(ref);

		if (err = validate_address((void PG_FAR *) block_data))
			break;
		
		globals = block_data->globals;
	
		if (err = validate_address(globals))
			break;
	
		if (globals->signature != CHECKSUM_SIG)
			err = BAD_LINK_ERR;
		
		pgFreePtr(ref);

		break;
	}

	if (err) {

#ifdef PG_DEBUG_STATICS
		debug_globals->debug_proc(err, ref);
#endif
		return	TRUE;
	}

	get_memory_record(ref, &data_rec);

#ifdef MAC_PLATFORM
	if ((size_check = GetHandleSize((Handle)ref)) < sizeof(mem_rec)) {

		data_rec.globals->debug_proc(BAD_LINK_ERR, ref);
		return	TRUE;
	}
#else

	rec_check = pgGetByteSize(data_rec.rec_size, data_rec.num_recs);

	if ((rec_check < 0) || (rec_check > REASONABLE_REC_SIZE)) {

		data_rec.globals->debug_proc(BAD_LINK_ERR, ref);
		return	TRUE;
	}

#endif

	globals = data_rec.globals;
	master_index = data_rec.master_index;
	debug_flags = globals->debug_flags;

#ifdef MAC_PLATFORM

	if ((master_index < 0) || (master_index >= (GetHandleSize(globals->master_handle) / sizeof(Handle)))) {
	
		globals->debug_proc(BAD_LINK_ERR, ref);
		return	TRUE;
	}

	if (debug_flags & NOT_DISPOSED_CHECK) {
		memory_ref_ptr		storage;
		memory_ref			ref_compare;
		
		storage = (memory_ref_ptr) pgMemoryPtr(globals->master_handle);
		ref_compare = storage[master_index];
		pgFreePtr(globals->master_list);

		if ((!ref_compare) || (ref_compare != ref)) {
		
			globals->debug_proc(REF_DISPOSED_ERR, ref);
			return	TRUE;
		}
	}

#endif

#ifdef WINDOWS_PLATFORM
	if (master_index <= 0) {

		globals->debug_proc(BAD_LINK_ERR, ref);
		return	TRUE;
	}

#endif

	if (debug_flags & OVERWRITE_CHECK) {
		long			data_size;
		
		if (!(data_rec.purge & PURGED_FLAG)) {

			data_size = pgGetByteSize(data_rec.rec_size, data_rec.num_recs) + NON_APP_SIZE;
			checksum = (pg_bits8_ptr) pgMemoryPtr(ref);
			checksum += (data_size - CHECKSUM_SIZE);
			
#ifdef PG_BIG_CHECKSUMS
			{
				long			big_check_ctr;

				for (big_check_ctr = CHECKSUM_SIZE; big_check_ctr; --big_check_ctr)
					if (*checksum++ != BIG_CHECKSUM_BYTE) {
					
						globals->debug_proc(CHECKSUM_ERR, ref);
						return	TRUE;
					}
			}
#else
			if (*checksum != CHECKSUM_BYTE1 || *(checksum + 1) != CHECKSUM_BYTE2
				|| *(checksum + 2) != CHECKSUM_BYTE3 || *(checksum + 3) != CHECKSUM_BYTE4) {
				
					globals->debug_proc(CHECKSUM_ERR, ref);
					return	TRUE;
				}
#endif
			
			pgFreePtr(ref);
		}
	} 

	return	FALSE;
}


/* This one checks the range when accessing a part of a block. If error, this
returns TRUE.  */
static pg_boolean check_range (memory_ref ref, long rec_access, long num_recs)
{
	mem_rec_ptr			data;
	pgm_globals_ptr		globals;
	long				real_start, real_end, real_size;
	pg_short_t			rec_size;
	
	data = pgMemoryPtr(ref);
	
	rec_size = data->rec_size;
	globals = data->globals;
	real_size = pgGetByteSize(rec_size, data->num_recs);
	
	pgFreePtr(ref);

	real_start = pgGetByteSize(rec_size, rec_access);
	real_end = real_start + pgGetByteSize(rec_size, num_recs);
	
	if ((real_start > real_size) || (real_end > real_size)) {
	
		globals->debug_proc(RANGE_ERR, ref);
		return	TRUE;
	}
	
	return	FALSE;
}

/* This call is made to append the "checksum" value to the end of block. */

static void append_checksum (memory_ref ref)
{
	mem_rec_ptr						data;
	register pg_bits8_ptr			data_ptr;
	
	data = pgMemoryPtr(ref);
	data_ptr = (pg_bits8_ptr) data;

	data_ptr += (pgGetByteSize(data->rec_size, data->num_recs) + NON_APP_SIZE
			- CHECKSUM_SIZE);

#ifdef PG_BIG_CHECKSUMS
	{
	long		checksum_ctr;

		for (checksum_ctr = CHECKSUM_SIZE; checksum_ctr; --checksum_ctr)
			*data_ptr++ = BIG_CHECKSUM_BYTE;
	}
#else
	*data_ptr++ = CHECKSUM_BYTE1;
	*data_ptr++ = CHECKSUM_BYTE2;
	*data_ptr++ = CHECKSUM_BYTE3;
	*data_ptr = CHECKSUM_BYTE4;
#endif

	pgFreePtr(ref);
}


/* validate_address returns NO_ERROR or something else if the address is bad */
static short validate_address (void PG_FAR *address)
{
	if (!(long) address)
		return NIL_ADDRESS_ERR;

#ifdef ADDRESS_2

  {
    unsigned long				src_ptr;
    
	src_ptr = (unsigned long) address;
	if (src_ptr & 1)
		return BAD_ADDRESS_ERR;
  }
#endif

	return NO_ERROR;
}
#ifdef PG_DEBUG
PG_PASCAL (pg_boolean) ValidMemoryRef (memory_ref ref)
{
	if (ref == MEM_NULL)
		return	FALSE;

	if (validate_ref(ref) == NO_ERROR)
		return	TRUE;
	
	return	FALSE;
}
#endif

/* This function validates the memory ref itself (checks to see if it exists). */

static pg_error validate_ref (memory_ref ref)
{
#ifdef PG_DEBUG_STATICS

	register pgm_globals_ptr	globals;
	register long 				index;
	long						master_list_size;

#ifdef MAC_PLATFORM
	register memory_ref_ptr		storage_list;

	globals = debug_globals;
	master_list_size = GetHandleSize(globals->master_handle) / sizeof(Handle);
	storage_list = (memory_ref_ptr) *(globals->master_handle);

	for (index = 0; index < master_list_size; ++index) {
		
		if (storage_list[index] == ref)
			return	NO_ERROR;
	}
	
#endif

#ifdef WINDOWS_PLATFORM
	HANDLE						next_master;
	register master_list_ptr	storage_list;

	globals = debug_globals;
	next_master = globals->master_handle;
	master_list_size = MAX_HANDLE_ENTRIES;
	
	while (next_master) {
		
		storage_list = GlobalLock(next_master);
		GlobalUnlock(next_master);
		
		for (index = 0; index < master_list_size; ++storage_list, ++index)
			if (ref == (memory_ref)storage_list)
				return		NO_ERROR;
		
		next_master = (HANDLE)*storage_list;
	}

#endif
	return	BAD_REF_ERR;
#endif
	return		NO_ERROR;
}

#endif
