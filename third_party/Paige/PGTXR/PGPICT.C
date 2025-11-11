/*
	PgPictToWMF.c

	Functions for translating Macintosh Pictures (PICT) to Windows Metafiles

  	by John Ruble
	DataPak Software
	started: 06/19/97
	finished vs :
*/

#include "CPUDEFS.H"

#ifdef WINDOWS_COMPILE

#include "Paige.h"
#include "pgCtlUtl.h"
#include "pgPict.h"
#include <math.h>

int	PICT_Is_Version_One = FALSE;

pgm_globals_ptr m_globals;


 
void DisposePixMap(memory_ref PG_FAR * mfptr_pixmap)
{

	if (*mfptr_pixmap != MEM_NULL)
	{
		pmac_pixmap		ptr_pixmap = NULL;
		
		ptr_pixmap = (pmac_pixmap)UseMemory(*mfptr_pixmap);
		if (ptr_pixmap->pmTable != MEM_NULL)
		{
			pmac_colorTable ptr_mct = NULL;
			ptr_mct = (pmac_colorTable)UseMemory(ptr_pixmap->pmTable);
			if (ptr_mct->cSpecArray != MEM_NULL)
			{
				DisposeMemory(ptr_mct->cSpecArray);
				ptr_mct->cSpecArray = MEM_NULL;
			}
			UnuseMemory(ptr_pixmap->pmTable);
			ptr_mct = NULL;
			DisposeMemory(ptr_pixmap->pmTable);
			ptr_pixmap->pmTable = MEM_NULL;
		}
		if (ptr_pixmap->baseAddr != MEM_NULL)
		{
			DisposeMemory(ptr_pixmap->baseAddr);
			ptr_pixmap->baseAddr = MEM_NULL;
		}
		UnuseMemory(*mfptr_pixmap);
		ptr_pixmap = NULL;
		DisposeMemory(*mfptr_pixmap);
		*mfptr_pixmap = MEM_NULL;
	}

	return;
}


void DisposeBMPackBitsRect(memory_ref PG_FAR * mfptr_pbr_data)
{
	pmac_packBitmapRect ptr_bm = NULL;

	if (*mfptr_pbr_data != MEM_NULL)
	{
		ptr_bm = (pmac_packBitmapRect)UseMemory(*mfptr_pbr_data);
		if (ptr_bm->mf_bitData != MEM_NULL)
		{
			DisposeMemory(ptr_bm->mf_bitData);
			ptr_bm->mf_bitData = MEM_NULL;
		}
		UnuseMemory(*mfptr_pbr_data);
		ptr_bm = NULL;
		DisposeMemory(*mfptr_pbr_data);
		*mfptr_pbr_data = MEM_NULL;
	}


	return;
}
  
void DisposeBMPackBitsRgn(memory_ref PG_FAR * mfptr_pbr_data)
{
	pmac_packBitmapRgn ptr_bm = NULL;

	if (*mfptr_pbr_data != MEM_NULL)
	{
		ptr_bm = (pmac_packBitmapRgn)UseMemory(*mfptr_pbr_data);
		if (ptr_bm->mf_bitData != MEM_NULL)
		{
			DisposeMemory(ptr_bm->mf_bitData);
			ptr_bm->mf_bitData = MEM_NULL;
		}
		UnuseMemory(*mfptr_pbr_data);
		ptr_bm = NULL;
		DisposeMemory(*mfptr_pbr_data);
		*mfptr_pbr_data = MEM_NULL;
	}


	return;
}

// temporary cleanup code
void DisposePackBitsRectParams(memory_ref PG_FAR * mfptr_pbr_params)
{

	pmac_packBitsRect  ptr_pbr_params = NULL;
	pmac_colorTable    ptr_ct = NULL;

	if (*mfptr_pbr_params != MEM_NULL)
	{	
		ptr_pbr_params = (pmac_packBitsRect)UseMemory(*mfptr_pbr_params);
		if (ptr_pbr_params->mf_colorTable != MEM_NULL)
		{
			ptr_ct = (pmac_colorTable)UseMemory(ptr_pbr_params->mf_colorTable);
			if (ptr_ct->cSpecArray != MEM_NULL)
			{
				DisposeMemory(ptr_ct->cSpecArray);
				ptr_ct->cSpecArray = MEM_NULL;
			}
			UnuseMemory(ptr_pbr_params->mf_colorTable);
			ptr_ct = NULL;
			DisposeMemory(ptr_pbr_params->mf_colorTable);
			ptr_pbr_params->mf_colorTable = MEM_NULL;
		}

		if (ptr_pbr_params->mf_PixData != MEM_NULL)
		{
			DisposeMemory(ptr_pbr_params->mf_PixData);
			ptr_pbr_params->mf_PixData = MEM_NULL;
		}

		UnuseMemory(*mfptr_pbr_params);
		ptr_pbr_params = NULL;
		DisposeMemory(*mfptr_pbr_params);
		*mfptr_pbr_params = MEM_NULL;
	}
	return;
}

void DisposePackBitsRgnParams(memory_ref PG_FAR * mfptr_pbr_params)
{

	pmac_packBitsRgn  ptr_pbr_params = NULL;
	pmac_colorTable    ptr_ct = NULL;

	if (*mfptr_pbr_params != MEM_NULL)
	{	
		ptr_pbr_params = (pmac_packBitsRgn)UseMemory(*mfptr_pbr_params);
		if (ptr_pbr_params->mf_colorTable != MEM_NULL)
		{
			ptr_ct = (pmac_colorTable)UseMemory(ptr_pbr_params->mf_colorTable);
			if (ptr_ct->cSpecArray != MEM_NULL)
			{
				DisposeMemory(ptr_ct->cSpecArray);
				ptr_ct->cSpecArray = MEM_NULL;
			}
			UnuseMemory(ptr_pbr_params->mf_colorTable);
			ptr_ct = NULL;
			DisposeMemory(ptr_pbr_params->mf_colorTable);
			ptr_pbr_params->mf_colorTable = MEM_NULL;
		}

		if (ptr_pbr_params->mf_PixData != MEM_NULL)
		{
			DisposeMemory(ptr_pbr_params->mf_PixData);
			ptr_pbr_params->mf_PixData = MEM_NULL;
		}

		UnuseMemory(*mfptr_pbr_params);
		ptr_pbr_params = NULL;
		DisposeMemory(*mfptr_pbr_params);
		*mfptr_pbr_params = MEM_NULL;
	}
	return;
}

int RetrieveCorePixData(pg_bits8_ptr PG_FAR * ptr2_data_bytes, pmac_corePixData ptr_core_pm)
{
	if (ptr_core_pm == NULL)
		return (NOSUCCESS);

	ptr_core_pm->rowBytes = (short)GetMacWordInc(ptr2_data_bytes);
	GetMacRectInc(ptr2_data_bytes, &(ptr_core_pm->bounds));
	ptr_core_pm->pmVersion = (short)GetMacWordInc(ptr2_data_bytes);
	ptr_core_pm->packType = (short)GetMacWordInc(ptr2_data_bytes);
	ptr_core_pm->packSize = (long)GetMacDWORDInc(ptr2_data_bytes);
	GetMacFixedInc(ptr2_data_bytes, &(ptr_core_pm->hRes));
	GetMacFixedInc(ptr2_data_bytes, &(ptr_core_pm->vRes));
	ptr_core_pm->pixelType = (short)GetMacWordInc(ptr2_data_bytes);
	ptr_core_pm->pixelSize = (short)GetMacWordInc(ptr2_data_bytes);
	ptr_core_pm->cmpCount = (short)GetMacWordInc(ptr2_data_bytes);
	ptr_core_pm->cmpSize = (short)GetMacWordInc(ptr2_data_bytes);
	ptr_core_pm->planeBytes = (long)GetMacDWORDInc(ptr2_data_bytes);

	return(SUCCESS);
}



int RetrieveMacCbPixmapData(pg_bits8_ptr PG_FAR * ptr2_data_bytes, pmac_cb_pixmapData ptr_cb_pm)
{
	pmac_corePixData ptr_core_pm = NULL;
	
	if (ptr_cb_pm == NULL)
		return (NOSUCCESS);

	ptr_core_pm = &(ptr_cb_pm->std_pm_data);
	if (ptr_core_pm == NULL)
		return (NOSUCCESS);


	if (RetrieveCorePixData(ptr2_data_bytes, ptr_core_pm))
	{
		ptr_core_pm = NULL;
		return (NOSUCCESS);
	}
	ptr_cb_pm->pmTable = (long)GetMacDWORDInc(ptr2_data_bytes);
	ptr_cb_pm->pmReserved = (long)GetMacDWORDInc(ptr2_data_bytes);
		

	return(SUCCESS);

}



int RetrieveColorSpecEntries(pg_bits8_ptr PG_FAR * ptr2_data_bytes, pmac_colorSpec ptr_colSpec, int NumCtEntries)
{
	int i = 0;

	if (ptr_colSpec == NULL)
		return (NOSUCCESS);
	if (NumCtEntries <= 0)
		return (NOSUCCESS);

	for (i = 0; i < NumCtEntries; i++)
	{
		ptr_colSpec[i].value = (short)GetMacWordInc(ptr2_data_bytes);
		ptr_colSpec[i].mac_rgb.red = GetMacWordInc(ptr2_data_bytes);
		ptr_colSpec[i].mac_rgb.green = GetMacWordInc(ptr2_data_bytes);
		ptr_colSpec[i].mac_rgb.blue = GetMacWordInc(ptr2_data_bytes);
			
	}

	return (SUCCESS);

}



int RetrieveMacColorTable(pg_bits8_ptr PG_FAR * ptr2_data_bytes, pmac_colorTable ptr_ct)
{
	int NumCtEntries = 0;
	if (ptr_ct == NULL)
		return (NOSUCCESS);

	ptr_ct->ctSeed = (long)GetMacDWORDInc(ptr2_data_bytes);
	ptr_ct->ctFlags = GetMacWordInc(ptr2_data_bytes);
	ptr_ct->ctSize	= GetMacWordInc(ptr2_data_bytes);
	NumCtEntries = ptr_ct->ctSize + 1;
	ptr_ct->cSpecArray = MemoryAllocClear(m_globals, sizeof(mac_colorSpec), NumCtEntries, 0);
	if (ptr_ct->cSpecArray == MEM_NULL)
		return (NOSUCCESS);
	else
	{
		pmac_colorSpec ptr_colSpec = (pmac_colorSpec)UseMemory(ptr_ct->cSpecArray);
		if (RetrieveColorSpecEntries(ptr2_data_bytes, ptr_colSpec, NumCtEntries))
		{
			UnuseMemory(ptr_ct->cSpecArray);
			ptr_colSpec = NULL;

			return (NOSUCCESS);
		}
		else
		{
			UnuseMemory(ptr_ct->cSpecArray);
			ptr_colSpec = NULL;
		}
	}

	return (SUCCESS);
}




int DefaultUnpackBMbits(pg_bits8_ptr PG_FAR * ptr2_data_bytes, pg_bits8_ptr ptr_BMbits, 
						int bWordByteCount, pmac_packBitmapRect ptr_bm, short rowBytes, mac_rect PG_FAR * dstRect, int bBitsRect)
{
	long i = 0, j = 0, k = 0;
	long number_scanlines = ptr_bm->m_bitmap.bounds.bottom - ptr_bm->m_bitmap.bounds.top;
	short byte_count = 0;
	short repeat_count = 0;

	pg_bits8 repeat_byte = 0;
	short count_rowBytes = 0;
	long total_num_bytes = 0;

    if (bBitsRect)
    {
     	pgBlockMove(*ptr2_data_bytes, ptr_BMbits, (rowBytes * number_scanlines));
     	*ptr2_data_bytes += (rowBytes * number_scanlines);
     	 return (SUCCESS);
    
    }
	for (i = 0; i < number_scanlines; i++)
	{
		count_rowBytes = 0;
		if (bWordByteCount)
		{
			do
			{
				byte_count = (short)GetMacWordInc(ptr2_data_bytes);
				total_num_bytes += 2;

			}
			while (byte_count == 0);
				
		}
		else
		{
			do
			{
				byte_count = (short)((BYTE)GetByteInc(ptr2_data_bytes));
				total_num_bytes++;
			}
			while (byte_count == 0);
		}
		for (j = rowBytes * i; j < (rowBytes * i) + rowBytes ; j=k)
		{
		
			do
			{
				count_rowBytes++;
				repeat_count = (short)(signed char)GetByteInc(ptr2_data_bytes);
				total_num_bytes++;
			} while (repeat_count == -128);
		
			if (repeat_count < 0)
			{
				repeat_count = (-repeat_count) + 1;
				repeat_byte = GetByteInc(ptr2_data_bytes);
				total_num_bytes++;
				count_rowBytes++;
				for (k = j; k < j + repeat_count; k++)
				{
					*(ptr_BMbits + k) = repeat_byte;
					
				}
			}
			else
			{
				repeat_count++;
				for (k = j; k < j + repeat_count; k++)
				{
					*(ptr_BMbits + k) = **ptr2_data_bytes;
					*ptr2_data_bytes += 1;
					count_rowBytes++;
					total_num_bytes++;
				

				}
			}
		}
	}

	return (SUCCESS);
}

int DefaultUnpackBMbitsRgn(pg_bits8_ptr PG_FAR * ptr2_data_bytes, pg_bits8_ptr ptr_BMbits, 
						int bWordByteCount, pmac_packBitmapRgn ptr_bm, short rowBytes, mac_rect PG_FAR * dstRect, int bBitsRect)
{
	long i = 0, j = 0, k = 0;
	long number_scanlines = ptr_bm->m_bitmap.bounds.bottom - ptr_bm->m_bitmap.bounds.top;
	short byte_count = 0;
	short repeat_count = 0;

	pg_bits8 repeat_byte = 0;
	short count_rowBytes = 0;
	long total_num_bytes = 0;

    if (bBitsRect)
    {
     	pgBlockMove(*ptr2_data_bytes, ptr_BMbits, (rowBytes * number_scanlines));
     	*ptr2_data_bytes += (rowBytes * number_scanlines);
  		return (SUCCESS);   	
    
    }

	for (i = 0; i < number_scanlines; i++)
	{
		count_rowBytes = 0;
		if (bWordByteCount)
		{
			do
			{
				byte_count = (short)GetMacWordInc(ptr2_data_bytes);
				total_num_bytes += 2;

			}
			while (byte_count == 0);
				
		}
		else
		{
			do
			{
				byte_count = (short)((BYTE)GetByteInc(ptr2_data_bytes));
				total_num_bytes++;
			}
			while (byte_count == 0);
		}
		for (j = rowBytes * i; j < (rowBytes * i) + rowBytes ; j=k)
		{
		
			do
			{
				count_rowBytes++;
				repeat_count = (short)(signed char)GetByteInc(ptr2_data_bytes);
				total_num_bytes++;
			} while (repeat_count == -128);
		
			if (repeat_count < 0)
			{
				repeat_count = (-repeat_count) + 1;
				repeat_byte = GetByteInc(ptr2_data_bytes);
				total_num_bytes++;
				count_rowBytes++;
				for (k = j; k < j + repeat_count; k++)
				{
					*(ptr_BMbits + k) = repeat_byte;
					
				}
			}
			else
			{
				repeat_count++;
				for (k = j; k < j + repeat_count; k++)
				{
					*(ptr_BMbits + k) = **ptr2_data_bytes;
					*ptr2_data_bytes += 1;
					count_rowBytes++;
					total_num_bytes++;
				

				}
			}
		}
	}


	return (SUCCESS);
}

int DefaultUnpackPixBits(pg_bits8_ptr PG_FAR * ptr2_data_bytes, pg_bits8_ptr ptr_pixbits, 
						 int bWordByteCount, pmac_corePixData ptr_core_pix, long pm_size, short rowBytes, mac_rect PG_FAR * dstRect)
{
	
	long i = 0, j = 0, k = 0;
	long number_scanlines = (ptr_core_pix->bounds.bottom - ptr_core_pix->bounds.top);
	short byte_count = 0;
	pg_bits8 repeat_byte = 0;
	short repeat_count = 0;
	short count_rowBytes = 0;
	long total_num_bytes = 0;

	for (i = 0; i < number_scanlines; i ++)
	{
		count_rowBytes = 0;
		if (bWordByteCount)
		{
			do
			{
				byte_count = (short)GetMacWordInc(ptr2_data_bytes);
				total_num_bytes += 2;

			}
			while (byte_count == 0);
				
		}
		else
		{
			do
			{
				byte_count = (short)((BYTE)GetByteInc(ptr2_data_bytes));
				total_num_bytes++;
			}
			while (byte_count == 0);
		}
		for (j = rowBytes * i; j < (rowBytes * i) + rowBytes ; j=k)
		{
		
			do
			{
				count_rowBytes++;
				repeat_count = (short)(signed char)GetByteInc(ptr2_data_bytes);
				total_num_bytes++;
			} while (repeat_count == -128);
		
			if (repeat_count < 0)
			{
				repeat_count = (-repeat_count) + 1;
				repeat_byte = GetByteInc(ptr2_data_bytes);
				total_num_bytes++;
				count_rowBytes++;
				for (k = j; k < j + repeat_count; k++)
				{
					*(ptr_pixbits + k) = repeat_byte;
					
				}
			}
			else
			{
				repeat_count++;
				for (k = j; k < j + repeat_count; k++)
				{
					*(ptr_pixbits + k) = **ptr2_data_bytes;
					*ptr2_data_bytes += 1;
					count_rowBytes++;
					total_num_bytes++;

				}
			}
		}
	}
	if (!PICT_Is_Version_One)
	{
		if (total_num_bytes % 2)
			*ptr2_data_bytes += 1;
    }
	return (SUCCESS);
}



int UnpackPixBitsThree(pg_bits8_ptr PG_FAR * ptr2_data_bytes, pg_bits8_ptr ptr_pixbits, int bWordByteCount, 
					   pmac_corePixData ptr_core_pix, long pms_size, short rowBytes)
{
	register long i = 0, j = 0, k = 0, n = 0, m = 0;
	long number_scanlines = (ptr_core_pix->bounds.bottom - ptr_core_pix->bounds.top);
	short packed_byte_count = 0;
	short word_count = 0;
	long total_num_bytes = 0;
	pg_word repeat_word = 0;

	for (i = 0; i < number_scanlines; i ++)
	{

		if (bWordByteCount)
		{
			do
			{
				packed_byte_count = (short)GetMacWordInc(ptr2_data_bytes);
				total_num_bytes += 2;

			}
			while (packed_byte_count == 0);
				
		}
		else
		{
			do
			{
				packed_byte_count = (short)((BYTE)GetByteInc(ptr2_data_bytes));
				total_num_bytes++;
			}
			while (packed_byte_count == 0);
		}
		for (j = rowBytes * i; j < (rowBytes * i) + rowBytes ; j=k)
		{
		
			do
			{

				word_count = (short)(signed char)GetByteInc(ptr2_data_bytes);
				total_num_bytes++;
			} while (word_count == -128);
		
			if (word_count < 0)
			{
				word_count = (-word_count) + 1;
				repeat_word = GetMacWordInc(ptr2_data_bytes);
				total_num_bytes += 2;

				for (k = j; k < j + (word_count * 2); k += 2)
				{
					*(ptr_pixbits + k) = (BYTE)(repeat_word >> 8);
					*(ptr_pixbits + k + 1) = (BYTE)(repeat_word & 0x00ff);

					
				}
			}
			else
			{
				word_count++;
				for (k = j; k < j + (word_count * 2); k += 2)
				{
					*(ptr_pixbits + k) = GetByteInc(ptr2_data_bytes);
					*(ptr_pixbits + k + 1) = GetByteInc(ptr2_data_bytes);

					total_num_bytes += 2;

				}
			}
		}
	}  
	if (!PICT_Is_Version_One)
	{
		if (total_num_bytes % 2)
			*ptr2_data_bytes += 1;
    }
	return (SUCCESS);
}


int UnpackPixBitsFour (pg_bits8_ptr PG_FAR * ptr2_data_bytes, pg_bits8_ptr ptr_pixbits, int bWordByteCount,
					pmac_corePixData ptr_core_pix, long pm_size, short rowBytes)
{
	register long i = 0, j = 0, k = 0, n = 0, m = 0;
	long number_scanlines = (ptr_core_pix->bounds.bottom - ptr_core_pix->bounds.top);
	long number_columns = (ptr_core_pix->bounds.right - ptr_core_pix->bounds.left);
	short packed_byte_count = 0;
	RGBTRIPLE PG_FAR * ptr_rgbt = NULL;
	long tmpRowBytes = (rowBytes / 4) * 3;
	short byte_count = 0;
	pg_bits8 repeat_byte = 0;
	long total_read_bytes = 0;


	ptr_rgbt = (RGBTRIPLE PG_FAR *)ptr_pixbits;

	for (i = 0; i < number_scanlines; i++)
	{
		long start_position = i * number_columns;
		n = 0;
		if (bWordByteCount)
		{
			packed_byte_count = (short)GetMacWordInc(ptr2_data_bytes);
			total_read_bytes +=2;
		}
		else
		{
			packed_byte_count = (short)GetByteInc(ptr2_data_bytes);
			total_read_bytes++;
		}

		for (m = 0, j = start_position; m < packed_byte_count; j = k)
		{
			long l = 0;
			byte_count = (short)((signed char)GetByteInc(ptr2_data_bytes));
			total_read_bytes++;
			m++;
			if (byte_count < 0)
			{
				byte_count = (-byte_count) + 1;
				repeat_byte = (pg_bits8)GetByteInc(ptr2_data_bytes);
				total_read_bytes++;
				m++;
			
				for (k = j, l = byte_count; l > 0; k++, l--)
				{
					if (k - start_position >= number_columns)
					{
						k = j = start_position;
						n++;
					}

					switch (n)
					{
					case 0:
						ptr_rgbt[k].rgbtRed = repeat_byte;
						break;
					case 1:
						ptr_rgbt[k].rgbtGreen = repeat_byte;
						break;
					case 2:
						ptr_rgbt[k].rgbtBlue = repeat_byte;
						break;
					};
				}
			}
			else
			{
				byte_count = byte_count + 1;
				for (k = j, l = byte_count; l > 0; k++, l--)
				{
					if (k - start_position >= number_columns)
					{
						k = j = start_position;
						n++;
					
					}
					switch (n)
					{
					case 0:
						ptr_rgbt[k].rgbtRed = **ptr2_data_bytes;
						break;
					case 1:
						ptr_rgbt[k].rgbtGreen = **ptr2_data_bytes;
						break;
					case 2:
						ptr_rgbt[k].rgbtBlue = **ptr2_data_bytes;
						break;
					};
					*ptr2_data_bytes += 1;
					total_read_bytes++;
					m++;
				}

			}
		}

	}
	if (!PICT_Is_Version_One)
	{
		if (total_read_bytes % 2)
		{
			*ptr2_data_bytes += 1;
		}
	}


	return (SUCCESS);

}


