/* 
	Paige Universal I/O API
	
	File:		pgIO.h

	by T. R. Shaw

	Copyright й 1995 OITC, Inc.
	All rights reserved.
		
	pgIO.c - Universal I/O routines

	This software has been privately developed and falls within 
	DFARS 252.227-7013(c)(1) and associated regulations and it or its
	derivatives may not be used outside of the License agreement with 
	T. R. Shaw and OITC.  Copyright may not be removed.
	
	oitc@iu.net

	Revision History
	03/27/95	1.0b1	TRS - Initial beta for external customer Paige release
	07/27/95	1.0b2	TRS/CP - Windows DLL fixes
*/


#ifndef PGIO_H
#define PGIO_H

#include "CPUDefs.h"
#include "pgMemMgr.h"

#ifdef MAC_PLATFORM

#include <Types.h>
#include <Files.h>
#include <Folders.h>

/* File I/O */

#define PG_IO_CREATOR	'pgIO'
#define PG_BINARY_TYPE	'bin '
#define PG_TEMP_TYPE	'temp'

#define PG_RDWR			0x0003		/* open the file in read/write mode */
#define PG_RDONLY		0x0001		/* open the file in read only mode */
#define PG_WRONLY		0x0002		/* open the file in write only mode */
#define PG_APPEND		0x0004		/* open the file in append mode */
#define PG_CREAT		0x0008		/* create the file if it doesn't exist */
#define PG_EXCL			0x0010		/* if the file already exists don't create it again */
#define PG_TRUNC		0x0020		/* truncate the file after opening it */
#define PG_BINARY		0x0040		/* open the file in binary mode (default is text mode) */
#define PG_TEXT			0x0000		/* open the file in text mode */

typedef FSSpecPtr		pg_file_desc_ptr;	/* Pointer to file descriptor */
typedef memory_ref		pg_file_desc_ref;	/* Reference to file descriptor */

#endif

#ifdef WINDOWS_PLATFORM

#ifdef WINDOWS_EMULATE_COMPILE

#include "WinMac.h"

#else
#include <WINDOWS.H>
#endif

#include <stdio.h>
#include <string.h>

#if 0
//ееее
#define PG_RDWR			O_RDWR		/* open the file in read/write mode */
#define PG_RDONLY		O_RDONLY	/* open the file in read only mode */
#define PG_WRONLY		O_WRONLY	/* open the file in write only mode */
#define PG_APPEND		O_APPEND	/* open the file in append mode */
#define PG_CREAT		O_CREAT		/* create the file if it doesn't exist */
#define PG_EXCL			O_EXCL		/* if the file already exists don't create it again */
#define PG_TRUNC		O_TRUNC		/* truncate the file after opening it */
#define PG_BINARY		O_BINARY	/* create/open the file in binary mode (default is text mode) */
#define PG_TEXT			0x0000		/* create/open the file in text mode */
#endif

#define FILE_FLAG_WRITE_THROUGH         0x80000000
#define FILE_FLAG_OVERLAPPED            0x40000000
#define FILE_FLAG_NO_BUFFERING          0x20000000
#define FILE_FLAG_RANDOM_ACCESS         0x10000000
#define FILE_FLAG_SEQUENTIAL_SCAN       0x08000000
#define FILE_FLAG_DELETE_ON_CLOSE       0x04000000
#define FILE_FLAG_BACKUP_SEMANTICS      0x02000000
#define FILE_FLAG_POSIX_SEMANTICS       0x01000000

#define CREATE_NEW          1
#define CREATE_ALWAYS       2
#define OPEN_EXISTING       3
#define OPEN_ALWAYS         4
#define TRUNCATE_EXISTING   5

typedef char PG_FAR *	pg_file_desc_ptr;	/* Pointer to file descriptor */
typedef memory_ref		pg_file_desc_ref;	/* Reference to file descriptor */

#endif

#ifdef UNIX_PLATFORM

#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>

#define PG_RDWR		O_RDWR
#define PG_RDONLY	O_RDONLY
#define PG_WRONLY	O_WRONLY
#define PG_APPEND	O_APPEND
#define PG_CREAT	O_CREAT
#define PG_EXCL		O_EXCL
#define PG_TRUNC	O_TRUNC
#define PG_BINARY	0
#define PG_TEXT		0

typedef char *	pg_file_desc_ptr;
typedef memory_ref	pg_file_desc_ref;

#endif


#ifdef __cplusplus
extern	"C" {
#endif

extern PG_C (pg_file_desc_ref) pgFileSpec2FileDescriptor(const pgm_globals_ptr mem_globals, const pg_file_desc_ptr spec);
extern PG_C (pg_file_desc_ref) pgCreateFileDescriptor(const pgm_globals_ptr mem_globals, const pg_file_name_ptr file_name);
extern PG_C (pg_error) pgCreateFile(pg_file_desc_ref ref, short perm, pg_file_unit_ptr ref_num);
extern PG_C (pg_error) pgOpenFile(pg_file_desc_ref ref, short perm, pg_file_unit_ptr ref_num);
extern PG_C (pg_error) pgCloseFile(pg_file_unit ref_num);
extern PG_C (pg_error) pgDeleteFile(pg_file_desc_ref ref);
extern PG_C (pg_error) pgRenameFile(pg_file_desc_ref ref, const pg_file_name_ptr new_file_name);
extern PG_C (pg_file_desc_ref) pgOpenTempFile(const pgm_globals_ptr mem_globals, pg_file_unit_ptr temp_ref_num);
extern PG_C (pg_error) pgReadFileData(pg_file_unit ref_num, size_t byte_size, void PG_FAR *buffer);
extern PG_C (pg_error) pgWriteFileData(pg_file_unit ref_num, size_t byte_size, const void PG_FAR *buffer);
extern PG_C (void) pgSetTypeCreator(const pgm_globals_ptr mem_globals, long creator, long fileType);


/* To be added
	Get current directory, set directory, etc.
*/
#ifdef __cplusplus
	}
