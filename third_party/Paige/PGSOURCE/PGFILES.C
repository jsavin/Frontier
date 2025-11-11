/* This file contains the various functions for file read/write support. Only the
functions common to both pgRead and pgWrite are contained here.   

Changed 12 July 1994 pgFindHandlerFromKey to use memory size rather than struct size to allow the 
reuse of this code for extensions that extend the structure - TRS OITC, Inc. Board Member */

#include "Paige.h"

#ifdef MAC_PLATFORM
#pragma segment pgfiles
#endif

#include "machine.h"
#include "defprocs.h"
#include "pgText.h"
#include "pgUtils.h"
#include "pgSelect.h"
#include "pgDefStl.h"
#include "pgErrors.h"
#include "PackDefs.h"
#include "pgFiles.h"

/* pgDummyReadHandler is a do-nothing read handler function that can be used to
create a handler that does nothing for reading. */

PG_PASCAL (pg_boolean) pgDummyReadHandler (paige_rec_ptr pg, pg_file_key key, memory_ref key_data,
		long PG_FAR *element_info, void PG_FAR *aux_data, long PG_FAR *unpacked_size)
{
#ifdef MAC_PLATFORM
#pragma unused (pg, key, key_data, element_info, aux_data)
#endif

	*unpacked_size = 0;

	return	TRUE;
}


/* pgDummyWriteHandler is a do-nothing write handler function that can be used to
create a handler that does nothing for writing. */

PG_PASCAL (pg_boolean) pgDummyWriteHandler (paige_rec_ptr pg, pg_file_key key, memory_ref key_data,
		long PG_FAR *element_info, void PG_FAR *aux_data, long PG_FAR *unpacked_size)
{
#ifdef MAC_PLATFORM
#pragma unused (pg, key, element_info, aux_data)
#endif

	*unpacked_size = 0;
	
	if (key_data)
		SetMemorySize(key_data, 0);

	return	TRUE;
}


/* pgSetHandler sets a new key handler (or replaces an existing one). Any of
the function params can be NULL in which case the following occurs: if a new
handler record is added, the standard function is used. If a handler record
for this key already exists the function is left alone. */

PG_PASCAL (void) pgSetHandler (pg_globals_ptr globals, pg_file_key key,
		pg_handler_proc read_handler, pg_handler_proc write_handler,
		file_io_proc read_data_proc, file_io_proc write_data_proc)
{
	register pg_handler_ptr		handlers;
	pg_short_t					insert_spot;

	if (!globals->file_handlers)
		pgInitStandardHandlers(globals);
	
	if (key < 0)
		return;

	if (!(handlers = pgFindHandlerFromKey(globals->file_handlers, key, &insert_spot))) {
	
		handlers = (pg_handler_ptr) InsertMemory(globals->file_handlers, insert_spot, 1);
		pgInitOneHandler(handlers, key);
	}

	if (read_handler) {
	
		handlers->read_handler = read_handler;
		handlers->flags &= (~HAS_STANDARD_READ_HANDLER);
	}
	
	if (write_handler) {
	
		handlers->write_handler = write_handler;
		handlers->flags &= (~HAS_STANDARD_WRITE_HANDLER);
	}
	if (read_data_proc)
		handlers->read_data_proc = read_data_proc;
	if (write_data_proc)
		handlers->write_data_proc = write_data_proc;

	UnuseMemory(globals->file_handlers);
}


/* pgGetHandler returns the handler for the specified key (within handler) or,
if not found, returns NO_HANDLER_ERR.	*/

PG_PASCAL (pg_error) pgGetHandler (pg_globals_ptr globals, pg_handler_ptr handler)
{
	pg_handler_ptr		handlers;

	if (!globals->file_handlers)
		pgInitStandardHandlers(globals);

	if (!(handlers = pgFindHandlerFromKey(globals->file_handlers, handler->key, NULL)))
		return	NO_HANDLER_ERR;
	
	pgBlockMove(handlers, handler, sizeof(pg_handler));
	UnuseMemory(globals->file_handlers);
	
	return	NO_ERROR;
}