int UnpackPixData(pg_bits8_ptr PG_FAR * ptr2_data_bytes, pg_bits8_ptr ptr_pixbits, 
				  pmac_corePixData ptr_core_pix, short rowBytes, long pm_size, mac_rect PG_FAR * dstRect)
{

	int bWordByteCount = FALSE;
	int i = 0, j = 0;
	short packType = ptr_core_pix->packType;

	if (rowBytes > 250)
	{
		bWordByteCount = TRUE;
	}

	if (packType == 0)
	{
		if (DefaultUnpackPixBits(ptr2_data_bytes, ptr_pixbits, bWordByteCount,ptr_core_pix, pm_size, rowBytes, dstRect))
			return (NOSUCCESS);
	}
	else if (packType == 3)
	{
		if (UnpackPixBitsThree(ptr2_data_bytes, ptr_pixbits, bWordByteCount, ptr_core_pix, pm_size, rowBytes))
			return (NOSUCCESS);
	}
	else if (packType == 4)
	{
		if (UnpackPixBitsFour (ptr2_data_bytes, ptr_pixbits, bWordByteCount,ptr_core_pix, pm_size, rowBytes))
			return (NOSUCCESS);
	}
	else
	{
		return(NOSUCCESS);
	}

	return (SUCCESS);
}


int RetrieveBMBits(pg_bits8_ptr PG_FAR * ptr2_data_bytes, pg_bits8_ptr ptr_BMbits, pmac_packBitmapRect ptr_bm, mac_rect PG_FAR * dstRect, int bBitsRect)
{
	int bWordCount = FALSE;
     
  
	if (ptr_bm->m_bitmap.rowBytes > 250)
		bWordCount = TRUE;

	if (DefaultUnpackBMbits(ptr2_data_bytes, ptr_BMbits, bWordCount, ptr_bm, 
		(short)(ptr_bm->m_bitmap.rowBytes), dstRect, bBitsRect))
	{
		return (NOSUCCESS);
	}
	
	return(SUCCESS);
}

int RetrieveBMBitsRgn(pg_bits8_ptr PG_FAR * ptr2_data_bytes, pg_bits8_ptr ptr_BMbits, pmac_packBitmapRgn ptr_bm, mac_rect PG_FAR * dstRect, int bBitsRect)
{
	int bWordCount = FALSE;

	if (ptr_bm->m_bitmap.rowBytes > 250)
		bWordCount = TRUE;

	if (DefaultUnpackBMbitsRgn(ptr2_data_bytes, ptr_BMbits, bWordCount, ptr_bm, 
		(short)(ptr_bm->m_bitmap.rowBytes), dstRect, bBitsRect))
	{
		return (NOSUCCESS);
	}
	
	return(SUCCESS);
}


int RetrievePixelBits(pg_bits8_ptr PG_FAR * ptr2_data_bytes, pg_bits8_ptr ptr_pixbits,pmac_corePixData ptr_core_pix,
					  mac_rect PG_FAR * dstRect)
{
	short rowBytes = (ptr_core_pix->rowBytes & 0x7fff);
	long  pm_size = rowBytes * (ptr_core_pix->bounds.bottom - ptr_core_pix->bounds.top); 
	short packType = ptr_core_pix->packType;
	short pixelSize = ptr_core_pix->pixelSize;
	long  packSize = ptr_core_pix->packSize;
	long  data_size = 0;

	//check for problems
	if (ptr_pixbits == NULL)
		return (NOSUCCESS);
	if ((packType < 0) || (packType > 4))
		return (NOSUCCESS);
	if (rowBytes < 0)
		return (NOSUCCESS);
	if ((packType == 3) && (pixelSize != 16))
		return (NOSUCCESS);

	if (((packType == 2) || (packType == 4)) && (pixelSize != 32))
		return (NOSUCCESS);



	if ((rowBytes < 8) || (packType == 1))
	{

		pgBlockMove(*ptr2_data_bytes, ptr_pixbits, pm_size);
		*ptr2_data_bytes += pm_size;
		return(SUCCESS);
	}
	else if (packType == 2)
	{  

		data_size = pm_size = (pm_size * 3) / 4;
		pgBlockMove(*ptr2_data_bytes, ptr_pixbits, data_size);
		*ptr2_data_bytes += data_size;
		return(SUCCESS);

	}
	else if (UnpackPixData(ptr2_data_bytes, ptr_pixbits, ptr_core_pix, rowBytes, pm_size, dstRect))
		return (NOSUCCESS);




	return (SUCCESS);
}



int RetrieveMacBmData(pg_bits8_ptr PG_FAR * ptr2_data_bytes, pmac_bitmap ptr_mac_bitmap)
{
	ptr_mac_bitmap->baseAddr = 0; 
	ptr_mac_bitmap->rowBytes = (short)GetMacWordInc(ptr2_data_bytes);
	GetMacRectInc(ptr2_data_bytes, &(ptr_mac_bitmap->bounds));


	return (SUCCESS);

}



int RetrieveBMPackBitsRectParams(pg_bits8_ptr PG_FAR * ptr2_data_bytes, memory_ref PG_FAR * mfptr_pbr_data, int bBitsRect)
{
	pmac_packBitmapRect ptr_bm = NULL;
 	pg_bits8_ptr start_loc = *ptr2_data_bytes;

	if (*mfptr_pbr_data == MEM_NULL)
		return (NOSUCCESS);

	ptr_bm = (pmac_packBitmapRect)UseMemory(*mfptr_pbr_data);

	if (RetrieveMacBmData(ptr2_data_bytes, &(ptr_bm->m_bitmap)))
	{
		UnuseMemory(*mfptr_pbr_data);
		ptr_bm = NULL;
		return (NOSUCCESS);
	}
	GetMacRectInc(ptr2_data_bytes, &(ptr_bm->srcRect));
	GetMacRectInc(ptr2_data_bytes, &(ptr_bm->dstRect));
	ptr_bm->mode = GetMacWordInc(ptr2_data_bytes);

	// retrieve Bitmap bits
	{
		pg_bits8_ptr ptr_BMbits = NULL;

		short tmpRowBytes = ptr_bm->m_bitmap.rowBytes;
		long  numScanLines = (ptr_bm->m_bitmap.bounds.bottom - ptr_bm->m_bitmap.bounds.top);
		long data_size =  tmpRowBytes *  numScanLines;
		ptr_bm->mf_bitData = MemoryAllocClear(m_globals, sizeof(pg_bits8_ptr), data_size, 0);
		if (ptr_bm->mf_bitData == MEM_NULL)
		{
			UnuseMemory(*mfptr_pbr_data);
			ptr_bm = NULL;
			return (NOSUCCESS);
		}
		
		 ptr_BMbits= (pg_bits8_ptr)UseMemory(ptr_bm->mf_bitData);


		if (RetrieveBMBits(ptr2_data_bytes, ptr_BMbits, ptr_bm, &(ptr_bm->dstRect), bBitsRect))
		{
			UnuseMemory(*mfptr_pbr_data);
			ptr_bm = NULL;
			return (NOSUCCESS);
		}
		else
		{
			UnuseMemory(ptr_bm->mf_bitData);
			ptr_BMbits=NULL;
			UnuseMemory(*mfptr_pbr_data);
			ptr_bm = NULL;

		}

	}
	if (!PICT_Is_Version_One)
    {
    	pg_bits8_ptr end_loc = *ptr2_data_bytes;
    	long total_num_bytes = end_loc - start_loc;
    	if (total_num_bytes % 2)
    		*ptr2_data_bytes += 1;
    }
	return (SUCCESS);
}

 int RetrieveBMPackBitsRgnParams(pg_bits8_ptr PG_FAR * ptr2_data_bytes, memory_ref PG_FAR * mfptr_pbr_data, int bBitsRect)
{
	pmac_packBitmapRgn ptr_bm = NULL;

	if (*mfptr_pbr_data == MEM_NULL)
		return (NOSUCCESS);

	ptr_bm = (pmac_packBitmapRgn)UseMemory(*mfptr_pbr_data);

	if (RetrieveMacBmData(ptr2_data_bytes, &(ptr_bm->m_bitmap)))
	{
		UnuseMemory(*mfptr_pbr_data);
		ptr_bm = NULL;
		return (NOSUCCESS);
	}
	GetMacRectInc(ptr2_data_bytes, &(ptr_bm->srcRect));
	GetMacRectInc(ptr2_data_bytes, &(ptr_bm->dstRect));
	ptr_bm->mode = GetMacWordInc(ptr2_data_bytes);   
	{
		mac_rgn tmprgn; 
		GetMacRgnAndScaleRect(ptr2_data_bytes, &tmprgn, &(ptr_bm->mask_rect), 1, 1);
	}
		

	// retrieve Bitmap bits
	{
		pg_bits8_ptr ptr_BMbits = NULL;

		short tmpRowBytes = ptr_bm->m_bitmap.rowBytes;
		long  numScanLines = (ptr_bm->m_bitmap.bounds.bottom - ptr_bm->m_bitmap.bounds.top);
		long data_size =  tmpRowBytes *  numScanLines;
		ptr_bm->mf_bitData = MemoryAllocClear(m_globals, sizeof(pg_bits8_ptr), data_size, 0);
		if (ptr_bm->mf_bitData == MEM_NULL)
		{
			UnuseMemory(*mfptr_pbr_data);
			ptr_bm = NULL;
			return (NOSUCCESS);
		}
		
		 ptr_BMbits= (pg_bits8_ptr)UseMemory(ptr_bm->mf_bitData);


		if (RetrieveBMBitsRgn(ptr2_data_bytes, ptr_BMbits, ptr_bm, &(ptr_bm->dstRect), bBitsRect))
		{
			UnuseMemory(*mfptr_pbr_data);
			ptr_bm = NULL;
			return (NOSUCCESS);
		}
		else
		{
			UnuseMemory(ptr_bm->mf_bitData);
			ptr_BMbits=NULL;
			UnuseMemory(*mfptr_pbr_data);
			ptr_bm = NULL;

		}

	}

	return (SUCCESS);
}


// Retrieves parameters for opcode PackBitsRect
int RetrievePackBitsRectParams(pg_bits8_ptr PG_FAR * ptr2_data_bytes, memory_ref PG_FAR * mfptr_pbr_data)
{
	
	pmac_packBitsRect ptr_pbr = NULL;
	pmac_cb_pixmapData ptr_cb_pm = NULL;
	pmac_colorTable ptr_ct = NULL;

	if (*mfptr_pbr_data == MEM_NULL)
		return (NOSUCCESS);

	ptr_pbr = (pmac_packBitsRect)UseMemory(*mfptr_pbr_data);
	ptr_cb_pm = &(ptr_pbr->cb_pixmapData);

	if (ptr_cb_pm == NULL)
		return (NOSUCCESS);


	if (RetrieveMacCbPixmapData(ptr2_data_bytes, ptr_cb_pm))
	{
		UnuseMemory(*mfptr_pbr_data);
		ptr_pbr = NULL;
		return (NOSUCCESS);
	}

	if (ptr_cb_pm->std_pm_data.pixelType == 0)
	{
		ptr_pbr->mf_colorTable = MemoryAllocClear(m_globals, sizeof(mac_colorTable), 1, 0);
		if (ptr_pbr->mf_colorTable == MEM_NULL)
		{
			UnuseMemory(*mfptr_pbr_data);
			ptr_pbr = NULL;
			return (NOSUCCESS);
		}
		 
		ptr_ct = (pmac_colorTable)UseMemory(ptr_pbr->mf_colorTable);
		if (RetrieveMacColorTable(ptr2_data_bytes, ptr_ct))
		{
			UnuseMemory(ptr_pbr->mf_colorTable);
			ptr_ct = NULL;
			UnuseMemory(*mfptr_pbr_data);
			ptr_pbr = NULL;
			return (NOSUCCESS);
		}
		UnuseMemory(ptr_pbr->mf_colorTable);
		ptr_ct = NULL;
	}
	GetMacRectInc(ptr2_data_bytes, &(ptr_pbr->srcRect));
	GetMacRectInc(ptr2_data_bytes, &(ptr_pbr->dstRect));
	ptr_pbr->mode = GetMacWordInc(ptr2_data_bytes);

	// retrieve pixel bits.
	{
		pg_bits8_ptr ptr_pixbits = NULL;
		pmac_corePixData ptr_core_pix = &(ptr_pbr->cb_pixmapData.std_pm_data);

		short tmpRowBytes = (ptr_pbr->cb_pixmapData.std_pm_data.rowBytes & 0x7FFF);
		long  numScanLines = (ptr_core_pix->bounds.bottom - ptr_core_pix->bounds.top);
		long data_size =  tmpRowBytes *  numScanLines;
		if ((ptr_core_pix->packType == 2) || (ptr_core_pix->packType == 4))
		{
			data_size = (data_size * 3) / 4;
		}
		ptr_pbr->mf_PixData = MemoryAllocClear(m_globals, sizeof(pg_bits8_ptr), data_size, 0);
		if (ptr_pbr->mf_PixData == MEM_NULL)
		{
			UnuseMemory(*mfptr_pbr_data);
			ptr_pbr = NULL;
			return (NOSUCCESS);
		}
		
		ptr_pixbits = (pg_bits8_ptr)UseMemory(ptr_pbr->mf_PixData);


		if (RetrievePixelBits(ptr2_data_bytes, ptr_pixbits, ptr_core_pix, &(ptr_pbr->dstRect)))
		{
			UnuseMemory(*mfptr_pbr_data);
			ptr_pbr = NULL;
			return (NOSUCCESS);
		}
		else
		{
			UnuseMemory(ptr_pbr->mf_PixData);
			ptr_pixbits=NULL;
			UnuseMemory(*mfptr_pbr_data);
			ptr_pbr = NULL;

		}

	}

	return (SUCCESS);

}

int RetrievePackBitsRgnParams(pg_bits8_ptr PG_FAR * ptr2_data_bytes, memory_ref PG_FAR * mfptr_pbr_data)
{
	
	pmac_packBitsRgn ptr_pbr = NULL;
	pmac_cb_pixmapData ptr_cb_pm = NULL;
	pmac_colorTable ptr_ct = NULL;

	if (*mfptr_pbr_data == MEM_NULL)
		return (NOSUCCESS);

	ptr_pbr = (pmac_packBitsRgn)UseMemory(*mfptr_pbr_data);
	ptr_cb_pm = &(ptr_pbr->cb_pixmapData);

	if (ptr_cb_pm == NULL)
		return (NOSUCCESS);


	if (RetrieveMacCbPixmapData(ptr2_data_bytes, ptr_cb_pm))
	{
		UnuseMemory(*mfptr_pbr_data);
		ptr_pbr = NULL;
		return (NOSUCCESS);
	}

	if (ptr_cb_pm->std_pm_data.pixelType == 0)
	{
		ptr_pbr->mf_colorTable = MemoryAllocClear(m_globals, sizeof(mac_colorTable), 1, 0);
		if (ptr_pbr->mf_colorTable == MEM_NULL)
		{
			UnuseMemory(*mfptr_pbr_data);
			ptr_pbr = NULL;
			return (NOSUCCESS);
		}
		 
		ptr_ct = (pmac_colorTable)UseMemory(ptr_pbr->mf_colorTable);
		if (RetrieveMacColorTable(ptr2_data_bytes, ptr_ct))
		{
			UnuseMemory(ptr_pbr->mf_colorTable);
			ptr_ct = NULL;
			UnuseMemory(*mfptr_pbr_data);
			ptr_pbr = NULL;
			return (NOSUCCESS);
		}
		UnuseMemory(ptr_pbr->mf_colorTable);
		ptr_ct = NULL;
	}
	GetMacRectInc(ptr2_data_bytes, &(ptr_pbr->srcRect));
	GetMacRectInc(ptr2_data_bytes, &(ptr_pbr->dstRect));
	ptr_pbr->mode = GetMacWordInc(ptr2_data_bytes);        
	{
		mac_rgn tmprgn;
		GetMacRgnAndScaleRect(ptr2_data_bytes, &tmprgn, &(ptr_pbr->mask_rect), 1, 1);
 	}

	// retrieve pixel bits.
	{
		pg_bits8_ptr ptr_pixbits = NULL;
		pmac_corePixData ptr_core_pix = &(ptr_pbr->cb_pixmapData.std_pm_data);

		short tmpRowBytes = (ptr_pbr->cb_pixmapData.std_pm_data.rowBytes & 0x7FFF);
		long  numScanLines = (ptr_core_pix->bounds.bottom - ptr_core_pix->bounds.top);
		long data_size =  tmpRowBytes *  numScanLines;
		if ((ptr_core_pix->packType == 2) || (ptr_core_pix->packType == 4))
		{
			data_size = (data_size * 3) / 4;
		}
		ptr_pbr->mf_PixData = MemoryAllocClear(m_globals, sizeof(pg_bits8_ptr), data_size, 0);
		if (ptr_pbr->mf_PixData == MEM_NULL)
		{
			UnuseMemory(*mfptr_pbr_data);
			ptr_pbr = NULL;
			return (NOSUCCESS);
		}
		
		ptr_pixbits = (pg_bits8_ptr)UseMemory(ptr_pbr->mf_PixData);


		if (RetrievePixelBits(ptr2_data_bytes, ptr_pixbits, ptr_core_pix, &(ptr_pbr->dstRect)))
		{
			UnuseMemory(*mfptr_pbr_data);
			ptr_pbr = NULL;
			return (NOSUCCESS);
		}
		else
		{
			UnuseMemory(ptr_pbr->mf_PixData);
			ptr_pixbits=NULL;
			UnuseMemory(*mfptr_pbr_data);
			ptr_pbr = NULL;

		}

	}

	return (SUCCESS);

}


int PaintPieWedge(HDC hdc, mac_rect PG_FAR * mrect, POINT PG_FAR * rect_center, short startAngle, short angleRun, 
				  ppen_track pen_stat, pbrush_track brush_stat)
{
	double	semi_major_axis = 0, semi_minor_axis = 0;
	double	rad_start_angle = 0, rad_end_angle = 0;
	short	end_angle = 0;
	short	bClockWise = FALSE, bStartXPos = FALSE, bStartYPos = FALSE;
	short   bEndXPos = FALSE, bEndYPos = FALSE;
	POINT	start_arc = {0,0}, end_arc = {0,0};
	HPEN	hpen_tmp = NULL;
	HBRUSH	hbrush_tmp = NULL;
	short		old_pen_style = pen_stat->current_pen.lopnStyle;

	pen_stat->current_pen.lopnStyle = PS_NULL;
	if (CreateAndSelectPen(hdc, pen_stat) == NULL)
		return (NOSUCCESS);
	pen_stat->current_pen.lopnStyle = old_pen_style;


	if (CreateAndSelectBrush(hdc, brush_stat) == NULL)
		return (NOSUCCESS);

	RotateAngles(&startAngle, angleRun, &end_angle, &bClockWise);
	DetermineSigns(&bStartXPos, &bStartYPos, &bEndXPos, &bEndYPos, startAngle, end_angle);
	DegreesToRadians(startAngle, &rad_start_angle);
	DegreesToRadians(end_angle, &rad_end_angle);

	if (IsHorizontalEllipse(mrect, &semi_major_axis, &semi_minor_axis))
	{
		CalculateArcPoints(mrect, rect_center, semi_major_axis, semi_minor_axis, rad_start_angle, 
			rad_end_angle, &start_arc, &end_arc, TRUE, bStartXPos, bStartYPos, bEndXPos, bEndYPos);
		
	}
	else
	{
		CalculateArcPoints(mrect, rect_center, semi_major_axis, semi_minor_axis, rad_start_angle, 
			rad_end_angle, &start_arc, &end_arc, FALSE, bStartXPos, bStartYPos, bEndXPos, bEndYPos);
	}
	if (bClockWise)
		SwapPoints(&start_arc, &end_arc);
	

	{
		int bPieSucceeded = FALSE;

		bPieSucceeded = Pie(hdc, mrect->left, mrect->top, mrect->right, mrect->bottom, start_arc.x, start_arc.y, 
				end_arc.x, end_arc.y);
		if (CreateAndSelectPen(hdc, pen_stat) == NULL)
			return (NOSUCCESS);

		if (bPieSucceeded)
		{
			return(SUCCESS);
		}
		else
		{
			return(NOSUCCESS);
		}
	}
}



