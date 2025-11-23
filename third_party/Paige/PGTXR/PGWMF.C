/* jWMF.c     
   Routines for decoding a metafile and displaying them on the screen.
 */
 
 #include "CPUDEFS.h"
 
 #ifdef MAC_PLATFORM
 
 #include "PGWMF.h"
 #include "utilities.h"
 #include "machine.h"
 #include <string.h>
 #include <Strings.h>
 #include "app.h"
 #include "pgtxr.h"
 #include "pgdeftbl.h"
 

static pgm_globals_ptr m_globals;
// Returns unsigned short and increments Ptr	

WORD GetWordFromByteArray(const pg_bits8_ptr DataBytes)
{
	WORD FirstWord = 0;
	pg_bits8_ptr tmpPtr = NULL;
	
	tmpPtr = DataBytes;
	
	
	FirstWord = *++tmpPtr;
	FirstWord = FirstWord << 8;
	FirstWord = FirstWord | *--tmpPtr;
	
	
	return (FirstWord);
}


// Returns unsigned long and increments Ptr	
DWORD GetDWordFromByteArray(const pg_bits8_ptr DataBytes)
{
	DWORD FirstDWord = 0;
	pg_bits8_ptr tmpPtr = NULL;
	
	tmpPtr = DataBytes;
	
	FirstDWord = (DWORD)(*tmpPtr++ & 0xFF);
	FirstDWord |= (((DWORD) (*tmpPtr++ & 0xFF)) << 8);
	FirstDWord |= (((DWORD) (*tmpPtr++ & 0xFF)) << 16);
	FirstDWord |= (((DWORD) (*tmpPtr & 0xFF )) << 24);
	
	return (FirstDWord);
}


// Gets the metafile header
int GetMetaHeaderFromByteArray(pg_bits8_ptr* DataBytes, WMFHEAD* metaHeader)
{
	
	
	metaHeader->FileType  = (short)GetWordFromByteArray(*DataBytes);
	*DataBytes += 2;
	if ((metaHeader->FileType != 1) && (metaHeader->FileType != 2))
		return (FAILED);
	
	metaHeader->HeaderSize = (short)GetWordFromByteArray(*DataBytes);
	*DataBytes += 2;

	if (metaHeader->HeaderSize != META_HEADER_SIZE)
		return (FAILED);
		
	metaHeader->Version = (short)GetWordFromByteArray(*DataBytes);
	*DataBytes += 2;

	if (metaHeader->Version != META_VERSION)
		return (FAILED);
		
	metaHeader->FileSize = (DWORD)GetDWordFromByteArray(*DataBytes);
	*DataBytes += 4;
	
//	if ((metaHeader->FileSize * 2) != data_size)
//		return (FAILED);
		

	metaHeader->NumOfObjects = (short)GetWordFromByteArray(*DataBytes);
	*DataBytes += 2;
	
	metaHeader->MaxRecordSize = (DWORD)GetDWordFromByteArray(*DataBytes);
	*DataBytes += 4;

	metaHeader->NoParamaters = (short)GetWordFromByteArray(*DataBytes);
	*DataBytes += 2;
	
	if (metaHeader->NoParamaters != 0)
		return (FAILED);
	
	return(SUCCESS);
}

//returns 0 if success, else 1
short GetMetaRecordParams(pg_bits8_ptr* DataBytes, WMFRECORD* metaRecord)
{
	long 		numParams = 0;
	short 		i = 0;
	WORD* 		ptr_Params = NULL;
	char arr[256];

	
	numParams = metaRecord->Size - 3;
	
	metaRecord->Parameters = MemoryAlloc(m_globals, sizeof(WORD), numParams, 0);
	if (metaRecord->Parameters == MEM_NULL)
	{
		return (FAILED);
	}
	else
	{
		ptr_Params = (WORD*)UseMemory(metaRecord->Parameters);
		for (i = 0; i < numParams; i++)
		{
			ptr_Params[i] = (short)GetWordFromByteArray(*DataBytes);
			*DataBytes += 2;
		}
		UnuseMemory(metaRecord->Parameters);
		ptr_Params = NULL;
		
	}
	
	return(SUCCESS);

}

// retrieves a metarecord from the data array
void GetMetaRecordFromByteArray(pg_bits8_ptr* DataBytes, WMFRECORD* metaRecord)
{
	
	metaRecord->Size = (DWORD)GetDWordFromByteArray(*DataBytes);
	*DataBytes += 4;
	metaRecord->Function = (short)GetWordFromByteArray(*DataBytes);
	*DataBytes += 2;
	metaRecord->Parameters = MEM_NULL;

	return;

}


// calculates the width in bytes of a scanline with padding.
short CalculateWINScanLineLength(short Width)
{
	short ScanlineLength = Width;
	
	
	while (ScanlineLength % WIN_SCAN_LINE_PADDING)
		ScanlineLength++;

	return (ScanlineLength);
}

// calculates the width in bytes of a scanline with padding.
short CalculateMACScanLineLength(short Width)
{
	short WINScanLineLength;
	short MacScanLineLength;
	
	WINScanLineLength = CalculateWINScanLineLength(Width);
	
	MacScanLineLength = WINScanLineLength;
	
	while (MacScanLineLength % MAC_SCAN_LINE_PADDING)
		MacScanLineLength++;
	
	return (MacScanLineLength);


}


// Returns 0 on Success, 1 on Failure
short AddMACScanLinePadding(memory_ref* mfptr_stretchDibParams)
{
	long 				currentIndex = 0;
	long 				tmpScanIndex = 0;
	long 				NewScanTableSize = 0;
	long 				WINScanLineLength = 0;
	long 				MacScanLineLength = 0;
	long 				width_bytes = 0;
	long 				DiffScanLineLength = 0;
	long 				i = 0, j = 0;
	short	 				bits_per_pixel = 0;
	long 				AdjustedScanLineLength = 0;
	
	memory_ref 			mf_tmpScan = MEM_NULL;
	STRETCHDIBPARAMS* 	ptr_StretchDibParams = NULL;
	pg_bits8_ptr		ptr_NewScanTable = NULL;
	pg_bits8_ptr		ptr_OriginalScanTable = NULL;
	BITMAPINFO* 		ptr_BitmapInfo = NULL;
	
	
	
	ptr_StretchDibParams = (STRETCHDIBPARAMS*)UseMemory(*mfptr_stretchDibParams);
	ptr_BitmapInfo = (BITMAPINFO*)UseMemory(ptr_StretchDibParams->bmi);
	if (ptr_BitmapInfo->bmiHeader.BitsPerPixel == 24)
	{
		UnuseMemory(ptr_StretchDibParams->bmi);
		ptr_BitmapInfo = NULL;
		UnuseMemory(*mfptr_stretchDibParams);
		ptr_StretchDibParams = NULL;
		return (SUCCESS);
	}
	
	width_bytes = GetWidthInBytes((short)ptr_BitmapInfo->bmiHeader.Width, (short)ptr_BitmapInfo->bmiHeader.BitsPerPixel);
	bits_per_pixel = (short)ptr_BitmapInfo->bmiHeader.BitsPerPixel;
	
	WINScanLineLength = CalculateWINScanLineLength(width_bytes);
	if (bits_per_pixel == 24)
	{
		AdjustedScanLineLength = WINScanLineLength + ptr_BitmapInfo->bmiHeader.Width;
	}
	else
	{
		AdjustedScanLineLength = WINScanLineLength;
	}
	MacScanLineLength = CalculateMACScanLineLength(AdjustedScanLineLength);
	DiffScanLineLength = MacScanLineLength - WINScanLineLength;
	
	if (DiffScanLineLength != 0)
	{
		NewScanTableSize = MacScanLineLength * ptr_BitmapInfo->bmiHeader.Height;
	
		mf_tmpScan = MemoryAlloc(m_globals, sizeof(BYTE), NewScanTableSize, 0);
		if (mf_tmpScan == MEM_NULL)
		{
			UnuseMemory(ptr_StretchDibParams->bmi);
			ptr_BitmapInfo = NULL;
			UnuseMemory(*mfptr_stretchDibParams);
			ptr_StretchDibParams = NULL;
			return (FAILED);
		}
	
		ptr_OriginalScanTable = (BYTE*)UseMemory(ptr_StretchDibParams->ScanTable);
		ptr_NewScanTable = (BYTE*)UseMemory(mf_tmpScan);
	
		for (i = 0; i < ptr_BitmapInfo->bmiHeader.Height; i++)
		{
			memcpy(&ptr_NewScanTable[tmpScanIndex], &ptr_OriginalScanTable[currentIndex], WINScanLineLength);
			tmpScanIndex += WINScanLineLength;
			currentIndex += WINScanLineLength;
		
			for (j = tmpScanIndex; j < tmpScanIndex + DiffScanLineLength; j++)
			{
				ptr_NewScanTable[j] = 0;
			}
			tmpScanIndex = j;
		}
		UnuseMemory(mf_tmpScan);
		ptr_NewScanTable = NULL;
	}
	UnuseMemory(ptr_StretchDibParams->bmi);
	ptr_BitmapInfo = NULL;

	if (DiffScanLineLength != 0)
	{
	
		UnuseMemory(ptr_StretchDibParams->ScanTable);
		ptr_OriginalScanTable = NULL;
		(ptr_StretchDibParams->ScanTable);
		ptr_StretchDibParams->ScanTable = MEM_NULL;
		ptr_StretchDibParams->ScanTable = mf_tmpScan;
	}
	mf_tmpScan = MEM_NULL;
	UnuseMemory(*mfptr_stretchDibParams);
	ptr_StretchDibParams = NULL;
	
	return (SUCCESS);	
	
}

//Create a new colortable, used for GWorld
ColorTable** CreateNewPixMapCTab(memory_ref mf_StretchDibParams)
{
	RGBColor 			rgbMac = {0, 0, 0};
	ColorSpec*			color_ptr = NULL;
	ColorTable**		new_table = NULL;
	STRETCHDIBPARAMS*	ptr_StretchDibParams = NULL;
	BITMAPINFO*			ptr_BitmapInfo = NULL;
	short					number_entries = 0;
	RGBQUAD*			ptr_rgbqColors = NULL;
	short					i = 0;
	
	ptr_StretchDibParams = (STRETCHDIBPARAMS*)UseMemory(mf_StretchDibParams);
	ptr_BitmapInfo = (BITMAPINFO*)UseMemory(ptr_StretchDibParams->bmi);
	ptr_rgbqColors = (RGBQUAD*)UseMemory(ptr_BitmapInfo->bmiColors);
	
	if (ptr_BitmapInfo->bmiHeader.ColorsUsed == 0)
	{
		number_entries = (1 << ptr_BitmapInfo->bmiHeader.BitsPerPixel);
	}
	else
	{
		number_entries = ptr_BitmapInfo->bmiHeader.ColorsUsed;
	}
	new_table = (ColorTable**)NewHandle(sizeof(ColorTable) + ((sizeof(ColorSpec) * (number_entries - 1))));
	if (new_table == NULL)
	{
		UnuseMemory(ptr_BitmapInfo->bmiColors);
		ptr_rgbqColors = NULL;
		
		UnuseMemory(ptr_StretchDibParams->bmi);
		ptr_BitmapInfo = NULL;
		
		UnuseMemory(mf_StretchDibParams);
		ptr_StretchDibParams = NULL;
		
		return (NULL);
	}
	HLock((char**)new_table);
	color_ptr = (*new_table)->ctTable;
	
	
	for (i = 0; i < number_entries; i++)
	{
		color_ptr[i].value = i;
		color_ptr[i].rgb = ConvertRGBQuadToRGBColor(ptr_rgbqColors[i]);
			
	}
	(**new_table).ctSeed = 0;
	(**new_table).ctFlags = 0;
	(**new_table).ctSize = number_entries -1;
	
	UnuseMemory(ptr_BitmapInfo->bmiColors);
	ptr_rgbqColors = NULL;
	UnuseMemory(ptr_StretchDibParams->bmi);
	ptr_BitmapInfo = NULL;
	UnuseMemory(mf_StretchDibParams);
	ptr_StretchDibParams = NULL;
	
	return (new_table);	
}

// Returns 0 on success, 1 on failure
short ConvertWIN24toMAC32(memory_ref mf_stretchDibParams)
{
	STRETCHDIBPARAMS* 	ptr_StretchDibParams = NULL;
	BYTE*				ptr_OriginalScanTable = NULL;
	BYTE*				ptr_NewScanTable = NULL;
	memory_ref			mf_newScanTable = MEM_NULL;
	BITMAPINFO*			ptr_BitmapInfo = NULL;
	long				MacScanlineLength = 0;
	long				SizeMacBitmap = 0;
	long				WINScanlineLength = 0;
	long				SizeWINBitmap = 0;
	long				WIN_width_bytes = 0;
	long				MAC_width_bytes = 0;
	long				i = 0, j = 0, k= 0;
	short					WIN_padding = 0;
	
	ptr_StretchDibParams = (STRETCHDIBPARAMS*)UseMemory(mf_stretchDibParams);
	ptr_OriginalScanTable = (BYTE*)UseMemory(ptr_StretchDibParams->ScanTable);
	ptr_BitmapInfo = (BITMAPINFO*)UseMemory(ptr_StretchDibParams->bmi);
	
	if (ptr_BitmapInfo->bmiHeader.BitsPerPixel != 24)
	{
		UnuseMemory(ptr_StretchDibParams->bmi);
		ptr_BitmapInfo = NULL;
		UnuseMemory(ptr_StretchDibParams->ScanTable);
		ptr_OriginalScanTable = NULL;
		UnuseMemory(mf_stretchDibParams);
		ptr_StretchDibParams = NULL;
		return (SUCCESS);
	}
	WIN_width_bytes = GetWidthInBytes((short)ptr_BitmapInfo->bmiHeader.Width, (short)ptr_BitmapInfo->bmiHeader.BitsPerPixel);
	WINScanlineLength = CalculateWINScanLineLength(WIN_width_bytes);
	WIN_padding = WINScanlineLength - WIN_width_bytes; 
	MAC_width_bytes = WIN_width_bytes + ptr_BitmapInfo->bmiHeader.Width;
	SizeMacBitmap = MAC_width_bytes * ptr_BitmapInfo->bmiHeader.Height;
	SizeWINBitmap = WINScanlineLength * ptr_BitmapInfo->bmiHeader.Height;
	
	
	mf_newScanTable = MemoryAllocClear(m_globals, sizeof(BYTE), SizeMacBitmap, 0);
	if (mf_newScanTable == MEM_NULL)
	{
		UnuseMemory(ptr_StretchDibParams->bmi);
		ptr_BitmapInfo = NULL;
		UnuseMemory(ptr_StretchDibParams->ScanTable);
		ptr_OriginalScanTable = NULL;
		UnuseMemory(mf_stretchDibParams);
		ptr_StretchDibParams = NULL;
		return (FAILED);
	}
	ptr_NewScanTable = (BYTE*)UseMemory(mf_newScanTable);
	
	for (i = 0; i < ptr_BitmapInfo->bmiHeader.Height; i++)
	{
		long start_index = WINScanlineLength * i;
		long stop_index = start_index + WIN_width_bytes;
		
		for (j = start_index, k = i * MAC_width_bytes; j < stop_index;  j += 3, k += 4)
		{
			ptr_NewScanTable[k] =  0;
			ptr_NewScanTable[k + 1] = ptr_OriginalScanTable[j + 2];
			ptr_NewScanTable[k + 2] = ptr_OriginalScanTable[j + 1];
			ptr_NewScanTable[k + 3] = ptr_OriginalScanTable[j];
		}
	}
	UnuseMemory(ptr_StretchDibParams->bmi);
	ptr_BitmapInfo = NULL;

	UnuseMemory(ptr_StretchDibParams->ScanTable);
	ptr_OriginalScanTable = NULL;
	
	
	DisposeMemory(ptr_StretchDibParams->ScanTable);
	ptr_StretchDibParams->ScanTable = MEM_NULL;

	UnuseMemory(mf_newScanTable);
	ptr_NewScanTable = NULL;
	ptr_StretchDibParams->ScanTable = mf_newScanTable;
	UnuseMemory(mf_stretchDibParams);
	ptr_StretchDibParams = NULL;
	
	return (SUCCESS);
	
}

// Decodes both RLE8 and RLE4 encoded Windows Bitmaps, Returns 0 if Failes, else Number of Bytes decoded. NOTE: number of bytes decoded may
// not be accurate if delta escape sequences occured since the bytes skipped in the scantable are not subtracted from the final index value
// in the scantable.