/* pgRemoveHandler removes the handler "key" completely. */

PG_PASCAL (pg_error) pgRemoveHandler (pg_globals_ptr globals, pg_file_key key)
{
	pg_handler_ptr		handlers;
	pg_short_t			loc;

	if (!globals->file_handlers)
		return	NO_HANDLER_ERR;

	if (!(handlers = pgFindHandlerFromKey(globals->file_handlers, key, &loc)))
		return	NO_HANDLER_ERR;
	
	UnuseMemory(globals->file_handlers);
	
	DeleteMemory(globals->file_handlers, loc, 1);
	
	if (!GetMemorySize(globals->file_handlers)) {
		
		DisposeMemory(globals->file_handlers);
		globals->file_handlers = MEM_NULL;
	}
	
	return	NO_ERROR;
}



#if 0
/* Given a specific key, this function returns the matching handler pointer
(which will be the result of a UseMemory on handlers_list). Or, if
the key is not found, NULL is returned.  If rec_location is non-NULL, the record
number is returned of the key found, or it is the place to insert a new one
(considering the numerical order of the key).  */

PG_PASCAL (pg_handler_ptr) pgFindHandlerFromKey (memory_ref handlers_list,
		pg_file_key key, pg_short_t PG_FAR *rec_location)
{
	pg_handler_ptr		result;
	pg_short_t			key_location, key_qty;

	if (key < 0)
		return	NULL;

	result = UseMemory(handlers_list);
	key_qty = GetMemorySize(handlers_list);
	key_location = 0;

	while (key_location < key_qty) {
		
		if (result->key >= key)
			break;

		++result;
		++key_location;
	}

	if (rec_location)
		*rec_location = key_location;

	if ((key_location == key_qty) || (result->key != key)) {
		
		UnuseMemory(handlers_list);
		result = NULL;
	}

	return	result;
}
#else
/* Given a specific key, this function returns the matching handler pointer
(which will be the result of a UseMemory on handlers_list). Or, if
the key is not found, NULL is returned.  If rec_location is non-NULL, the record
number is returned of the key found, or it is the place to insert a new one
(considering the numerical order of the key).
5/13/94 TRS/OITC Modified to use memory record size for walking the structure */

PG_PASCAL (pg_handler_ptr) pgFindHandlerFromKey (memory_ref handlers_list,
		pg_file_key key, pg_short_t PG_FAR *rec_location)
{
	pg_handler_ptr		result = NULL;
	pg_short_t			key_location = 0;
	pg_short_t			key_qty;

	if (key < 0)
		return	NULL;

	if (handlers_list && (key_qty = (pg_short_t)GetMemorySize(handlers_list)))
	{
		result = (pg_handler_ptr) UseMemoryRecord (handlers_list, key_location, USE_ALL_RECS, TRUE);
	
		for (;;) {
			
			if (result->key >= key)
				break;

			if (++key_location >= key_qty)
				break;
			
			result = (pg_handler_ptr) UseMemoryRecord (handlers_list, key_location, USE_ALL_RECS, FALSE);
		}
	
		if ((key_location == key_qty) || (result->key != key)) {
			
			UnuseMemory(handlers_list);
			result = NULL;
		}
	}
	if (rec_location)
		*rec_location = key_location;	
	
	return	result;
}
#endif


/* pgBuildHandlerList builds a list of file handler records based on keys / num_keys.
If keys are NULL, the default set of handlers in globals is used.  */