int DrawBMPackBitsRect(HDC hdc, memory_ref PG_FAR * mfptr_pbr_data, double XScaleFactor, double YScaleFactor, POINT PG_FAR * winExt)
{

	memory_ref scantable = MEM_NULL;
	pg_bits8_ptr lpvBits = NULL , ptr_mac_bits = NULL;
	pmac_packBitmapRect	ptr_bm = NULL;
	short tmpRowBytes = 0; 
	long  numScanLines = 0; 
	long data_size =  0;
	memory_ref mf_bmi = MEM_NULL;
	BITMAPINFO PG_FAR * bmi;
	RGBQUAD PG_FAR * rgbq_colors = NULL;
	UINT fuColorUse = DIB_RGB_COLORS;
	DWORD fdwROP = SRCCOPY;
	register long i = 0, j = 0, k = 0;
	int numEntriesInColorTable = 2;
	int scanlines_copied = 0; //debugging
	short bytes_padding = 0;
	memory_ref mf_new_pal = MEM_NULL;
	LOGPALETTE PG_FAR * pal = NULL;
	HPALETTE new_hpal = NULL, old_hpal = NULL;

	if (*mfptr_pbr_data == MEM_NULL)
		return (NOSUCCESS);

	ptr_bm = (pmac_packBitmapRect)UseMemory(*mfptr_pbr_data);
	ptr_mac_bits = (pg_bits8_ptr)UseMemory(ptr_bm->mf_bitData);
	tmpRowBytes = ptr_bm->m_bitmap.rowBytes;
	numScanLines = ptr_bm->m_bitmap.bounds.bottom - ptr_bm->m_bitmap.bounds.top;

	while ((tmpRowBytes + bytes_padding) % 4)
	{
		bytes_padding++;
	}

	data_size = numScanLines * (tmpRowBytes + bytes_padding);

	scantable = MemoryAlloc(m_globals, sizeof(pg_bits8), data_size, 0);
	if (scantable == MEM_NULL)
		return (NOSUCCESS);

	lpvBits = (pg_bits8_ptr)UseMemory(scantable);


	for (i = 0, j = numScanLines - 1; i < numScanLines; i++, j--)
	{
		long position = 0;
		long padded_rowBytes = tmpRowBytes + bytes_padding;
		memcpy(&(lpvBits[j * padded_rowBytes]), &(ptr_mac_bits[i * tmpRowBytes]), tmpRowBytes);
		position = (j * padded_rowBytes) + tmpRowBytes;
		for (k = position; k < position + bytes_padding; k++)
		{
			lpvBits[k] = 0;
		}

	}


	mf_bmi = MemoryAlloc(m_globals, (pg_short_t)(sizeof(BITMAPINFO) + (sizeof(RGBQUAD) * (numEntriesInColorTable - 1))), 1, 0);

	if (mf_bmi == MEM_NULL)
	{
		return (NOSUCCESS);
	}
	
	bmi = (BITMAPINFO PG_FAR *)UseMemory(mf_bmi);

	bmi->bmiColors[1].rgbBlue = 0;
	bmi->bmiColors[1].rgbRed =  0;
	bmi->bmiColors[1].rgbGreen =  0;
	bmi->bmiColors[1].rgbReserved =  0;

	bmi->bmiColors[0].rgbBlue = 0xff;
	bmi->bmiColors[0].rgbRed =  0xff;
	bmi->bmiColors[0].rgbGreen =  0xff;
	bmi->bmiColors[0].rgbReserved =  0;



	mf_new_pal = MemoryAlloc(m_globals,(pg_short_t)((numEntriesInColorTable - 1) * sizeof(PALETTEENTRY) + sizeof(LOGPALETTE)), 1, 0);
	if (mf_new_pal == MEM_NULL)
		return (NOSUCCESS);

	pal = (LPLOGPALETTE) UseMemory(mf_new_pal);

	pal->palVersion = 0x0300;
	pal->palNumEntries = numEntriesInColorTable;
	for (i = 0; i < numEntriesInColorTable; i++)
	{
		pal->palPalEntry[i].peRed = bmi->bmiColors[i].rgbRed;
		pal->palPalEntry[i].peBlue = bmi->bmiColors[i].rgbBlue;
		pal->palPalEntry[i].peGreen = bmi->bmiColors[i].rgbGreen;
		pal->palPalEntry[i].peFlags = 0;
	}
	new_hpal = CreatePalette(pal);
	if (new_hpal == NULL)
		return (NOSUCCESS);

	old_hpal = SelectPalette(hdc, new_hpal, FALSE);
	RealizePalette(hdc);
	bmi->bmiHeader.biSize = 40;

	{
		int pixels_per_byte =  8;
		bmi->bmiHeader.biWidth = (long)(ptr_bm->m_bitmap.bounds.right - ptr_bm->m_bitmap.bounds.left);
	}
	
	bmi->bmiHeader.biHeight = numScanLines;
	bmi->bmiHeader.biPlanes = 1;
	bmi->bmiHeader.biBitCount = 1;
	bmi->bmiHeader.biCompression = BI_RGB;
	bmi->bmiHeader.biSizeImage = 0;
	bmi->bmiHeader.biXPelsPerMeter = 0;
	bmi->bmiHeader.biYPelsPerMeter = 0;
	bmi->bmiHeader.biClrUsed = numEntriesInColorTable;
	bmi->bmiHeader.biClrImportant = numEntriesInColorTable;

	MyScaleRect(&(ptr_bm->dstRect), XScaleFactor, YScaleFactor);
	{

		int srcExtentX = 0, srcExtentY = 0; 
		int dstExtentX = 0, dstExtentY = 0;   
        
 		dstExtentX = abs(ptr_bm->dstRect.right - ptr_bm->dstRect.left);
		dstExtentY = abs(ptr_bm->dstRect.bottom - ptr_bm->dstRect.top);

		srcExtentY = abs(ptr_bm->srcRect.bottom - ptr_bm->srcRect.top);
		srcExtentX = abs(ptr_bm->srcRect.right - ptr_bm->srcRect.left);

        if (dstExtentX > winExt->x)
			dstExtentX = winExt->x;
		if (dstExtentY > winExt->y)
			dstExtentY = winExt->y;

 
		if (ptr_bm->mode == 1)
		{
			fdwROP = SRCAND; 
		}
		
		SetStretchBltMode(hdc, STRETCH_ANDSCANS);
		scanlines_copied = StretchDIBits(hdc, ptr_bm->dstRect.left, ptr_bm->dstRect.top, dstExtentX, 
			dstExtentY, 0, 0, srcExtentX, srcExtentY, lpvBits, bmi, DIB_RGB_COLORS, fdwROP); 

	}

	SelectPalette(hdc, old_hpal, FALSE);
	RealizePalette(hdc);
	DeleteObject(new_hpal);
	new_hpal = NULL;
	DeleteObject(old_hpal);
	old_hpal = NULL;
	UnuseMemory(mf_new_pal);
	pal = NULL;
	DisposeMemory(mf_new_pal);
	mf_new_pal = MEM_NULL;

	UnuseMemory(mf_bmi);
	bmi = NULL;

	DisposeMemory(mf_bmi);
	mf_bmi = MEM_NULL;

	UnuseMemory(ptr_bm->mf_bitData);
	ptr_mac_bits = NULL;


	UnuseMemory(*mfptr_pbr_data);
	ptr_bm = NULL;

	UnuseMemory(scantable);
	lpvBits = NULL;

	DisposeMemory(scantable);
	scantable = MEM_NULL;

	return (SUCCESS);
}

 int DrawBMPackBitsRgn(HDC hdc, memory_ref PG_FAR * mfptr_pbr_data, double XScaleFactor, double YScaleFactor, POINT PG_FAR * winExt)
{

	memory_ref scantable = MEM_NULL;
	pg_bits8_ptr lpvBits = NULL , ptr_mac_bits = NULL;
	pmac_packBitmapRgn	ptr_bm = NULL;
	short tmpRowBytes = 0; 
	long  numScanLines = 0; 
	long data_size =  0;
	memory_ref mf_bmi = MEM_NULL;
	BITMAPINFO PG_FAR * bmi;
	RGBQUAD PG_FAR * rgbq_colors = NULL;
	UINT fuColorUse = DIB_RGB_COLORS;
	DWORD fdwROP = SRCCOPY;
	register long i = 0, j = 0, k = 0;
	int numEntriesInColorTable = 2;
	int scanlines_copied = 0; //debugging
	short bytes_padding = 0;
	memory_ref mf_new_pal = MEM_NULL;
	LOGPALETTE PG_FAR * pal = NULL;
	HPALETTE new_hpal = NULL, old_hpal = NULL;

	if (*mfptr_pbr_data == MEM_NULL)
		return (NOSUCCESS);

	ptr_bm = (pmac_packBitmapRgn)UseMemory(*mfptr_pbr_data);
	ptr_mac_bits = (pg_bits8_ptr)UseMemory(ptr_bm->mf_bitData);
	tmpRowBytes = ptr_bm->m_bitmap.rowBytes;
	numScanLines = ptr_bm->m_bitmap.bounds.bottom - ptr_bm->m_bitmap.bounds.top;

	while ((tmpRowBytes + bytes_padding) % 4)
	{
		bytes_padding++;
	}

	data_size = numScanLines * (tmpRowBytes + bytes_padding);

	scantable = MemoryAlloc(m_globals, sizeof(pg_bits8), data_size, 0);
	if (scantable == MEM_NULL)
		return (NOSUCCESS);

	lpvBits = (pg_bits8_ptr)UseMemory(scantable);


	for (i = 0, j = numScanLines - 1; i < numScanLines; i++, j--)
	{
		long position = 0;
		long padded_rowBytes = tmpRowBytes + bytes_padding;
		memcpy(&(lpvBits[j * padded_rowBytes]), &(ptr_mac_bits[i * tmpRowBytes]), tmpRowBytes);
		position = (j * padded_rowBytes) + tmpRowBytes;
		for (k = position; k < position + bytes_padding; k++)
		{
			lpvBits[k] = 0;
		}

	}


	mf_bmi = MemoryAlloc(m_globals, (pg_short_t)(sizeof(BITMAPINFO) + (sizeof(RGBQUAD) * (numEntriesInColorTable - 1))), 1, 0);

	if (mf_bmi == MEM_NULL)
	{
		return (NOSUCCESS);
	}
	
	bmi = (BITMAPINFO PG_FAR *)UseMemory(mf_bmi);

	bmi->bmiColors[1].rgbBlue = 0;
	bmi->bmiColors[1].rgbRed =  0;
	bmi->bmiColors[1].rgbGreen =  0;
	bmi->bmiColors[1].rgbReserved =  0;

	bmi->bmiColors[0].rgbBlue = 0xff;
	bmi->bmiColors[0].rgbRed =  0xff;
	bmi->bmiColors[0].rgbGreen =  0xff;
	bmi->bmiColors[0].rgbReserved =  0;



	mf_new_pal = MemoryAlloc(m_globals,(pg_short_t)((numEntriesInColorTable - 1) * sizeof(PALETTEENTRY) + sizeof(LOGPALETTE)), 1, 0);
	if (mf_new_pal == MEM_NULL)
		return (NOSUCCESS);

	pal = (LPLOGPALETTE) UseMemory(mf_new_pal);

	pal->palVersion = 0x0300;
	pal->palNumEntries = numEntriesInColorTable;
	for (i = 0; i < numEntriesInColorTable; i++)
	{
		pal->palPalEntry[i].peRed = bmi->bmiColors[i].rgbRed;
		pal->palPalEntry[i].peBlue = bmi->bmiColors[i].rgbBlue;
		pal->palPalEntry[i].peGreen = bmi->bmiColors[i].rgbGreen;
		pal->palPalEntry[i].peFlags = 0;
	}
	new_hpal = CreatePalette(pal);
	if (new_hpal == NULL)
		return (NOSUCCESS);

	old_hpal = SelectPalette(hdc, new_hpal, FALSE);
	RealizePalette(hdc);
	bmi->bmiHeader.biSize = 40;

	{
		int pixels_per_byte =  8;
		bmi->bmiHeader.biWidth = (long)(ptr_bm->m_bitmap.bounds.right - ptr_bm->m_bitmap.bounds.left);
	}
	
	bmi->bmiHeader.biHeight = numScanLines;
	bmi->bmiHeader.biPlanes = 1;
	bmi->bmiHeader.biBitCount = 1;
	bmi->bmiHeader.biCompression = BI_RGB;
	bmi->bmiHeader.biSizeImage = 0;
	bmi->bmiHeader.biXPelsPerMeter = 0;
	bmi->bmiHeader.biYPelsPerMeter = 0;
	bmi->bmiHeader.biClrUsed = numEntriesInColorTable;
	bmi->bmiHeader.biClrImportant = numEntriesInColorTable;

	MyScaleRect(&(ptr_bm->dstRect), XScaleFactor, YScaleFactor);
	{

		int srcExtentX = 0, srcExtentY = 0; 
		int dstExtentX = 0, dstExtentY = 0;
		
		dstExtentX = abs(ptr_bm->dstRect.right - ptr_bm->dstRect.left);
		dstExtentY = abs(ptr_bm->dstRect.bottom - ptr_bm->dstRect.top);

		srcExtentY = abs(ptr_bm->srcRect.bottom - ptr_bm->srcRect.top);
		srcExtentX = abs(ptr_bm->srcRect.right - ptr_bm->srcRect.left);
        
        if (dstExtentX > winExt->x)
			dstExtentX = winExt->x;
		if (dstExtentY > winExt->y)
			dstExtentY = winExt->y;
                                       
  
            
		if (ptr_bm->mode == 1)
			fdwROP = SRCAND;                                                               
		SetStretchBltMode(hdc, STRETCH_ANDSCANS);
		scanlines_copied = StretchDIBits(hdc, ptr_bm->dstRect.left, ptr_bm->dstRect.top, dstExtentX, 
			dstExtentY, 0, 0, srcExtentX, srcExtentY, lpvBits, bmi, DIB_RGB_COLORS, fdwROP);
	}

	SelectPalette(hdc, old_hpal, FALSE);
	RealizePalette(hdc);
	DeleteObject(new_hpal);
	new_hpal = NULL;
	DeleteObject(old_hpal);
	old_hpal = NULL;
	UnuseMemory(mf_new_pal);
	pal = NULL;
	DisposeMemory(mf_new_pal);
	mf_new_pal = MEM_NULL;

	UnuseMemory(mf_bmi);
	bmi = NULL;

	DisposeMemory(mf_bmi);
	mf_bmi = MEM_NULL;

	UnuseMemory(ptr_bm->mf_bitData);
	ptr_mac_bits = NULL;


	UnuseMemory(*mfptr_pbr_data);
	ptr_bm = NULL;

	UnuseMemory(scantable);
	lpvBits = NULL;

	DisposeMemory(scantable);
	scantable = MEM_NULL;

	return (SUCCESS);
}


int Expand2to4bits(pg_bits8_ptr ptr_Dest, pg_bits8_ptr ptr_Src, long numScanLines, 
				   long dstBytesPadding, long srcRowBytes)
{
	register long i = 0, j = 0, k = 0;
	long dstRowWidthBytes = srcRowBytes * 2 + dstBytesPadding;
	
	for (i = numScanLines - 1, j = 0; j < numScanLines; i--, j++)
	{
	pg_bits8 current_byte = 0, tmpByte = 0;
		pg_bits8_ptr ptrSrcLine = ptr_Src + (i * srcRowBytes);
		pg_bits8_ptr ptrDstLine = ptr_Dest + (j * dstRowWidthBytes);
		for (k = 0; k < srcRowBytes * 2; k += 2)
		{
			current_byte = GetByteInc(&ptrSrcLine);
			ptrDstLine[k] = (current_byte & 0xc0) >> 2;
			ptrDstLine[k] |= (current_byte & 0x30) >> 4;
			ptrDstLine[k+1] = (current_byte & 0x0c) << 2;
			ptrDstLine[k+1] |= (current_byte & 0x03);
		}
		for (; k < dstRowWidthBytes; k++)
		{
			ptrDstLine[k] = (pg_bits8)0;
		}

	}

	return(SUCCESS);
}




int Expand16to24Bits(pg_bits8_ptr ptr_Dest, pg_bits8_ptr ptr_Src, long numScanLines, 
					 long dstBytesPadding, long srcRowBytes)
{
	long i = 0, j = 0;
	register long k = 0,  l = 0 ;
	long numColumns = srcRowBytes / 2;
	long dstRowWidthBytes = numColumns * 3 + dstBytesPadding;

	for (i = numScanLines - 1, j = 0; j < numScanLines; i--, j++)
	{
		pg_word current_pixel = 0;
		pg_bits8 tmpByte = 0;
		pg_bits8_ptr tmpBytePtr = NULL;
		pg_bits8_ptr ptrSrcLine = ptr_Src + (i * srcRowBytes);
		RGBTRIPLE PG_FAR * ptrDstLine = (RGBTRIPLE PG_FAR *)(ptr_Dest + (j * dstRowWidthBytes));
		for (k = 0; k < numColumns; k++)
		{
			current_pixel = GetMacWordInc(&ptrSrcLine);

			tmpByte =  ptrDstLine[k].rgbtRed = (pg_bits8)((current_pixel & PIX_16_RED) >> 7);
			ptrDstLine[k].rgbtRed |= ((tmpByte & 0xe0) >> 5);

			tmpByte = ptrDstLine[k].rgbtGreen = (pg_bits8)((current_pixel & PIX_16_GREEN) >> 2);
			ptrDstLine[k].rgbtGreen |= ((tmpByte & 0xe0) >> 5);

			tmpByte = ptrDstLine[k].rgbtBlue = (pg_bits8)((current_pixel & PIX_16_BLUE) << 3);
			ptrDstLine[k].rgbtBlue |= ((tmpByte & 0xe0) >> 5);
		}
		tmpBytePtr = (pg_bits8_ptr)(ptrDstLine + k);
		for (l = 0; l < dstBytesPadding; l++)
		{
			*(tmpBytePtr + l) = 0; 
		}


	}
	return(SUCCESS);
}