//NOTE: RLE4 Encoded Bitmaps are untested, I could not find an example.
long DecodeRLEScanTable(pg_bits8_ptr ptr_ScanTable, short ScanlineLength, long SizeScanTable, short compress_method, pg_bits8_ptr* DataBytes)
{
	BYTE 		run_count 		= 0;  	// num pixels in run
	BYTE 		run_value 		= 0;	// value of pixels in run
	BYTE 		tmp_value 		= 0;	// temporary pixel value holder
	long		ScanTableIndex	= 0;	// index in ptr_ScanTable
	short			bLeast_4bits	= 0;
	
	if ((compress_method != WIN_COMPRESS_RLE8) && (compress_method != WIN_COMPRESS_RLE4))
	{
		return (FAILED);
	}
	
	while (ScanTableIndex < SizeScanTable)
	{
		run_count = **DataBytes;
		*DataBytes += 1;
		run_value = **DataBytes;
		*DataBytes += 1;
		
		switch(run_count)
		{
			case 0:
				switch(run_value)
				{
					case 0:
						{ 
							long rem_scanline_bytes = 0;
							
							if (ScanTableIndex % ScanlineLength != 0)
								rem_scanline_bytes = ScanlineLength - (ScanTableIndex % ScanlineLength);
						
							if (bLeast_4bits)
							{
								ptr_ScanTable[ScanTableIndex++] != 0;
								rem_scanline_bytes--;
							}
						
							while (rem_scanline_bytes)
							{
								ptr_ScanTable[ScanTableIndex++] = 0;
								rem_scanline_bytes--;
							}
						}		
								
						break;
					case 1:
						return (ScanTableIndex);
						break;
					case 2:
						{
							BYTE 	XOffset = 0;
							BYTE 	YOffset = 0;
							long	total_linear_offset = 0;
	
							XOffset = **DataBytes;
							*DataBytes += 1;
						
							YOffset = **DataBytes;
							*DataBytes += 1;
							
							if (compress_method == WIN_COMPRESS_RLE8)
							{
							
								total_linear_offset = (YOffset * ScanlineLength) + XOffset;
								if (ScanTableIndex + total_linear_offset >= SizeScanTable)
									return (0);
							
								ScanTableIndex += total_linear_offset;
							}
							else // WIN_COMPRESS_RLE4 case
							{
								//not coded yet
								return (0);
							}
										
						}
						break;
					default:  // deal with literal run
						if (compress_method == WIN_COMPRESS_RLE8)
						{
							BYTE tmp_run_value = run_value;
							
							if (ScanTableIndex + run_value > SizeScanTable)
							{
								return(0);
							}
							
							while (run_value--)
							{
								ptr_ScanTable[ScanTableIndex++] = **DataBytes;
								*DataBytes += 1;
							}
							if (tmp_run_value % 2)
							{
								*DataBytes += 1;
							}
						}	
						else if (compress_method == WIN_COMPRESS_RLE4)
						{
							BYTE tmp_run_value2 = run_value;
							
							if (ScanTableIndex + ((double)run_count / (double)2) >= SizeScanTable)
							{
								return(0);
							}

							while (run_value--)
							{
								tmp_value = **DataBytes;
								*DataBytes += 1;
								
								if (bLeast_4bits)
								{
									ptr_ScanTable[ScanTableIndex++] |= highest_nibble_in_byte(tmp_value);
									
									if (run_value--)
									{	
										ptr_ScanTable[ScanTableIndex] = (least_nibble_in_byte(tmp_value) << 4);
									}
									else
									{
										bLeast_4bits = FALSE;
									}
								}
								else
								{
									ptr_ScanTable[ScanTableIndex] = (highest_nibble_in_byte(tmp_value) << 4);
								
									if (run_value--)
									{
										ptr_ScanTable[ScanTableIndex++] |= least_nibble_in_byte(tmp_value);
									
									}
									else
									{
										bLeast_4bits = TRUE;
									}
								}
								
							}
						}
						break;		
				}
				break;
			default: 	// deal with RLE encoded runs
				if (compress_method == WIN_COMPRESS_RLE4)
				{
					if (ScanTableIndex + ((double)run_count / (double)2) >= SizeScanTable)
					{
						return(0);
					}
					
					while (run_count--)
					{
						ptr_ScanTable[ScanTableIndex] = (highest_nibble_in_byte(run_value) << 4);
						if (run_count--)
						{
							ptr_ScanTable[ScanTableIndex++] |= least_nibble_in_byte(run_value);
						}
					}
				}
				else if (compress_method == WIN_COMPRESS_RLE8)
				{
					if (ScanTableIndex + run_count >= SizeScanTable)
						return(0);
					
					while (run_count--)
					{
						ptr_ScanTable[ScanTableIndex++] = run_value;
					}
				}
				else
				{
					// unrecognized compress_method value
					return (0);
				
				}
				break;
		
		}
	}// end main while loop	
	return (ScanTableIndex);

}

//Returns 0 on Success and 1 on Failure
short GetBitmapInfo(pg_bits8_ptr* DataBytes, memory_ref* bitmapInfo, memory_ref* ScanTableEntries)
{
	short 				number_entries = 0;
	long 				ScanlineLength = 0;
	short 				width_bytes = 0;
	BITMAPINFO* 		ptr_BitmapInfo = NULL; 
	pg_bits8_ptr		ptr_ScanTable = NULL;
	RGBQUAD* 			ptr_Colors = NULL;
	long				SizeScanTable = 0;
	
	ptr_BitmapInfo = (BITMAPINFO*)UseMemory(*bitmapInfo);
	
	
	ptr_BitmapInfo->bmiHeader.Size = GetDWordFromByteArray(*DataBytes);
	*DataBytes += 4;
	ptr_BitmapInfo->bmiHeader.Width = (long) GetDWordFromByteArray(*DataBytes);
	*DataBytes += 4;
	ptr_BitmapInfo->bmiHeader.Height = (long) GetDWordFromByteArray(*DataBytes);
	*DataBytes += 4;
	ptr_BitmapInfo->bmiHeader.Planes = GetWordFromByteArray(*DataBytes);
	*DataBytes += 2;
	ptr_BitmapInfo->bmiHeader.BitsPerPixel = GetWordFromByteArray(*DataBytes);
	*DataBytes += 2;
	ptr_BitmapInfo->bmiHeader.Compression = GetDWordFromByteArray(*DataBytes);
	*DataBytes += 4;
	ptr_BitmapInfo->bmiHeader.SizeOfBitmap = GetDWordFromByteArray(*DataBytes);
	*DataBytes += 4;
	ptr_BitmapInfo->bmiHeader.HorzResolution = (long)GetDWordFromByteArray(*DataBytes);
	*DataBytes += 4;
	ptr_BitmapInfo->bmiHeader.VertResolution = (long)GetDWordFromByteArray(*DataBytes);
	*DataBytes += 4;
	ptr_BitmapInfo->bmiHeader.ColorsUsed = GetDWordFromByteArray(*DataBytes);
	*DataBytes += 4;
	ptr_BitmapInfo->bmiHeader.ColorsImportant = GetDWordFromByteArray(*DataBytes);
	*DataBytes += 4;
	
	if (ptr_BitmapInfo->bmiHeader.BitsPerPixel <= 8)
	{
		if (ptr_BitmapInfo->bmiHeader.ColorsUsed == 0)
		{
			if (ptr_BitmapInfo->bmiHeader.BitsPerPixel == 1)
			{
				number_entries = 2;
			}
			else
			{
				number_entries = (1 << ptr_BitmapInfo->bmiHeader.BitsPerPixel);
			}
		}
		else
		{
			number_entries = ptr_BitmapInfo->bmiHeader.ColorsUsed;
		}
		ptr_BitmapInfo->bmiColors = MemoryAlloc(m_globals, sizeof(RGBQUAD), number_entries, 0);
		if (ptr_BitmapInfo->bmiColors == MEM_NULL)
		{
			UnuseMemory(*bitmapInfo);
			ptr_BitmapInfo = NULL;
			DisposeMemory(*bitmapInfo);
			*bitmapInfo = MEM_NULL;
			return (FAILED);
		}
		else
		{
			ptr_Colors = (RGBQUAD*)UseMemory(ptr_BitmapInfo->bmiColors);
			memcpy(ptr_Colors, *DataBytes, ((size_t)sizeof(RGBQUAD) * number_entries));
			*DataBytes += (sizeof(RGBQUAD) * number_entries);
			UnuseMemory(ptr_BitmapInfo->bmiColors);
			ptr_Colors = NULL;
		}
	}
	else
	{
		ptr_BitmapInfo->bmiColors = MEM_NULL;
	}
	
	
	width_bytes = GetWidthInBytes((short)ptr_BitmapInfo->bmiHeader.Width, (short)ptr_BitmapInfo->bmiHeader.BitsPerPixel);
	ScanlineLength = CalculateWINScanLineLength(width_bytes);

	if (*ScanTableEntries != MEM_NULL)
	{
		DisposeMemory(*ScanTableEntries);
		*ScanTableEntries = MEM_NULL;
	}
	
	SizeScanTable = ScanlineLength * ptr_BitmapInfo->bmiHeader.Height;

	
	*ScanTableEntries = MemoryAlloc(m_globals, sizeof(BYTE), SizeScanTable, 0);
	if (*ScanTableEntries == MEM_NULL)
	{
		if (ptr_BitmapInfo->bmiColors != MEM_NULL)
		{
			DisposeMemory(ptr_BitmapInfo->bmiColors);
			ptr_BitmapInfo->bmiColors = MEM_NULL;
		}
		UnuseMemory(*bitmapInfo);
		ptr_BitmapInfo = NULL;
		DisposeMemory(*bitmapInfo);
		*bitmapInfo = MEM_NULL;
		return (FAILED);
	}
	else
	{
		ptr_ScanTable = (BYTE*)UseMemory(*ScanTableEntries);
		if (ptr_BitmapInfo->bmiHeader.Compression == WIN_COMPRESS_RGB)
		{
			memcpy(ptr_ScanTable, *DataBytes, SizeScanTable);
			*DataBytes += SizeScanTable;
		}
		else
		{
			
			if (!(DecodeRLEScanTable(ptr_ScanTable, ScanlineLength, SizeScanTable, ptr_BitmapInfo->bmiHeader.Compression, DataBytes)))
			{
				UnuseMemory(*ScanTableEntries);
				ptr_ScanTable = NULL;
				DisposeMemory(*ScanTableEntries);
				*ScanTableEntries = MEM_NULL;
				if (ptr_BitmapInfo->bmiColors != MEM_NULL)
				{
					DisposeMemory(ptr_BitmapInfo->bmiColors);
					ptr_BitmapInfo->bmiColors = MEM_NULL;
				}
				UnuseMemory(*bitmapInfo);
				ptr_BitmapInfo = NULL;
				DisposeMemory(*bitmapInfo);
				*bitmapInfo = MEM_NULL;
				return(FAILED);
			}
		}
		UnuseMemory(*ScanTableEntries);
		ptr_ScanTable = NULL;
	}
	
	UnuseMemory(*bitmapInfo);
	ptr_BitmapInfo = NULL;
	
	return (SUCCESS);
}
	

// Returns 0 on Success or 1 on Failure
short GetStretchDibParams(pg_bits8_ptr* DataBytes, memory_ref mf_stretchDibParams)
{
	STRETCHDIBPARAMS* 	ptr_stretchDIBParams = NULL;
	BITMAPINFO*			ptr_BitmapInfo = NULL;
	pg_bits8_ptr		ptr_ScanTable = NULL;
	
	ptr_stretchDIBParams = (STRETCHDIBPARAMS*)UseMemory(mf_stretchDibParams);
	
	
	ptr_stretchDIBParams->dwRop = GetDWordFromByteArray(*DataBytes);
	*DataBytes += 4;

	ptr_stretchDIBParams->iUsage = GetWordFromByteArray(*DataBytes);
	*DataBytes += 2;


	ptr_stretchDIBParams->nSrcHeight = GetWordFromByteArray(*DataBytes);
	*DataBytes += 2;


	ptr_stretchDIBParams->nSrcWidth = GetWordFromByteArray(*DataBytes);
	*DataBytes += 2;


	ptr_stretchDIBParams->YSrc = GetWordFromByteArray(*DataBytes);
	*DataBytes += 2;


	ptr_stretchDIBParams->XSrc = GetWordFromByteArray(*DataBytes);
	*DataBytes += 2;


	ptr_stretchDIBParams->nDestHeight = GetWordFromByteArray(*DataBytes);
	*DataBytes += 2;
	
	ptr_stretchDIBParams->nDestWidth = GetWordFromByteArray(*DataBytes);
	*DataBytes += 2;
	
	ptr_stretchDIBParams->YDest = GetWordFromByteArray(*DataBytes);
	*DataBytes += 2;

	ptr_stretchDIBParams->XDest = GetWordFromByteArray(*DataBytes);
	*DataBytes += 2;

	ptr_stretchDIBParams->ScanTable = MEM_NULL;
	
	ptr_stretchDIBParams->bmi = MemoryAlloc(m_globals, sizeof(BITMAPINFO), 1, 0);
	if (ptr_stretchDIBParams->bmi == MEM_NULL)
	{
		UnuseMemory(mf_stretchDibParams);
		ptr_stretchDIBParams = NULL;
		DisposeMemory(mf_stretchDibParams);
		mf_stretchDibParams = MEM_NULL;
		return (FAILED);
	}
	
	if (GetBitmapInfo(DataBytes, &(ptr_stretchDIBParams->bmi), &(ptr_stretchDIBParams->ScanTable)))
	{
		if (ptr_stretchDIBParams->bmi != MEM_NULL)
		{
			ptr_BitmapInfo = (BITMAPINFO*)UseMemory(ptr_stretchDIBParams->bmi);
			if (ptr_BitmapInfo->bmiColors != MEM_NULL)
			{
				DisposeMemory(ptr_BitmapInfo->bmiColors);
				ptr_BitmapInfo->bmiColors = MEM_NULL;
			}
			UnuseMemory(ptr_stretchDIBParams->bmi);
			ptr_BitmapInfo = NULL;
			DisposeMemory(ptr_stretchDIBParams->bmi);
			ptr_stretchDIBParams->bmi = MEM_NULL;
		}
		if (ptr_stretchDIBParams->ScanTable != MEM_NULL)
		{
			DisposeMemory(ptr_stretchDIBParams->ScanTable);
			ptr_stretchDIBParams->ScanTable = MEM_NULL;
		}
		UnuseMemory(mf_stretchDibParams);
		ptr_stretchDIBParams = NULL;
		DisposeMemory(mf_stretchDibParams);
		mf_stretchDibParams = MEM_NULL;
		return (FAILED);
	}


	
	//cleanup
	UnuseMemory(mf_stretchDibParams);
	ptr_stretchDIBParams = NULL;
	
	return (SUCCESS);
}

RGBColor ConvertCOLORREFtoRGBColor(DWORD ColorRef)
{
	RGBColor rgbMac;
	
	rgbMac.red = (short)((ColorRef & COLORREF_RED) << 8);
	rgbMac.red += (short)(ColorRef & COLORREF_RED);
	rgbMac.green = (short)(ColorRef & COLORREF_GREEN);
	rgbMac.green += (short)((ColorRef & COLORREF_GREEN) >> 8);
	rgbMac.blue = (short)((ColorRef & COLORREF_BLUE) >> 8);
	rgbMac.blue += (short)((ColorRef & COLORREF_BLUE) >> 16);

	return (rgbMac);
}
	
RGBColor ConvertRGBQuadToRGBColor(RGBQUAD rgbq)
{
	RGBColor rgbMac;
	
	rgbMac.red  = (short)((rgbq.Red << 8) + rgbq.Red);
	rgbMac.green = (short)((rgbq.Green << 8) + rgbq.Green);
	rgbMac.blue = (short)((rgbq.Blue << 8) + rgbq.Blue);
	
	return (rgbMac);
}

RGBColor GetCOLORREF(pg_bits8_ptr* DataBytes)
{
	DWORD ColorRef = 0;
	

	ColorRef = (*DataBytes)[3];
	ColorRef = (ColorRef << 8);
	ColorRef += (BYTE)(*DataBytes)[2];
	ColorRef = (ColorRef << 8);
	ColorRef += (*DataBytes)[1];
	ColorRef = ColorRef << 8;
	ColorRef += (*DataBytes)[0];



	*DataBytes +=4;
	
	return (ConvertCOLORREFtoRGBColor(ColorRef));
}





// Returns width of scanline in bytes, or 0 if fails
short GetWidthInBytes(short width_pixels, short bits_per_pixel)
{
	double tmpDbl = 0;
	
	switch (bits_per_pixel)
	{
		case (1):
			tmpDbl = ((double)width_pixels / (double)8);
			return ((short)(tmpDbl + 0.9));
			break;
		case (4):
			tmpDbl = ((double)width_pixels / (double)2);
			return ((short)(tmpDbl + 0.5));
			break;
		case(8):
			return (width_pixels);
			break;
		case(24):
			return (width_pixels * 3);
			break;
		default:
			return (0);
			break;
	}
	 return (0);
}
	
	