#endif


#ifdef MAC_PLATFORM

/* File I/O */

#ifdef NO_C_INLINE

#ifdef __cplusplus
extern	"C" {
#endif

extern PG_C (void) pgDisposeFileDescriptor(pg_file_desc_ref ref);
extern PG_C (pg_error) pgGetFileEOF(pg_file_unit ref_num, size_t PG_FAR *offset_result);
extern PG_C (pg_error) pgSetFileEOF(pg_file_unit ref_num, size_t offset);
extern PG_C (pg_error) pgGetFilePos(pg_file_unit ref_num, size_t PG_FAR *offset_result);
extern PG_C (pg_error) pgSetFilePos(pg_file_unit ref_num, size_t offset);
extern PG_C (pg_error) pgReadFileBytes(pg_file_unit ref_num, size_t PG_FAR *byte_size, void PG_FAR *buffer);
extern PG_C (pg_error) pgWriteFileBytes(pg_file_unit ref_num, size_t PG_FAR *byte_size,const void PG_FAR *buffer);

#ifdef __cplusplus
	}
#endif

#else

#define pgDisposeFileDescriptor(ref)					DisposeNonNilFailedMemory(ref)
#define pgGetFileEOF(ref_num,offset_result)				GetEOF(ref_num, offset_result)
#define pgSetFileEOF(ref_num,offset)					SetEOF(ref_num, offset)
#define pgGetFilePos(ref_num,offset_result)				GetFPos(ref_num, offset_result)
#define pgSetFilePos(ref_num,offset)					SetFPos(ref_num, fsFromStart, offset)
#define pgReadFileBytes(ref_num,byte_size,buffer)		FSRead(ref_num, byte_size, buffer)
#define pgWriteFileBytes(ref_num,byte_size,buffer)		FSWrite(ref_num, byte_size, buffer)

#endif

#endif

#ifdef UNIX_PLATFORM

#ifdef __cplusplus
extern	"C" {
#endif

extern PG_C (void) pgDisposeFileDescriptor(pg_file_desc_ref ref);
extern PG_C (pg_error) pgGetFileEOF(pg_file_unit ref_num, long PG_FAR *offset_result);
extern PG_C (pg_error) pgSetFileEOF(pg_file_unit ref_num, long offset);
extern PG_C (pg_error) pgGetFilePos(pg_file_unit ref_num, long PG_FAR *offset_result);
extern PG_C (pg_error) pgSetFilePos(pg_file_unit ref_num, long offset);
extern PG_C (pg_error) pgReadFileBytes(pg_file_unit ref_num, size_t PG_FAR *byte_size, void PG_FAR *buffer);
extern PG_C (pg_error) pgWriteFileBytes(pg_file_unit ref_num, size_t PG_FAR *byte_size, const void PG_FAR *buffer);

#ifdef __cplusplus
	}
#endif

#endif

#ifdef WINDOWS_PLATFORM

/* File I/O */

#ifdef __cplusplus
extern	"C" {
#endif

extern PG_C (pg_error) pgReadFileBytes(pg_file_unit ref_num, size_t PG_FAR *byte_size, void PG_FAR *buffer);
extern PG_C (pg_error) pgWriteFileBytes(pg_file_unit ref_num, size_t PG_FAR *byte_size, void PG_FAR *buffer);
extern PG_C (pg_error) pgSetFileEOF(pg_file_unit ref_num, size_t offset);

#ifdef __cplusplus
	}
#endif

#ifdef NO_C_INLINE

#ifdef __cplusplus
extern	"C" {
#endif

extern PG_C (void) pgDisposeFileDescriptor(pg_file_desc_ref ref);
extern PG_C (pg_error) pgGetFileEOF(pg_file_unit ref_num, size_t PG_FAR *offset_result);
extern PG_C (pg_error) pgGetFilePos(pg_file_unit ref_num, size_t PG_FAR *offset_result);
extern PG_C (pg_error) pgSetFilePos(pg_file_unit ref_num, size_t offset);

#ifdef __cplusplus
	}
#endif

#else

#define pgDisposeFileDescriptor(ref)					DisposeNonNilFailedMemory(ref)
#define pgGetFileEOF(ref_num, offset_result)			((*offset_result = _llseek(ref_num, 0L, SEEK_END)) == EOF) ? (pg_error)HFILE_ERROR : (pg_error)NO_ERROR
#define pgGetFilePos(ref_num, offset_result)			((*offset_result = _llseek(ref_num, 0L, SEEK_CUR)) == EOF) ? (pg_error)HFILE_ERROR : (pg_error)NO_ERROR
#define pgSetFilePos(ref_num, offset)					(_llseek(ref_num, (LONG)offset, SEEK_SET) == EOF) ? (pg_error)HFILE_ERROR : (pg_error)NO_ERROR

#endif

#endif

#endif