// need to carefully delete mf_pbr_data as there are memory_refs within memory_refs.
int DrawPackBitsRect(HDC hdc, memory_ref PG_FAR * mf_pbr_data, double XScaleFactor, double YScaleFactor, POINT PG_FAR * winExt)
{
	memory_ref scantable = MEM_NULL;
	pg_bits8_ptr lpvBits = NULL , ptr_mac_bits = NULL;
	pmac_packBitsRect ptr_pbr_data = NULL;
	pmac_colorTable  ptr_mac_ct = NULL;
	short tmpRowBytes = 0; 
	long  numScanLines = 0; 
	long data_size =  0; 
	memory_ref mf_bmi = MEM_NULL;
	BITMAPINFO PG_FAR * bmi;
	RGBQUAD PG_FAR * rgbq_colors = NULL;
	UINT fuColorUse = DIB_RGB_COLORS;
	DWORD fdwROP = SRCCOPY;
	register long i = 0, j = 0, k = 0;
	int numEntriesInColorTable = 0;
	pmac_colorSpec ptr_cs = NULL;
	int scanlines_copied = 0; //debugging
	short bytes_padding = 0;
	memory_ref mf_new_pal = MEM_NULL;
	LOGPALETTE PG_FAR * pal = NULL;
	HPALETTE new_hpal = NULL, old_hpal = NULL;
	

	ptr_pbr_data = (pmac_packBitsRect)UseMemory(*mf_pbr_data);
	tmpRowBytes = (ptr_pbr_data->cb_pixmapData.std_pm_data.rowBytes & 0x7FFF);
	if (ptr_pbr_data->cb_pixmapData.std_pm_data.pixelSize == 0x0020)
	{
		tmpRowBytes = (tmpRowBytes * 3) / 4;
	}
	else if (ptr_pbr_data->cb_pixmapData.std_pm_data.pixelSize == 0x0010)
	{
		tmpRowBytes = (tmpRowBytes / 2) * 3;
	}
	else if (ptr_pbr_data->cb_pixmapData.std_pm_data.pixelSize == 0x0002)
	{
		tmpRowBytes = tmpRowBytes * 2;
	}

	numScanLines = (ptr_pbr_data->cb_pixmapData.std_pm_data.bounds.bottom - ptr_pbr_data->cb_pixmapData.std_pm_data.bounds.top);
	while ((tmpRowBytes + bytes_padding) % 4)
	{
		bytes_padding++;
	}
	data_size = numScanLines * (tmpRowBytes + bytes_padding);

	ptr_mac_bits = (pg_bits8_ptr)UseMemory(ptr_pbr_data->mf_PixData);
	scantable = MemoryAlloc(m_globals, sizeof(pg_bits8), data_size, 0);
	if (scantable == MEM_NULL)
		return (NOSUCCESS);

	lpvBits = (pg_bits8_ptr)UseMemory(scantable);

	if (ptr_pbr_data->cb_pixmapData.std_pm_data.pixelSize == 0x0010)
	{
		long OriginalRowBytes = (ptr_pbr_data->cb_pixmapData.std_pm_data.rowBytes & 0x7FFF);

		if (Expand16to24Bits(lpvBits, ptr_mac_bits, numScanLines, bytes_padding, OriginalRowBytes))
			return (NOSUCCESS);

	}
	else if (ptr_pbr_data->cb_pixmapData.std_pm_data.pixelSize == 0x0002)
	{
		long OriginalRowBytes = (ptr_pbr_data->cb_pixmapData.std_pm_data.rowBytes & 0x7FFF);
		if (Expand2to4bits(lpvBits, ptr_mac_bits, numScanLines, bytes_padding, OriginalRowBytes))
			return (NOSUCCESS);

	}
	else
	{

		for (i = 0, j = numScanLines - 1; i < numScanLines; i++, j--)
		{
			long position = 0;
			long padded_rowBytes = tmpRowBytes + bytes_padding;
			memcpy(&(lpvBits[j * padded_rowBytes]), &(ptr_mac_bits[i * tmpRowBytes]), tmpRowBytes);
			position = (j * padded_rowBytes) + tmpRowBytes;
			for (k = position; k < position + bytes_padding; k++)
			{
				lpvBits[k] = 0;
			}

		}
	}

	if ((ptr_pbr_data->cb_pixmapData.std_pm_data.pixelSize != 32) &&
		(ptr_pbr_data->cb_pixmapData.std_pm_data.pixelSize != 16))
	{
		ptr_mac_ct = (pmac_colorTable)UseMemory(ptr_pbr_data->mf_colorTable);
		ptr_cs = (pmac_colorSpec)UseMemory(ptr_mac_ct->cSpecArray);
		numEntriesInColorTable = ptr_mac_ct->ctSize + 1;
	
		mf_bmi = MemoryAlloc(m_globals, (pg_short_t)(sizeof(BITMAPINFO) + (sizeof(RGBQUAD) * (numEntriesInColorTable - 1))), 1, 0);
	
	}
	else
	{
		mf_bmi = MemoryAlloc(m_globals, (pg_short_t)(sizeof(BITMAPINFO)), 1,0);
	}

	if ((mf_bmi == MEM_NULL) && ((ptr_pbr_data->cb_pixmapData.std_pm_data.pixelSize != 32) &&
		(ptr_pbr_data->cb_pixmapData.std_pm_data.pixelSize != 16)))
	{
		
		UnuseMemory(ptr_mac_ct->cSpecArray);
		ptr_cs = NULL;
		UnuseMemory(ptr_pbr_data->mf_colorTable);
		ptr_mac_ct = NULL;

		
		return (NOSUCCESS);
	}
	
	bmi = (BITMAPINFO PG_FAR *)UseMemory(mf_bmi);


	if ((ptr_pbr_data->cb_pixmapData.std_pm_data.pixelSize != 32) &&
		(ptr_pbr_data->cb_pixmapData.std_pm_data.pixelSize != 16))
	{
		for (i = 0; i < numEntriesInColorTable; i ++)
		{
			bmi->bmiColors[i].rgbBlue = (BYTE)((ptr_cs[i].mac_rgb.blue & 0xff00) >> 8);
			bmi->bmiColors[i].rgbRed =  (BYTE)((ptr_cs[i].mac_rgb.red & 0xff00) >> 8);
			bmi->bmiColors[i].rgbGreen =  (BYTE)((ptr_cs[i].mac_rgb.green & 0xff00) >> 8);
			bmi->bmiColors[i].rgbReserved =  0;

		}
		mf_new_pal = MemoryAlloc(m_globals,(pg_short_t)((numEntriesInColorTable - 1) * sizeof(PALETTEENTRY) + sizeof(LOGPALETTE)), 1, 0);
		if (mf_new_pal == MEM_NULL)
			return (NOSUCCESS);

		pal = (LPLOGPALETTE) UseMemory(mf_new_pal);

		pal->palVersion = 0x0300;
		pal->palNumEntries = numEntriesInColorTable;
		for (i = 0; i < numEntriesInColorTable; i++)
		{
			pal->palPalEntry[i].peRed = bmi->bmiColors[i].rgbRed;
			pal->palPalEntry[i].peBlue = bmi->bmiColors[i].rgbBlue;
			pal->palPalEntry[i].peGreen = bmi->bmiColors[i].rgbGreen;
			pal->palPalEntry[i].peFlags = 0;
		}
	}
	else
	{
		pg_bits8 red = 0, green = 0, blue = 0;
		int num_colors = 256;

		//create a spectrum palette
		mf_new_pal = MemoryAlloc(m_globals,(pg_short_t)((num_colors - 1) * sizeof(PALETTEENTRY) + sizeof(LOGPALETTE)), 1, 0);
		if (mf_new_pal == MEM_NULL)
			return (NOSUCCESS);

		pal = (LPLOGPALETTE) UseMemory(mf_new_pal);

		pal->palVersion = 0x0300;
		pal->palNumEntries = num_colors;
		for (i = 0; i < num_colors; i++)
		{
			pal->palPalEntry[i].peRed = red;
			pal->palPalEntry[i].peBlue = blue;
			pal->palPalEntry[i].peGreen = green;
			pal->palPalEntry[i].peFlags = 0;

			if (!(red += 32))
				if (!(green += 32))
					blue += 64;
		}
	}

	new_hpal = CreatePalette(pal);
	if (new_hpal == NULL)
		return (NOSUCCESS);

	old_hpal = SelectPalette(hdc, new_hpal, FALSE);
	RealizePalette(hdc);


	bmi->bmiHeader.biSize = 40;

	bmi->bmiHeader.biWidth = (long)(ptr_pbr_data->cb_pixmapData.std_pm_data.bounds.right - ptr_pbr_data->cb_pixmapData.std_pm_data.bounds.left);

	bmi->bmiHeader.biHeight = numScanLines;
	bmi->bmiHeader.biPlanes = 1;
	if ((ptr_pbr_data->cb_pixmapData.std_pm_data.pixelSize != 32) &&
		(ptr_pbr_data->cb_pixmapData.std_pm_data.pixelSize != 16))
	{
		bmi->bmiHeader.biBitCount = ptr_pbr_data->cb_pixmapData.std_pm_data.pixelSize;
		if (bmi->bmiHeader.biBitCount == 2)
			bmi->bmiHeader.biBitCount = 4;
	}
	else
	{
		bmi->bmiHeader.biBitCount = 24;

	}
	bmi->bmiHeader.biCompression = BI_RGB;
	bmi->bmiHeader.biSizeImage = 0;
	bmi->bmiHeader.biXPelsPerMeter = 0;
	bmi->bmiHeader.biYPelsPerMeter = 0;

	if ((ptr_pbr_data->cb_pixmapData.std_pm_data.pixelSize != 32) &&
		(ptr_pbr_data->cb_pixmapData.std_pm_data.pixelSize != 16))
	{
		bmi->bmiHeader.biClrUsed = numEntriesInColorTable;
		bmi->bmiHeader.biClrImportant = numEntriesInColorTable;
	}
	else
	{
		bmi->bmiHeader.biClrUsed = 0;
		bmi->bmiHeader.biClrImportant = 0;
	}

	MyScaleRect(&(ptr_pbr_data->dstRect), XScaleFactor, YScaleFactor);
	{
		int srcExtentX = 0, srcExtentY = 0;
		int dstExtentX = 0, dstExtentY = 0;

		dstExtentX = abs(ptr_pbr_data->dstRect.right - ptr_pbr_data->dstRect.left);
		dstExtentY = abs(ptr_pbr_data->dstRect.bottom - ptr_pbr_data->dstRect.top);
		srcExtentY = abs(ptr_pbr_data->srcRect.bottom - ptr_pbr_data->srcRect.top);
		srcExtentX = abs(ptr_pbr_data->srcRect.right - ptr_pbr_data->srcRect.left);
          
        if (dstExtentX > winExt->x)
			dstExtentX = winExt->x;
		if (dstExtentY > winExt->y)
			dstExtentY = winExt->y;
                                       
		if (ptr_pbr_data->mode == 1)
			fdwROP = SRCAND;

		if ((numEntriesInColorTable > 2) || (bmi->bmiHeader.biBitCount > 8))
			SetStretchBltMode(hdc, STRETCH_DELETESCANS); 
		else
			SetStretchBltMode(hdc, STRETCH_ANDSCANS);
		
		scanlines_copied = StretchDIBits(hdc, ptr_pbr_data->dstRect.left, ptr_pbr_data->dstRect.top, dstExtentX, 
			dstExtentY, 0, 0,  srcExtentX, srcExtentY, lpvBits, bmi, DIB_RGB_COLORS, fdwROP);
	}

	SelectPalette(hdc, old_hpal, FALSE);
	RealizePalette(hdc);
	DeleteObject(new_hpal);
	new_hpal = NULL;
	DeleteObject(old_hpal);
	old_hpal = NULL;
	UnuseMemory(mf_new_pal);
	pal = NULL;
	DisposeMemory(mf_new_pal);
	mf_new_pal = MEM_NULL;
	

	UnuseMemory(mf_bmi);
	bmi = NULL;

	DisposeMemory(mf_bmi);
	mf_bmi = MEM_NULL;

	if ((ptr_pbr_data->cb_pixmapData.std_pm_data.pixelSize != 32) && 
		(ptr_pbr_data->cb_pixmapData.std_pm_data.pixelSize != 16))
	{
		UnuseMemory(ptr_mac_ct->cSpecArray);
		ptr_cs = NULL;

		UnuseMemory(ptr_pbr_data->mf_colorTable);
		ptr_mac_ct = NULL;
	}

	UnuseMemory(ptr_pbr_data->mf_PixData);
	ptr_mac_bits = NULL;


	UnuseMemory(*mf_pbr_data);
	ptr_pbr_data = NULL;

	UnuseMemory(scantable);
	lpvBits = NULL;

	DisposeMemory(scantable);
	scantable = MEM_NULL;

	return (SUCCESS);

}
          
 int DrawPackBitsRgn(HDC hdc, memory_ref PG_FAR * mf_pbr_data, double XScaleFactor, double YScaleFactor, POINT PG_FAR * winExt)
{
	memory_ref scantable = MEM_NULL;
	pg_bits8_ptr lpvBits = NULL , ptr_mac_bits = NULL;
	pmac_packBitsRgn ptr_pbr_data = NULL;
	pmac_colorTable  ptr_mac_ct = NULL;
	short tmpRowBytes = 0; 
	long  numScanLines = 0; 
	long data_size =  0; 
	memory_ref mf_bmi = MEM_NULL;
	BITMAPINFO PG_FAR * bmi;
	RGBQUAD PG_FAR * rgbq_colors = NULL;
	UINT fuColorUse = DIB_RGB_COLORS;
	DWORD fdwROP = SRCCOPY;
	register long i = 0, j = 0, k = 0;
	int numEntriesInColorTable = 0;
	pmac_colorSpec ptr_cs = NULL;
	int scanlines_copied = 0; //debugging
	short bytes_padding = 0;
	memory_ref mf_new_pal = MEM_NULL;
	LOGPALETTE PG_FAR * pal = NULL;
	HPALETTE new_hpal = NULL, old_hpal = NULL;
	

	ptr_pbr_data = (pmac_packBitsRgn)UseMemory(*mf_pbr_data);
	tmpRowBytes = (ptr_pbr_data->cb_pixmapData.std_pm_data.rowBytes & 0x7FFF);
	if (ptr_pbr_data->cb_pixmapData.std_pm_data.pixelSize == 0x0020)
	{
		tmpRowBytes = (tmpRowBytes * 3) / 4;
	}
	else if (ptr_pbr_data->cb_pixmapData.std_pm_data.pixelSize == 0x0010)
	{
		tmpRowBytes = (tmpRowBytes / 2) * 3;
	}
	else if (ptr_pbr_data->cb_pixmapData.std_pm_data.pixelSize == 0x0002)
	{
		tmpRowBytes = tmpRowBytes * 2;
	}

	numScanLines = (ptr_pbr_data->cb_pixmapData.std_pm_data.bounds.bottom - ptr_pbr_data->cb_pixmapData.std_pm_data.bounds.top);
	while ((tmpRowBytes + bytes_padding) % 4)
	{
		bytes_padding++;
	}
	data_size = numScanLines * (tmpRowBytes + bytes_padding);

	ptr_mac_bits = (pg_bits8_ptr)UseMemory(ptr_pbr_data->mf_PixData);
	scantable = MemoryAlloc(m_globals, sizeof(pg_bits8), data_size, 0);
	if (scantable == MEM_NULL)
		return (NOSUCCESS);

	lpvBits = (pg_bits8_ptr)UseMemory(scantable);

	if (ptr_pbr_data->cb_pixmapData.std_pm_data.pixelSize == 0x0010)
	{
		long OriginalRowBytes = (ptr_pbr_data->cb_pixmapData.std_pm_data.rowBytes & 0x7FFF);

		if (Expand16to24Bits(lpvBits, ptr_mac_bits, numScanLines, bytes_padding, OriginalRowBytes))
			return (NOSUCCESS);

	}
	else if (ptr_pbr_data->cb_pixmapData.std_pm_data.pixelSize == 0x0002)
	{
		long OriginalRowBytes = (ptr_pbr_data->cb_pixmapData.std_pm_data.rowBytes & 0x7FFF);
		if (Expand2to4bits(lpvBits, ptr_mac_bits, numScanLines, bytes_padding, OriginalRowBytes))
			return (NOSUCCESS);

	}
	else
	{

		for (i = 0, j = numScanLines - 1; i < numScanLines; i++, j--)
		{
			long position = 0;
			long padded_rowBytes = tmpRowBytes + bytes_padding;
			memcpy(&(lpvBits[j * padded_rowBytes]), &(ptr_mac_bits[i * tmpRowBytes]), tmpRowBytes);
			position = (j * padded_rowBytes) + tmpRowBytes;
			for (k = position; k < position + bytes_padding; k++)
			{
				lpvBits[k] = 0;
			}

		}
	}

	if ((ptr_pbr_data->cb_pixmapData.std_pm_data.pixelSize != 32) &&
		(ptr_pbr_data->cb_pixmapData.std_pm_data.pixelSize != 16))
	{
		ptr_mac_ct = (pmac_colorTable)UseMemory(ptr_pbr_data->mf_colorTable);
		ptr_cs = (pmac_colorSpec)UseMemory(ptr_mac_ct->cSpecArray);
		numEntriesInColorTable = ptr_mac_ct->ctSize + 1;
	
		mf_bmi = MemoryAlloc(m_globals, (pg_short_t)(sizeof(BITMAPINFO) + (sizeof(RGBQUAD) * (numEntriesInColorTable - 1))), 1, 0);
	
	}
	else
	{
		mf_bmi = MemoryAlloc(m_globals, (pg_short_t)(sizeof(BITMAPINFO)), 1,0);
	}

	if ((mf_bmi == MEM_NULL) && ((ptr_pbr_data->cb_pixmapData.std_pm_data.pixelSize != 32) &&
		(ptr_pbr_data->cb_pixmapData.std_pm_data.pixelSize != 16)))
	{
		
		UnuseMemory(ptr_mac_ct->cSpecArray);
		ptr_cs = NULL;
		UnuseMemory(ptr_pbr_data->mf_colorTable);
		ptr_mac_ct = NULL;

		
		return (NOSUCCESS);
	}
	
	bmi = (BITMAPINFO PG_FAR *)UseMemory(mf_bmi);


	if ((ptr_pbr_data->cb_pixmapData.std_pm_data.pixelSize != 32) &&
		(ptr_pbr_data->cb_pixmapData.std_pm_data.pixelSize != 16))
	{
		for (i = 0; i < numEntriesInColorTable; i ++)
		{
			bmi->bmiColors[i].rgbBlue = (BYTE)((ptr_cs[i].mac_rgb.blue & 0xff00) >> 8);
			bmi->bmiColors[i].rgbRed =  (BYTE)((ptr_cs[i].mac_rgb.red & 0xff00) >> 8);
			bmi->bmiColors[i].rgbGreen =  (BYTE)((ptr_cs[i].mac_rgb.green & 0xff00) >> 8);
			bmi->bmiColors[i].rgbReserved =  0;

		}
		mf_new_pal = MemoryAlloc(m_globals,(pg_short_t)((numEntriesInColorTable - 1) * sizeof(PALETTEENTRY) + sizeof(LOGPALETTE)), 1, 0);
		if (mf_new_pal == MEM_NULL)
			return (NOSUCCESS);

		pal = (LPLOGPALETTE) UseMemory(mf_new_pal);

		pal->palVersion = 0x0300;
		pal->palNumEntries = numEntriesInColorTable;
		for (i = 0; i < numEntriesInColorTable; i++)
		{
			pal->palPalEntry[i].peRed = bmi->bmiColors[i].rgbRed;
			pal->palPalEntry[i].peBlue = bmi->bmiColors[i].rgbBlue;
			pal->palPalEntry[i].peGreen = bmi->bmiColors[i].rgbGreen;
			pal->palPalEntry[i].peFlags = 0;
		}
	}
	else
	{
		pg_bits8 red = 0, green = 0, blue = 0;
		int num_colors = 256;

		//create a spectrum palette
		mf_new_pal = MemoryAlloc(m_globals,(pg_short_t)((num_colors - 1) * sizeof(PALETTEENTRY) + sizeof(LOGPALETTE)), 1, 0);
		if (mf_new_pal == MEM_NULL)
			return (NOSUCCESS);

		pal = (LPLOGPALETTE) UseMemory(mf_new_pal);

		pal->palVersion = 0x0300;
		pal->palNumEntries = num_colors;
		for (i = 0; i < num_colors; i++)
		{
			pal->palPalEntry[i].peRed = red;
			pal->palPalEntry[i].peBlue = blue;
			pal->palPalEntry[i].peGreen = green;
			pal->palPalEntry[i].peFlags = 0;

			if (!(red += 32))
				if (!(green += 32))
					blue += 64;
		}
	}

	new_hpal = CreatePalette(pal);
	if (new_hpal == NULL)
		return (NOSUCCESS);

	old_hpal = SelectPalette(hdc, new_hpal, FALSE);
	RealizePalette(hdc);


	bmi->bmiHeader.biSize = 40;

	bmi->bmiHeader.biWidth = (long)(ptr_pbr_data->cb_pixmapData.std_pm_data.bounds.right - ptr_pbr_data->cb_pixmapData.std_pm_data.bounds.left);

	bmi->bmiHeader.biHeight = numScanLines;
	bmi->bmiHeader.biPlanes = 1;
	if ((ptr_pbr_data->cb_pixmapData.std_pm_data.pixelSize != 32) &&
		(ptr_pbr_data->cb_pixmapData.std_pm_data.pixelSize != 16))
	{
		bmi->bmiHeader.biBitCount = ptr_pbr_data->cb_pixmapData.std_pm_data.pixelSize;
		if (bmi->bmiHeader.biBitCount == 2)
			bmi->bmiHeader.biBitCount = 4;
	}
	else
	{
		bmi->bmiHeader.biBitCount = 24;

	}
	bmi->bmiHeader.biCompression = BI_RGB;
	bmi->bmiHeader.biSizeImage = 0;
	bmi->bmiHeader.biXPelsPerMeter = 0;
	bmi->bmiHeader.biYPelsPerMeter = 0;

	if ((ptr_pbr_data->cb_pixmapData.std_pm_data.pixelSize != 32) &&
		(ptr_pbr_data->cb_pixmapData.std_pm_data.pixelSize != 16))
	{
		bmi->bmiHeader.biClrUsed = numEntriesInColorTable;
		bmi->bmiHeader.biClrImportant = numEntriesInColorTable;
	}
	else
	{
		bmi->bmiHeader.biClrUsed = 0;
		bmi->bmiHeader.biClrImportant = 0;
	}

	MyScaleRect(&(ptr_pbr_data->dstRect), XScaleFactor, YScaleFactor);
	{
		int srcExtentX = 0, srcExtentY = 0;
		int dstExtentX = 0, dstExtentY = 0;

		dstExtentX = abs(ptr_pbr_data->dstRect.right - ptr_pbr_data->dstRect.left);
		dstExtentY = abs(ptr_pbr_data->dstRect.bottom - ptr_pbr_data->dstRect.top);
		srcExtentY = abs(ptr_pbr_data->srcRect.bottom - ptr_pbr_data->srcRect.top);
		srcExtentX = abs(ptr_pbr_data->srcRect.right - ptr_pbr_data->srcRect.left);

        if (dstExtentX > winExt->x)
			dstExtentX = winExt->x;
		if (dstExtentY > winExt->y)
			dstExtentY = winExt->y;
                                       
  
		if (ptr_pbr_data->mode == 1)
			fdwROP = SRCAND;   
		
		if ((numEntriesInColorTable > 2) || (bmi->bmiHeader.biBitCount > 8))
			SetStretchBltMode(hdc, STRETCH_DELETESCANS); 
		else
			SetStretchBltMode(hdc, STRETCH_ANDSCANS);

		scanlines_copied = StretchDIBits(hdc, ptr_pbr_data->dstRect.left, ptr_pbr_data->dstRect.top, dstExtentX, 
			dstExtentY, 0, 0,  srcExtentX, srcExtentY, lpvBits, bmi, DIB_RGB_COLORS, fdwROP);
	}

	SelectPalette(hdc, old_hpal, FALSE);
	RealizePalette(hdc);
	DeleteObject(new_hpal);
	new_hpal = NULL;
	DeleteObject(old_hpal);
	old_hpal = NULL;
	UnuseMemory(mf_new_pal);
	pal = NULL;
	DisposeMemory(mf_new_pal);
	mf_new_pal = MEM_NULL;
	

	UnuseMemory(mf_bmi);
	bmi = NULL;

	DisposeMemory(mf_bmi);
	mf_bmi = MEM_NULL;

	if ((ptr_pbr_data->cb_pixmapData.std_pm_data.pixelSize != 32) && 
		(ptr_pbr_data->cb_pixmapData.std_pm_data.pixelSize != 16))
	{
		UnuseMemory(ptr_mac_ct->cSpecArray);
		ptr_cs = NULL;

		UnuseMemory(ptr_pbr_data->mf_colorTable);
		ptr_mac_ct = NULL;
	}

	UnuseMemory(ptr_pbr_data->mf_PixData);
	ptr_mac_bits = NULL;


	UnuseMemory(*mf_pbr_data);
	ptr_pbr_data = NULL;

	UnuseMemory(scantable);
	lpvBits = NULL;

	DisposeMemory(scantable);
	scantable = MEM_NULL;

	return (SUCCESS);

}
        
          
int OldQDColorToRGB(RGBTRIPLE PG_FAR * rgbcolor, long old_qd_color)
{
	RGBTRIPLE yellow = {0x05,0xf3,0xfc}, magenta = {0x84, 0x08, 0xf2};
	RGBTRIPLE red = {0x06, 0x08, 0xdd}, cyan = {0xea, 0xab, 0x02};
	RGBTRIPLE green = {0x11, 0x80, 0x00}, blue = {0xd4, 0x00, 0x00};
	
	switch (old_qd_color)
	{
	case OQD_blackColor:
		rgbcolor->rgbtRed = 0;
		rgbcolor->rgbtBlue = 0;
		rgbcolor->rgbtGreen = 0;
		break;

	case OQD_whiteColor:
		rgbcolor->rgbtRed = 255;
		rgbcolor->rgbtBlue = 255;
		rgbcolor->rgbtGreen = 255;
		break;

	case OQD_redColor:
		rgbcolor->rgbtRed = red.rgbtRed;
		rgbcolor->rgbtBlue = red.rgbtBlue;
		rgbcolor->rgbtGreen = red.rgbtGreen;
		break;

	case OQD_greenColor:
		rgbcolor->rgbtRed = green.rgbtRed;
		rgbcolor->rgbtBlue = green.rgbtBlue;
		rgbcolor->rgbtGreen = green.rgbtGreen;
		break;

	case OQD_blueColor:
		rgbcolor->rgbtRed = blue.rgbtRed;
		rgbcolor->rgbtBlue = blue.rgbtBlue;
		rgbcolor->rgbtGreen = blue.rgbtGreen;
		break;
	
	case OQD_cyanColor:
		rgbcolor->rgbtRed = cyan.rgbtRed;
		rgbcolor->rgbtBlue = cyan.rgbtBlue;
		rgbcolor->rgbtGreen = cyan.rgbtGreen;
		break;

	case OQD_magentaColor:
		rgbcolor->rgbtRed = magenta.rgbtRed;
		rgbcolor->rgbtBlue = magenta.rgbtBlue;
		rgbcolor->rgbtGreen = magenta.rgbtGreen;
		break;

	case OQD_yellowColor:
		rgbcolor->rgbtRed = yellow.rgbtRed;
		rgbcolor->rgbtBlue = yellow.rgbtBlue;
		rgbcolor->rgbtGreen = yellow.rgbtGreen;
		break;


	default:
		rgbcolor->rgbtRed = 0;
		rgbcolor->rgbtBlue = 0;
		rgbcolor->rgbtGreen = 0;
		return (NOSUCCESS);
	}
	return (SUCCESS);

}