// REturns 0 on success or 1 on failure	
short FlipDibScanTable(memory_ref* mfptr_stretchDibParams)
{

	STRETCHDIBPARAMS* 	ptr_StretchDibParams = NULL;
	BITMAPINFO* 		ptr_BitmapInfo = NULL;
	short 				ScanLineLength = 0;

	short 				width_bytes = 0;
	short 				Height = 0;
	long 				tmpScanIndex = 0;
	short 				tmpHeight = 0;
	memory_ref 			mf_tmpScan = MEM_NULL;
	pg_bits8_ptr		ptr_NewScanTable = NULL;
	pg_bits8_ptr		ptr_OriginalScanTable = NULL;
	short 				bits_per_pixel = 0;
	
	
	ptr_StretchDibParams = (STRETCHDIBPARAMS*)UseMemory(*mfptr_stretchDibParams);
	ptr_BitmapInfo = (BITMAPINFO*)UseMemory(ptr_StretchDibParams->bmi);
	
	width_bytes = GetWidthInBytes((short)ptr_BitmapInfo->bmiHeader.Width, (short)ptr_BitmapInfo->bmiHeader.BitsPerPixel);
	bits_per_pixel = ptr_BitmapInfo->bmiHeader.BitsPerPixel;
	
	Height = ptr_BitmapInfo->bmiHeader.Height;
	
	if (bits_per_pixel == 24)
	{
		ScanLineLength = width_bytes + ptr_BitmapInfo->bmiHeader.Width;
	}
	else
	{
		ScanLineLength = CalculateWINScanLineLength (width_bytes);
	}
	
	UnuseMemory(ptr_StretchDibParams->bmi);
	ptr_BitmapInfo = NULL;

	mf_tmpScan = MemoryAlloc(m_globals, sizeof(BYTE), ScanLineLength * Height, 0);
	if (mf_tmpScan == MEM_NULL)
	{
		UnuseMemory(*mfptr_stretchDibParams);
		ptr_StretchDibParams = NULL;
		return (FAILED);
	}
	
	ptr_OriginalScanTable = (BYTE*)UseMemory(ptr_StretchDibParams->ScanTable);
	ptr_NewScanTable = (BYTE*)UseMemory(mf_tmpScan);
	
	for (tmpHeight = Height - 1, tmpScanIndex = 0; tmpHeight >= 0; tmpHeight--, tmpScanIndex += ScanLineLength)
	{
		if (tmpHeight)
		{
			memcpy(&ptr_NewScanTable[tmpScanIndex], (ptr_OriginalScanTable + (ScanLineLength * tmpHeight)), ScanLineLength);
		}
		else
		{
			memcpy(&ptr_NewScanTable[tmpScanIndex], ptr_OriginalScanTable, ScanLineLength);
		}
	}
	UnuseMemory(ptr_StretchDibParams->ScanTable);
	DisposeMemory(ptr_StretchDibParams->ScanTable);
	ptr_StretchDibParams->ScanTable = MEM_NULL;
	UnuseMemory(mf_tmpScan);
	ptr_StretchDibParams->ScanTable = mf_tmpScan;
	mf_tmpScan = MEM_NULL;
	ptr_NewScanTable = NULL;
	ptr_OriginalScanTable = NULL;
	UnuseMemory(*mfptr_stretchDibParams);
	ptr_StretchDibParams = NULL;
	
	
	return(SUCCESS);

}



short GetWINOrigin (pg_bits8_ptr* DataBytes, Point* WINOrigin)
{
	WINOrigin->v = (short)GetWordFromByteArray(*DataBytes);
	*DataBytes += 2;
	WINOrigin->h = (short)GetWordFromByteArray(*DataBytes);
	*DataBytes += 2;
	
	return (1);
}

void GetRect(pg_bits8_ptr* DataBytes, Rect* rectangle, Point WINOrigin, Point WINExt, Rect localframe, WMFRECORD* metaRecord)
{
	double		ConversionFactorX = 1;
	double		ConversionFactorY = 1;
	short			data_size_in_bytes =  0;
	pg_bits8_ptr target_data_pos = NULL;

	if (metaRecord != NULL)
	{
		data_size_in_bytes = ((*metaRecord).Size - 3) * 2;
		target_data_pos = *DataBytes + data_size_in_bytes;
	}
	
	ConversionFactorY = ((double)localframe.bottom/(double)(abs(WINExt.v)));
	ConversionFactorX = ((double)localframe.right/ (double)(abs(WINExt.h)));
	
	

	
	rectangle->bottom = /*abs*/((short)GetWordFromByteArray(*DataBytes));
	rectangle->bottom = /*abs*/((short)(((rectangle->bottom - WINOrigin.v) * ConversionFactorY) )); 
	*DataBytes += 2;
	
	rectangle->right = /*abs*/((short)GetWordFromByteArray(*DataBytes));
	rectangle->right = /*abs*/((short)(((rectangle->right - WINOrigin.h) * ConversionFactorX) ));
	*DataBytes +=2;
	
	
	
	rectangle->top = /*abs*/((short)GetWordFromByteArray(*DataBytes));
	rectangle->top = /*abs*/((short)(((rectangle->top - WINOrigin.v) * ConversionFactorY) ));
	*DataBytes += 2;
	
	rectangle->left = /*abs*/((short)GetWordFromByteArray(*DataBytes));
	rectangle->left = /*abs*/((short)(((rectangle->left - WINOrigin.h) * ConversionFactorX) ));
	*DataBytes += 2;
	
	
	if (rectangle->bottom < rectangle->top)
	{
		short tmp = -1;
		
		tmp = rectangle->bottom;
		rectangle->bottom = rectangle->top;
		rectangle->top = (short)tmp;
	}
	
	if (rectangle->right < rectangle->left)
	{
		short tmp = -1;
		
		tmp = rectangle->right;
		rectangle->right = rectangle->left;
		rectangle->left = (short)tmp;
	}

	
	if (metaRecord != NULL)
		*DataBytes = target_data_pos;
	
	return;
}

	

memory_ref GetPolygonParams(pg_bits8_ptr* DataBytes,short *NumPoints, Point WINOrigin, Point WINExt, Rect localframe, WMFRECORD* metaRecord)
{
	
	memory_ref				mf_Pts = MEM_NULL;
	Point*					ptr_Points = NULL;
	short 					i;
	double					ConversionFactorX = 1;
	double					ConversionFactorY = 1;
	long					data_size_in_bytes = ((*metaRecord).Size - 3)* 2;
	pg_bits8_ptr 			target_data_pos = *DataBytes + data_size_in_bytes;

	ConversionFactorY = (double)((double)localframe.bottom / (double)(abs(WINExt.v)));
	ConversionFactorX = (double)((double)localframe.right / (double)(abs(WINExt.h)));

	
	*NumPoints = (short)GetWordFromByteArray(*DataBytes);
	*DataBytes+=2;
	
	mf_Pts = MemoryAlloc(m_globals, sizeof(Point), *NumPoints, 0);
	if (mf_Pts == MEM_NULL)
	{
		return(MEM_NULL);
	}
	else
	{
		ptr_Points = (Point*)UseMemory(mf_Pts);
		
		for (i = 0; i < *NumPoints; i++)
		{
			ptr_Points[i].h =  (short)GetWordFromByteArray(*DataBytes);
			ptr_Points[i].h = abs((short)(((ptr_Points[i].h - WINOrigin.h) * ConversionFactorX)));
			*DataBytes += 2;
		
			ptr_Points[i].v = (short)GetWordFromByteArray(*DataBytes);
			ptr_Points[i].v = abs((short)((ptr_Points[i].v - WINOrigin.v) * ConversionFactorY));
			*DataBytes += 2;
			
		}
		UnuseMemory(mf_Pts);
		ptr_Points = NULL;
	}
	*DataBytes = target_data_pos;
	
	return (mf_Pts);

}




void GetFontParams(pg_bits8_ptr* DataBytes, LOGFONT* font, WMFRECORD* metaRecord)
{
	short i = -1;
	pg_bits8_ptr target_data_pos = NULL;
	short size_data_in_bytes = ((*metaRecord).Size - 3) * 2;
	
	target_data_pos = *DataBytes + size_data_in_bytes;
	
	font->lfHeight = (short)GetWordFromByteArray(*DataBytes);
	*DataBytes += 2;
	
	font->lfWidth = (short)GetWordFromByteArray(*DataBytes);
	*DataBytes += 2;
	
	font->lfEscapement = (short)GetWordFromByteArray(*DataBytes);
	*DataBytes +=2;
	
	font->lfOrientation = (short)GetWordFromByteArray(*DataBytes);
	*DataBytes += 2;
	
	font->lfWeight = (short)GetWordFromByteArray(*DataBytes);
	*DataBytes += 2;
	
	font->lfItalic = **DataBytes;
	*DataBytes += 1;
	
	font->lfUnderline = **DataBytes;
	*DataBytes += 1;
	
	font->lfStrikeOut = **DataBytes;
	*DataBytes += 1;
	
	font->lfCharSet = **DataBytes;
	*DataBytes += 1;
	
	font->lfOutPrecision = **DataBytes;
	*DataBytes += 1;
	
	font->lfClipPrecision = **DataBytes;
	*DataBytes += 1;
	
	font->lfQuality = **DataBytes;
	*DataBytes += 1;
	
	font->lfPitchAndFamily = **DataBytes;
	*DataBytes += 1;
	
	for (i = 0; i < LF_FACESIZE; i++)
	{
		font->lfFaceName[i] = (char)**DataBytes;
		*DataBytes += 1;
		if (font->lfFaceName[i] == NULL)
			break;
	}
	i = -1;

	*DataBytes = target_data_pos;
	return;
}
	
	


void GetPenParams(pg_bits8_ptr* DataBytes, LOGPEN* pen, WMFRECORD* metaRecord)
{
	short size_data_in_bytes = ((*metaRecord).Size - 3) * 2;
	pg_bits8_ptr target_data_pos = *DataBytes + size_data_in_bytes;
	
	pen->lopnStyle = (short)GetWordFromByteArray(*DataBytes);
	*DataBytes += 2;
	pen->lopnWidth.v = (short)GetWordFromByteArray(*DataBytes);
	*DataBytes += 2;
	pen->lopnWidth.h = (short)GetWordFromByteArray(*DataBytes);
	*DataBytes += 2;
	pen->lopnColor = GetCOLORREF(DataBytes);

	
	if (pen->lopnWidth.v == 0)
		pen->lopnWidth.v = 1;
	if (pen->lopnWidth.h == 0)
		pen->lopnWidth.h = 1;
	
	*DataBytes = target_data_pos;
	
	return;
	
}


void GetBrushParams (pg_bits8_ptr* DataBytes, LOGBRUSH* brush, WMFRECORD* metaRecord)
{
	short data_size_in_bytes = ((*metaRecord).Size - 3) * 2;
	pg_bits8_ptr target_data_pos = *DataBytes + data_size_in_bytes;
	
	brush->lbStyle = (short)GetWordFromByteArray(*DataBytes);
	*DataBytes += 2;
	brush->lbColor = GetCOLORREF(DataBytes);
	brush->lbHatch = (short)GetWordFromByteArray(*DataBytes);
	*DataBytes += 2;
	
	if (brush->lbStyle == BS_HATCHED)
	{
		if (brush->lbHatch == 0)
		{
			brush->lbHatch = HS_HORIZONTAL;
		}
		else if (brush->lbHatch == -1)
		{
			
			brush->lbStyle = BS_SOLID;
			brush->lbHatch = 0;
		}
	}
	
	
	*DataBytes = target_data_pos;
	return;
}

//Returns 0 on Success, 1 on Failure
short AddFontToFontTable(short* currentFontIndex, short* currentNumElements, memory_ref* mfptr_font_table, LOGFONT* ptr_font)
{

	short 				i = -1;
	long 				MemSize = 0;
	LOGFONT* 			ptr_tmpFont = NULL;
	
	if (*currentFontIndex == -1)
	{
		*mfptr_font_table = MemoryAlloc(m_globals, sizeof(LOGFONT), GDI_OBJECT_MIN_RESIZE, GDI_OBJECT_MIN_RESIZE);
		if (*mfptr_font_table == MEM_NULL)
		{
			return(FAILED);
		}
		else
		{
			*currentFontIndex = 0;
			*currentNumElements = GDI_OBJECT_MIN_RESIZE;
		}
	}
	if ((*currentFontIndex) >= (*currentNumElements))
	{
		MemSize = GetMemorySize(*mfptr_font_table);
		SetMemorySize(*mfptr_font_table, MemSize + GDI_OBJECT_MIN_RESIZE);
		*currentNumElements = MemSize + GDI_OBJECT_MIN_RESIZE;
		 
	}
	
	ptr_tmpFont = (LOGFONT*)UseMemory(*mfptr_font_table);
	
	ptr_tmpFont[*currentFontIndex].lfHeight = ptr_font->lfHeight;
	ptr_tmpFont[*currentFontIndex].lfWidth = ptr_font->lfWidth;
	ptr_tmpFont[*currentFontIndex].lfEscapement = ptr_font->lfEscapement;
	ptr_tmpFont[*currentFontIndex].lfOrientation = ptr_font->lfOrientation;
	ptr_tmpFont[*currentFontIndex].lfWeight = ptr_font->lfWeight;
	ptr_tmpFont[*currentFontIndex].lfItalic = ptr_font->lfItalic;
	ptr_tmpFont[*currentFontIndex].lfUnderline = ptr_font->lfUnderline;
	ptr_tmpFont[*currentFontIndex].lfStrikeOut = ptr_font->lfStrikeOut;
	ptr_tmpFont[*currentFontIndex].lfCharSet = ptr_font->lfCharSet;
	ptr_tmpFont[*currentFontIndex].lfOutPrecision = ptr_font->lfOutPrecision;
	ptr_tmpFont[*currentFontIndex].lfClipPrecision = ptr_font->lfClipPrecision;
	ptr_tmpFont[*currentFontIndex].lfQuality = ptr_font->lfQuality;
	ptr_tmpFont[*currentFontIndex].lfPitchAndFamily = ptr_font->lfPitchAndFamily;
	
	for (i = 0; i < LF_FACESIZE; i++)
	{
		ptr_tmpFont[*currentFontIndex].lfFaceName[i] = ptr_font->lfFaceName[i];
	}
	i = -1;
	
	if (ptr_tmpFont[*currentFontIndex].lfFaceName == NULL)
	{
		strcpy (ptr_tmpFont[*currentFontIndex].lfFaceName, "System");
	
	}
	
	*currentFontIndex += 1;
	
	UnuseMemory(*mfptr_font_table);
	ptr_tmpFont = NULL;
	return (SUCCESS);

}


//Returns 0 on success or 1 for failed
short AddPenToPenTable(short* currentPenIndex, short* CurrentNumElements, memory_ref* mfptr_pen_table, LOGPEN* ptr_pen)
{
	long 		MemSize = 0;
	long 		tmpLong = 0;
	LOGPEN* 	ptr_tmpPenTable = NULL;
	
	if(*currentPenIndex == -1)
	{
		*mfptr_pen_table = MemoryAlloc(m_globals, sizeof(LOGPEN), GDI_OBJECT_MIN_RESIZE, GDI_OBJECT_MIN_RESIZE);
		if (*mfptr_pen_table == MEM_NULL)
		{
			return (FAILED);
		}
		else
		{
			*currentPenIndex = 0;
			*CurrentNumElements = GDI_OBJECT_MIN_RESIZE;
		}
	}
	if ((*currentPenIndex) >= (*CurrentNumElements))
	{
		MemSize = GetMemorySize(*mfptr_pen_table);
		tmpLong = *CurrentNumElements + MemSize;
		SetMemorySize(*mfptr_pen_table, tmpLong);
		*CurrentNumElements = tmpLong;
	}
	
	ptr_tmpPenTable = (LOGPEN*)UseMemory(*mfptr_pen_table);
	
	ptr_tmpPenTable[*currentPenIndex].lopnStyle = ptr_pen->lopnStyle;
	ptr_tmpPenTable[*currentPenIndex].lopnWidth = ptr_pen->lopnWidth;
	ptr_tmpPenTable[*currentPenIndex].lopnColor = ptr_pen->lopnColor;
	
	*currentPenIndex += 1;
	UnuseMemory(*mfptr_pen_table);
	ptr_tmpPenTable = NULL;
	MemSize = 0;
	tmpLong = 0;
	
	
	return (SUCCESS);


}


//disposes of memory_ref containing STRETCHDIBPARAMS structure. NOTE: memory_ref access counter should be 0.
void DisposeStretchDibRef(memory_ref* mfptr_stretchDib)
{
	STRETCHDIBPARAMS* ptr_SDBP = NULL;
	BITMAPINFO*		  ptr_bmi = NULL;
	
	if (*mfptr_stretchDib == MEM_NULL)
		return;
		
	ptr_SDBP = (STRETCHDIBPARAMS*)UseMemory(*mfptr_stretchDib);
	if (ptr_SDBP->bmi != MEM_NULL)
	{
		ptr_bmi = (BITMAPINFO*)UseMemory(ptr_SDBP->bmi);
		if (ptr_bmi->bmiColors != MEM_NULL)
		{
			DisposeMemory(ptr_bmi->bmiColors);
			ptr_bmi->bmiColors = MEM_NULL;
		}	
		UnuseMemory(ptr_SDBP->bmi);
		ptr_bmi = NULL;
		DisposeMemory(ptr_SDBP->bmi);
		ptr_SDBP->bmi = MEM_NULL;
	}
	if (ptr_SDBP->ScanTable != MEM_NULL)
	{
		DisposeMemory(ptr_SDBP->ScanTable);
		ptr_SDBP->ScanTable = MEM_NULL;
	}
	UnuseMemory(*mfptr_stretchDib);
	ptr_SDBP = NULL;
	DisposeMemory(*mfptr_stretchDib);
	*mfptr_stretchDib = MEM_NULL;
	return;
}