PG_PASCAL (memory_ref) pgBuildHandlerList (pg_globals_ptr globals, pg_file_key_ptr keys,
		pg_short_t num_keys)
{
	memory_ref		result;

	if (!keys)
		result = MemoryDuplicate(globals->file_handlers);
	else {
		pg_handler_ptr		handlers, def_handlers;
		pg_file_key_ptr		keys_to_use;
		pg_short_t			key_qty, keys_found;

		result = MemoryAlloc(globals->mem_globals, sizeof(pg_handler), num_keys, 2);
		handlers = (pg_handler_ptr) UseMemory(result);
		keys_to_use = keys;

		for (key_qty = num_keys, keys_found = 0; key_qty; ++keys_to_use, --key_qty)
			if (def_handlers = pgFindHandlerFromKey(globals->file_handlers,
					*keys_to_use, NULL)) {
				
				pgBlockMove(def_handlers, handlers, sizeof(pg_handler));
				++handlers;
				++keys_found;
				UnuseMemory(globals->file_handlers);
			}
		
		UnuseMemory(result);
		SetMemorySize(result, keys_found);
	}
	
	return	result;
}


/* pgSetupPacker sets up a pack_walk record given the data reference and
beginning offset. */

PG_PASCAL (void) pgSetupPacker (pack_walk_ptr walker, memory_ref ref, long first_offset)
{
	pgFillBlock(walker, sizeof(pack_walk), 0);

	walker->data_ref = ref;
	walker->transfered = walker->first_offset = first_offset;
	walker->remaining_ctr = GetMemorySize(ref) - first_offset;
	walker->data = (pg_bits8_ptr) UseMemory(ref);
	walker->data += first_offset;
}



/* pgDoExceptionKey handles an error by calling the exception_key handler. (The
last error is placed in pg->globals->mem_globals->last_error).  */

PG_PASCAL (pg_error) pgDoExceptionKey (paige_rec_ptr pg, memory_ref handlers,
	 short error_code, short for_write, memory_ref data)
{
	pg_handler_ptr		handler;
	pg_handler_proc		proc_to_call;
	memory_ref			handlers_to_use;
	size_t				original_size;
	long				error_for_call;
	pg_error			final_error;

	pg->globals->mem_globals->last_error = error_code;
	final_error = error_code;
	error_for_call = final_error;
	if (!(handlers_to_use = handlers))
		handlers_to_use = pg->globals->file_handlers;
	
	original_size = 0;

	if (handler = pgFindHandlerFromKey(handlers_to_use, exception_key, NULL)) {
		
		if (for_write)
			proc_to_call = handler->write_handler;
		else
			proc_to_call = handler->read_handler;
		
		for (;;)
			if (proc_to_call(pg, exception_key, data, &error_for_call, NULL, &original_size))
				break;

		UnuseMemory(handlers_to_use);
		
		final_error = (pg_error)error_for_call;
	}
	else
		final_error = NO_ERROR;
	
	pg->globals->mem_globals->last_error = final_error;

	return	final_error;
}


/* pgUnpackHex translates the incoming hex number and returns the number of
bytes looked at.  The result (unpacked value) is in *value, but value can be
NULL which simply returns the number of packed bytes looked at.  */

PG_PASCAL (long) pgUnpackHex (pg_bits8_ptr data, long PG_FAR *value)
{
	register pg_bits8_ptr		hex_data;
	register long				in_ctr, result;
	register pg_bits8			the_byte;
	pg_bits8					first_byte;

	hex_data = data;
	in_ctr = result = 0;
	
	if ((first_byte = *hex_data) == MINUS_SIGN) {
		
		++hex_data;
		++in_ctr;
	}

	for (;;) {

		the_byte = *hex_data++;
		
		if ((the_byte < '0') || (the_byte > 'F'))
			break;
		
		++in_ctr;

		result <<= 4;
		result |= pgHexToByte(the_byte);
	}

	if (first_byte == MINUS_SIGN)
		result = -result;

	if (value)
		*value = result;
	
	return	in_ctr;
}


/* pgHexToByte a real number translated from an ASCII hex byte */

PG_PASCAL (pg_short_t) pgHexToByte (pg_bits8 hex_byte)
{
	pg_short_t		result;
	
	result = hex_byte;

	if ((result -= '0') > 9)
		result -= 7;
	
	return	result;
}