void MacRgnToMacRect(mac_rgn PG_FAR * mrgn, mac_rect PG_FAR * rectangle)
{
	rectangle->top = mrgn->rgnBBox.top;
	rectangle->bottom = mrgn->rgnBBox.bottom;
	rectangle->left = mrgn->rgnBBox.left;
	rectangle->right = mrgn->rgnBBox.right;
	return;
}



void ScalePolyPts(mac_poly PG_FAR * mpoly, long numPoints, double XScaleFactor, double YScaleFactor)
{
	POINT PG_FAR * ptr_points = NULL;
	register int i = 0;
	

	ptr_points = (POINT PG_FAR *)UseMemory(mpoly->polyPoints);
	for (i = 0; i < numPoints; i++)
	{
		ScalePt(&(ptr_points[i]), XScaleFactor, YScaleFactor);
	}
	UnuseMemory(mpoly->polyPoints);
	ptr_points = NULL;
	return;
}


int IsPixmapPBR(pg_bits8_ptr ptr_data_bytes)	
{
	
	short tmpRowBytes = (short)GetMacWordInc(&ptr_data_bytes);
	if (tmpRowBytes & 0x8000)
		return (TRUE);
	else
		return (FALSE);
}


pg_word GetNewPenWidth(pg_bits8_ptr PG_FAR * ptr2_data_bytes, ppen_track pen_stat, double XScaleFactor, double YScaleFactor)
{
	pg_word oldStyle = pen_stat->current_pen.lopnStyle;
	pen_stat->current_pen.lopnWidth.x = (short)GetMacWordInc(ptr2_data_bytes);
	pen_stat->current_pen.lopnWidth.y = (short)GetMacWordInc(ptr2_data_bytes);
	if ((pen_stat->current_pen.lopnWidth.x == 0) && (pen_stat->current_pen.lopnWidth.y == 0))
	{

		pen_stat->current_pen.lopnStyle = PS_NULL;
	}
	if (pen_stat->current_pen.lopnWidth.x == 0)
		pen_stat->current_pen.lopnWidth.x = 1;
	if (pen_stat->current_pen.lopnWidth.y == 0)
		pen_stat->current_pen.lopnWidth.y = 1;
	
	ScalePt(&(pen_stat->current_pen.lopnWidth), XScaleFactor, YScaleFactor);

	return (oldStyle);

}

//Creates and selects the new pen into the hdc, then deletes old pen.
// old pen is not deleted if new pen cannot be created. Returns NULL if fails, else
// returns handle to newly created and selected pen.
HPEN CreateAndSelectPen(HDC hdc, ppen_track pen_stat)
{
	HPEN tmp = NULL;

	 
	tmp = CreatePenIndirect(&(pen_stat->current_pen));
	if (tmp != NULL)
	{
		SelectObject(hdc, tmp);
		DeleteObject(pen_stat->hpen_current);
		pen_stat->hpen_current = tmp;
		return (pen_stat->hpen_current);
	}
	else
	{
		return (tmp);
	}

}

HFONT CreateAndSelectFont(HDC hdc, pfont_track font_stat)
{
 	HFONT tmp = NULL;

	 
	tmp = CreateFontIndirect(&(font_stat->current_font));
	if (tmp != NULL)
	{
		SelectObject(hdc, tmp);
		DeleteObject(font_stat->hfont_current);
		font_stat->hfont_current = tmp;
		return (font_stat->hfont_current);
	}
	else
	{
		return (tmp);
	}
} 




HBRUSH CreateAndSelectBrush(HDC hdc, pbrush_track brush_stat)
{
	HBRUSH tmp = NULL;

	tmp = CreateBrushIndirect(&(brush_stat->current_brush));
	if (tmp != NULL)
	{
		SelectObject(hdc, tmp);
		DeleteObject(brush_stat->hbrush_current);
		brush_stat->hbrush_current = tmp;
		return (brush_stat->hbrush_current);

	}
	else 
	{
		return (tmp);
	}

}

int PictPaintOval(HDC hdc, ppen_track pen_stat, pbrush_track brush_stat, mac_rect PG_FAR * mrect)
{
	pg_word oldPenStyle = pen_stat->current_pen.lopnStyle;
	pen_stat->current_pen.lopnStyle = PS_NULL;
	if(CreateAndSelectPen(hdc, pen_stat) == NULL)
		return (NOSUCCESS);

	if(CreateAndSelectBrush(hdc, brush_stat) == NULL)
		return (NOSUCCESS);

	if (!(Ellipse(hdc, mrect->left, mrect->top, mrect->right, mrect->bottom)))
		return (NOSUCCESS);

	
	pen_stat->current_pen.lopnStyle = oldPenStyle;
	if (CreateAndSelectPen(hdc, pen_stat) == NULL)
		return (NOSUCCESS);

	return (SUCCESS);
}



int PictFrameOval(HDC hdc, ppen_track pen_stat, pbrush_track brush_stat, mac_rect PG_FAR * mrect)
{
	pg_word oldBrushStyle = brush_stat->current_brush.lbStyle;

	brush_stat->current_brush.lbStyle = BS_NULL;
	if(CreateAndSelectBrush(hdc, brush_stat) == NULL)
		return (NOSUCCESS);
	
	if (CreateAndSelectPen(hdc, pen_stat) == NULL)
		return (NOSUCCESS);

	if (!(Ellipse(hdc, mrect->left, mrect->top, mrect->right, mrect->bottom)))
		return (NOSUCCESS);

	brush_stat->current_brush.lbStyle = oldBrushStyle;
	if(CreateAndSelectBrush(hdc, brush_stat) == NULL)
		return (NOSUCCESS);


	return (SUCCESS);
}



void GetArcAngles (pg_bits8_ptr PG_FAR * ptr2_data_bytes, short PG_FAR * startAngle, short PG_FAR * angleRun)
{
	*startAngle = (short)GetMacWordInc(ptr2_data_bytes);
	*angleRun = (short)GetMacWordInc(ptr2_data_bytes);

	return;
}



void GetArcParams(pg_bits8_ptr PG_FAR * ptr2_data_bytes, mac_rect PG_FAR * mrect, short PG_FAR * startAngle, 
				  short PG_FAR * angleRun, POINT PG_FAR * rect_center, double XScaleFactor, double YScaleFactor)
{
	GetMacRectInc(ptr2_data_bytes, mrect);
	MyScaleRect(mrect, XScaleFactor, YScaleFactor);
	GetArcAngles (ptr2_data_bytes, startAngle, angleRun);
	GetRectCenter(mrect, rect_center);
	return;
}



int RetrieveAndDrawPixmap(pg_bits8_ptr PG_FAR * ptr2_data_bytes, HDC hdc, double XScaleFactor,
					double YScaleFactor, POINT PG_FAR * winExt, int bBitsRect)
{
	memory_ref mf_pbr_data = MEM_NULL;
    
	if (IsPixmapPBR(*ptr2_data_bytes))
	{
		mf_pbr_data = MemoryAllocClear(m_globals, sizeof(mac_packBitsRect), 1, 0);
		if (mf_pbr_data == MEM_NULL)
		{
			return (NOSUCCESS);
		}
		else if (RetrievePackBitsRectParams(ptr2_data_bytes, &mf_pbr_data))
		{
			DisposePackBitsRectParams(&mf_pbr_data);
			return(NOSUCCESS);
		}
		if (DrawPackBitsRect(hdc, &mf_pbr_data, XScaleFactor, YScaleFactor, winExt))
		{
			DisposePackBitsRectParams(&mf_pbr_data);
			return (NOSUCCESS);
		}
		DisposePackBitsRectParams(&mf_pbr_data);

	}
	else
	{
		mf_pbr_data = MemoryAllocClear(m_globals, sizeof(mac_packBitmapRect), 1, 0);
		if (mf_pbr_data == MEM_NULL)
		{
			return (NOSUCCESS);
		}
		else if (RetrieveBMPackBitsRectParams(ptr2_data_bytes, &mf_pbr_data, bBitsRect))
		{
			DisposeBMPackBitsRect(&mf_pbr_data);
			return (NOSUCCESS);
		}
		if (DrawBMPackBitsRect(hdc, &mf_pbr_data, XScaleFactor, YScaleFactor, winExt))
		{
			DisposeBMPackBitsRect(&mf_pbr_data);
			return (NOSUCCESS);
		}
		DisposeBMPackBitsRect(&mf_pbr_data);

	}
	return (SUCCESS);
}


int RetrieveAndDrawPixmapRgn(pg_bits8_ptr PG_FAR * ptr2_data_bytes, HDC hdc, double XScaleFactor,
					double YScaleFactor, POINT PG_FAR * winExt, int bBitsRect)
{
	memory_ref mf_pbr_data = MEM_NULL;

	if (IsPixmapPBR(*ptr2_data_bytes))
	{
		mf_pbr_data = MemoryAllocClear(m_globals, sizeof(mac_packBitsRgn), 1, 0);
		if (mf_pbr_data == MEM_NULL)
		{
			return (NOSUCCESS);
		}
		else if (RetrievePackBitsRgnParams(ptr2_data_bytes, &mf_pbr_data))
		{
			DisposePackBitsRgnParams(&mf_pbr_data);
			return(NOSUCCESS);
		}
		if (DrawPackBitsRgn(hdc, &mf_pbr_data, XScaleFactor, YScaleFactor, winExt))
		{
			DisposePackBitsRgnParams(&mf_pbr_data);
			return (NOSUCCESS);
		}
		DisposePackBitsRgnParams(&mf_pbr_data);

	}
	else
	{
		mf_pbr_data = MemoryAllocClear(m_globals, sizeof(mac_packBitmapRgn), 1, 0);
		if (mf_pbr_data == MEM_NULL)
		{
			return (NOSUCCESS);
		}
		else if (RetrieveBMPackBitsRgnParams(ptr2_data_bytes, &mf_pbr_data, bBitsRect))
		{
			DisposeBMPackBitsRgn(&mf_pbr_data);
			return (NOSUCCESS);
		}
		if (DrawBMPackBitsRgn(hdc, &mf_pbr_data, XScaleFactor, YScaleFactor, winExt))
		{
			DisposeBMPackBitsRgn(&mf_pbr_data);
			return (NOSUCCESS);
		}
		DisposeBMPackBitsRgn(&mf_pbr_data);

	}
	return (SUCCESS);
}

void SkipDefaultPackedPixData(pg_bits8_ptr PG_FAR * ptr2_data_bytes, long numScanlines, short rowBytes)
{
	register short i = 0;
	pg_word byte_count = 0;
	long bytes_read = 0;
	for (i = 0; i < numScanlines; i++)
	{

		if (rowBytes > 250)
		{
			byte_count = GetMacWordInc(ptr2_data_bytes);
			bytes_read += 2;
		}
		else
		{
			byte_count = (pg_word)GetByteInc(ptr2_data_bytes);
			bytes_read++;
		}
		*ptr2_data_bytes += byte_count;
		bytes_read += byte_count;
	}
	if (bytes_read % 2)
		*ptr2_data_bytes += 1;


	return;
}


// returns TRUE if code not found, else false
int SkipOpCode(pg_bits8_ptr PG_FAR * ptr2_data_bytes, pg_word current_opcode)
{
	switch(current_opcode)
	{
		// skip opcodes with 0 bytes data
	case PICT_NOP:
	case PICT_hiliteMode:
	case PICT_defHilite:
	case PICT_eraseSameRect:
	case PICT_invertSameRect:
	case PICT_eraseSameRRect:
	case PICT_invertSameRRect:
	case PICT_eraseSameOval:
	case PICT_invertSameOval:
	case PICT_eraseSamePoly:
	case PICT_invertSamePoly:
	case PICT_frameSameRgn:
	case PICT_paintSameRgn:
	case PICT_fillSameRgn:
	case PICT_eraseSameRgn:
	case PICT_invertSameRgn:
		return (FALSE);
		break;

		
	
		// skip opcodes with 2 bytes data
	case PICT_txMode:
	case PICT_pnLocHFrac:
	case PICT_chExtra:
		*ptr2_data_bytes += 2;
		return (FALSE);
		break;


		//skip opcodes with 4 bytes data
	case PICT_spExtra:
	case PICT_eraseSameArc:
	case PICT_invertSameArc:
		*ptr2_data_bytes += 4;
		return (FALSE);
		break;

		//skip opcodes with 6 bytes data
	case PICT_ShortLine:
	case PICT_opColor:
	case PICT_RGBFgCol:
	case PICT_RGBBkCol:
	case PICT_HiliteColor:
		*ptr2_data_bytes += 6;
		return (FALSE);
		break;
 	case PICT_glyphState:
	{ 
	 	pg_word data_size = GetMacWordInc(ptr2_data_bytes);   
	 	*ptr2_data_bytes += data_size;
	 	if (!PICT_Is_Version_One)
	 	{
	 		if (data_size % 2)
	 			*ptr2_data_bytes += 1;  
	 	}
	 	return (FALSE);
	}
    break;
		//skip opcodes with 8 bytes data
	case PICT_bkPat:
	case PICT_txRatio:
	case PICT_eraseRect:
	case PICT_invertRect:
	case PICT_eraseRRect:
	case PICT_invertRRect:
	case PICT_eraseOval:
	case PICT_invertOval:
		*ptr2_data_bytes += 8;
		return (FALSE);
		break;

		//skip opcodes with 10 bytes data
	case PICT_lineJustify:
		*ptr2_data_bytes += 10;
		return (FALSE);
		break;

		//skip opcodes with 12 bytes data
	case PICT_eraseArc:
	case PICT_invertArc:
		*ptr2_data_bytes += 12;
		return (FALSE);
		break;

		// skip opcodes with 4 + data length bytes data
	case PICT_compressedQuicktime:
	case PICT_uncompressedQuicktime:
		{
			DWORD data_length = GetMacDWORDInc(ptr2_data_bytes);
			*ptr2_data_bytes += data_length;
			if (!PICT_Is_Version_One)
			{
			    if (data_length % 2)
			    	*ptr2_data_bytes += 1;
			 	
			}
			return (FALSE);
		}
		break;

   	case PICT_DirectBitsRect:
   		{
   			DWORD baseAddr = GetMacDWORDInc(ptr2_data_bytes);
			memory_ref mf_pbr_data = MEM_NULL;
		    
			mf_pbr_data = MemoryAllocClear(m_globals, sizeof(mac_packBitsRect), 1, 0);
			if (mf_pbr_data == MEM_NULL)
			{
				return (TRUE);
			}
			else if (RetrievePackBitsRectParams(ptr2_data_bytes, &mf_pbr_data))
			{
				DisposePackBitsRectParams(&mf_pbr_data);
				return(TRUE);
			}
			DisposePackBitsRectParams(&mf_pbr_data);
	        return (FALSE);
   		}   
   		break;
   		
   	case  PICT_directBitsRgn:
   		{
  			DWORD baseAddr = GetMacDWORDInc(ptr2_data_bytes);
		 	memory_ref mf_pbr_data = MEM_NULL;
		
			mf_pbr_data = MemoryAllocClear(m_globals, sizeof(mac_packBitsRgn), 1, 0);
			if (mf_pbr_data == MEM_NULL)
			{
				return (TRUE);
			}
			else if (RetrievePackBitsRgnParams(ptr2_data_bytes, &mf_pbr_data))
			{
				DisposePackBitsRgnParams(&mf_pbr_data);
				return(TRUE);
			}
		  		
			DisposePackBitsRgnParams(&mf_pbr_data);  
			return (FALSE);
	  } 
	  break;
	  
		// skip pattern opcodes;
	case PICT_bkPixPat:
	case PICT_pnPixPat:
	case PICT_fillPixPat:
		{                
			pg_bits8_ptr original_location = *ptr2_data_bytes;
			pg_word patType = GetMacWordInc(ptr2_data_bytes);
			*ptr2_data_bytes += 8; // skip old pattern 8bits x 8 bits
			if (patType == MAC_DITHER_PAT)
			{
				*ptr2_data_bytes += 6; // skip RGBColor;
			}
			else
			{
				long numBytesToSkip = 0;
				if (IsPixmapPBR(*ptr2_data_bytes))
				{
					mac_cb_pixmapData pixmap;
					RetrieveMacCbPixmapData(ptr2_data_bytes, &pixmap);
				// assuming pixdata is uncompressed. This may be wrong.
					numBytesToSkip = pixmap.std_pm_data.rowBytes * (pixmap.std_pm_data.bounds.bottom - pixmap.std_pm_data.bounds.top);
					if (pixmap.std_pm_data.pixelSize <= 8)
					{
						short numEntriesColorTable = 0;
					// skip to size of number of entries in colorTable
						*ptr2_data_bytes += 6;
						numEntriesColorTable = GetMacWordInc(ptr2_data_bytes);
						*ptr2_data_bytes += (numEntriesColorTable + 1) * SIZE_COLORSPEC_ENTRY;
					}
				}
				else
				{
					mac_bitmap bm;
					RetrieveMacBmData(ptr2_data_bytes, &bm);
					numBytesToSkip = bm.rowBytes * (bm.bounds.bottom - bm.bounds.top);
				}
	
				*ptr2_data_bytes += numBytesToSkip;  
				if (!PICT_Is_Version_One)
				{
				 	if ((*ptr2_data_bytes - original_location) %2)
				 		*ptr2_data_bytes += 1;
				}

			}
			return (FALSE);
		}
		break;


	default:
		if (((current_opcode >= PICT_appleRes009C) && (current_opcode <= PICT_appleRes009F)) ||
			((current_opcode >= PICT_appleRes0024) && (current_opcode <= PICT_appleRes0027)) || 
			((current_opcode >= PICT_appleRes002C) && (current_opcode <= PICT_appleRes002F)) ||
			((current_opcode >= PICT_appleRes0092) && (current_opcode <= PICT_appleRes0097)) ||
			((current_opcode >= PICT_appleRes00A2) && (current_opcode <= PICT_appleRes00AF)))
		{
			pg_word data_length = (pg_word)GetMacWordInc(ptr2_data_bytes);
			*ptr2_data_bytes += data_length;   
			if (!PICT_Is_Version_One)
			{
				if ((2 + data_length) % 2)
					*ptr2_data_bytes += 1;
			}
				
			return (FALSE);

		}
		else if (((current_opcode <= PICT_appleRes0075) && (current_opcode >= PICT_appleRes0077)) ||
				 (current_opcode == PICT_invertPoly))
		{
			mac_poly	mpoly;
			long numPoints = 0;

			mpoly.polyPoints = MEM_NULL;
			if (RetrievePolyParams(ptr2_data_bytes, &mpoly,&numPoints))
				return (TRUE);
			DisposeMemory(mpoly.polyPoints);
			mpoly.polyPoints = MEM_NULL;

		}
		else if ((current_opcode <= PICT_appleRes0085) && (current_opcode >= PICT_appleRes0087))
		{
			mac_rgn rgn;
			GetMacRgn(ptr2_data_bytes, &rgn);
			
		}
		else if ((current_opcode >= PICT_appleRes0100) && (current_opcode <= PICT_appleRes01FF))
		{
			*ptr2_data_bytes += 2;
			return (FALSE);
		}
		else if ((current_opcode > PICT_appleRes01FF) && (current_opcode <= PICT_appleRes7FFF))
		{
			short tmp = (((current_opcode >> 4) & 0x0f) * 2);
			*ptr2_data_bytes += tmp;
			if (!PICT_Is_Version_One)
			{
			 	if (tmp % 2)
			 		*ptr2_data_bytes += 1;
			}
			return (FALSE);
		}
		else if (((current_opcode >= PICT_appleRes8100) && (current_opcode <= PICT_appleResFFFF)) ||
				((current_opcode >= PICT_appleRes00D0) && (current_opcode <= PICT_appleRes00FE)))
		{
			DWORD data_size = GetMacDWORDInc(ptr2_data_bytes);
			*ptr2_data_bytes += data_size; 
			if (!PICT_Is_Version_One)
			{
			 	if (data_size % 2)
			 		*ptr2_data_bytes += 1;
			
			}
			return(FALSE);

		}
		else if (((current_opcode >= PICT_appleRes0017) && (current_opcode <= PICT_appleRes0019)) ||
			((current_opcode >= PICT_appleRes003d) && (current_opcode <= PICT_appleRes003f)) ||
			((current_opcode >= PICT_appleRes004d) && (current_opcode <= PICT_appleRes004f)) ||
			((current_opcode >= PICT_appleRes005d) && (current_opcode <= PICT_appleRes005f)) ||
			((current_opcode >= PICT_appleRes007d) && (current_opcode <= PICT_appleRes007f)) ||
			((current_opcode >= PICT_appleRes008d) && (current_opcode <= PICT_appleRes008f)) ||
			((current_opcode >= PICT_appleRes00B0) && (current_opcode <= PICT_appleRes00cf)) ||
			((current_opcode >= PICT_appleRes8000) && (current_opcode <= PICT_appleRes80ff)))
		{
			return (FALSE);
		}
		else if ((current_opcode >= PICT_appleRes006d) && (current_opcode <= PICT_appleRes006f))
		{
			*ptr2_data_bytes += 4;
			return (FALSE);
		}
		else if (((current_opcode >= PICT_appleRes0035) && (current_opcode <= PICT_appleRes0037)) ||
			((current_opcode >= PICT_appleRes0045) && (current_opcode <= PICT_appleRes0047)) ||
			((current_opcode >= PICT_appleRes0055) && (current_opcode <= PICT_appleRes0057)))
		{
			*ptr2_data_bytes += 8;
			return (FALSE);
		}
		else if ((current_opcode >= PICT_appleRes0065) && (current_opcode <= PICT_appleRes0067))
		{
			*ptr2_data_bytes += 12;
			return (FALSE);
		}
		else
		{

			return (TRUE);
		}
		break;
}; // end switch
	
	return (FALSE);

}