//makes a copy of STRETCHDIBPARAMS structure stored in memory_ref and returns the new memory_ref. Returns MEM_NULL
// if fails.
memory_ref MakeCopyStretchDib(memory_ref mf_stretchDibParams)
{
	memory_ref mf_newSDB = MEM_NULL;
	
	if ((mf_newSDB = MemoryDuplicate(mf_stretchDibParams)) != MEM_NULL)
	{
		STRETCHDIBPARAMS* ptrSDBP = NULL;
		STRETCHDIBPARAMS* ptrOriginalSDBP = NULL;
		BITMAPINFO*		  ptr_bmi = NULL;
		BITMAPINFO*       ptr_Original_bmi = NULL;

		ptrSDBP = (STRETCHDIBPARAMS*)UseMemory(mf_newSDB);
		ptrOriginalSDBP = (STRETCHDIBPARAMS*)UseMemory(mf_stretchDibParams);
		
		if ((ptrSDBP->bmi = MemoryDuplicate(ptrOriginalSDBP->bmi)) != MEM_NULL)
		{
			ptr_bmi = (BITMAPINFO*)UseMemory(ptrSDBP->bmi);
			ptr_Original_bmi = (BITMAPINFO*)UseMemory(ptrOriginalSDBP->bmi);
		
			if ((ptr_bmi->bmiColors = MemoryDuplicate(ptr_Original_bmi->bmiColors)) == MEM_NULL)
			{
				
				UnuseMemory(ptrOriginalSDBP->bmi);
				ptr_Original_bmi = NULL;
				UnuseMemory(ptrSDBP->bmi);
				ptr_bmi = NULL;
				DisposeMemory(ptrSDBP->bmi);
				ptrSDBP->bmi = MEM_NULL;
				UnuseMemory(mf_newSDB);
				ptrSDBP = NULL;
				DisposeMemory(mf_newSDB);
				mf_newSDB = MEM_NULL;
				UnuseMemory(mf_stretchDibParams);
				ptrOriginalSDBP = NULL;
				return (MEM_NULL);		
			}
			UnuseMemory(ptrOriginalSDBP->bmi);
			ptr_Original_bmi = NULL;
			UnuseMemory(ptrSDBP->bmi);
			ptr_bmi = NULL;
		}
		else
		{
				UnuseMemory(mf_newSDB);
				ptrSDBP = NULL;
				DisposeMemory(mf_newSDB);
				mf_newSDB = MEM_NULL;
				UnuseMemory(mf_stretchDibParams);
				ptrOriginalSDBP = NULL;
				return (MEM_NULL);		
			
		}	
			
		
		if ((ptrSDBP->ScanTable = MemoryDuplicate(ptrOriginalSDBP->ScanTable)) == MEM_NULL)
		{
			
				UnuseMemory(mf_newSDB);
				ptrSDBP = NULL;
				DisposeMemory(mf_newSDB);
				mf_newSDB = MEM_NULL;
				UnuseMemory(mf_stretchDibParams);
				ptrOriginalSDBP = NULL;
				return (MEM_NULL);		
		}
		
		UnuseMemory(mf_newSDB);
		ptrSDBP = NULL;
		UnuseMemory(mf_stretchDibParams);
		ptrOriginalSDBP = NULL;
	
		return (mf_newSDB);
	}
	else
	{
		return (MEM_NULL);
	
	}

}


// Untested code. should handle bitmap objects like brush and pen objects are handled.
short AddBitmapToBitmapTable(short* currentBitmapIndex, short* CurrentNumElements, memory_ref* mfptr_bitmap_table, memory_ref mf_stretchDibParams)
{
	long 				MemSize = 0;
	long				tmpLong = 0;
	BITMAPOBJECT*		ptr_tmpBitmapTable = NULL;
	
	
	if (*currentBitmapIndex == -1)
	{
		*mfptr_bitmap_table = MemoryAllocClear(m_globals, sizeof(BITMAPOBJECT), GDI_OBJECT_MIN_RESIZE, GDI_OBJECT_MIN_RESIZE);
		if (*mfptr_bitmap_table == MEM_NULL)
		{
			return (FAILED);
		}
		else
		{
			*currentBitmapIndex = 0;
			*CurrentNumElements = GDI_OBJECT_MIN_RESIZE;
		
		}
	}
	if ((*currentBitmapIndex) >= (*CurrentNumElements))
	{
		MemSize = GetMemorySize(*mfptr_bitmap_table);
		tmpLong = MemSize + GDI_OBJECT_MIN_RESIZE;
		SetMemorySize(*mfptr_bitmap_table, tmpLong);
		*CurrentNumElements = tmpLong;
	}
	ptr_tmpBitmapTable = (BITMAPOBJECT*)UseMemory(*mfptr_bitmap_table);
	if (ptr_tmpBitmapTable[*currentBitmapIndex].mf_stretchDib = MakeCopyStretchDib(mf_stretchDibParams) != MEM_NULL)
	{
		*currentBitmapIndex += 1;
		UnuseMemory(*mfptr_bitmap_table);
		ptr_tmpBitmapTable = NULL;
		MemSize = 0;
		tmpLong = 0;

		return(SUCCESS);
	}
	else
	{	
		UnuseMemory(*mfptr_bitmap_table);
		ptr_tmpBitmapTable = NULL;
		return(FAILED);
	}
		
	return (SUCCESS);
}

// Adds the Palette to a Pallet table, works like brush table and pen table.
short AddPaletteToPaletteTable(short* currentPaletteIndex, short* CurrentNumElements, memory_ref* mfptr_palette_table, PALOBJECT* ptr_pal)
{
	long 				MemSize = 0;
	long 				tmpLong = 0;
	PALOBJECT* 			ptr_paletteTable = NULL;

	if (*currentPaletteIndex == -1)
	{
		*mfptr_palette_table = MemoryAlloc(m_globals, sizeof(PALOBJECT), GDI_OBJECT_MIN_RESIZE, GDI_OBJECT_MIN_RESIZE);
		if (*mfptr_palette_table == MEM_NULL)
		{
			return (FAILED);
		}
		else
		{
			*currentPaletteIndex = 0;
			*CurrentNumElements = GDI_OBJECT_MIN_RESIZE;
		}
	}
	if ((*currentPaletteIndex) >= (*CurrentNumElements))
	{
		MemSize = GetMemorySize(*mfptr_palette_table);
		tmpLong = MemSize + GDI_OBJECT_MIN_RESIZE;
		SetMemorySize (*mfptr_palette_table, tmpLong);
		*CurrentNumElements = tmpLong;
	}
	
	ptr_paletteTable = (PALOBJECT*)UseMemory(*mfptr_palette_table);
	ptr_paletteTable[*currentPaletteIndex].numberEntries = ptr_pal->numberEntries;

	
	*currentPaletteIndex += 1;
	UnuseMemory(*mfptr_palette_table);
	ptr_paletteTable = NULL;
	MemSize = 0;
	tmpLong = 0;
	
	return(SUCCESS);
}			
	




//Returns 0 on success, or 1 when fails		
short AddBrushToBrushTable(short* currentBrushIndex, short* CurrentNumElements, memory_ref* mfptr_brush_table, LOGBRUSH* ptr_brush)
{
	long 				MemSize = 0;
	long 				tmpLong = 0;
	LOGBRUSH* 			ptr_tmpBrushTable = NULL;

	if (*currentBrushIndex == -1)
	{
		*mfptr_brush_table = MemoryAlloc(m_globals, sizeof(LOGBRUSH), GDI_OBJECT_MIN_RESIZE, GDI_OBJECT_MIN_RESIZE);
		if (*mfptr_brush_table == MEM_NULL)
		{
			return (FAILED);
		}
		else
		{
			*currentBrushIndex = 0;
			*CurrentNumElements = GDI_OBJECT_MIN_RESIZE;
		}
	}
	if ((*currentBrushIndex) >= (*CurrentNumElements))
	{
		MemSize = GetMemorySize(*mfptr_brush_table);
		tmpLong = MemSize + GDI_OBJECT_MIN_RESIZE;
		SetMemorySize (*mfptr_brush_table, tmpLong);
		*CurrentNumElements = tmpLong;
	}
	
	ptr_tmpBrushTable = (LOGBRUSH*)UseMemory(*mfptr_brush_table);
	ptr_tmpBrushTable[*currentBrushIndex].lbStyle = ptr_brush->lbStyle;
	ptr_tmpBrushTable[*currentBrushIndex].lbColor = ptr_brush->lbColor;
	ptr_tmpBrushTable[*currentBrushIndex].lbHatch = ptr_brush->lbHatch;
	
	*currentBrushIndex += 1;
	UnuseMemory(*mfptr_brush_table);
	ptr_tmpBrushTable = NULL;
	MemSize = 0;
	tmpLong = 0;
	
	return(SUCCESS);
}			



//Returns 0 on Success, 1 on fails
short AddObjectToGDIObjectTable(short object_type, short* CurrentObjectIndex, short* CurrentNumElements,
		memory_ref* mfptr_object_table, short specific_object_index)
{
	short 			i = -1;
	GDIOBJECT* 		ptr_tmpGDIObject = NULL;
	long 			MemSize = 0;
	long 			tmpLong =  0;
	
	if (*CurrentObjectIndex == -1)
	{
		*mfptr_object_table = MemoryAlloc(m_globals, sizeof(GDIOBJECT), GDI_OBJECT_MIN_RESIZE, GDI_OBJECT_MIN_RESIZE);
		if (*mfptr_object_table == MEM_NULL)
		{
			return(FAILED);
		}
		else
		{
			*CurrentObjectIndex = 0;
			*CurrentNumElements = GDI_OBJECT_MIN_RESIZE;
			ptr_tmpGDIObject = (GDIOBJECT*)UseMemory(*mfptr_object_table);
			for (i = 0; i < *CurrentNumElements; i++)
			{	
				ptr_tmpGDIObject[i].index_gdi_object = -1;
				ptr_tmpGDIObject[i].gdi_object_type = -1;
			}
			UnuseMemory(*mfptr_object_table);
			ptr_tmpGDIObject = NULL;
			i = -1;
		}
	}
	if ((*CurrentObjectIndex) >= (*CurrentNumElements))
	{
		MemSize = GetMemorySize(*mfptr_object_table);
		tmpLong = MemSize + GDI_OBJECT_MIN_RESIZE;
		SetMemorySize(*mfptr_object_table, tmpLong);
		ptr_tmpGDIObject = (GDIOBJECT*)UseMemory(*mfptr_object_table);
		for (i = *CurrentNumElements; i < tmpLong; i++)
		{
				ptr_tmpGDIObject[i].index_gdi_object = -1;
				ptr_tmpGDIObject[i].gdi_object_type = -1;
		}	
		UnuseMemory(*mfptr_object_table);	
		*CurrentNumElements = tmpLong;
		tmpLong = 0;
		ptr_tmpGDIObject = NULL;
		i = -1;
	}
	ptr_tmpGDIObject = (GDIOBJECT*)UseMemory(*mfptr_object_table);
	for (i= 0; i < *CurrentNumElements; i++)
	{
		if (ptr_tmpGDIObject[i].index_gdi_object == -1)
			break;
	}
	if (i == *CurrentObjectIndex)
	{
		ptr_tmpGDIObject[*CurrentObjectIndex].gdi_object_type = object_type;
		ptr_tmpGDIObject[*CurrentObjectIndex].index_gdi_object = specific_object_index;	
	
	
		*CurrentObjectIndex += 1;
	}
	else
	{
			ptr_tmpGDIObject[i].gdi_object_type = object_type;
			ptr_tmpGDIObject[i].index_gdi_object = specific_object_index;	
	
			i = -1;
	}
	UnuseMemory(*mfptr_object_table);
	ptr_tmpGDIObject = NULL;
	
	return (SUCCESS);

	

}

void DeleteGDIObject(short ObjectIndex, memory_ref* mfptr_object_table)
{
	GDIOBJECT* 			ptr_GDIObjectTable = NULL;

	ptr_GDIObjectTable = (GDIOBJECT*)UseMemory(*mfptr_object_table);
	
    ptr_GDIObjectTable[ObjectIndex].index_gdi_object = -1;
    ptr_GDIObjectTable[ObjectIndex].gdi_object_type = -1; 
    
    UnuseMemory(*mfptr_object_table);
    ptr_GDIObjectTable = NULL;
	
	return;
}


//Returns 0 on success, 1 on Failure
short SelectGDIObject(short ObjectIndex, memory_ref* mfptr_object_table, memory_ref* mfptr_selected_objects, BITMAPOBJECT* ptr_current_bitmap, LOGPEN* ptr_current_pen, 
	LOGBRUSH* ptr_current_brush, LOGFONT* ptr_current_font, memory_ref mf_bitmap_table, memory_ref mf_pen_table, memory_ref mf_brush_table, memory_ref mf_font_table)
{
	short 					object_type = -1;
	GDIOBJECT* 				ptr_tmpGDIObject = NULL;
	LOGPEN* 				ptr_tmpPenTable = NULL;
	LOGBRUSH* 				ptr_tmpBrushTable = NULL;
	LOGFONT*				ptr_tmpFontTable = NULL;
	BITMAPOBJECT*			ptr_tmpBitmapTable = NULL;
	short*					ptr_tmpSelectedObjects = NULL;
	short 					tmpIndex = -1;
	
	ptr_tmpGDIObject = (GDIOBJECT*)UseMemory(*mfptr_object_table);
	ptr_tmpSelectedObjects = (short*)UseMemory(*mfptr_selected_objects);
	
	if (mf_brush_table != MEM_NULL)
	{
		ptr_tmpBrushTable = (LOGBRUSH*)UseMemory(mf_brush_table);
	}
	
	if (mf_pen_table != MEM_NULL)
	{
		ptr_tmpPenTable = (LOGPEN*)UseMemory(mf_pen_table);
	}
	
	if (mf_font_table != MEM_NULL)
	{
		ptr_tmpFontTable = (LOGFONT*)UseMemory(mf_font_table);
	}
	
	object_type = ptr_tmpGDIObject[ObjectIndex].gdi_object_type;
	ptr_tmpSelectedObjects[object_type] = ptr_tmpGDIObject[ObjectIndex].index_gdi_object;
	tmpIndex = ptr_tmpGDIObject[ObjectIndex].index_gdi_object;
	
	switch(object_type)
	{
		case WIN_GDI_PEN:
			(*ptr_current_pen).lopnStyle = ptr_tmpPenTable[tmpIndex].lopnStyle;
			(*ptr_current_pen).lopnWidth = ptr_tmpPenTable[tmpIndex].lopnWidth;
			(*ptr_current_pen).lopnColor = ptr_tmpPenTable[tmpIndex].lopnColor;
			break;
		case WIN_GDI_BRUSH:
			(*ptr_current_brush).lbStyle = ptr_tmpBrushTable[tmpIndex].lbStyle;
			(*ptr_current_brush).lbColor = ptr_tmpBrushTable[tmpIndex].lbColor;
			(*ptr_current_brush).lbHatch = ptr_tmpBrushTable[tmpIndex].lbHatch;
			break;
		case WIN_GDI_FONT:
			(*ptr_current_font).lfHeight = ptr_tmpFontTable[tmpIndex].lfHeight;
			(*ptr_current_font).lfWidth = ptr_tmpFontTable[tmpIndex].lfWidth;
			(*ptr_current_font).lfEscapement = ptr_tmpFontTable[tmpIndex].lfEscapement;
			(*ptr_current_font).lfOrientation = ptr_tmpFontTable[tmpIndex].lfOrientation;
			(*ptr_current_font).lfWeight = ptr_tmpFontTable[tmpIndex].lfWeight;
			(*ptr_current_font).lfItalic = ptr_tmpFontTable[tmpIndex].lfItalic;
			(*ptr_current_font).lfUnderline = ptr_tmpFontTable[tmpIndex].lfUnderline;
			(*ptr_current_font).lfStrikeOut = ptr_tmpFontTable[tmpIndex].lfStrikeOut;
			(*ptr_current_font).lfCharSet = ptr_tmpFontTable[tmpIndex].lfCharSet;
			(*ptr_current_font).lfOutPrecision = ptr_tmpFontTable[tmpIndex].lfOutPrecision;
			(*ptr_current_font).lfClipPrecision = ptr_tmpFontTable[tmpIndex].lfClipPrecision;
			(*ptr_current_font).lfQuality = ptr_tmpFontTable[tmpIndex].lfQuality;
			(*ptr_current_font).lfPitchAndFamily = ptr_tmpFontTable[tmpIndex].lfPitchAndFamily;
			strcpy((*ptr_current_font).lfFaceName, ptr_tmpFontTable[tmpIndex].lfFaceName);
			break;
			
		case WIN_GDI_BITMAP:
		{
			memory_ref tmp = MEM_NULL;
			if ((tmp = MakeCopyStretchDib(ptr_tmpBitmapTable[tmpIndex].mf_stretchDib)) == MEM_NULL)
			{
				DisposeStretchDibRef(&tmp);
				break;
			}
			else
			{
				DisposeStretchDibRef(&(ptr_current_bitmap->mf_stretchDib));
				ptr_current_bitmap->mf_stretchDib = tmp;
				tmp = MEM_NULL;
			}
		}
			break;
		default:
			// unknown object type
			if (ptr_tmpBrushTable)
			{
				UnuseMemory(mf_brush_table);
				ptr_tmpBrushTable = NULL;
			}
			if (ptr_tmpPenTable)
			{
				UnuseMemory(mf_pen_table);
				ptr_tmpPenTable = NULL;
			}
			if (ptr_tmpFontTable)
			{
				UnuseMemory(mf_font_table);
				ptr_tmpFontTable = NULL;
			}
			UnuseMemory(*mfptr_selected_objects);
			ptr_tmpSelectedObjects = NULL;
			
			UnuseMemory(*mfptr_object_table);
			ptr_tmpGDIObject = NULL;
			return (FAILED);
			break;
	}
	if (mf_pen_table != MEM_NULL)
	{
		UnuseMemory(mf_pen_table);
		ptr_tmpPenTable = NULL;
	}
	
	if (mf_brush_table != MEM_NULL)
	{
		UnuseMemory(mf_brush_table);
		ptr_tmpBrushTable = NULL;
	}
	
	if (mf_font_table != MEM_NULL)
	{
		UnuseMemory (mf_font_table);
		ptr_tmpFontTable = NULL;
	}
	
	UnuseMemory(*mfptr_object_table);
	ptr_tmpGDIObject = NULL;
	UnuseMemory(*mfptr_selected_objects);
	ptr_tmpSelectedObjects = NULL;
	
	return (SUCCESS);


		
}

// returns first add index of selected_objects_table if succeeds, else returns -1;
short InitializeSelectedObjectsTable(memory_ref* selected_objects, short NumObjects)
{
	short i = 0;
	short* tmpPtr = NULL;
	
	*selected_objects = MemoryAlloc(m_globals, sizeof(short), NumObjects, GDI_OBJECT_MIN_RESIZE);
	if (*selected_objects == MEM_NULL)
	{
		return (-1);
	}
	tmpPtr = (short*)UseMemory(*selected_objects);
	
	for (i = 0; i < NumObjects; i++)
	{
		*(tmpPtr + i) = -1;
	}
	
	UnuseMemory(*selected_objects);
	tmpPtr = NULL;
	
	return (0);
}



void CalculateRectCenter (Rect* rectangle, Point* rect_center)
{
		
	rect_center->v = (short)((rectangle->bottom - rectangle->top) / 2) + rectangle->top;
	rect_center->h = (short)((rectangle->right - rectangle->left) / 2) + rectangle->left;
	
	return;
}




void GetArcPoints(pg_bits8_ptr* DataBytes, Rect* rectangle, Point WINOrigin, Point WINExt, Rect localframe)
{
	double		ConversionFactorX = 1;
	double		ConversionFactorY = 1;


		
	ConversionFactorY = ((double)localframe.bottom/(double)(abs(WINExt.v)));
	ConversionFactorX = ((double)localframe.right/ (double)(abs(WINExt.h)));
	
	
	
	rectangle->bottom = abs((short)GetWordFromByteArray(*DataBytes));
	rectangle->bottom = abs((short)(((rectangle->bottom - WINOrigin.v) * ConversionFactorY) )); 
	*DataBytes += 2;
	
	
	rectangle->right = abs((short)GetWordFromByteArray(*DataBytes));
	rectangle->right = abs((short)(((rectangle->right - WINOrigin.h) * ConversionFactorX)));
	*DataBytes +=2;
	
	
	rectangle->top = abs((short)GetWordFromByteArray(*DataBytes));
	rectangle->top = abs((short)(((rectangle->top - WINOrigin.v) * ConversionFactorY)));
	*DataBytes += 2;
	
	rectangle->left = abs((short)GetWordFromByteArray(*DataBytes));
	rectangle->left = abs((short)(((rectangle->left - WINOrigin.h) * ConversionFactorX)));
	*DataBytes += 2;
	
	
	
	return;
}

void GetArcParams(pg_bits8_ptr* DataBytes, Rect* rectangle, short* start_angle, short* stop_angle, Point WINOrigin, Point WINExt,
		 Rect localframe, WMFRECORD* metaRecord)
{
	Fixed start_slope = 0, end_slope = 0;
	Rect arc_points = {0, 0, 0, 0}; // XStartArc == arc_points.left, YStartArc == arc_points.top, XEndArc == arc_points.right, YEndArc == arc_points.bottom
	short angle_travelled = 0;
	Point start_point = {0, 0};
	Point end_point = {0, 0};
	short data_size_in_bytes = ((*metaRecord).Size - 3) * 2;
	pg_bits8_ptr	target_data_pos = *DataBytes + data_size_in_bytes;
	
	double		ConversionFactorX = 1;
	double		ConversionFactorY = 1;


	GetArcPoints(DataBytes, &arc_points, WINOrigin, WINExt, localframe);	


	ConversionFactorY = ((double)localframe.bottom/(double)(abs(WINExt.v)));
	ConversionFactorX = ((double)localframe.right/ (double)(abs(WINExt.h)));
	
	

	
	rectangle->bottom = (short)GetWordFromByteArray(*DataBytes);
	rectangle->bottom = (short)(((rectangle->bottom - WINOrigin.v) * ConversionFactorY) ); 
	*DataBytes += 2;
	
	rectangle->right = (short)GetWordFromByteArray(*DataBytes);
	rectangle->right = (short)(((rectangle->right - WINOrigin.h) * ConversionFactorX));
	*DataBytes +=2;
	
	
	
	rectangle->top = (short)GetWordFromByteArray(*DataBytes);
	rectangle->top = (short)(((rectangle->top - WINOrigin.v) * ConversionFactorY));
	*DataBytes += 2;
	
	rectangle->left = (short)GetWordFromByteArray(*DataBytes);
	rectangle->left = (short)(((rectangle->left - WINOrigin.h) * ConversionFactorX));
	*DataBytes += 2;
	
	
	if (rectangle->bottom < rectangle->top)
	{
		short tmp = -1;
		
		tmp = rectangle->bottom;
		rectangle->bottom = rectangle->top;
		rectangle->top = (short)tmp;
	}
	
	if (rectangle->right < rectangle->left)
	{
		short tmp = -1;
		
		tmp = rectangle->right;
		rectangle->right = rectangle->left;
		rectangle->left = (short)tmp;
	}
	
	start_point.h =(short)arc_points.left;
	start_point.v = (short)arc_points.top;
	
	end_point.h =(short)arc_points.right;
	end_point.v = (short)arc_points.bottom;
	
	
	PtToAngle (rectangle, start_point, start_angle);
	PtToAngle (rectangle, end_point, stop_angle);
	
	if (*start_angle >= *stop_angle)
	{
		angle_travelled = abs(*start_angle - *stop_angle);
		angle_travelled = -angle_travelled;
		*stop_angle = (short)angle_travelled;
	}
	else 
	{
		angle_travelled = (360 - *stop_angle) + *start_angle;
		*stop_angle =(short) -angle_travelled;
	}
		
	*start_angle -= 360;
	*DataBytes = target_data_pos;
	return;
}


void GetWedgeParams(pg_bits8_ptr* DataBytes, Rect* rectangle, short* start_angle, short* stop_angle, Point* start_point, 
	Point* stop_point, Point* rect_center, Point WINOrigin, Point WINExt, Rect localframe, WMFRECORD* metaRecord)
{
	short angle_travelled = 0;
	Rect arc_points = {0, 0, 0, 0}; // arc_points.left == nXRadial1, arc_points.top == nYRadial1, ...
										//   ... arc_points.right == nXRadial2, arc_points.bottom == nYRadial2.	
	long	data_size_in_bytes = ((*metaRecord).Size - 3) * 2;
	pg_bits8_ptr target_data_pos = *DataBytes + data_size_in_bytes;
	double		ConversionFactorX = 1;
	double		ConversionFactorY = 1;
	

	GetArcPoints(DataBytes, &arc_points, WINOrigin, WINExt, localframe);	

	ConversionFactorY = ((double)localframe.bottom/(double)(abs(WINExt.v)));
	ConversionFactorX = ((double)localframe.right/ (double)(abs(WINExt.h)));
	
	

	
	rectangle->bottom = (short)GetWordFromByteArray(*DataBytes);
	rectangle->bottom = (short)(((rectangle->bottom - WINOrigin.v) * ConversionFactorY) ); 
	*DataBytes += 2;
	
	rectangle->right = (short)GetWordFromByteArray(*DataBytes);
	rectangle->right = (short)(((rectangle->right - WINOrigin.h) * ConversionFactorX));
	*DataBytes +=2;
	
	
	
	rectangle->top = (short)GetWordFromByteArray(*DataBytes);
	rectangle->top = (short)(((rectangle->top - WINOrigin.v) * ConversionFactorY));
	*DataBytes += 2;
	
	rectangle->left = (short)GetWordFromByteArray(*DataBytes);
	rectangle->left = (short)(((rectangle->left - WINOrigin.h) * ConversionFactorX));
	*DataBytes += 2;
	
	
	if (rectangle->bottom < rectangle->top)
	{
		short tmp = -1;
		
		tmp = rectangle->bottom;
		rectangle->bottom = rectangle->top;
		rectangle->top = (short)tmp;
	}
	
	if (rectangle->right < rectangle->left)
	{
		short tmp = -1;
		
		tmp = rectangle->right;
		rectangle->right = rectangle->left;
		rectangle->left = (short)tmp;
	}

	start_point->h =(short)arc_points.left;
	start_point->v = (short)arc_points.top;
	
	stop_point->h =(short)arc_points.right;
	stop_point->v = (short)arc_points.bottom;
	
	CalculateRectCenter(rectangle, rect_center);
	
	PtToAngle (rectangle, *start_point, start_angle);
	PtToAngle (rectangle, *stop_point, stop_angle);
	
	if (*start_angle >= *stop_angle)
	{
		angle_travelled = abs(*start_angle - *stop_angle);
		angle_travelled = -angle_travelled;
		*stop_angle = (short)angle_travelled;
	}
	else 
	{
		angle_travelled = (360 - *stop_angle) + *start_angle;
		*stop_angle =(short) -angle_travelled;
	}
		
	*start_angle -= 360;
	*DataBytes = target_data_pos;
	
	return;
}


void FrameWedge (const Rect* r, short start_angle, short arc_angle, Point* start_point, Point* stop_point, Point* rect_center)
{
	FrameArc(r, start_angle, arc_angle);
	MoveTo(rect_center->h, rect_center->v);
	LineTo(start_point->h, start_point->v);
	MoveTo(rect_center->h, rect_center->v);
	LineTo(stop_point->h, stop_point->v);
	

	return;
}


void DrawPie(LOGBRUSH* current_brush, LOGPEN* current_pen, Rect* rectangle, short start_angle, short stop_angle, Point* start_point,
				 Point* stop_point, Point* rect_center, short BKMode, RGBColor* rgbBackColor/*, PatPtr ptr_ppat*/)
{
	Pattern					pen_pattern = {0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000};
	RGBColor 				black = {0x0000, 0x0000, 0x0000};
	PatPtr					ptr_ppat = NULL;
	if ((BKMode == OPAQUE) && (((*current_brush).lbStyle == BS_HATCHED) || ((*current_pen).lopnStyle != PS_SOLID)))
	{
		RGBForeColor(rgbBackColor);
		PaintArc(rectangle, start_angle, stop_angle);
	}

	RGBForeColor(&((*current_brush).lbColor));
	RGBBackColor(rgbBackColor);			
	if ((*current_brush).lbStyle == BS_HOLLOW)
	{
		FrameWedge(rectangle, start_angle, stop_angle, start_point, stop_point, rect_center);
	}
	else
	{
		if ((*current_brush).lbStyle == BS_HATCHED)
		{
			GetIndPattern(&pen_pattern, 128, (*current_brush).lbHatch + 1);
			FillArc(rectangle,start_angle, stop_angle, &pen_pattern);

		}
		else if ((*current_brush).lbStyle == BS_PATTERN)
		{
				
				ptr_ppat = (PatPtr)UseMemory(current_brush->lbHatch);
				FillArc(rectangle,start_angle, stop_angle, ptr_ppat);
				UnuseMemory(current_brush->lbHatch);
				ptr_ppat = NULL;
		}
		else if ((*current_brush).lbStyle == BS_SOLID)
		{
			
			PaintArc(rectangle, start_angle, stop_angle);

		}
	}
				
				
	if ((*current_pen).lopnStyle != PS_NULL)
	{
		RGBForeColor(&((*current_pen).lopnColor));
		PenSize(((*current_pen).lopnWidth.h), ((*current_pen).lopnWidth.v));
		FrameWedge(rectangle, start_angle, stop_angle, start_point, stop_point, rect_center);
	}
	PenSize(1,1);
	return;
}


void DrawRectangle(LOGBRUSH* current_brush, LOGPEN* current_pen, Rect* rectangle, RGBColor* rgbBackColor, short BKMode)
{
	
	Pattern					pen_pattern = {0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000};
	RGBColor 				black = {0x0000, 0x0000, 0x0000};
	PatPtr					ptr_ppat = NULL;
	
	
	if ((*current_brush).lbStyle != BS_HOLLOW)
	{
		if ((BKMode == OPAQUE) && ((((*current_brush).lbStyle == BS_HATCHED) || ((*current_brush).lbStyle == BS_PATTERN))) || ((*current_pen).lopnStyle != PS_SOLID))
		{
			RGBForeColor(rgbBackColor);
			PaintRect(rectangle);
		}
	}

	RGBForeColor(&((*current_brush).lbColor));
	RGBBackColor(rgbBackColor);
	if ((*current_brush).lbStyle == BS_HOLLOW)
	{
		FrameRect(rectangle);
	}
	else
	{
		if ((*current_brush).lbStyle == BS_HATCHED)
		{
			GetIndPattern(&pen_pattern, 128, (*current_brush).lbHatch + 1);
			FillRect(rectangle, &pen_pattern);
		}
		else if ((*current_brush).lbStyle == BS_PATTERN)
		{
			ptr_ppat= (PatPtr)UseMemory(current_brush->lbHatch);
			FillRect(rectangle, ptr_ppat);			
			UnuseMemory(current_brush->lbHatch);
			ptr_ppat = NULL;
			
		}
		else if ((*current_brush).lbStyle == BS_SOLID)
		{
			
	 		PaintRect(rectangle);
		}
	 }
	
	if ((*current_pen).lopnStyle != PS_NULL)
	{
		RGBForeColor(&((*current_pen).lopnColor));
		PenSize(((*current_pen).lopnWidth.h), ((*current_pen).lopnWidth.v));
		FrameRect(rectangle);
	}
	PenSize(1,1);

	return;
	
}


void GetRoundRectParams(pg_bits8_ptr* DataBytes, Rect* rectangle, short* ovalWidth, short* ovalHeight, Point* WINOrigin,
	 Point* WINExt, Rect* localframe, WMFRECORD* metaRecord)
{
	Point tmpPoint = {0, 0};
	Rect srcRect = {0, 0, 0, 0};
	short   data_size_in_bytes = ((*metaRecord).Size - 3) * 2;
	pg_bits8_ptr target_data_pos = *DataBytes + data_size_in_bytes;
			
	srcRect.bottom = abs((*WINExt).v);
	srcRect.right = abs((*WINExt).h);
	*ovalHeight = (short)GetWordFromByteArray(*DataBytes);
	*DataBytes += 2;
	*ovalWidth = (short)GetWordFromByteArray(*DataBytes);
	*DataBytes += 2;
					
	// scale oval dimensions
	tmpPoint.h = *ovalWidth;
	tmpPoint.v = *ovalHeight;
	ScalePt(&tmpPoint, &srcRect, localframe);
	
	GetRect(DataBytes, rectangle, *WINOrigin, *WINExt, *localframe, NULL);
	*DataBytes = target_data_pos;
	

	return;
	
}