int PictSetPnMode(HDC hdc, pg_word current_pen_mode, ppen_track pen_stat)
{

	switch (current_pen_mode)
	{

	case PNMODE_patCopy:
		SetROP2(hdc, R2_COPYPEN);
		break;
	case PNMODE_patBic: //not currently supported, defaulting to OR.
	case PNMODE_patOr:
		SetROP2(hdc, R2_MERGEPEN);
		break;

	case PNMODE_patXor:
		SetROP2(hdc, R2_XORPEN);
		break;
	
	case PNMODE_notPatCopy:
		SetROP2(hdc, R2_NOTCOPYPEN);
		break;

	case PNMODE_notPatBic:  //not currently supported, defaulting to NotOr.
	case PNMODE_notPatOr:
		SetROP2(hdc,R2_NOTMERGEPEN);
		break;

	case PNMODE_notPatXor:
		SetROP2(hdc, R2_NOTXORPEN);
		break;
	default:
		{
			pg_word oldPenStyle = pen_stat->current_pen.lopnStyle;
			pen_stat->current_pen.lopnStyle = PS_NULL;
			if (CreateAndSelectPen(hdc, pen_stat) == NULL)
				return (NOSUCCESS);
			pen_stat->current_pen.lopnStyle = oldPenStyle;
		}
		break;

	}; // end switch penmode



	return (SUCCESS);
}
   
   
void RetrievePt(pg_bits8_ptr PG_FAR * ptr2_data_bytes, POINT PG_FAR * pt)
{
   	pt->y = (short)GetMacWordInc(ptr2_data_bytes);
	pt->x = (short)GetMacWordInc(ptr2_data_bytes);
	
	return;

} 



void RetrieveAdjustedScaledPt(pg_bits8_ptr PG_FAR * ptr2_data_bytes, POINT PG_FAR * pt, double XScaleFactor,
							  double YScaleFactor)
{
	pt->y = (short)GetMacWordInc(ptr2_data_bytes);
	pt->x = (short)GetMacWordInc(ptr2_data_bytes);
	if (pt->y < 0) 
		pt->y = 0;
	if (pt->x < 0)
		pt->x = 0;
	ScalePt(pt, XScaleFactor, YScaleFactor);

	return;
}

int PictPaintRect(HDC hdc, mac_rect PG_FAR * mrect, ppen_track pen_stat, pbrush_track brush_stat)
{
	
	pg_word oldPenStyle = pen_stat->current_pen.lopnStyle;

	pen_stat->current_pen.lopnStyle = PS_NULL;
	if (CreateAndSelectPen(hdc, pen_stat) == NULL)
		return (NOSUCCESS);

	if (CreateAndSelectBrush(hdc, brush_stat) == NULL)
		return (NOSUCCESS);

	if (!(Rectangle(hdc, mrect->left, mrect->top, mrect->right, mrect->bottom)))
		return (NOSUCCESS);
	
	pen_stat->current_pen.lopnStyle = oldPenStyle;
	if (CreateAndSelectPen(hdc, pen_stat) == NULL)
		return (NOSUCCESS);


	return (SUCCESS);

}

int PictFrameRect(HDC hdc, mac_rect PG_FAR * mrect, ppen_track pen_stat, pbrush_track brush_stat)
{

	pg_word oldBrushStyle = brush_stat->current_brush.lbStyle;

	brush_stat->current_brush.lbStyle = BS_NULL;
	if (CreateAndSelectBrush(hdc, brush_stat) == NULL)
		return (NOSUCCESS);

	pen_stat->current_pen.lopnStyle = PS_SOLID;
	if (CreateAndSelectPen(hdc, pen_stat) == NULL)
		return (NOSUCCESS);

	if (!(Rectangle(hdc, mrect->left, mrect->top, mrect->right, mrect->bottom)))
		return (NOSUCCESS);

	brush_stat->current_brush.lbStyle = oldBrushStyle;
	if (CreateAndSelectBrush(hdc, brush_stat) == NULL)
		return (NOSUCCESS);

	return(SUCCESS);
}



int PictPaintRRect(HDC hdc, mac_rect PG_FAR * mrect, POINT PG_FAR * ovSize, ppen_track pen_stat, pbrush_track brush_stat)
{
	pg_word oldPenStyle = pen_stat->current_pen.lopnStyle;

	pen_stat->current_pen.lopnStyle = PS_NULL;
	if (CreateAndSelectPen(hdc, pen_stat) == NULL)
		return (NOSUCCESS);


	if (CreateAndSelectBrush(hdc, brush_stat) == NULL)
		return (NOSUCCESS);

	if (!(RoundRect(hdc, mrect->left, mrect->top, mrect->right, mrect->bottom, ovSize->x, ovSize->y)))
		return (NOSUCCESS);
	
	pen_stat->current_pen.lopnStyle = oldPenStyle;
	if (CreateAndSelectPen(hdc, pen_stat) == NULL)
		return (NOSUCCESS);


	return (SUCCESS);
}



int PictFrameRRect(HDC hdc, mac_rect PG_FAR * mrect, POINT PG_FAR * ovSize, ppen_track pen_stat, pbrush_track brush_stat)
{
	pg_word oldBrushStyle = brush_stat->current_brush.lbStyle;

	brush_stat->current_brush.lbStyle = BS_NULL;
	if(CreateAndSelectBrush(hdc, brush_stat) == NULL)
		return (NOSUCCESS);
	
	if(CreateAndSelectPen(hdc, pen_stat) == NULL)
		return (NOSUCCESS);

	if(!(RoundRect(hdc, mrect->left, mrect->top, mrect->right, mrect->bottom, ovSize->x, ovSize->y)))
		return (NOSUCCESS);

	brush_stat->current_brush.lbStyle = oldBrushStyle;
	if(CreateAndSelectBrush(hdc, brush_stat) == NULL)
		return (NOSUCCESS);

	return (SUCCESS);
}



int PictFramePoly(HDC hdc, pmac_poly mpoly, long numPoints, ppen_track pen_stat, pbrush_track brush_stat, 
				  double XScaleFactor, double YScaleFactor)
{
	POINT PG_FAR * ptr_points = NULL;
	pg_word oldBrushStyle = brush_stat->current_brush.lbStyle;
	short bFailed = FALSE;

	ScalePolyPts(mpoly, numPoints, XScaleFactor, YScaleFactor);
	brush_stat->current_brush.lbStyle = BS_NULL;
	if (CreateAndSelectBrush(hdc, brush_stat) == NULL)
		return (NOSUCCESS);
	
	pen_stat->current_pen.lopnStyle = PS_SOLID;
	if (CreateAndSelectPen(hdc, pen_stat) == NULL)
		return (NOSUCCESS);

	ptr_points = (POINT PG_FAR *)UseMemory(mpoly->polyPoints);
	if ((ptr_points[0].x == ptr_points[numPoints -1].x) 
		&& (ptr_points[0].y == ptr_points[numPoints -1].y))
	{
		if (!(Polygon(hdc, ptr_points, (int)numPoints)))
			bFailed = TRUE;
	}
	else
	{
		if (!(Polyline(hdc, ptr_points, (int)numPoints)))
			bFailed = TRUE;
	}
	UnuseMemory(mpoly->polyPoints);
	ptr_points = NULL;
	if (bFailed)
		return (NOSUCCESS);
	brush_stat->current_brush.lbStyle = oldBrushStyle;
	if (CreateAndSelectBrush(hdc, brush_stat) == NULL)
		return (NOSUCCESS);


	return (SUCCESS);
}


int PictPaintPoly(HDC hdc, pmac_poly mpoly, long numPoints,ppen_track pen_stat, pbrush_track brush_stat, 
				  double XScaleFactor, double YScaleFactor)
{
	POINT PG_FAR * ptr_points = NULL;
	pg_word oldPenStyle = pen_stat->current_pen.lopnStyle;
	short bFailed = FALSE;

	ScalePolyPts(mpoly, numPoints, XScaleFactor, YScaleFactor);
	pen_stat->current_pen.lopnStyle = PS_NULL;
	if (CreateAndSelectPen(hdc, pen_stat) == NULL)
		return (NOSUCCESS);

    brush_stat->current_brush.lbStyle = BS_SOLID;
	if (CreateAndSelectBrush(hdc, brush_stat) == NULL)
		return (NOSUCCESS);

	ptr_points = (POINT PG_FAR *)UseMemory(mpoly->polyPoints);

	if (!(Polygon(hdc, ptr_points, (int)numPoints)))
		bFailed = TRUE;

	UnuseMemory(mpoly->polyPoints);
	ptr_points = NULL;
	if (bFailed)
		return (NOSUCCESS);
	pen_stat->current_pen.lopnStyle = oldPenStyle;
	if (CreateAndSelectPen(hdc, pen_stat) == NULL)
		return (NOSUCCESS);
				
	return (SUCCESS);
}

int RetrieveDHDVText(pg_bits8_ptr PG_FAR* ptr2_data_bytes, int PG_FAR* string_length, int PG_FAR* horizontal_offset, int PG_FAR* vertical_offset, memory_ref PG_FAR* mfptr_string_to_display, pg_bits8 flDhDv)
{                    
	LPSTR ptr_string_to_display = NULL; 
    int total_data_bytes = 0;
   switch(flDhDv)
   {
   		case DV_FLAG:
   			*vertical_offset = (int)((signed char)GetByteInc(ptr2_data_bytes)); 
   			total_data_bytes++;
   			break;
   		case DH_FLAG:
   			*horizontal_offset = (int)((signed char)GetByteInc(ptr2_data_bytes)); 
   			total_data_bytes++;
   			break;
   		case DVDH_FLAG:
   			*horizontal_offset = (int)((signed char)GetByteInc(ptr2_data_bytes));
   			*vertical_offset = (int)((signed char)GetByteInc(ptr2_data_bytes));
   			total_data_bytes += 2;  
   			break;
   		default:
   			return (NOSUCCESS);
   				
   }
 	*string_length = (int)GetByteInc(ptr2_data_bytes); 
 	total_data_bytes++; 
 	if (*string_length <= 0)
 	{
		return (NOSUCCESS);

 	}
 	*mfptr_string_to_display = MemoryAllocClear(m_globals, sizeof(char), *string_length + 1, 0);
 	if (*mfptr_string_to_display == MEM_NULL)
 	{
		return (NOSUCCESS);
 	} 
	 	 	
 	ptr_string_to_display = (LPSTR)UseMemory(*mfptr_string_to_display);
 	pgBlockMove(*ptr2_data_bytes, ptr_string_to_display, *string_length);
 	ptr_string_to_display[*string_length] = 0;   
 	*ptr2_data_bytes += *string_length; 
  
 	if (!PICT_Is_Version_One)
 	{
 		if ((total_data_bytes + (*string_length)) % 2)
 			*ptr2_data_bytes += 1;
     }
     UnuseMemory(*mfptr_string_to_display);
     ptr_string_to_display = NULL;
	
 	return (SUCCESS);
}     


int DhDvDrawText(HDC hdc, int PG_FAR* last_vertOffset, int PG_FAR* last_horzOffset, memory_ref PG_FAR* mfptr_string_to_display, int string_length, int vertical_offset, 
					int horizontal_offset, POINT PG_FAR* winExt, pg_bits8 flDvDh)
{
    RECT TxtRect = {0,0,0,0};
    LPSTR ptr_string_to_display = NULL;
    int bFailed = FALSE;
    
    if (*mfptr_string_to_display == MEM_NULL)
    {
    	return (NOSUCCESS);
    }
    
    switch (flDvDh)
    {
     	case DV_FLAG: 
     		horizontal_offset = *last_horzOffset;
     		vertical_offset += *last_vertOffset; 
    		*last_vertOffset = vertical_offset;
 			
     		break;
     	case DH_FLAG:
     		vertical_offset = *last_vertOffset; 
     		horizontal_offset += *last_horzOffset;
     		*last_horzOffset = horizontal_offset;
     		break;
     	case DVDH_FLAG:   
     		vertical_offset += *last_vertOffset; 
     		*last_vertOffset = vertical_offset;
     		horizontal_offset += *last_horzOffset;
     		*last_horzOffset = horizontal_offset;
     		break; 
     	case LONG_FLAG: 
	   		*last_vertOffset = vertical_offset;
     		*last_horzOffset = horizontal_offset;  

      		break;
     	default:
     		return (NOSUCCESS);
            break;
    }
    
    TxtRect.top = TxtRect.left = 0;
    TxtRect.bottom = winExt->y;
    TxtRect.right = winExt->x;
    
    ptr_string_to_display = (LPSTR)UseMemory(*mfptr_string_to_display);
    
    if (!(ExtTextOut(hdc, horizontal_offset, vertical_offset, ETO_CLIPPED, &TxtRect, ptr_string_to_display, string_length, NULL)))
    {
     	bFailed = TRUE;
    }
    
 	if (ptr_string_to_display != NULL)
 	{
 	 	UnuseMemory(*mfptr_string_to_display);
 	 	ptr_string_to_display = NULL;
 	}
 	
 	if (bFailed) 
     	return (NOSUCCESS);
    
 
 	return(SUCCESS);
} 
    
    
    
void DisposeTranslateOpcodesData(memory_ref PG_FAR * mfp_current_pnpat, pmac_poly mpoly, ppen_track pen_stat, pbrush_track brush_stat, pfont_track font_stat, pshort_polyline shortline_poly)
{

	if (*mfp_current_pnpat)
	{
		DisposeMemory(*mfp_current_pnpat);
		*mfp_current_pnpat = MEM_NULL;
	}
	
	if (mpoly->polyPoints != MEM_NULL)
	{
		DisposeMemory(mpoly->polyPoints);
		mpoly->polyPoints = MEM_NULL;
	}

	if (pen_stat->hpen_current != NULL)
	{
		DeleteObject(pen_stat->hpen_current);
		pen_stat->hpen_current = NULL;

	}

	if (brush_stat->hbrush_current != NULL)
	{
		DeleteObject(brush_stat->hbrush_current);
		brush_stat->hbrush_current == NULL;
	}
      
    if (font_stat->hfont_current != NULL)
    {
    	DeleteObject(font_stat->hfont_current);
    	font_stat->hfont_current;
    } 
    if (shortline_poly->polyPoints != MEM_NULL)
    {
    	DisposeMemory(shortline_poly->polyPoints);
    	shortline_poly->polyPoints = MEM_NULL;
    }
    
	return;
}