void DrawRoundRect (LOGBRUSH* current_brush, LOGPEN* current_pen, Rect* rectangle, short ovalWidth, short ovalHeight, 
		RGBColor* rgbBackColor, short BKMode)
{
	Pattern					pen_pattern = {0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000};
	RGBColor 				black = {0x0000, 0x0000, 0x0000};
	PatPtr 					ptr_ppat = NULL;
					
	if ((BKMode == OPAQUE) && ((((*current_brush).lbStyle == BS_HATCHED) || ((*current_brush).lbStyle == BS_PATTERN)) || ((*current_pen).lopnStyle != PS_SOLID)))
	{
		RGBForeColor(rgbBackColor);
		PaintRoundRect(rectangle, ovalWidth, ovalHeight);
	}

	RGBForeColor(&((*current_brush).lbColor));
	RGBBackColor(rgbBackColor);
	if ((*current_brush).lbStyle == BS_HOLLOW)
	{
		FrameRoundRect(rectangle, ovalWidth, ovalHeight);
	}
	else
	{
		if ((*current_brush).lbStyle == BS_HATCHED)
		{
			GetIndPattern(&pen_pattern, 128, (*current_brush).lbHatch + 1);
			FillRoundRect(rectangle, ovalWidth, ovalHeight, &pen_pattern);
		}
		else if ((*current_brush).lbStyle == BS_PATTERN)
		{
			ptr_ppat = (PatPtr)UseMemory(current_brush->lbHatch);
			FillRoundRect(rectangle, ovalWidth, ovalHeight, ptr_ppat);
			UnuseMemory(current_brush->lbHatch);
			ptr_ppat = NULL;
		}
		else if ((*current_brush).lbStyle == BS_SOLID)
		{
			PaintRoundRect(rectangle, ovalWidth, ovalHeight);
		}
 	}

	if ((*current_pen).lopnStyle != PS_NULL)
	{
		RGBForeColor(&((*current_pen).lopnColor));
		PenSize(((*current_pen).lopnWidth.h), ((*current_pen).lopnWidth.v));
		FrameRoundRect(rectangle, ovalWidth, ovalHeight);
	}
	PenSize(1,1);

	return;
}

void DrawEllipse (LOGBRUSH* current_brush, LOGPEN* current_pen, Rect* rectangle, RGBColor* rgbBackColor, short BKMode)
{
	Pattern					pen_pattern = {0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000};
	RGBColor 				black = {0x0000, 0x0000, 0x0000};
	PatPtr 					ptr_ppat = NULL;
	
	if ((BKMode == OPAQUE) && ((((*current_brush).lbStyle == BS_HATCHED) || ((*current_brush).lbStyle == BS_PATTERN)) || ((*current_pen).lopnStyle != PS_SOLID)))
	{
		RGBForeColor(rgbBackColor);
		PaintOval(rectangle);
	}

	RGBForeColor(&((*current_brush).lbColor));
	RGBBackColor(rgbBackColor);
	if ((*current_brush).lbStyle == BS_HOLLOW)
	{
		FrameOval(rectangle);
	}
	else
	{
		if ((*current_brush).lbStyle == BS_HATCHED)
		{
			GetIndPattern(&pen_pattern, 128, (*current_brush).lbHatch + 1);
			FillOval(rectangle, &pen_pattern);
		}
		else if ((*current_brush).lbStyle == BS_PATTERN)
		{
			ptr_ppat = (PatPtr)UseMemory(current_brush->lbHatch);	
			FillOval(rectangle, ptr_ppat);
			UnuseMemory(current_brush->lbHatch);
			ptr_ppat = NULL;
		}
		else if ((*current_brush).lbStyle == BS_SOLID)
		{
			PaintOval(rectangle);
		}
	 }


	if ((*current_pen).lopnStyle != PS_NULL)
	{
		RGBForeColor(&((*current_pen).lopnColor));
		PenSize(((*current_pen).lopnWidth.h), ((*current_pen).lopnWidth.v));
		FrameOval(rectangle);
	}
	PenSize(1,1);

	return;
}


void DrawPolygon(LOGBRUSH* current_brush, LOGPEN* current_pen, memory_ref mf_PolyPoints, long NumPoints, 
		short BKMode, RGBColor* rgbBackColor)
{
	Pattern					pen_pattern = {0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000};
	PatPtr 					ptr_ppat = NULL;
	RGBColor 				black = {0x0000, 0x0000, 0x0000};
	Point* 					ptr_Points = NULL;
	PolyHandle				polyH = NULL;
	short						i = 0;
	
	ptr_Points = (Point*)UseMemory(mf_PolyPoints);
					
	polyH = OpenPoly();
	MoveTo(ptr_Points[0].h, ptr_Points[0].v);
	for (i = 1; i < NumPoints; i++)
	{
		LineTo(ptr_Points[i].h, ptr_Points[i].v);
	}
	if ((ptr_Points[NumPoints - 1].h != ptr_Points[0].h) || (ptr_Points[NumPoints - 1].v != ptr_Points[0].v))
	{
		LineTo(ptr_Points[0].h,ptr_Points[0].v);
	}
	ClosePoly();
	UnuseMemory(mf_PolyPoints);
	ptr_Points = NULL;
	
	if ((BKMode == OPAQUE) && ((((*current_brush).lbStyle == BS_HATCHED) || ((*current_brush).lbStyle == BS_PATTERN)) || ((*current_pen).lopnStyle != PS_SOLID)))
	{
		RGBForeColor(rgbBackColor);
		PaintPoly(polyH);
	}

	RGBForeColor(&((*current_brush).lbColor));
	RGBBackColor(rgbBackColor);
	if ((*current_brush).lbStyle == BS_HOLLOW)
	{
		FramePoly(polyH);
	}
	else
	{
		if ((*current_brush).lbStyle == BS_HATCHED)
		{
			GetIndPattern(&pen_pattern, 128, (*current_brush).lbHatch + 1);
			FillPoly(polyH, &pen_pattern);

		}
		else if ((*current_brush).lbStyle == BS_PATTERN)
		{
			ptr_ppat = (PatPtr)UseMemory(current_brush->lbHatch);
			FillPoly(polyH, ptr_ppat);
			UnuseMemory(current_brush->lbHatch);
			ptr_ppat = NULL;
			
		}
		else if ((*current_brush).lbStyle == BS_SOLID)
		{
			PaintPoly(polyH);
		}
	 }

	if ((*current_pen).lopnStyle != PS_NULL)
	{
		RGBForeColor(&((*current_pen).lopnColor));
		PenSize(((*current_pen).lopnWidth.h), ((*current_pen).lopnWidth.v));
		FramePoly(polyH);
	}
	PenSize(1,1);
				
	KillPoly(polyH);	

	return;
	
}

void myDrawText(char* ptr_string_to_display, short* ptr_character_spacing,int firstByteLoc, int ByteCount, Point* WINExt, Rect* localframe)
{
	register int i = 0;
	PenState oldPenState;
	short newXposition = 0;
	double conversionFactorX = 1;
	
	
	DrawText(ptr_string_to_display, 0, ByteCount);

	return;
} 

int ExtTextOut (pg_bits8_ptr* DataBytes, WMFRECORD* ptr_metaRecord, int current_mapmode, Point* ptr_WINOrigin, Point* ptr_WINExt, Rect* ptr_localframe,
					LOGFONT* ptr_current_font, GrafPtr currentPort, short text_align)
{
	int				nYStart =  0;
	int 			nXStart = 0;
	WORD 			cbString = 	0;
	WORD			fuOptions =  0;
	Rect			text_rect = {0,0,0,0};
	memory_ref		mf_string_to_display = MEM_NULL;
	memory_ref		mf_character_spacing = MEM_NULL;
	char*			ptr_string_to_display = NULL;
	short*			ptr_character_spacing = NULL;
	long 			data_size_in_bytes = ((*ptr_metaRecord).Size - 3) * 2;
	pg_bits8_ptr 	target_data_pos = *DataBytes + data_size_in_bytes;
	double			ConversionFactorX = 1;
	double 			ConversionFactorY = 1;
	font_info		info;
	
	pgFillBlock(&info, sizeof(font_info),0);
	
	nYStart = (int)GetWordFromByteArray(*DataBytes);
	*DataBytes += 2;
	nXStart = (int)GetWordFromByteArray(*DataBytes);
	*DataBytes += 2;
	cbString = GetWordFromByteArray(*DataBytes);
	*DataBytes += 2;
	fuOptions = GetWordFromByteArray(*DataBytes);
	*DataBytes += 2;
	
	GetRect(DataBytes, &text_rect, *ptr_WINOrigin, *ptr_WINExt, *ptr_localframe, NULL);
	ConversionFactorX = ((double)(*ptr_localframe).right/ (double)(abs((*ptr_WINExt).h)));
	ConversionFactorY = ((double)(*ptr_localframe).bottom/(double)(abs((*ptr_WINExt).v)));
	
	nYStart = (int)((double)(nYStart - ptr_WINOrigin->v) * ConversionFactorY);
	nXStart = (int)((double)(nXStart - ptr_WINOrigin->h)* ConversionFactorX);
	
	if (cbString != 0)
	{
		mf_string_to_display = MemoryAlloc(m_globals, sizeof(char), cbString + 1, 0);
		if (mf_string_to_display == MEM_NULL)
			return (FAILED);
		ptr_string_to_display = (char*)UseMemory(mf_string_to_display);
		pgBlockMove(*DataBytes, ptr_string_to_display, cbString + 1);
		*DataBytes += cbString + 1;
		
		if (*DataBytes < target_data_pos  - 4)
		{
			mf_character_spacing = MemoryAlloc(m_globals, sizeof(short), cbString, 0);
			if (mf_character_spacing == MEM_NULL)
			{
				if (mf_string_to_display != MEM_NULL)
				{
					if (ptr_string_to_display != NULL)
					{
						UnuseMemory(mf_string_to_display);
						ptr_string_to_display = NULL;
					}
					DisposeMemory(mf_string_to_display);
					mf_string_to_display = MEM_NULL;
				}
				return (FAILED);
			}
			ptr_character_spacing = (short*)UseMemory(mf_character_spacing);
			{
				register int i = 0;
				for (i = 0; i < cbString; i++, *DataBytes += 2)
				{
					ptr_character_spacing[i] = (short)GetWordFromByteArray(*DataBytes);
				}
			}
			
		}
	
	}

	{
		StringPtr new_face_name = NULL;
		Str255 default_face_name = DEFAULT_FONT_NAME;
		StringPtr ptr_default_face_name = c2pstr((char*)&default_face_name);
		if (ptr_current_font->lfFaceName[0] != 0)
			new_face_name = c2pstr(&(*ptr_current_font).lfFaceName[0]);
		else
			new_face_name = ptr_default_face_name;
		strcpy((char*)info.name, (char*)new_face_name);
		info.family_id = ptr_current_font->lfPitchAndFamily;
		info.machine_var[PG_OUT_PRECISION] = ptr_current_font->lfOutPrecision;
		info.machine_var[PG_CLIP_PRECISION] = ptr_current_font->lfClipPrecision;
		info.machine_var[PG_QUALITY] = ptr_current_font->lfQuality;
		info.machine_var[PG_CHARSET] = ptr_current_font->lfCharSet;
	
	}
	
	//Determine if font exists on system, if not find closest match
	if (pgIsRealFont(&paige_rsrv, &info, FALSE))
	{
		short fontnum = 0;
		
		GetFNum((StringPtr)info.name, &fontnum);
		TextFont (fontnum);
	}
	else
	{
		int byte_count = 0;
		short fontnum = 0;
		pg_char_ptr table_ptr = (pg_char_ptr) &cross_font_table;
		byte_count = (int)pgCompareFontTable((pg_char_ptr)&((*ptr_current_font).lfFaceName[0]),table_ptr);
		if (byte_count == 0)
			return (FAILED);
		else
		{
			int tmpIndex = 0;
			pg_char new_font_name[64];
			pgFillBlock(&(new_font_name[0]), sizeof(pg_char) * 64, 0);
			
			while (table_ptr[byte_count] != (pg_char) NULL)
			{
				new_font_name[tmpIndex] = table_ptr[byte_count];
				tmpIndex++;
				byte_count++;
			}
			new_font_name[tmpIndex] = table_ptr[byte_count];
			GetFNum((StringPtr)new_font_name, &fontnum);
			TextFont(fontnum);
				
		}
	}
	
	//set Text Style
	TextFace(0);
	if (ptr_current_font->lfWeight == FW_BOLD)
	{
		TextFace((*currentPort).txFace + bold);
	}
	
	if (ptr_current_font->lfItalic)
	{
		TextFace((*currentPort).txFace + italic);
		
	}

	if (ptr_current_font->lfUnderline)
	{
		TextFace((*currentPort).txFace + underline);
	}
	
	
	{
	
		FontInfo  			info;
		int		  			tx_pixel_height = 0;
		PenState  			oldPenState;
		
		
		tx_pixel_height = (int) ((abs(ptr_current_font->lfHeight) * ConversionFactorX) + 0.5);
		TextSize (tx_pixel_height);

		GetFontInfo(&info);
		if (ptr_current_font->lfUnderline)
		{
			tx_pixel_height += 2;
		}
		GetPenState(&oldPenState);
		
		if (text_align == TA_TOP)
			MoveTo(nXStart, nYStart + tx_pixel_height );
		else if ((text_align && TA_BASELINE) || (text_align && TA_BOTTOM))
			MoveTo(nXStart, nYStart);
			
		myDrawText(ptr_string_to_display, ptr_character_spacing, 0, cbString, ptr_WINExt, ptr_localframe);
		SetPenState(&oldPenState);
		
	}
	*DataBytes = target_data_pos;
	
	
	UnuseMemory(mf_string_to_display);
	ptr_string_to_display = NULL;
	DisposeMemory(mf_string_to_display);
	mf_string_to_display = MEM_NULL;
	if (ptr_character_spacing != NULL)
	{
		UnuseMemory(mf_character_spacing);
		ptr_character_spacing = NULL;
		DisposeMemory(mf_character_spacing);
		mf_character_spacing = MEM_NULL;
	}
	
	return (SUCCESS);
}


void GetPatBltParams(pg_bits8_ptr* DataBytes, Point* ptr_nLeftTopRect, short* ptr_width, short* ptr_height, DWORD* ptr_fdwRop, WMFRECORD* ptr_metarecord)
{
	pg_bits8_ptr target_data_pos = NULL;
	int size_data_in_bytes = ((*ptr_metarecord).Size - 3) * 2;
	target_data_pos = *DataBytes + size_data_in_bytes;
	
	*ptr_fdwRop = GetDWordFromByteArray(*DataBytes);
	*DataBytes += 4;
	
	*ptr_height = (short)GetWordFromByteArray(*DataBytes);
	*DataBytes += 2;
	
	*ptr_width = (short)GetWordFromByteArray(*DataBytes);
	*DataBytes += 2;
	
	(*ptr_nLeftTopRect).v = (short)GetWordFromByteArray(*DataBytes);
	*DataBytes += 2;
	
	(*ptr_nLeftTopRect).h = (short)GetWordFromByteArray(*DataBytes);
	*DataBytes += 2;
	
	*DataBytes = target_data_pos;
	

	return;
}

void ScalePatBltParams (Point* nTopLeftRect, short* width, short* height, Point* WINOrigin, Point* WINExt, Rect* localframe)
{
	
	double		ConversionFactorX = 1;
	double		ConversionFactorY = 1;
	int			data_size_in_bytes =  0;
	
	ConversionFactorY = ((double)(*localframe).bottom/(double)(abs((*WINExt).v)));
	ConversionFactorX = ((double)(*localframe).right/ (double)(abs((*WINExt).h)));

	if ((*nTopLeftRect).v != 0)
		(*nTopLeftRect).v = abs((short)(((*nTopLeftRect).v - (*WINOrigin).v) * ConversionFactorY) + 0.5); 
	
	if ((*nTopLeftRect).h != 0)
		(*nTopLeftRect).h = abs((short)(((*nTopLeftRect).h - (*WINOrigin).v) * ConversionFactorX) + 0.5); 
	
	*width = abs((short)((*width) * ConversionFactorX) + 0.5 ); 
	if (*width < 1)
		*width = 1;
	
	*height = abs((short)((*height) * ConversionFactorY) + 0.5); 
	if (*height < 1) 
		*height = 1;
	
	

	return;
}

int MyPatBlt(Point* nTopLeftRect, short* width, short* height, DWORD* fdwRop, LOGBRUSH* current_brush, LOGPEN* current_pen)
{
	Rect tmpRect = {0,0,0,0};
	int bBlack = FALSE;
	int bWhite = FALSE;
	
	tmpRect.top = (*nTopLeftRect).v;
	tmpRect.left = (*nTopLeftRect).h;
	tmpRect.bottom = tmpRect.top + *height;
	tmpRect.right = tmpRect.left + *width;
	
	switch (*fdwRop)
	{
		case PATCOPY:
			PenMode(patCopy);
			break;
		case PATPAINT:
			PenMode(patCopy);
			break;
		case PATINVERT:
			PenMode(notPatCopy);
			break;
		case DSTINVERT:
			PenMode(patXor);
			break;
		case BLACKNESS:
			bBlack = TRUE;
			break;
		case WHITENESS:
			bWhite = TRUE;
			break;
		default:
			return (FAILED);
			break;
	} // end switch
	
	if ((*current_brush).lbStyle < 2)
	{
		if (bBlack)
		{
			RGBColor black = {0,0,0};
			RGBForeColor(&black);
		}
		else if (bWhite)
		{
			RGBColor white = {0xFFFF, 0xFFFF, 0xFFFF};
			RGBForeColor(&white);
		}
		else
		{	
			RGBForeColor(&((*current_brush).lbColor));
		}
		
		PaintRect(&tmpRect);
		
	}
	else 
	{
		return (FAILED);
	}
	
	if ((*current_pen).lopnStyle != 5)
	{
		if (bBlack)
		{
			RGBColor black = {0,0,0};
			RGBForeColor(&black);
		}
		else if (bWhite)
		{
			RGBColor white = {0xFFFF, 0xFFFF, 0xFFFF};
			RGBForeColor(&white);
		}
		else
		{	
			RGBForeColor(&((*current_pen).lopnColor));
		}
		FrameRect(&tmpRect);
	}

	return(SUCCESS);	
}


// create a brush object with the memory_ref to a pattern in the lbHatch member of LOGBRUSH. The lbStyle member is set to
// BS_PATTERN. Returns 1 if fails, 0 if succeeds.
short CreateMacPattern(DWORD fnColorSpec, memory_ref patternInfo, memory_ref scanBits, memory_ref* mfptr_mac_pattern)
{
	pg_bits8_ptr ptr_originalscan = NULL;
	BITMAPINFO* ptr_pattern_data = NULL;
	RGBQUAD* ptr_colors = NULL;

	if ((patternInfo == MEM_NULL) || (scanBits == MEM_NULL))
	{
		return (FAILED);
	}


	ptr_pattern_data = (BITMAPINFO*)UseMemory(patternInfo);
	ptr_originalscan = (pg_bits8_ptr)UseMemory(scanBits);
	ptr_colors = (RGBQUAD*)UseMemory(ptr_pattern_data->bmiColors);
						
	if (fnColorSpec == DIB_PATTERN)
	{
		pg_bits8_ptr ptr_ppat = NULL;
		register short i = 0, j = 0;
		short widthbytes = 0;
		short Height = ptr_pattern_data->bmiHeader.Height;
		short WidthPixels = ptr_pattern_data->bmiHeader.Width;
		double bytesPerPixel = (double)ptr_pattern_data->bmiHeader.BitsPerPixel / (double)8;
		widthbytes = (short)((double)WidthPixels * bytesPerPixel);
		while (widthbytes % WIN_SCAN_LINE_PADDING)
		{
			widthbytes++;
		}
			
		*mfptr_mac_pattern = MemoryAllocClear(m_globals, sizeof(Pattern), 1, 0);
		if (*mfptr_mac_pattern == MEM_NULL)
		{
			if (ptr_pattern_data != NULL)
			{
				if (ptr_colors != NULL)
				{
					UnuseMemory(ptr_pattern_data->bmiColors);
					ptr_colors = NULL;
				}
				UnuseMemory(patternInfo);
				ptr_pattern_data = NULL;
			}
			if (ptr_originalscan != NULL)
			{
				UnuseMemory(scanBits);
				ptr_originalscan = NULL;
			}
			return (FAILED);
			
		}
		else
		{
			ptr_ppat = (pg_bits8_ptr)UseMemory(*mfptr_mac_pattern);
		
			for (i = 0, j = 0; j < Height, i < widthbytes * Height; i += widthbytes, j++)
			{
				ptr_ppat[j] = ~(ptr_originalscan[i]);
			}
			UnuseMemory(*mfptr_mac_pattern);
			ptr_ppat = NULL;
		
		}

												
				
	}
	else
	{
		if (ptr_pattern_data != NULL)
		{
			if (ptr_colors != NULL)
			{
				UnuseMemory(ptr_pattern_data->bmiColors);
				ptr_colors = NULL;
			}
			UnuseMemory(patternInfo);
			ptr_pattern_data = NULL;
		}
		if (ptr_originalscan != NULL)
		{
			UnuseMemory(scanBits);
			ptr_originalscan = NULL;
		}
		return (FAILED);
	}
	
	if (ptr_pattern_data != NULL)
	{
		if (ptr_colors != NULL)
		{
			UnuseMemory(ptr_pattern_data->bmiColors);
			ptr_colors = NULL;
		}
		UnuseMemory(patternInfo);
		ptr_pattern_data = NULL;
	}
	if (ptr_originalscan != NULL)
	{
		UnuseMemory(scanBits);
		ptr_originalscan = NULL;
	}
		
	return(SUCCESS);
}


// BLTs windows DIB to currentPort. Returns 1 if fails, 0 if succeeds.
short MacStretchDib(memory_ref* mfptr_stretchDibParams, Rect frame, Rect* rectangle, GrafPtr currentPort, Point* WINOrigin, Point* WINExt)
{

	long				i = 0, j = 0;
	short				bits_per_pixel = 0;
	Rect				BmpLocalFrame = {0,0,0,0};
	Rect				BmpDestRect = {0,0,0,0};
	long				width_bytes = 0;
	GrafPtr 			yourGWorld = NULL;
	GWorldPtr			gworld = NULL;
	RgnHandle			mask_rgn;
	PixMapHandle		pixmap;
	pg_bits8_ptr		translatedBitmapBits;
	short				bytesPerScanline;
	long				SizeDibBitmap = 0;
	long				SizeGWorldSpace = 0;
	long				WidthDib = 0;
	long				HeightDib = 0;
	ColorTable**		new_pix_color_table = NULL;
	STRETCHDIBPARAMS* 	ptr_StretchDibParams = NULL;
	BITMAPINFO*	 		ptr_BitmapInfo = NULL;
	pg_bits8_ptr 		ptr_ScanTable = NULL;
	double				ConversionFactorX = 1;
	double				ConversionFactorY = 1;
	
	if (*mfptr_stretchDibParams == MEM_NULL)
		return (FAILED);
						
	ptr_StretchDibParams = (STRETCHDIBPARAMS*)UseMemory(*mfptr_stretchDibParams);
	ptr_BitmapInfo = (BITMAPINFO*)UseMemory(ptr_StretchDibParams->bmi);
	ptr_ScanTable = (BYTE*)UseMemory(ptr_StretchDibParams->ScanTable);


	ConversionFactorY = (double)((double)frame.bottom / (double)(abs(WINExt->v)));
	ConversionFactorX = (double)((double)frame.right / (double)(abs(WINExt->h)));

						
	BmpLocalFrame.top = 0;
	BmpLocalFrame.left = 0;
	BmpLocalFrame.bottom = (short)ptr_BitmapInfo->bmiHeader.Height;
	BmpLocalFrame.right = (short)ptr_BitmapInfo->bmiHeader.Width;
						
		
	BmpDestRect.top = abs((short)(((ptr_StretchDibParams->YDest - WINOrigin->v) * ConversionFactorY)) + 0.5);
	BmpDestRect.left = abs((short)(((ptr_StretchDibParams->XDest - WINOrigin->h) * ConversionFactorX)) + 0.5);
	{
		short DestWidth = abs((short)(((ptr_StretchDibParams->nDestWidth * ConversionFactorX)) + 0.5));
		short DestHeight =abs((short)((ptr_StretchDibParams->nDestHeight * ConversionFactorY)) + 0.5);
			
		BmpDestRect.bottom = BmpDestRect.top + DestHeight;
		BmpDestRect.right =BmpDestRect.left + DestWidth;
	}
	
	width_bytes = GetWidthInBytes((short)ptr_BitmapInfo->bmiHeader.Width, (short)ptr_BitmapInfo->bmiHeader.BitsPerPixel);
	if (ptr_BitmapInfo->bmiHeader.BitsPerPixel == 24)
	{
		bits_per_pixel = 32;
	}
	else
	{
		bits_per_pixel = (short)ptr_BitmapInfo->bmiHeader.BitsPerPixel;
	}
			
	if ((NewGWorld(&gworld, bits_per_pixel, &BmpLocalFrame, NULL, NULL, 0)) != 0)
	{
		UnuseMemory(ptr_StretchDibParams->ScanTable);
		ptr_ScanTable = NULL;
		DisposeMemory(ptr_StretchDibParams->ScanTable);
		ptr_StretchDibParams->ScanTable = MEM_NULL;
	
 		if (ptr_BitmapInfo->bmiColors != MEM_NULL)
		{
			DisposeMemory(ptr_BitmapInfo->bmiColors);
			ptr_BitmapInfo->bmiColors = MEM_NULL;
			
		}
		UnuseMemory(ptr_StretchDibParams->bmi);
		ptr_BitmapInfo = NULL;
		DisposeMemory(ptr_StretchDibParams->bmi);
		ptr_StretchDibParams->bmi = MEM_NULL;
			
		UnuseMemory(*mfptr_stretchDibParams);
		ptr_StretchDibParams = NULL;
		DisposeMemory(*mfptr_stretchDibParams);
		*mfptr_stretchDibParams = MEM_NULL;
			
		return(FAILED);
	}

	pixmap = GetGWorldPixMap(gworld);
	bytesPerScanline = (**pixmap).rowBytes & 0x0FFF;

	LockPixels(pixmap);
	if (bits_per_pixel <= 8)
	{
		new_pix_color_table = CreateNewPixMapCTab(*mfptr_stretchDibParams);
		if (new_pix_color_table == NULL)
		{
			UnuseMemory(ptr_StretchDibParams->ScanTable);
			ptr_ScanTable = NULL;
			DisposeMemory(ptr_StretchDibParams->ScanTable);
			ptr_StretchDibParams->ScanTable = MEM_NULL;

 			if (ptr_BitmapInfo->bmiColors != MEM_NULL)
			{
				DisposeMemory(ptr_BitmapInfo->bmiColors);
				ptr_BitmapInfo->bmiColors = MEM_NULL;
			
			}
	
			UnuseMemory(ptr_StretchDibParams->bmi);
			ptr_BitmapInfo = NULL;
			DisposeMemory(ptr_StretchDibParams->bmi);
			ptr_StretchDibParams->bmi = MEM_NULL;
			
			UnuseMemory(*mfptr_stretchDibParams);
			ptr_StretchDibParams = NULL;
			DisposeMemory(*mfptr_stretchDibParams);
			*mfptr_stretchDibParams = MEM_NULL;
			return (FAILED);
								
		}	
		DisposeCTable((**pixmap).pmTable);
		(**pixmap).pmTable = new_pix_color_table;
		CTabChanged ((**pixmap).pmTable);
		HUnlock((char**)new_pix_color_table);
		new_pix_color_table = NULL;
	}
	translatedBitmapBits = (pg_bits8_ptr)GetPixBaseAddr(pixmap);
		
	HeightDib = ptr_BitmapInfo->bmiHeader.Height;
	if (ptr_BitmapInfo->bmiHeader.BitsPerPixel == 24)
	{
		width_bytes = width_bytes + ptr_BitmapInfo->bmiHeader.Width;
	}
	WidthDib = CalculateMACScanLineLength(width_bytes);	
	SizeDibBitmap = (HeightDib * WidthDib);
	SizeGWorldSpace = (HeightDib * bytesPerScanline);

	for (i = 0, j = 0; i <  SizeDibBitmap, j < SizeGWorldSpace; i += WidthDib, j += bytesPerScanline)
	{
		memcpy ((void*)(translatedBitmapBits + j), (void*)(ptr_ScanTable + i) , WidthDib);
			
	}

	// do some cleanup
	UnuseMemory(ptr_StretchDibParams->ScanTable);
	ptr_ScanTable = NULL;
	DisposeMemory(ptr_StretchDibParams->ScanTable);
	ptr_StretchDibParams->ScanTable = MEM_NULL;
		
		
	if (ptr_BitmapInfo->bmiColors != MEM_NULL)
	{
		DisposeMemory(ptr_BitmapInfo->bmiColors);
		ptr_BitmapInfo->bmiColors = MEM_NULL;
	}
		
	UnuseMemory(ptr_StretchDibParams->bmi);
	ptr_BitmapInfo = NULL;
	DisposeMemory(ptr_StretchDibParams->bmi);
	ptr_StretchDibParams->bmi = MEM_NULL;
	
	UnuseMemory(*mfptr_stretchDibParams);
	ptr_StretchDibParams = NULL;
	DisposeMemory(*mfptr_stretchDibParams);
	*mfptr_stretchDibParams = MEM_NULL;
			
	yourGWorld = (GrafPtr)gworld;
	SetPort(currentPort);
	{
		RGBColor white = {0xffff, 0xffff, 0xffff};
		RGBColor black = {0x0000, 0x0000, 0x0000};
							
		RGBForeColor(&black);
		RGBBackColor(&white);
						
	}
	mask_rgn = NewRgn();
	RectRgn(mask_rgn, &BmpDestRect);
	EraseRect(&BmpDestRect);
	CopyBits(&yourGWorld->portBits, &currentPort->portBits, &BmpLocalFrame, &BmpDestRect, srcCopy + ditherCopy, mask_rgn);

	UnlockPixels(pixmap);
	DisposeRgn(mask_rgn);
	DisposeGWorld(gworld);
						

	return (SUCCESS);
}