// reads PICT opcodes and calls associated metafile commands
// returns 1 if fails, otherwise returns 0. Increments ptr.
int TranslatePictOpCodes(pg_bits8_ptr PG_FAR * ptr2_data_bytes, HDC hdc_metafile, POINT PG_FAR * winExt ,
						 HDC target_dc)
{
	pg_word		current_opcode = 0;
	pg_word		previous_opcode = 0;
	pg_word		current_pen_mode = 0;
	POINT		current_position = {0,0};
	RGBTRIPLE	current_forecolor = {0,0,0};
	RGBTRIPLE	current_backcolor = {0,0,0};
	memory_ref	mf_current_pnpat = MEM_NULL;
	mac_rect	mrect = {0,0,0,0};
	mac_poly	mpoly;
	POINT		ovSize = {0,0};
	short		bNotFound = FALSE;
	mac_rgn		mrgn;
	int			horzres = 0, vertres = 0;
	double		XScaleFactor = 1, YScaleFactor = 1;
	short		startAngle = 0, angleRun = 0;
	POINT		rect_center = {0,0};
	pen_track   pen_stat;
	brush_track	brush_stat; 
	font_track	font_stat;
	long 		numPoints = 0;
	HBITMAP 	hnd_bitmap = NULL;
    short_polyline shortline_poly;
 	int			last_vertOffset = 0;
    int			last_horzOffset = 0;
	pg_bits8 outline_preferred = 0;
	pg_bits8 preserve_glyph = 0; 
	pg_bits8 fractional_widths = 0;
	pg_bits8 scaling_disabled;
	POINT num = {1,1};
    POINT denom = {1,1};  
    
    pgFillBlock(&shortline_poly, sizeof(short_polyline), 0);
    
	mpoly.polyPoints = MEM_NULL;

	pen_stat.hpen_current = NULL;
	brush_stat.hbrush_current = NULL;

	pen_stat.current_pen.lopnStyle = PS_SOLID;
	pen_stat.current_pen.lopnWidth.x = 1;
	pen_stat.current_pen.lopnWidth.y = 1;
	pen_stat.current_pen.lopnColor = RGB(0,0,0);

	brush_stat.current_brush.lbStyle = BS_SOLID;
	brush_stat.current_brush.lbColor = RGB(0,0,0);
	brush_stat.current_brush.lbHatch = 0;            
	
	pgFillBlock(&font_stat, sizeof(font_track), 0);  
	font_stat.current_font.lfHeight = -9;

	horzres = GetDeviceCaps(target_dc, LOGPIXELSX);
	vertres = GetDeviceCaps(target_dc, LOGPIXELSY);
	
	XScaleFactor = 1.0; 
	YScaleFactor = 1.0; 
	

	current_opcode = GetOpCodeInc(ptr2_data_bytes);
	while (current_opcode != PICT_opEndPic)
	{
		bNotFound = FALSE;
		switch (current_opcode)
		{
		
		case PICT_Clip:
			DealWithClip(ptr2_data_bytes, hdc_metafile);
			break;
	
		case PICT_PnSize:
			{
				pg_word  oldPenStyle = 0;
				oldPenStyle = GetNewPenWidth(ptr2_data_bytes, &pen_stat, XScaleFactor, YScaleFactor);
				if (CreateAndSelectPen(hdc_metafile, &pen_stat) == NULL)
				{
					DisposeTranslateOpcodesData(&mf_current_pnpat, &mpoly, &pen_stat, &brush_stat, &font_stat, &shortline_poly);
					return (NOSUCCESS);
				}
				pen_stat.current_pen.lopnStyle = oldPenStyle;
			}
			break;
		case PICT_PnMode:
			current_pen_mode = GetMacWordInc(ptr2_data_bytes);
			if(PictSetPnMode(hdc_metafile, current_pen_mode, &pen_stat))
			{
				DisposeTranslateOpcodesData(&mf_current_pnpat, &mpoly, &pen_stat, &brush_stat, &font_stat, &shortline_poly);
				return(NOSUCCESS);
			}
			break;

		case PICT_PnPat:
			if (RetrievePenPat(ptr2_data_bytes, &mf_current_pnpat))
			{
				DisposeTranslateOpcodesData(&mf_current_pnpat, &mpoly, &pen_stat, &brush_stat, &font_stat, &shortline_poly);
				return (NOSUCCESS);
			}
			break;
	
		case PICT_Origin:
			{
				POINT origin = {0,0};
				RetrieveAdjustedScaledPt(ptr2_data_bytes, &origin, XScaleFactor, YScaleFactor);
				SetWindowOrgEx(hdc_metafile, origin.x, origin.y, NULL);
			}
			break;

		case PICT_OvSize:
			ovSize.y = (short)GetMacWordInc(ptr2_data_bytes);
			ovSize.x = (short)GetMacWordInc(ptr2_data_bytes);
			ScalePt(&ovSize, XScaleFactor, YScaleFactor);
			break;

		
		case PICT_FgColor:
			{	
				long old_qd_color = 0;
				RGBTRIPLE rgbcolor = {0,0,0};

				old_qd_color = GetMacDWORDInc(ptr2_data_bytes);
				if (OldQDColorToRGB(&rgbcolor, old_qd_color))
				{
					DisposeTranslateOpcodesData(&mf_current_pnpat, &mpoly, &pen_stat, &brush_stat, &font_stat, &shortline_poly);
					return (NOSUCCESS);

				}
				else
				{
					pen_stat.current_pen.lopnColor = brush_stat.current_brush.lbColor = RGB(rgbcolor.rgbtRed, rgbcolor.rgbtGreen, rgbcolor.rgbtBlue);
					if (CreateAndSelectPen(hdc_metafile, &pen_stat) == NULL)
					{
						DisposeTranslateOpcodesData(&mf_current_pnpat, &mpoly, &pen_stat, &brush_stat, &font_stat, &shortline_poly);
						return (NOSUCCESS);
					}
					if (CreateAndSelectBrush(hdc_metafile, &brush_stat) == NULL)
					{
						DisposeTranslateOpcodesData(&mf_current_pnpat, &mpoly, &pen_stat, &brush_stat, &font_stat, &shortline_poly);
						return (NOSUCCESS);
					}
				}
			}
			break;

		case PICT_BkColor:
			{	
				long old_qd_color = 0;
				RGBTRIPLE rgbcolor = {0,0,0};

				old_qd_color = GetMacDWORDInc(ptr2_data_bytes);
				if (OldQDColorToRGB(&rgbcolor, old_qd_color))
				{
					DisposeTranslateOpcodesData(&mf_current_pnpat, &mpoly, &pen_stat, &brush_stat, &font_stat, &shortline_poly);
					return (NOSUCCESS);
				}
				else
				{
					SetBkColor (hdc_metafile, RGB(rgbcolor.rgbtRed, rgbcolor.rgbtGreen, rgbcolor.rgbtBlue));
				}
			}

			break;

		case PICT_RGBFgCol:
			if (PICT_Is_Version_One)
			{
				if (SkipOpCode(ptr2_data_bytes, current_opcode))
				{
					DisposeTranslateOpcodesData(&mf_current_pnpat, &mpoly, &pen_stat, &brush_stat, &font_stat, &shortline_poly);
					bNotFound = TRUE;
					return (NOSUCCESS);
				}
			
			    break;
			
			}
			RetrieveForeColor(ptr2_data_bytes, &current_forecolor);
			pen_stat.current_pen.lopnColor = brush_stat.current_brush.lbColor = ConvertRGBTtoColorRef(&current_forecolor);
			if (CreateAndSelectPen(hdc_metafile, &pen_stat) == NULL)
			{
				DisposeTranslateOpcodesData(&mf_current_pnpat, &mpoly, &pen_stat, &brush_stat, &font_stat, &shortline_poly);
				return (NOSUCCESS);
			}

			if (CreateAndSelectBrush(hdc_metafile, &brush_stat) == NULL)
			{
				DisposeTranslateOpcodesData(&mf_current_pnpat, &mpoly, &pen_stat, &brush_stat, &font_stat, &shortline_poly);
				return (NOSUCCESS);
			}
			break;
	
		case PICT_RGBBkCol:
			if (PICT_Is_Version_One)
			{
				if (SkipOpCode(ptr2_data_bytes, current_opcode))
				{
					DisposeTranslateOpcodesData(&mf_current_pnpat, &mpoly, &pen_stat, &brush_stat, &font_stat, &shortline_poly);
					bNotFound = TRUE;
					return (NOSUCCESS);
				}
			
			    break;
			
			}
			RetrieveForeColor(ptr2_data_bytes, &current_backcolor);
			SetBkColor (hdc_metafile, ConvertRGBTtoColorRef(&current_backcolor));
			break;

		case PICT_Line:
			{
				POINT start_spot = {0,0}, stop_point = {0,0};
				RetrieveAdjustedScaledPt(ptr2_data_bytes, &start_spot, XScaleFactor, YScaleFactor);
				RetrieveAdjustedScaledPt(ptr2_data_bytes, &stop_point, XScaleFactor, YScaleFactor);
				MoveToEx(hdc_metafile, start_spot.x, start_spot.y, NULL);
				LineTo(hdc_metafile, stop_point.x, stop_point.y);   
				if (PICT_Is_Version_One)
					*ptr2_data_bytes += 1;
					
			}

			break;

		case PICT_LineFrom:
			{
				POINT stop_point = {0,0};
				RetrieveAdjustedScaledPt(ptr2_data_bytes, &stop_point, XScaleFactor, YScaleFactor);  
				if (shortline_poly.polyPoints == MEM_NULL)  
				{
					LineTo(hdc_metafile, stop_point.x, stop_point.y);
				}
				else
				{   
					POINT PG_FAR * ptr_poly_points = NULL;
					long numRecs = GetMemorySize(shortline_poly.polyPoints); 
				    if ((shortline_poly.index) >= (numRecs))
				    {
				     	SetMemorySize(shortline_poly.polyPoints, numRecs + SHORT_POLYLINE_INCREASE);
				    
				    }
					ptr_poly_points = (POINT PG_FAR*)UseMemory(shortline_poly.polyPoints);
				
					ptr_poly_points[shortline_poly.index].y = stop_point.y;
					ptr_poly_points[shortline_poly.index].x = stop_point.x;  
					(shortline_poly.index)++;
					(shortline_poly.numPoints)++;
					
					UnuseMemory(shortline_poly.polyPoints);
					ptr_poly_points = NULL;
				
				}
			}

			break;
        
        case PICT_ShortLine:
        	{           
        		if (shortline_poly.polyPoints != MEM_NULL)
        		{    
					int bFailed = FALSE;
	 				pg_word oldBrushStyle = brush_stat.current_brush.lbStyle;
     	 			POINT* ptr_poly_points = (POINT*)UseMemory(shortline_poly.polyPoints);
                    
					brush_stat.current_brush.lbStyle = BS_NULL;
					if (CreateAndSelectBrush(hdc_metafile, &brush_stat) == NULL)
						return (NOSUCCESS);
					
					pen_stat.current_pen.lopnStyle = PS_SOLID;
					if (CreateAndSelectPen(hdc_metafile, &pen_stat) == NULL)
						return (NOSUCCESS);
                  
 	      		    if (!(Polyline(hdc_metafile, ptr_poly_points, shortline_poly.numPoints)))
 	      		    	bFailed = TRUE;
        		    	
        		    UnuseMemory(shortline_poly.polyPoints);
        		    ptr_poly_points = NULL;
        		    DisposeMemory(shortline_poly.polyPoints);
        		    shortline_poly.polyPoints = MEM_NULL;
    				pgFillBlock(&shortline_poly, sizeof(short_polyline), 0);
    				if (bFailed)  
    				{
						DisposeTranslateOpcodesData(&mf_current_pnpat, &mpoly, &pen_stat, &brush_stat, &font_stat, &shortline_poly);
    					return(NOSUCCESS);
    				}
				   	brush_stat.current_brush.lbStyle = oldBrushStyle;
					if (CreateAndSelectBrush(hdc_metafile, &brush_stat) == NULL)
						return (NOSUCCESS);
     			} 
  
    
            	if (DealWithShortLine(ptr2_data_bytes, &shortline_poly, XScaleFactor,YScaleFactor,hdc_metafile))
            	{
					DisposeTranslateOpcodesData(&mf_current_pnpat, &mpoly, &pen_stat, &brush_stat, &font_stat, &shortline_poly);
            		return(NOSUCCESS);
            	}
        	}
        	break;
        case PICT_ShortLineFrom:
        	{   
        		if(DealShortLineFrom(ptr2_data_bytes, &shortline_poly, XScaleFactor, YScaleFactor, hdc_metafile))
        		{
					DisposeTranslateOpcodesData(&mf_current_pnpat, &mpoly, &pen_stat, &brush_stat, &font_stat, &shortline_poly);
        			return(NOSUCCESS);
        		}
        	}
        	break;
        	
        case PICT_fontName:
		{ 
			if (PICT_Is_Version_One)
			{
				if (SkipOpCode(ptr2_data_bytes, current_opcode))
				{
					DisposeTranslateOpcodesData(&mf_current_pnpat, &mpoly, &pen_stat, &brush_stat, &font_stat, &shortline_poly);
					bNotFound = TRUE;
					return (NOSUCCESS);
				}
			}
			else
			{
				pg_word 	data_size = 0;
				pg_word 	old_font_ID = 0;
				pg_bits8    name_length = 0;
				
				
				data_size = GetMacWordInc(ptr2_data_bytes);
				old_font_ID = GetMacWordInc(ptr2_data_bytes);
				name_length = GetByteInc(ptr2_data_bytes);
				if (name_length < LF_FACESIZE)
				{
					pgBlockMove(*ptr2_data_bytes, font_stat.current_font.lfFaceName, name_length);   
					font_stat.current_font.lfFaceName[name_length] = 0; 
					*ptr2_data_bytes += name_length;
				}
				else
				{
				 	pgBlockMove(*ptr2_data_bytes, font_stat.current_font.lfFaceName, LF_FACESIZE);
				 	font_stat.current_font.lfFaceName[LF_FACESIZE -1] = 0;
				 	*ptr2_data_bytes += LF_FACESIZE;
				 	
				} 
				if (!PICT_Is_Version_One)
				{
					if ((data_size + 2) % 2)
						*ptr2_data_bytes += 1;   
				
				}
			}		
		}
		break;  
	
		case PICT_txFont:
		{
			pg_word font_family_ID = 0;
			font_family_ID = GetMacWordInc(ptr2_data_bytes);
				
		}
		break;      
		
		case PICT_txFace:
		{
		 	pg_bits8 txFace = GetByteInc(ptr2_data_bytes);
		 	if (txFace & PICT_BOLD_TXT)
		 		font_stat.current_font.lfWeight = FW_BOLD; 
		 	else
		 		font_stat.current_font.lfWeight = FW_NORMAL; 
		 	
	        if (txFace & PICT_ITALIC_TXT)
	        	font_stat.current_font.lfItalic = TRUE;   
	        else
	        	font_stat.current_font.lfItalic = FALSE;   
	
	        if (txFace & PICT_UNDERLINE_TXT)
	        	font_stat.current_font.lfUnderline = TRUE;
	        else
	        	font_stat.current_font.lfUnderline = FALSE;
	        
	        if (!PICT_Is_Version_One)
	        	*ptr2_data_bytes += 1;	
		}
		break;
        
        case PICT_txRatio:
        {
         	
        	double YFontScale = 0.0;
          	
         	RetrievePt(ptr2_data_bytes, &num);
         	RetrievePt(ptr2_data_bytes, &denom);
         	YFontScale = ((double)(num.y) / (double)(denom.y));
			font_stat.current_font.lfHeight = (int)((double)font_stat.current_font.lfHeight * YFontScale);
        	
          
        }
        break;
		
		case PICT_txSize:
		{
			pg_word font_size = 0;
			font_size = GetMacWordInc(ptr2_data_bytes);   
			font_stat.current_font.lfHeight = -((short)font_size); 
			
			if (font_stat.current_font.lfHeight == 0)
				font_stat.current_font.lfHeight = -9;
				
		}
		break;
        
        case PICT_dhDvText:
        {
   	 	 	int 		horizontal_offset = 0;
	 	 	int 		vertical_offset = 0;
	 	 	int		 	string_length = 0;    
	 	 	memory_ref	mf_string_to_display = MEM_NULL;
	 	 	RECT		TxtRect = {0,0,0,0};
	 	 	pg_bits8    flDhDv = DVDH_FLAG;
	 	 	
	 	 	if (RetrieveDHDVText(ptr2_data_bytes, &string_length, &horizontal_offset, &vertical_offset, &mf_string_to_display, flDhDv))
	 	 	{
				DisposeTranslateOpcodesData(&mf_current_pnpat, &mpoly, &pen_stat, &brush_stat, &font_stat, &shortline_poly);
				return (NOSUCCESS);
	 	 	}
            
            if ((CreateAndSelectFont(hdc_metafile, &font_stat)) == NULL)
            {   
            	if (mf_string_to_display != MEM_NULL)
            	{
            	 	DisposeMemory(mf_string_to_display);
            	 	mf_string_to_display = MEM_NULL;
            	}
				DisposeTranslateOpcodesData(&mf_current_pnpat, &mpoly, &pen_stat, &brush_stat, &font_stat, &shortline_poly);
				return (NOSUCCESS);
            
            } 
         
			if (DhDvDrawText(hdc_metafile, &last_vertOffset, &last_horzOffset, &mf_string_to_display, string_length, 
  							vertical_offset, horizontal_offset, winExt, flDhDv))
  			{
  	 	 		DisposeMemory(mf_string_to_display);
	 	 		mf_string_to_display = MEM_NULL;
				DisposeTranslateOpcodesData(&mf_current_pnpat, &mpoly, &pen_stat, &brush_stat, &font_stat, &shortline_poly);
				return (NOSUCCESS);
			
  			
  			}

   	  		DisposeMemory(mf_string_to_display);
	  		mf_string_to_display = MEM_NULL;

		}
		break;

		case PICT_glyphState:
		{ 
		 
		 	if (PICT_Is_Version_One)
		 	{
				if (SkipOpCode(ptr2_data_bytes, current_opcode))
				{
					DisposeTranslateOpcodesData(&mf_current_pnpat, &mpoly, &pen_stat, &brush_stat, &font_stat, &shortline_poly);
					bNotFound = TRUE;
					return (NOSUCCESS);
				}
		 	}
		 	else
		 	{
			 	pg_word data_size = GetMacWordInc(ptr2_data_bytes); 
			 	outline_preferred = GetByteInc(ptr2_data_bytes);
			 	preserve_glyph = GetByteInc(ptr2_data_bytes);
			 	fractional_widths = GetByteInc(ptr2_data_bytes);
			 	scaling_disabled = GetByteInc(ptr2_data_bytes);
			 	if (!PICT_Is_Version_One)
			 	{
			 	 	if (data_size % 2)
			 	 		*ptr2_data_bytes += 1;
			 	} 
			 }
			 	
		}
		break;
		 
		case PICT_longText:
		{  
			POINT start_point = {0,0};
			pg_bits8 count = 0;   
			memory_ref mf_string_to_display = MEM_NULL;
			LPSTR ptr_string_to_display = NULL;
	 	 	pg_bits8    flDhDv = LONG_FLAG;

			RetrieveAdjustedScaledPt(ptr2_data_bytes, &start_point, XScaleFactor, YScaleFactor);  
 			count = GetByteInc(ptr2_data_bytes);   
			
		 	mf_string_to_display = MemoryAllocClear(m_globals, sizeof(char), count + 1, 0);
		 	if (mf_string_to_display == MEM_NULL)  
		 	{
				DisposeTranslateOpcodesData(&mf_current_pnpat, &mpoly, &pen_stat, &brush_stat, &font_stat, &shortline_poly);
				return (NOSUCCESS);
		 	} 
			 	 	
		 	ptr_string_to_display = (LPSTR)UseMemory(mf_string_to_display);
		 	pgBlockMove(*ptr2_data_bytes, ptr_string_to_display, count); 
		 	ptr_string_to_display[count] = 0;   

		 	UnuseMemory(mf_string_to_display);
		 	ptr_string_to_display = NULL;
		 	*ptr2_data_bytes += count; 
		 	if (!PICT_Is_Version_One)
		 	{
		 	 	if ((count + 1) % 2)
		 	 		*ptr2_data_bytes += 1;
		 	}
	         if ((CreateAndSelectFont(hdc_metafile, &font_stat)) == NULL)
	         {   
            	if (mf_string_to_display != MEM_NULL)
            	{
            	 	DisposeMemory(mf_string_to_display);
            	 	mf_string_to_display = MEM_NULL;
            	}
				DisposeTranslateOpcodesData(&mf_current_pnpat, &mpoly, &pen_stat, &brush_stat, &font_stat, &shortline_poly);
				return (NOSUCCESS);
	            
	         }
	          
			if (DhDvDrawText(hdc_metafile, &last_vertOffset, &last_horzOffset, &mf_string_to_display, count, 
  							start_point.y, start_point.x, winExt, flDhDv))
  			{
  	 	 		DisposeMemory(mf_string_to_display);
	 	 		mf_string_to_display = MEM_NULL;
				DisposeTranslateOpcodesData(&mf_current_pnpat, &mpoly, &pen_stat, &brush_stat, &font_stat, &shortline_poly);
				return (NOSUCCESS);
			
  			
  			} 
            
  	  		DisposeMemory(mf_string_to_display);
	  		mf_string_to_display = MEM_NULL;
		 	 
			
			
		}  
		break;
		
	 	case PICT_dhText:
	 	{
	 	 	int 		horizontal_offset = 0;
	 	 	int 		vertical_offset = 0;
	 	 	int		 	string_length = 0;    
	 	 	memory_ref	mf_string_to_display = MEM_NULL;
	 	 	RECT		TxtRect = {0,0,0,0};
	 	 	pg_bits8    flDhDv = DH_FLAG;
	 	 	
	 	 	if (RetrieveDHDVText(ptr2_data_bytes, &string_length, &horizontal_offset, &vertical_offset, &mf_string_to_display, flDhDv))
	 	 	{
				DisposeTranslateOpcodesData(&mf_current_pnpat, &mpoly, &pen_stat, &brush_stat, &font_stat, &shortline_poly);
				return (NOSUCCESS);
	 	 	}
            
            if ((CreateAndSelectFont(hdc_metafile, &font_stat)) == NULL)
            {   
            	if (mf_string_to_display != MEM_NULL)
            	{
            	 	DisposeMemory(mf_string_to_display);
            	 	mf_string_to_display = MEM_NULL;
            	}
				DisposeTranslateOpcodesData(&mf_current_pnpat, &mpoly, &pen_stat, &brush_stat, &font_stat, &shortline_poly);
				return (NOSUCCESS);
            
            } 
           
			if (DhDvDrawText(hdc_metafile, &last_vertOffset, &last_horzOffset, &mf_string_to_display, string_length, 
  							vertical_offset, horizontal_offset, winExt, flDhDv))
  			{
  	 	 		DisposeMemory(mf_string_to_display);
	 	 		mf_string_to_display = MEM_NULL;
				DisposeTranslateOpcodesData(&mf_current_pnpat, &mpoly, &pen_stat, &brush_stat, &font_stat, &shortline_poly);
				return (NOSUCCESS);
			
  			
  			} 

   	  		DisposeMemory(mf_string_to_display);
	  		mf_string_to_display = MEM_NULL;

	 	}       
	 	break;   
	 	
		case PICT_dvText:
		{
 	
	 	 	int 		horizontal_offset = 0;
	 	 	int 		vertical_offset = 0;
	 	 	int		 	string_length = 0;    
	 	 	memory_ref	mf_string_to_display = MEM_NULL;
	 	 	RECT		TxtRect = {0,0,0,0};
	 	 	pg_bits8    flDhDv = DV_FLAG;
	 	 	
	 	 	if (RetrieveDHDVText(ptr2_data_bytes, &string_length, &horizontal_offset, &vertical_offset, &mf_string_to_display, flDhDv))
	 	 	{
				DisposeTranslateOpcodesData(&mf_current_pnpat, &mpoly, &pen_stat, &brush_stat, &font_stat, &shortline_poly);
				return (NOSUCCESS);
	 	 	}
            
            if ((CreateAndSelectFont(hdc_metafile, &font_stat)) == NULL)
            {   
            	if (mf_string_to_display != MEM_NULL)
            	{
            	 	DisposeMemory(mf_string_to_display);
            	 	mf_string_to_display = MEM_NULL;
            	}
				DisposeTranslateOpcodesData(&mf_current_pnpat, &mpoly, &pen_stat, &brush_stat, &font_stat, &shortline_poly);
				return (NOSUCCESS);
            
            } 
    
  			if (DhDvDrawText(hdc_metafile, &last_vertOffset, &last_horzOffset, &mf_string_to_display, string_length, 
  							vertical_offset, horizontal_offset, winExt, flDhDv))
  			{
  	 	 		DisposeMemory(mf_string_to_display);
	 	 		mf_string_to_display = MEM_NULL;
				DisposeTranslateOpcodesData(&mf_current_pnpat, &mpoly, &pen_stat, &brush_stat, &font_stat, &shortline_poly);
				return (NOSUCCESS);
  			} 

   	  		DisposeMemory(mf_string_to_display);
	  		mf_string_to_display = MEM_NULL;

	 	 	
	 	}       
		break;
         
		case PICT_FillPat:
			{
				BITMAP bitmap;
				memory_ref mf_bitmap_bits = MEM_NULL;
				short count = 0, bAllZeros = TRUE;

				bitmap.bmType = 0;
				bitmap.bmWidth = 8;
				bitmap.bmHeight = 8;
				bitmap.bmWidthBytes = 2;
				bitmap.bmPlanes = 1;
				bitmap.bmBitsPixel = 1;
				bitmap.bmBits = NULL;

				mf_bitmap_bits = MemoryAlloc(m_globals, sizeof(pg_bits8), bitmap.bmHeight * bitmap.bmWidthBytes, 0);
				if (mf_bitmap_bits == MEM_NULL)
				{
					DisposeTranslateOpcodesData(&mf_current_pnpat, &mpoly, &pen_stat, &brush_stat, &font_stat, &shortline_poly);
					return (NOSUCCESS);
				}
				bitmap.bmBits = (pg_bits8_ptr)UseMemory(mf_bitmap_bits);
				for (count = 0; count < bitmap.bmHeight * bitmap.bmWidthBytes; count += 2)
				{
					*((pg_bits8_ptr)bitmap.bmBits + count) = ~(GetByteInc(ptr2_data_bytes));
					if (*((pg_bits8_ptr)bitmap.bmBits + count))
						bAllZeros = FALSE;
					*((pg_bits8_ptr)bitmap.bmBits + count + 1) = 0;
				}
				
				if (hnd_bitmap != NULL)
				{
					DeleteObject(hnd_bitmap);
					hnd_bitmap = NULL;
				}
				if (!(bAllZeros))
				{
					hnd_bitmap = CreateBitmapIndirect(&bitmap);
					if (hnd_bitmap == NULL)
					{

						UnuseMemory(mf_bitmap_bits);
						bitmap.bmBits = NULL;
						DisposeMemory (mf_bitmap_bits);
						mf_bitmap_bits = MEM_NULL;

						DisposeTranslateOpcodesData(&mf_current_pnpat, &mpoly, &pen_stat, &brush_stat, &font_stat, &shortline_poly);
						return (NOSUCCESS);
					}

					brush_stat.current_brush.lbStyle = BS_PATTERN;
					brush_stat.current_brush.lbHatch = (int)hnd_bitmap;
					if ((CreateAndSelectBrush(hdc_metafile, &brush_stat)) == NULL)
					{
						DeleteObject(hnd_bitmap);
						hnd_bitmap = NULL;
						UnuseMemory(mf_bitmap_bits);
						bitmap.bmBits = NULL;
						DisposeMemory (mf_bitmap_bits);
						mf_bitmap_bits = MEM_NULL;
						DisposeTranslateOpcodesData(&mf_current_pnpat, &mpoly, &pen_stat, &brush_stat, &font_stat, &shortline_poly);
						return (NOSUCCESS);
					}
				}
				else
				{

					brush_stat.current_brush.lbStyle = BS_SOLID;
					brush_stat.current_brush.lbHatch = 0;	
				}
				UnuseMemory(mf_bitmap_bits);
				bitmap.bmBits = NULL;
				DisposeMemory (mf_bitmap_bits);
				mf_bitmap_bits = MEM_NULL;
			}
			break;

		case PICT_fillRect:
		case PICT_paintRect:
			GetMacRectAndScaleInc(ptr2_data_bytes, &mrect,XScaleFactor, YScaleFactor);
		case PICT_fillSameRect:
		case PICT_paintSameRect:
			if (PictPaintRect(hdc_metafile, &mrect, &pen_stat, &brush_stat))
			{
				DisposeTranslateOpcodesData(&mf_current_pnpat, &mpoly, &pen_stat, &brush_stat, &font_stat, &shortline_poly);
				return (NOSUCCESS);
			}  

			break;

		case PICT_frameRect:
			GetMacRectInc(ptr2_data_bytes, &mrect);
			MyScaleRect(&mrect, XScaleFactor, YScaleFactor);
			
		case PICT_frameSameRect:
			if (PictFrameRect(hdc_metafile, &mrect, &pen_stat, &brush_stat))
			{
				DisposeTranslateOpcodesData(&mf_current_pnpat, &mpoly, &pen_stat, &brush_stat, &font_stat, &shortline_poly);
				return (NOSUCCESS);
			} 

			break;

		
		case PICT_fillRRect:
		case PICT_paintRRect:
			GetMacRectAndScaleInc(ptr2_data_bytes, &mrect,XScaleFactor, YScaleFactor);
		case PICT_fillSameRRect:
		case PICT_paintSameRRect:
			if (PictPaintRRect(hdc_metafile, &mrect, &ovSize, &pen_stat, &brush_stat))
			{
				DisposeTranslateOpcodesData(&mf_current_pnpat, &mpoly, &pen_stat, &brush_stat, &font_stat, &shortline_poly);
				return (NOSUCCESS);
			}
			break;

		case PICT_frameRRect:
			GetMacRectInc(ptr2_data_bytes, &mrect);
		case PICT_frameSameRRect:
			if (PictFrameRRect(hdc_metafile, &mrect, &ovSize, &pen_stat, &brush_stat))
			{
				DisposeTranslateOpcodesData(&mf_current_pnpat, &mpoly, &pen_stat, &brush_stat, &font_stat, &shortline_poly);
				return (NOSUCCESS);
			}
			break;

        case PICT_fillRgn:
		case PICT_paintRgn:
			 {
			  	mac_rect mrect = {0,0,0,0};
				if (GetMacRgnAndScaleRect(ptr2_data_bytes, &mrgn, &mrect, XScaleFactor, YScaleFactor))
				{
					DisposeTranslateOpcodesData(&mf_current_pnpat, &mpoly, &pen_stat, &brush_stat, &font_stat, &shortline_poly);
					return (NOSUCCESS);
				}
			  	if (mrgn.rgnSize == 0)  
			 		break;         
			 	if ((mrect.top == 0) && (mrect.bottom == 0) && (mrect.left == 0) && (mrect.right == 0))
			 		break;
			 	else
			 	{
			 		if (PictPaintRect(hdc_metafile, &mrect, &pen_stat, &brush_stat))
					{
						DisposeTranslateOpcodesData(&mf_current_pnpat, &mpoly, &pen_stat, &brush_stat, &font_stat, &shortline_poly);
						return (NOSUCCESS);
					}  

			 	}
 
			 }
			 break;


		case PICT_LongComment:
			if (SkipLongComment(ptr2_data_bytes))
			{
				DisposeTranslateOpcodesData(&mf_current_pnpat, &mpoly, &pen_stat, &brush_stat, &font_stat, &shortline_poly);
				return (NOSUCCESS);
			}
			break;
		
		case PICT_ShortComment:
			*ptr2_data_bytes += 2;
			break;

		
		case PICT_framePoly:
			numPoints = 0;
			if (RetrievePolyParams(ptr2_data_bytes, &mpoly, &numPoints))
			{
				DisposeTranslateOpcodesData(&mf_current_pnpat, &mpoly, &pen_stat, &brush_stat, &font_stat, &shortline_poly);
				return (NOSUCCESS);
			}
		case PICT_frameSamePoly:  
			if (PictFramePoly(hdc_metafile, &mpoly, numPoints, &pen_stat, &brush_stat, 
				  XScaleFactor, YScaleFactor))
			{
				DisposeTranslateOpcodesData(&mf_current_pnpat, &mpoly, &pen_stat, &brush_stat, &font_stat, &shortline_poly);
				return (NOSUCCESS);
			}  

			break;
		
		case PICT_fillPoly:
		case PICT_paintPoly:
			numPoints = 0;
			if (RetrievePolyParams(ptr2_data_bytes, &mpoly, &numPoints))
			{
				DisposeTranslateOpcodesData(&mf_current_pnpat, &mpoly, &pen_stat, &brush_stat, &font_stat, &shortline_poly);
				return (NOSUCCESS);
			}
		case PICT_fillSamePoly:
		case PICT_paintSamePoly:
			if (PictPaintPoly(hdc_metafile, &mpoly, numPoints, &pen_stat, &brush_stat, 
				  XScaleFactor, YScaleFactor))
			{
				DisposeTranslateOpcodesData(&mf_current_pnpat, &mpoly, &pen_stat, &brush_stat, &font_stat, &shortline_poly);
				return (NOSUCCESS);
			} 
			break;
		 
		case PICT_fillSameRgn:
		case PICT_frameSameRgn:
			{
				if (PICT_Is_Version_One)
				{
					if (SkipOpCode(ptr2_data_bytes, current_opcode))
					{
						DisposeTranslateOpcodesData(&mf_current_pnpat, &mpoly, &pen_stat, &brush_stat, &font_stat, &shortline_poly);
						bNotFound = TRUE;
						return (NOSUCCESS);
					}
				}   
				else
				{
					
					mac_rect mrect = {0,0,0,0}; 
					if (mrgn.rgnSize == 0)
					{
						DisposeTranslateOpcodesData(&mf_current_pnpat, &mpoly, &pen_stat, &brush_stat, &font_stat, &shortline_poly);
						return (NOSUCCESS); 
					}
					MacRgnToMacRect(&mrgn, &mrect);
					if (PictPaintRect(hdc_metafile, &mrect, &pen_stat, &brush_stat))
					{
						DisposeTranslateOpcodesData(&mf_current_pnpat, &mpoly, &pen_stat, &brush_stat, &font_stat, &shortline_poly);
						return (NOSUCCESS);
					}
				} 
			} 
	  
		    break;
		    
		case PICT_frameRgn:
			 {
			 	  mac_rect mrect = {0,0,0,0};
			 	  GetMacRgnAndScaleRect(ptr2_data_bytes, &mrgn, &mrect, XScaleFactor, YScaleFactor);  
			 	  if (mrgn.rgnSize == 0)  
			 	  	break;
			 	  else
			 	  {
			 	 	if (PictFrameRect(hdc_metafile, &mrect, &pen_stat, &brush_stat))
					{
						DisposeTranslateOpcodesData(&mf_current_pnpat, &mpoly, &pen_stat, &brush_stat, &font_stat, &shortline_poly);
						return (NOSUCCESS);
					}  

			 	  } 

			 }
			 break; 
		
			
		
		case PICT_fillOval:
		case PICT_paintOval:
			GetMacRectAndScaleInc(ptr2_data_bytes, &mrect,XScaleFactor, YScaleFactor);
		case PICT_fillSameOval:
		case PICT_paintSameOval:
			if (PictPaintOval(hdc_metafile, &pen_stat, &brush_stat, &mrect))
			{
				DisposeTranslateOpcodesData(&mf_current_pnpat, &mpoly, &pen_stat, &brush_stat, &font_stat, &shortline_poly);
				return (NOSUCCESS);
			}
			break;

		case PICT_frameOval:
			GetMacRectAndScaleInc(ptr2_data_bytes, &mrect,XScaleFactor, YScaleFactor);
		case PICT_frameSameOval:
			if (PictFrameOval(hdc_metafile, &pen_stat, &brush_stat, &mrect))
			{
				DisposeTranslateOpcodesData(&mf_current_pnpat, &mpoly, &pen_stat, &brush_stat, &font_stat, &shortline_poly);
				return (NOSUCCESS);
			}
			break;

	
		//skip opcodes DirectBitsRgn,bitsRgn,PackBitsRgn
		case PICT_directBitsRgn:
			{
				if (PICT_Is_Version_One)
				{
					if (SkipOpCode(ptr2_data_bytes, current_opcode))
					{
						DisposeTranslateOpcodesData(&mf_current_pnpat, &mpoly, &pen_stat, &brush_stat, &font_stat, &shortline_poly);
						bNotFound = TRUE;
						return (NOSUCCESS);
					}
				
				   break;
				}
				else
				{
					DWORD baseAddr = GetMacDWORDInc(ptr2_data_bytes);
					if (baseAddr != PICT_DirectBits_BaseAddr)
					{
						DisposeTranslateOpcodesData(&mf_current_pnpat, &mpoly, &pen_stat, &brush_stat, &font_stat, &shortline_poly);
						return (NOSUCCESS);
					}     
				}
			}
		case PICT_bitsRgn:
		case PICT_packBitsRgn:
			{ 
				int bBitsRect = FALSE;
				if (current_opcode == PICT_bitsRgn)
					bBitsRect = TRUE;
				if (RetrieveAndDrawPixmapRgn(ptr2_data_bytes, hdc_metafile, XScaleFactor,
					YScaleFactor, winExt, bBitsRect))
				{
					DisposeTranslateOpcodesData(&mf_current_pnpat, &mpoly, &pen_stat, &brush_stat, &font_stat, &shortline_poly);
					return(NOSUCCESS);
				}
			}
			break;


		case PICT_DirectBitsRect:
			{     
				if (PICT_Is_Version_One)
				{
					if (SkipOpCode(ptr2_data_bytes, current_opcode))
					{
						DisposeTranslateOpcodesData(&mf_current_pnpat, &mpoly, &pen_stat, &brush_stat, &font_stat, &shortline_poly);
						bNotFound = TRUE;
						return (NOSUCCESS);
					}
				
				   break;
				}
				else
				{
					DWORD baseAddr = GetMacDWORDInc(ptr2_data_bytes);
					if (baseAddr != PICT_DirectBits_BaseAddr)
					{
						DisposeTranslateOpcodesData(&mf_current_pnpat, &mpoly, &pen_stat, &brush_stat, &font_stat, &shortline_poly);
						return (NOSUCCESS);
					}
				}
			}
	
		case PICT_BitsRect:
		case PICT_PackBitsRect:
			{
				int bBitsRect = FALSE;  
				if (current_opcode == PICT_BitsRect)
					bBitsRect = TRUE;
				if (RetrieveAndDrawPixmap(ptr2_data_bytes, hdc_metafile, XScaleFactor,
					YScaleFactor, winExt,bBitsRect))
				{
					DisposeTranslateOpcodesData(&mf_current_pnpat, &mpoly, &pen_stat, &brush_stat, &font_stat, &shortline_poly);
					return(NOSUCCESS);
				}
			}
			break;

		case PICT_frameArc:
			GetArcParams(ptr2_data_bytes, &mrect, &startAngle, &angleRun, &rect_center,
				XScaleFactor, YScaleFactor);
			if (FrameArc(hdc_metafile, &mrect, &rect_center, startAngle, angleRun, 
				&pen_stat, &brush_stat))
			{
				DisposeTranslateOpcodesData(&mf_current_pnpat, &mpoly, &pen_stat, &brush_stat, &font_stat, &shortline_poly);
				return (NOSUCCESS);
			}
			break;

		case PICT_frameSameArc:
			GetArcAngles(ptr2_data_bytes, &startAngle, &angleRun);
			GetRectCenter(&mrect, &rect_center);
			if (FrameArc(hdc_metafile, &mrect, &rect_center, startAngle, angleRun, 
				&pen_stat, &brush_stat))
			{
				DisposeTranslateOpcodesData(&mf_current_pnpat, &mpoly, &pen_stat, &brush_stat, &font_stat, &shortline_poly);
				return (NOSUCCESS);
			}
			break;

		case PICT_fillArc:
		case PICT_paintArc:
			GetArcParams(ptr2_data_bytes, &mrect, &startAngle, &angleRun, &rect_center, 
				XScaleFactor, YScaleFactor);
			if(PaintPieWedge(hdc_metafile, &mrect, &rect_center, startAngle, angleRun, 
				&pen_stat, &brush_stat))
			{
				DisposeTranslateOpcodesData(&mf_current_pnpat, &mpoly, &pen_stat, &brush_stat, &font_stat, &shortline_poly);
				return (NOSUCCESS);
			}
			break;

		case PICT_fillSameArc:
		case PICT_paintSameArc:
			GetArcAngles(ptr2_data_bytes, &startAngle, &angleRun);
			GetRectCenter(&mrect, &rect_center);
			if(PaintPieWedge(hdc_metafile, &mrect, &rect_center, startAngle, angleRun, 
				&pen_stat, &brush_stat))
			{
				DisposeTranslateOpcodesData(&mf_current_pnpat, &mpoly, &pen_stat, &brush_stat, &font_stat, &shortline_poly);
				return (NOSUCCESS);
			}
			break;

		default:
			if (SkipOpCode(ptr2_data_bytes, current_opcode))
			{
				DisposeTranslateOpcodesData(&mf_current_pnpat, &mpoly, &pen_stat, &brush_stat, &font_stat, &shortline_poly);
				bNotFound = TRUE;
				return (NOSUCCESS);
			}
			break;
		} // end switch
	
		previous_opcode = current_opcode;
		current_opcode = GetOpCodeInc(ptr2_data_bytes);
		if (current_opcode == PICT_opEndPic)
		{	
			int bEndFound = TRUE;
		}
	}
	RestoreDC(hdc_metafile, -1);
	DisposeTranslateOpcodesData(&mf_current_pnpat, &mpoly, &pen_stat, &brush_stat, &font_stat, &shortline_poly);
    last_vertOffset = 0;
    last_horzOffset = 0;
	return (SUCCESS);
}