// Main Translation function. Reads in Windows Metafile Records one at a time and calls the appropriate functions to create
// a PICT that performs the same functionality. Returns 0 if Succeeds and 1 if Fails.
short TranslateMetaRecords(pg_bits8_ptr* DataBytes, Rect localframe, GrafPtr currentPort, short NumObjects, Rect frame)
{

	WMFRECORD				metaRecord;
	RGBColor				rgbMac;
	RGBColor				rgbFillColor;
	RGBColor				rgbBackColor = { 0xFFFF, 0xFFFF, 0xFFFF};
	short 					NumPoints = 0;
	memory_ref				mf_Points = MEM_NULL;
	short						i;
	Point					WINOrigin;
	Point					WINExt;
	LOGBRUSH				brush;
	LOGPEN					pen;
	LOGFONT					font;
	short					pen_table_add_index = -1;
	short					pen_table_size = 0;
	memory_ref				pen_table = MEM_NULL;
	short					brush_table_add_index = -1;
	short					brush_table_size = 0;
	memory_ref				brush_table = MEM_NULL;
	short					bitmap_table_add_index = -1;
	short					bitmap_table_size = 0;
	memory_ref				bitmap_table = MEM_NULL;
	short					font_table_add_index = -1;
	short					font_table_size = 0;
	memory_ref				font_table = MEM_NULL;
	short 					palette_table_add_index = -1;
	short					palette_table_size = 0;
	memory_ref				palette_table = MEM_NULL;
	short					region_table_add_index = -1;
	short					region_table_size = 0;
	memory_ref				region_table = MEM_NULL;
	short					gdi_object_add_index = -1;
	short					gdi_object_size = 0;
	memory_ref				gdi_object_table = MEM_NULL;						
	short					selected_object_index = -1;
	memory_ref				selected_objects_table = MEM_NULL;
	short					bFirstWinExt = TRUE;
	short					bFirstWinOrg = TRUE;
	RGBColor 				black = {0x0000, 0x0000, 0x0000};
	RGBColor				white = {0xFFFF, 0xFFFF, 0xFFFF};
	Rect					rectangle = { -1, -1, -1, -1};
	short					BKMode = 2;
	LOGPEN					current_pen;
	LOGBRUSH				current_brush;
	BITMAPOBJECT			current_bitmap = {MEM_NULL};
	PALOBJECT				current_pal = {0};
	LOGFONT					current_font;
	short					current_mapmode = MM_TEXT;
	short 					bFailed = FALSE;
	short					number_palettes = 0;
	short					text_align = TA_TOP;
	
	selected_object_index = InitializeSelectedObjectsTable(&selected_objects_table, NumObjects);
	if (selected_object_index == -1)
		return (FAILED);
		

	
	GetMetaRecordFromByteArray(DataBytes, &metaRecord);
	while ((metaRecord.Function != 0) && (!bFailed))
	{
		switch (metaRecord.Function)
		{
			case META_SETBKCOLOR:
				rgbBackColor = GetCOLORREF(DataBytes);
				RGBBackColor(&rgbBackColor);
				break;
			
			case META_SETBKMODE:
				BKMode = OPAQUE;
				BKMode =(short)GetWordFromByteArray(*DataBytes);
				*DataBytes += 2;
				break;
				
			case META_SETTEXTALIGN:
				text_align = (short)GetWordFromByteArray(*DataBytes);
				*DataBytes += 2;
				break;
				
			case META_SETTEXTCOLOR:
				rgbMac = GetCOLORREF(DataBytes);
				RGBForeColor(&rgbMac);
				rgbFillColor = rgbMac;
				break;
			
			case META_SETWINDOWORG:
				GetWINOrigin(DataBytes, &WINOrigin);
//				SetOrigin(WINOrigin.h, WINOrigin.v);
				break;

			case META_SETWINDOWEXT:
				GetWINOrigin(DataBytes, &WINExt);
				break;
				
			case META_LINETO:
				{
					short nXEnd = 0, nYEnd = 0;
					
					nYEnd = (short)GetWordFromByteArray(*DataBytes);
					*DataBytes += 2;
					nXEnd = (short)GetWordFromByteArray(*DataBytes);
					*DataBytes += 2;
					
					LineTo (nXEnd, nYEnd);
				}
				
				break;
			
			case META_MOVETO:
				{
					short nXEnd = 0, nYEnd = 0;

					
					nYEnd = (short)GetWordFromByteArray(*DataBytes);
					*DataBytes += 2;
					nXEnd = (short)GetWordFromByteArray(*DataBytes);
					*DataBytes += 2;
					
					MoveTo (nXEnd, nYEnd);
				}	
					
				break;
				
			
			case META_ARC:
				{
					short start_angle = 0, stop_angle = 0;
					
					GetArcParams(DataBytes, &rectangle, &start_angle, &stop_angle, WINOrigin, WINExt, localframe, &metaRecord);
					if (current_pen.lopnStyle != PS_NULL)
					{
						RGBForeColor(&(current_pen.lopnColor));
						PenSize((current_pen.lopnWidth.h), (current_pen.lopnWidth.v));
						FrameArc(&rectangle, start_angle, stop_angle);
					}
					PenSize(1,1);
				}
				break;
				
				
			case META_PIE:
				{
					short start_angle = 0, stop_angle = 0;
					Point start_point = {0, 0}, stop_point = {0, 0}, rect_center = {0, 0};
					
					GetWedgeParams(DataBytes, &rectangle, &start_angle, &stop_angle, &start_point,
						&stop_point, &rect_center, WINOrigin, WINExt, localframe, &metaRecord);

					DrawPie(&current_brush, &current_pen, &rectangle, start_angle, stop_angle, &start_point, &stop_point, 
								&rect_center, BKMode, &rgbBackColor);
				}
				break;
				
						
			case META_RECTANGLE:
				GetRect(DataBytes, &rectangle, WINOrigin, WINExt, localframe, &metaRecord);
				DrawRectangle(&current_brush, &current_pen, &rectangle, &rgbBackColor, BKMode);
				break;
				
			case META_ROUNDRECT:
				{
					short ovalWidth = 0, ovalHeight = 0;
					GetRoundRectParams(DataBytes, &rectangle, &ovalWidth, &ovalHeight, &WINOrigin, &WINExt, &localframe, &metaRecord);
					DrawRoundRect(&current_brush, &current_pen, &rectangle, ovalWidth, ovalHeight, &rgbBackColor, BKMode);
				}
				break;
				
			case META_ELLIPSE:
				GetRect(DataBytes, &rectangle, WINOrigin, WINExt, localframe, &metaRecord);
				DrawEllipse(&current_brush, &current_pen, &rectangle, &rgbBackColor, BKMode);					
				break;
				
			case META_CREATEFONTINDIRECT:
				GetFontParams(DataBytes, &font, &metaRecord);
				AddFontToFontTable(&font_table_add_index, &font_table_size, &font_table, &font);
				AddObjectToGDIObjectTable((short)WIN_GDI_FONT, &gdi_object_add_index, &gdi_object_size,
					 &gdi_object_table, (short)font_table_add_index - 1);
					 
				break;
				
				
			case META_CREATEPENINDIRECT:
				GetPenParams(DataBytes, &pen, & metaRecord);
				{
					Rect srcRect = {0,0,0,0};
					srcRect.bottom = abs(WINExt.v);
					srcRect.right = abs(WINExt.h);
					
					
					ScalePt(&pen.lopnWidth, &srcRect, &localframe);
				}
				AddPenToPenTable(&pen_table_add_index, &pen_table_size, &pen_table, &pen);
				AddObjectToGDIObjectTable((short)WIN_GDI_PEN, &gdi_object_add_index, &gdi_object_size,
					&gdi_object_table, (short)pen_table_add_index - 1);
				break;
				
			case META_DIBCREATEPATTERNBRUSH:
				{
					DWORD fnColorSpec = 0;
					memory_ref patternInfo = MEM_NULL, scanBits = MEM_NULL;
					memory_ref mf_mac_pattern = MEM_NULL;

					fnColorSpec = (pg_word)GetDWordFromByteArray(*DataBytes);
					*DataBytes += 4;
					
					patternInfo =  MemoryAllocClear(m_globals, sizeof(BITMAPINFO), 1, 0);
					if (patternInfo == MEM_NULL)
					{
						bFailed = TRUE;
					}
					else
					{
						if (GetBitmapInfo(DataBytes, &patternInfo, &scanBits))
						{
							if (patternInfo != MEM_NULL)
							{
								DisposeMemory(patternInfo);
								patternInfo = MEM_NULL;
							}
							if (scanBits != MEM_NULL)
							{
								DisposeMemory(scanBits);
								scanBits = MEM_NULL;
							}
							bFailed = TRUE;
							break;
						}
						
						if (CreateMacPattern(fnColorSpec, patternInfo, scanBits, &mf_mac_pattern))
						{
							if (patternInfo != MEM_NULL)
							{
								DisposeMemory(patternInfo);
								patternInfo = MEM_NULL;
							}
							if (scanBits != MEM_NULL)
							{
								DisposeMemory(scanBits);
								scanBits = MEM_NULL;
							}
							if (mf_mac_pattern != MEM_NULL)
							{
								DisposeMemory(mf_mac_pattern);
								mf_mac_pattern = MEM_NULL;
							}
							bFailed = TRUE;
							break;
						}
						
						// create new brush
						brush.lbStyle = BS_PATTERN;
						brush.lbColor = rgbFillColor;
						brush.lbHatch = mf_mac_pattern;
						mf_mac_pattern = MEM_NULL;
						AddBrushToBrushTable(&brush_table_add_index, &brush_table_size, &brush_table, &brush);
						AddObjectToGDIObjectTable((short)WIN_GDI_BRUSH, &gdi_object_add_index, &gdi_object_size,
							&gdi_object_table, (short)brush_table_add_index - 1);

					}
					if (patternInfo != MEM_NULL)
					{
						DisposeMemory(patternInfo);
						patternInfo = MEM_NULL;
					}
					if (scanBits != MEM_NULL)
					{
						DisposeMemory(scanBits);
						scanBits = MEM_NULL;
					}
				
				}
				break;
						
				
			case META_CREATEBRUSHINDIRECT:
				GetBrushParams(DataBytes, &brush, &metaRecord);
				AddBrushToBrushTable(&brush_table_add_index, &brush_table_size, &brush_table, &brush);
				AddObjectToGDIObjectTable((short)WIN_GDI_BRUSH, &gdi_object_add_index, &gdi_object_size,
					&gdi_object_table, (short)brush_table_add_index - 1);
				break;
			
			case META_SELECTOBJECT:
				GetMetaRecordParams(DataBytes, &metaRecord);
				{
					WORD* tmpParams = NULL;
					
					tmpParams = (WORD*)UseMemory(metaRecord.Parameters);
				
					SelectGDIObject((short)tmpParams[0], &gdi_object_table, &selected_objects_table,  & current_bitmap, &current_pen, &current_brush, &current_font, bitmap_table, pen_table, brush_table, font_table);
					UnuseMemory(metaRecord.Parameters);
					tmpParams = NULL;
				}
				break;
				
			case META_DELETEOBJECT:
				GetMetaRecordParams(DataBytes, &metaRecord);
				{
					WORD* tmpParams = NULL;
					
					tmpParams = (WORD*)UseMemory(metaRecord.Parameters);
					
					DeleteGDIObject((short)tmpParams[0], &gdi_object_table);
					UnuseMemory(metaRecord.Parameters);
					tmpParams = NULL;
				}
				break;
			
			case META_EXTTEXTOUT:
				if (ExtTextOut (DataBytes, &metaRecord, current_mapmode, &WINOrigin, &WINExt, &localframe,
					&current_font, currentPort, text_align))
				{
					bFailed = TRUE;
				}
				break;
				
			case META_PATBLT:
				{
					Point nTopLeftRect = {0,0};
					short width = 0, height = 0;
					DWORD fdwRop = 0;
				
					GetPatBltParams(DataBytes, &nTopLeftRect, &width, &height, &fdwRop, &metaRecord);
					ScalePatBltParams(&nTopLeftRect, &width, &height, &WINOrigin, &WINExt, &localframe);
					if (MyPatBlt(&nTopLeftRect, &width, &height, &fdwRop, &current_brush, &current_pen))
						bFailed = TRUE;
				}
				break;

			case META_SETMAPMODE:
				GetMetaRecordParams(DataBytes, &metaRecord);
				{
					WORD* tmpParams = NULL;
					tmpParams = (WORD*)UseMemory(metaRecord.Parameters);
					current_mapmode = (short)tmpParams[0];
					UnuseMemory(metaRecord.Parameters);
					tmpParams = NULL;
				}
				break;
				
			case META_POLYGON:
				mf_Points = GetPolygonParams(DataBytes, &NumPoints, WINOrigin, WINExt, localframe, &metaRecord);
				if (mf_Points != MEM_NULL)
				{
					DrawPolygon(&current_brush, &current_pen, mf_Points, NumPoints, BKMode, &rgbBackColor);
				}
						
				if (mf_Points != MEM_NULL)
				{
					DisposeMemory(mf_Points);
					mf_Points = MEM_NULL;
				}
										
				break;
				
			case META_POLYLINE:
				mf_Points = GetPolygonParams(DataBytes, &NumPoints, WINOrigin, WINExt, localframe, &metaRecord);
				
				
				if (mf_Points != MEM_NULL)
				{
					Point* ptr_Points = NULL;
					
					RGBForeColor(&(current_pen.lopnColor));
					PenSize ((current_pen.lopnWidth.h), (current_pen.lopnWidth.v));
					
					ptr_Points = (Point*)UseMemory(mf_Points);
					MoveTo(ptr_Points[0].h, ptr_Points[0].v);
					for (i = 1; i < NumPoints; i++)
					{
						LineTo(ptr_Points[i].h, ptr_Points[i].v);
					}
					UnuseMemory(mf_Points);
					ptr_Points = NULL;
					PenSize(1,1);
				}
				

				if (mf_Points != MEM_NULL)
				{
					DisposeMemory(mf_Points);
					mf_Points = MEM_NULL;
				}

				break;
			

			case META_CREATEPALETTE:
				GetMetaRecordParams(DataBytes, &metaRecord);
				number_palettes++;
				current_pal.numberEntries = number_palettes;
				AddPaletteToPaletteTable(&palette_table_add_index, &palette_table_size, &palette_table, &current_pal);
				AddObjectToGDIObjectTable((short)WIN_GDI_PALETTE, &gdi_object_add_index, &gdi_object_size,
					&gdi_object_table, (short)palette_table_add_index - 1);
				
				
				break;
			case META_STRETCHDIB:
			{
				memory_ref mf_stretchDibParams = MEM_NULL;
				pg_bits8_ptr target_pos = ((metaRecord.Size - 3) * 2) + *DataBytes;

				mf_stretchDibParams = MemoryAlloc(m_globals,sizeof(STRETCHDIBPARAMS), 1, 0);
				if (mf_stretchDibParams == MEM_NULL)
				{
					bFailed = TRUE;
					GetMetaRecordParams(DataBytes, &metaRecord);
					break;
				}
				if (GetStretchDibParams(DataBytes, mf_stretchDibParams))
				{
				
					bFailed = TRUE;
					break;
				}
				else
				{
				
					if (ConvertWIN24toMAC32(mf_stretchDibParams))
					{
						if (mf_stretchDibParams != MEM_NULL)
						{
							DisposeStretchDibRef( &mf_stretchDibParams);						
						}
						bFailed = TRUE;
						break;
					}
					
					if (FlipDibScanTable(&mf_stretchDibParams))
					{
						if (mf_stretchDibParams != MEM_NULL)
						{
							DisposeStretchDibRef( &mf_stretchDibParams);						
						}
						bFailed = TRUE;
						break;
								
						
					}
					
					if (AddMACScanLinePadding (&mf_stretchDibParams))
					{
						if (mf_stretchDibParams != MEM_NULL)
						{
							DisposeStretchDibRef( &mf_stretchDibParams);						
						}
						bFailed = TRUE;
						break;
					}
					if (MacStretchDib(&mf_stretchDibParams, frame, &rectangle, currentPort, &WINOrigin, &WINExt))
					{
						bFailed = TRUE;
						break;
					}

				}

				*DataBytes = target_pos;	
			}
				break;
			default:
				GetMetaRecordParams(DataBytes, &metaRecord);
				break;
				
		}// end switch
		if (metaRecord.Parameters != MEM_NULL)
		{
			DisposeMemory(metaRecord.Parameters);
			metaRecord.Parameters = MEM_NULL;
			
		}
		GetMetaRecordFromByteArray(DataBytes, &metaRecord);
	}
	if (metaRecord.Parameters != MEM_NULL)
	{
		DisposeMemory(metaRecord.Parameters);
		metaRecord.Parameters = MEM_NULL;
	}

	//delete gdi object tables
	if (pen_table != MEM_NULL)
	{
		DisposeMemory(pen_table);
		pen_table = MEM_NULL;
	}

	if (brush_table != MEM_NULL)
	{
		register short i = 0;
		LOGBRUSH* ptr_brush_table = NULL;
		ptr_brush_table = (LOGBRUSH*)UseMemory(brush_table);
		for (i = 0; i < brush_table_size; i++)
		{
			if (ptr_brush_table[i].lbStyle == BS_PATTERN)
			{
				if (ptr_brush_table[i].lbHatch != MEM_NULL)
				{
					DisposeMemory(ptr_brush_table[i].lbHatch);
					ptr_brush_table[i].lbHatch = MEM_NULL;
				}
			}
		}
		UnuseMemory(brush_table);
		ptr_brush_table = NULL;
		DisposeMemory(brush_table);
		brush_table = MEM_NULL;
	}

	if (region_table != MEM_NULL)
	{
		DisposeMemory(region_table);
		region_table = MEM_NULL;
	}

	if (bitmap_table != MEM_NULL)
	{
		register short k = 0;
		BITMAPOBJECT* ptr_bitmap_table = NULL;
		ptr_bitmap_table = (BITMAPOBJECT*)UseMemory(bitmap_table);
		for (k = 0; k < bitmap_table_size; k++)
		{
			DisposeStretchDibRef(&(ptr_bitmap_table[k].mf_stretchDib));
		}
		UnuseMemory(bitmap_table);
		ptr_bitmap_table = NULL;	
		DisposeMemory(bitmap_table);
		bitmap_table = MEM_NULL;
	}

	if (font_table != MEM_NULL)
	{
	
		DisposeMemory(font_table);
		font_table = MEM_NULL;
	}

	if (gdi_object_table != MEM_NULL)
	{


		DisposeMemory(gdi_object_table);
		gdi_object_table = MEM_NULL;
	}

	if (selected_objects_table != MEM_NULL)
	{

		DisposeMemory(selected_objects_table);
		selected_objects_table = MEM_NULL;
	}

	
	RGBForeColor(&black);
	RGBBackColor(&white);
	if (bFailed == TRUE)
		return (FAILED);
	return (SUCCESS);
}


/* Translates the WMF to a PICT format and draws it to the designated rectangle. Returns a Pict Handle if successfull. 
Pict handle can be used for future redraws. If not successful, returns NULL.
*/
PicHandle TranslateWMFToPicture(pgm_globals_ptr mem, pg_bits8_ptr* DataBytes, Rect* frame, DWORD data_size)
{
	PicHandle				hdl_NewPicture = NULL;
	GrafPtr					whereDrawingGoes;
	GrafPtr 				originalPort;
	Rect					localFrame;
	WMFHEAD					metaHeader;
	char					bugs[2];
	RgnHandle				clipRgn = NULL;
 	
   m_globals = mem;

#ifdef PG_DEBUG
   bugs[0] = bugs[1] = 0x55;
#endif

 	if (data_size < META_HEADER_SIZE_BYTES)
 	{
 		return (NULL);
 	}
 	GetPort(&originalPort);
 	whereDrawingGoes = (GrafPtr)NewPtr(sizeof(CGrafPort));
 	OpenCPort((CGrafPtr)whereDrawingGoes);
 	
	if (GetMetaHeaderFromByteArray(DataBytes, &metaHeader))
	{
		SetPort(originalPort);
		return(NULL);
	}
	
#ifdef PG_DEBUG
	if (bugs[0] != 0x55 || bugs[1] != 0x55)
		pgSourceDebugBreak(MEM_NULL, (char *)"\pError in image conversion");
#endif

	localFrame = *frame;
	OffsetRect(&localFrame, -frame->left, -frame->top);
	
	// open picture
	SetPort(whereDrawingGoes);
	hdl_NewPicture = OpenPicture(frame);
		
	if (!hdl_NewPicture) {

		SetPort(originalPort);
		CloseCPort((CGrafPtr)whereDrawingGoes);
		DisposePtr((Ptr)whereDrawingGoes);

		return (NULL);
	}
	
	ClipRect(&localFrame);
	if (TranslateMetaRecords(DataBytes, localFrame, whereDrawingGoes, metaHeader.NumOfObjects, *frame)) {
		
		ClosePicture();
		KillPicture(hdl_NewPicture);
		SetPort(originalPort);
		CloseCPort((CGrafPtr)whereDrawingGoes);
		DisposePtr((Ptr)whereDrawingGoes);
		return(NULL);
	}

	//Close picture and return PicHandle
	ClosePicture();
	SetPort(originalPort);

	CloseCPort((CGrafPtr)whereDrawingGoes);
	DisposePtr((Ptr)whereDrawingGoes);

	return (hdl_NewPicture);
	
}

#endif