// Main function to translate a Pict to a metafile
// returns null if fails, otherwise returns a handle to the new metafile. 
// Increments ptr
PG_PASCAL (HMETAFILE) TranslatePictToWMFInc (pgm_globals_ptr mem, pg_bits8_ptr PG_FAR * ptr2_data_bytes, long data_size_bytes, POINT PG_FAR * winExt, 
								  HDC target_dc)
{
	 mac_pict_header		pic_header;
	 HMETAFILE				hdl_metafile = NULL;	
	 HDC					hdc_metafile = NULL;  
	 pg_bits8_ptr 			ptr_original = *ptr2_data_bytes;
	 POINT					org = {0,0};
	 
   m_globals = mem;

	if (GetMacPictHeaderInc(ptr2_data_bytes, &pic_header)) 
	{
		*ptr2_data_bytes = ptr_original;	
		return (NULL);       
	}  
	if (!PICT_Is_Version_One)  
	{
		if ((pic_header.fBBox.bottom.mac_fixed_int == -1) || 
			((pic_header.fBBox.top.mac_fixed_int != 288) && (pic_header.fBBox.left.mac_fixed_int != 288)))
		{
			winExt->y = pic_header.picFrame.bottom - pic_header.picFrame.top;
			winExt->x = pic_header.picFrame.right - pic_header.picFrame.left;
			org.x = pic_header.picFrame.left;
			org.y = pic_header.picFrame.top;
		} 
		else
		{
			winExt->y = pic_header.fBBox.right.mac_fixed_int- pic_header.fBBox.bottom.mac_fixed_int;
			winExt->x = pic_header.fBBox.right.mac_fixed_fract - pic_header.fBBox.bottom.mac_fixed_fract;
			org.x = pic_header.fBBox.bottom.mac_fixed_fract;
			org.y = pic_header.fBBox.bottom.mac_fixed_int;
	
		}
	}
	else
	{
		
		winExt->y = pic_header.picFrame.bottom - pic_header.picFrame.top;
		winExt->x = pic_header.picFrame.right - pic_header.picFrame.left;
		org.x = pic_header.picFrame.left;
		org.y = pic_header.picFrame.top;
	
	}
	

	if (IsBadPictHeader(&pic_header, data_size_bytes))
	{
		*ptr2_data_bytes = ptr_original;	
		return (NULL);       
	}
	
	hdc_metafile = CreateMetaFile(NULL); //create memory metafile
	if (hdc_metafile == NULL)
	{
		*ptr2_data_bytes = ptr_original;	
		return (NULL);       
	}  
	
	SetMapMode(hdc_metafile, MM_ANISOTROPIC);
	SetWindowOrgEx(hdc_metafile,org.x,org.y, NULL);

	SetWindowExtEx(hdc_metafile, winExt->x , winExt->y, NULL);
	
	SetROP2(hdc_metafile, R2_COPYPEN);
	SetTextAlign(hdc_metafile, TA_BASELINE);
	SetTextColor(hdc_metafile, RGB(0,0,0));
	SetBkColor(hdc_metafile, RGB(255,255,255));
	SetBkMode(hdc_metafile, TRANSPARENT);

	if (TranslatePictOpCodes (ptr2_data_bytes, hdc_metafile, winExt, target_dc))
	{
		hdl_metafile = CloseMetaFile(hdc_metafile);
		DeleteDC(hdc_metafile);
		if (hdl_metafile != NULL)
			DeleteMetaFile(hdl_metafile); 
		*ptr2_data_bytes = ptr_original;	

		return (NULL);
	}

	hdl_metafile = CloseMetaFile(hdc_metafile);
	DeleteDC(hdc_metafile);
	if (hdl_metafile == NULL)
	{
		*ptr2_data_bytes = ptr_original;	
		return (NULL);       
	}
    
    
 	*ptr2_data_bytes = ptr_original;
	 PICT_Is_Version_One = FALSE;	
	return (hdl_metafile);
}

#endif



