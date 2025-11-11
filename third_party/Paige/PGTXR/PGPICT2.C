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

extern int	PICT_Is_Version_One;
extern pgm_globals_ptr m_globals;

// returns byte in data array and increments ptr to next byte
pg_byte GetByteInc(pg_bits8_ptr PG_FAR * ptr2_data_bytes)
{
	pg_byte new_byte = **ptr2_data_bytes;
	*ptr2_data_bytes += 1;
	return (new_byte);
}


// reads and returns a macintosh word, and incremetns ptr to next byte
pg_word GetMacWordInc (pg_bits8_ptr PG_FAR * ptr2_data_bytes)
{
		pg_word new_word = (**ptr2_data_bytes);
		new_word = (new_word << 8);
		*ptr2_data_bytes += 1;
		new_word |= **ptr2_data_bytes;
		*ptr2_data_bytes += 1;

		return (new_word);
}

pg_word GetOpCodeInc(pg_bits8_ptr PG_FAR * ptr2_data_bytes)
{
	pg_word new_word = 0;

	if (PICT_Is_Version_One)
	{
		new_word = **ptr2_data_bytes;
		*ptr2_data_bytes += 1;

		return (new_word);
	}
	else
	{
		return (GetMacWordInc(ptr2_data_bytes));
	}
	return (new_word);
}

// retrieves a DWORD in the Macintosh MSB byte order and increments pointer
// to next byte
DWORD GetMacDWORDInc (pg_bits8_ptr PG_FAR * ptr2_data_bytes)
{
	register DWORD new_dword;
	
	new_dword = ((DWORD)(**ptr2_data_bytes & 0xFF));
	*ptr2_data_bytes += 1;
	new_dword = new_dword << 8;

	new_dword |= ((DWORD) (**ptr2_data_bytes & 0xFF));
	*ptr2_data_bytes += 1;
	new_dword = new_dword << 8;

	new_dword |= ((DWORD) (**ptr2_data_bytes & 0xFF));
	*ptr2_data_bytes += 1;;
	new_dword = new_dword << 8;

	new_dword |= ((DWORD) (**ptr2_data_bytes & 0xFF));
	*ptr2_data_bytes += 1;;

	return (new_dword);
}


// fills a Macintosh Rect structur from array of data bytes and 
// increments the ptr.
void GetMacRectInc(pg_bits8_ptr PG_FAR * ptr2_data_bytes, mac_rect PG_FAR * mrect)
{
	mrect->top = (short)GetMacWordInc(ptr2_data_bytes);
	mrect->left = (short)GetMacWordInc(ptr2_data_bytes);
	mrect->bottom = (short)GetMacWordInc(ptr2_data_bytes);
	mrect->right = (short)GetMacWordInc(ptr2_data_bytes);

	return;
}



//fills a mac_fixed structure with data from an array of bytes and
// increments the ptr
void GetMacFixedInc(pg_bits8_ptr PG_FAR * ptr2_data_bytes, mac_fixed PG_FAR * tmp_fixed)
{
	
	tmp_fixed->mac_fixed_int = (short)GetMacWordInc(ptr2_data_bytes);
	tmp_fixed->mac_fixed_fract =  (pg_word)GetMacWordInc(ptr2_data_bytes);
	return;

}




// fills a mac_fpREct structure with data froma byte array, incremetns ptr to
// data
void GetMacFixedRectInc(pg_bits8_ptr PG_FAR * ptr2_data_bytes, mac_fpRect PG_FAR * fp_rect)
{
	GetMacFixedInc(ptr2_data_bytes, &(fp_rect->top ));
	GetMacFixedInc(ptr2_data_bytes, &(fp_rect->left));
	GetMacFixedInc(ptr2_data_bytes, &(fp_rect->bottom));
	GetMacFixedInc(ptr2_data_bytes, &(fp_rect->right));


	return;
}


// retrieves a mac_rgn structure from byte array and increments ptr.
int GetMacRgn(pg_bits8_ptr PG_FAR * ptr2_data_bytes, mac_rgn PG_FAR * clip_rgn)
{
	int bError = FALSE;

	clip_rgn->rgnSize = (short)GetMacWordInc(ptr2_data_bytes);
	GetMacRectInc(ptr2_data_bytes, &(clip_rgn->rgnBBox));
	if (clip_rgn->rgnSize == 10)
	{
		return(SUCCESS);
	}
	else
	{
	//	bError = TRUE;
		// for now skip data  
		if (!PICT_Is_Version_One)
		{
			if (clip_rgn->rgnSize % 2)
				clip_rgn->rgnSize++;
		}
		*ptr2_data_bytes += clip_rgn->rgnSize - 10;
	//	return(NOSUCCESS);
	}

	return(SUCCESS);
}

//Gets the parameters passed with the PICT_Clip opcode
// and calls associated metafile commands.
// returns 0 on success and 1 on failure
int DealWithClip(pg_bits8_ptr PG_FAR * ptr2_data_bytes, HDC hdc_metafile)
{
	mac_rgn clip_rgn;
	
	if (GetMacRgn(ptr2_data_bytes, &clip_rgn))
		return (NOSUCCESS);
	
	return(SUCCESS);
}

//retrieves the parameters for ShortLine opcode and performs associated
// metafile calls, returns 0 if succes else 1.
int DealWithShortLine(pg_bits8_ptr PG_FAR * ptr2_data_bytes, short_polyline PG_FAR * shortline_poly, double XScaleFactor, double YScaleFactor, HDC hdc_metafile)
{
	POINT	start_loc = {0,0};
	int	dx = 0, dy = 0;
	signed char scdx = 0, scdy = 0;
	POINT PG_FAR* ptr_poly_points = NULL;
	
	if (shortline_poly->polyPoints != MEM_NULL)
		return(NOSUCCESS);
	
	if (shortline_poly->index != 0)
		return (NOSUCCESS);
		
	if (shortline_poly->numPoints != 0)
		return(NOSUCCESS);
	
	shortline_poly->polyPoints = MemoryAllocClear(m_globals, sizeof(POINT),SHORT_POLYLINE_INCREASE, SHORT_POLYLINE_INCREASE);
	if (shortline_poly->polyPoints == MEM_NULL)
		return (NOSUCCESS);
		
	ptr_poly_points = (POINT PG_FAR*)UseMemory(shortline_poly->polyPoints);
	
	start_loc.y = (short)GetMacWordInc(ptr2_data_bytes);  
	ptr_poly_points[(shortline_poly->index)].y  = start_loc.y;
	start_loc.x = (short)GetMacWordInc(ptr2_data_bytes); 
	ptr_poly_points[(shortline_poly->index)].x = start_loc.x;
	(shortline_poly->index)++;
	(shortline_poly->numPoints)++;
	
	dx = (signed char)GetByteInc(ptr2_data_bytes);
	dx = (signed char)GetByteInc(ptr2_data_bytes);  
    
    if ((dx != 0 ) || (dy != 0))
    {
		start_loc.y += dy;
		start_loc.x += dx;
		ptr_poly_points[shortline_poly->index].y  = start_loc.y;
	  	ptr_poly_points[shortline_poly->index].x = start_loc.x;
	  	(shortline_poly->index)++;
		(shortline_poly->numPoints)++;  
	}
    
    UnuseMemory(shortline_poly->polyPoints);
    ptr_poly_points = NULL;
	return (SUCCESS);
}


//Skips long comments in opcodes, returns 0 if succeeds, 1 if fails.
int SkipLongComment(pg_bits8_ptr PG_FAR * ptr2_data_bytes)
{
	pg_word kind = 0;
	pg_word	size = 0;

	kind = GetMacWordInc (ptr2_data_bytes);
	size = GetMacWordInc (ptr2_data_bytes);
	if (!size)
		return (SUCCESS);
	else
	{
		if (!PICT_Is_Version_One)
		{
			if (size % 2)
			size++;      
		}
		*ptr2_data_bytes += size;
	}
	return (SUCCESS);
}

// Handles shortLineFrom opcode, returns 0 if success else 1
int DealShortLineFrom(pg_bits8_ptr PG_FAR * ptr2_data_bytes, short_polyline PG_FAR * shortline_poly, double XScaleFactor, double YScaleFactor, HDC hdc_metafile)
{
	int dx = 0, dy= 0;
 	signed char scdx = 0, scdy = 0;
	POINT PG_FAR* ptr_poly_points = NULL;
    long numRecs = 0;
	if (shortline_poly->polyPoints == MEM_NULL)
		return(NOSUCCESS);
	
	if (shortline_poly->index == 0)
		return (NOSUCCESS);
		
	if (shortline_poly->numPoints == 0)
		return(NOSUCCESS);
   
   	numRecs = GetMemorySize(shortline_poly->polyPoints); 
    if ((shortline_poly->index) >= (numRecs))
    {
     	SetMemorySize(shortline_poly->polyPoints, numRecs + SHORT_POLYLINE_INCREASE);
    
    }
	dx = (signed char)GetByteInc(ptr2_data_bytes);
	dy = (signed char)GetByteInc(ptr2_data_bytes);  
     
    if ((dx != 0) || (dy != 0))
    {
		ptr_poly_points = (POINT PG_FAR*)UseMemory(shortline_poly->polyPoints);

		ptr_poly_points[shortline_poly->index].y = ptr_poly_points[shortline_poly->index - 1].y + dy;
		ptr_poly_points[shortline_poly->index].x = ptr_poly_points[shortline_poly->index - 1].x + dx;  
		(shortline_poly->index)++;
		(shortline_poly->numPoints)++;
	
		UnuseMemory(shortline_poly->polyPoints);
		ptr_poly_points = NULL;
	}
	return (SUCCESS);
}

//retrieves rgbMac color information from byte stream, converts it to Windows
// RGBTRIPLE and passes it back.
void RetrieveForeColor(pg_bits8_ptr PG_FAR * ptr2_data_bytes, RGBTRIPLE PG_FAR * current_forecolor)
{
	rgbMac mac_forecolor;

	mac_forecolor.red = GetMacWordInc(ptr2_data_bytes);
	mac_forecolor.green = GetMacWordInc(ptr2_data_bytes);
	mac_forecolor.blue = GetMacWordInc(ptr2_data_bytes);

	current_forecolor->rgbtRed = (pg_bits8)((mac_forecolor.red & 0xFF00) >> 8);
	current_forecolor->rgbtGreen = (pg_bits8)((mac_forecolor.green & 0xFF00) >> 8);
	current_forecolor->rgbtBlue = (pg_bits8)((mac_forecolor.blue & 0xFF00) >> 8);

	return;
}


//retrieves a macintosh PenPat from byte array
//returns 0 if successful, 1 if fails.
int RetrievePenPat(pg_bits8_ptr PG_FAR * ptr2_data_bytes, memory_ref PG_FAR * mfptr_pnpat)
{
	pg_bits8_ptr pat= NULL;

	if (*mfptr_pnpat != MEM_NULL)
	{
		DisposeMemory(*mfptr_pnpat);
		*mfptr_pnpat = MEM_NULL;
	}
	*mfptr_pnpat = MemoryAlloc(m_globals, sizeof(pg_byte), 8, 0);
	if (*mfptr_pnpat == MEM_NULL)
		return (NOSUCCESS);

	pat = (pg_bits8_ptr)UseMemory(*mfptr_pnpat);
	memcpy(pat, *ptr2_data_bytes, 8);
	*ptr2_data_bytes += 8;
	UnuseMemory(*mfptr_pnpat);
	pat = NULL;

	return (SUCCESS);
	

}
 
 //Fills mac_pict_header structure from byte array and icrements ptr.
int GetMacPictHeaderInc(pg_bits8_ptr PG_FAR * ptr2_data_bytes, mac_pict_header PG_FAR * pic_header)
{
	pic_header->picSize = GetMacWordInc(ptr2_data_bytes);
	GetMacRectInc(ptr2_data_bytes, &(pic_header->picFrame));
	pic_header->version_op = GetMacWordInc(ptr2_data_bytes);
	if (pic_header->version_op == PICT_VersionOp)
	{
		pic_header->version = GetMacWordInc(ptr2_data_bytes);
		pic_header->header_op = GetMacWordInc(ptr2_data_bytes);
		pic_header->pict_size = GetMacDWORDInc(ptr2_data_bytes);
		GetMacFixedRectInc(ptr2_data_bytes, &(pic_header->fBBox));
		pic_header->reserved1 = GetMacDWORDInc(ptr2_data_bytes);
		PICT_Is_Version_One = FALSE;

		return (SUCCESS);
	}
	else if (pic_header->version_op == PICT_VersionOP1)
	{

		pic_header->version_op = PICT_VersionOp;
		pic_header->version = PICT_Version1_Version; 
  		PICT_Is_Version_One = TRUE;
		return(SUCCESS);

	}
	else
	{
		return (NOSUCCESS);
	}
	return (SUCCESS);

}

// Check the information in the PICT header to see if its valid
// if it is return 0, else return 1.
int IsBadPictHeader(mac_pict_header PG_FAR * pic_header, long data_size_bytes)
{


	if (pic_header->version_op != PICT_VersionOp)
		return (NOSUCCESS);
	
	if (PICT_Is_Version_One)
	{
		if (pic_header->version != PICT_Version1_Version)
			return (NOSUCCESS);
	}
	else
	{
		if (pic_header->version != PICT_Version)
			return (NOSUCCESS);

		if (pic_header->header_op != PICT_HeaderOp)
			return (NOSUCCESS);
		
	}


	return (SUCCESS);


}



int RetrievePolyParams(pg_bits8_ptr PG_FAR * ptr2_data_bytes, pmac_poly mpoly, long PG_FAR * numPoints)
{
	POINT PG_FAR * ptr_Points = NULL;
	int i = 0;


	if (mpoly->polyPoints != MEM_NULL)
	{
		DisposeMemory(mpoly->polyPoints);
		mpoly->polyPoints = MEM_NULL;
	}

	mpoly->polySize = (short)GetMacWordInc(ptr2_data_bytes);
	*numPoints = ((mpoly->polySize - 10) / 4);
	GetMacRectInc(ptr2_data_bytes, &(mpoly->polyBBox));
	mpoly->polyPoints = MemoryAlloc(m_globals, sizeof(POINT), *numPoints, 0);
	if (mpoly->polyPoints == MEM_NULL)
		return (NOSUCCESS);

	ptr_Points = (POINT PG_FAR *)UseMemory(mpoly->polyPoints);
	for (i = 0; i < *numPoints; i++)
	{
		ptr_Points[i].y = (short)GetMacWordInc(ptr2_data_bytes);
		ptr_Points[i].x = (short)GetMacWordInc(ptr2_data_bytes);

	}
    if (!PICT_Is_Version_One)
    {
    	if (mpoly->polySize % 2)
    		*ptr2_data_bytes += 1;
    }
    
	UnuseMemory(mpoly->polyPoints);
	ptr_Points = NULL;
     
	return (SUCCESS);
}

//Returns valid COLORREF if successful, else returns -1 in high byte.
COLORREF ConvertRGBTtoColorRef(RGBTRIPLE PG_FAR * color)
{
	COLORREF convertedCR = RGB(0,0,0);

	convertedCR = (convertedCR | (*color).rgbtBlue) << 8;
	convertedCR = (convertedCR | (*color).rgbtGreen) << 8;
	convertedCR = (convertedCR | (*color).rgbtRed);

	return(convertedCR);
}

// scales mac pts to current resolution on PC.
void ScalePt(POINT PG_FAR * pt, double XScaleFactor, double YScaleFactor)
{
	pt->x = (short)(((double)(pt->x) * XScaleFactor) /*+ 0.5 */);

	pt->y = (short)(((double)(pt->y) * YScaleFactor) /* + 0.5 */);


	return;
}



void MyScaleRect(mac_rect PG_FAR * rectangle, double XScaleFactor, double YScaleFactor)
{


	rectangle->left = (short)(((double)(rectangle->left) * XScaleFactor));
	rectangle->right = (short)(((double)(rectangle->right) * XScaleFactor));
	rectangle->top = (short)(((double)(rectangle->top) * YScaleFactor));
	rectangle->bottom = (short)(((double)(rectangle->bottom) * YScaleFactor));
	return;
}
   
   

void AdjustRect(mac_rect PG_FAR * mrect)
{
	if (mrect->top < 0)
	{
		mrect->bottom -=mrect->top;
		mrect->top = 0;
	}
	if (mrect->left < 0)
	{
		mrect->right -=mrect->left;
		mrect->left = 0;
	}

	return;
}


void GetMacRectAndScaleInc(pg_bits8_ptr PG_FAR * ptr2_data_bytes, mac_rect PG_FAR * mrect, double XScaleFactor, double YScaleFactor)
{
	GetMacRectInc(ptr2_data_bytes, mrect);
	AdjustRect(mrect);
	MyScaleRect(mrect, XScaleFactor, YScaleFactor);

	return;
}


int GetMacRgnAndScaleRect(pg_bits8_ptr PG_FAR * ptr2_data_bytes, mac_rgn PG_FAR * mrgn, mac_rect PG_FAR * mrect, double XScaleFactor, double YScaleFactor)
{
	int bError = FALSE;
	if(GetMacRgn(ptr2_data_bytes, mrgn))
		bError = TRUE;
	MacRgnToMacRect(mrgn, mrect);
	AdjustRect(mrect);
	MyScaleRect(mrect, XScaleFactor, YScaleFactor);
	
	if (bError)
		return (NOSUCCESS);
	else
		return(SUCCESS);
}


void GetRectCenter(mac_rect PG_FAR * mrect, POINT PG_FAR * center)
{
	long width = 0, height = 0;

	if (((mrect->left < 0) && (mrect->right < 0)) || ((mrect->left >= 0) && (mrect->right >= 0)))
	{
		width = abs(mrect->left - mrect->right);
	}
	else
		width = abs((*mrect).left) + abs((*mrect).right);

	if (((mrect->top < 0) && (mrect->bottom < 0)) || ((mrect->top >= 0) && (mrect->bottom >= 0)))
		height = abs(mrect->top - mrect->bottom);
	else
		height = abs((*mrect).top) + abs((*mrect).bottom);

	width = (int) (((double)width / (double)2) + 0.5);
	height = (int) (((double)height / (double)2) + 0.5);

	(*center).x = (pg_word)(mrect->left + width);
	(*center).y = (pg_word)(mrect->top + height);

	return;
}



void DegreesToRadians(short startAngle, double PG_FAR * rad_start_angle)
{	
	
	*rad_start_angle = (double)startAngle / NUM_CIRCLE_DEGREES;
	*rad_start_angle *= TWO_PI;
	return;

}

void CalculateEndAngle (short startAngle, short angleRun, short PG_FAR * end_angle, int PG_FAR * bClockWise)
{
	*end_angle = (startAngle) + angleRun;

	*end_angle = *end_angle % NUM_CIRCLE_DEGREES;

	if (startAngle > (*end_angle))
		*bClockWise = TRUE;
	else
		*bClockWise = FALSE;

	return;

}



int IsHorizontalEllipse(mac_rect PG_FAR * mrect, double PG_FAR * semi_major_axis, double PG_FAR * semi_minor_axis)
{
	long width = 0, height = 0;
	
	
	if ((((*mrect).top < 0) && ((*mrect).bottom < 0)) 
		|| (((*mrect).top >= 0) && ((*mrect).bottom >= 0)))
	{
		height = abs((*mrect).top - (*mrect).bottom);
	}
	else
		height = abs((*mrect).top) + abs((*mrect).bottom);


	if ((((*mrect).left < 0) && ((*mrect).right < 0)) 
		|| (((*mrect).left >= 0) && ((*mrect).right >= 0)))
	{
		width = abs((*mrect).left - (*mrect).right);
	}
	else
		width = abs((*mrect).left) + abs((*mrect).right);
		
	if (width >= height)
	{
		*semi_major_axis = (double)width / (double)2;
		*semi_minor_axis = (double)height / (double)2;
		return (TRUE);
	}
	else
	{
		*semi_major_axis = (double)height / (double)2;
		*semi_minor_axis = (double)width / (double)2;
		return (FALSE);
	}

}



void CalculateArcPoints(mac_rect PG_FAR * mrect, POINT PG_FAR * rect_center, double semi_major_axis, double semi_minor_axis, 
						double rad_start_angle, double rad_end_angle, POINT PG_FAR * start_arc, POINT PG_FAR * end_arc, int bHorizontal,
						short bStartXPos, short bStartYPos, short bEndXPos, short bEndYPos)
{
	if (bHorizontal)
	{

		(*start_arc).x = (short)((semi_major_axis * (cos(rad_start_angle))) + 0.5);
		(*start_arc).y = ((short)((semi_minor_axis * (sin(rad_start_angle))) + 0.5));
		(*end_arc).x   = (short) ((semi_major_axis * (cos(rad_end_angle))) + 0.5);
		(*end_arc).y   = ((short) ((semi_minor_axis * (sin(rad_end_angle))) + 0.5));

	}
	else
	{
		(*start_arc).x = (short)((semi_major_axis * (cos(rad_start_angle))) + 0.5);
		(*start_arc).y = ((short)((semi_minor_axis * (sin(rad_start_angle))) + 0.5));
		(*end_arc).x   = (short) ((semi_major_axis * (cos(rad_end_angle))) + 0.5);
		(*end_arc).y   = ((short) ((semi_minor_axis * (sin(rad_end_angle))) + 0.5));
	}

	if (bStartXPos)
	{
		if (start_arc->x < 0)
			start_arc->x = -(start_arc->x);
	}
	else
	{
		if (start_arc->x >0)
			start_arc->x = -(start_arc->x);
	}

	if (bStartYPos)
	{
		if (start_arc->y < 0)
			start_arc->y = -(start_arc->y);
	}
	else
	{
		if (start_arc->y > 0)
			start_arc->y = -(start_arc->y);
	}

	if (bEndXPos)
	{
		if (end_arc->x < 0)
			end_arc->x = -(end_arc->x);
	}
	else
	{
		if (end_arc->x > 0)
			end_arc->x = -(end_arc->x);
	}
	
	if (bEndYPos)
	{
		if (end_arc->y < 0)
			end_arc->y = -(end_arc->y);
	}
	else
	{
		if (end_arc->y > 0)
			end_arc->y = -(end_arc->y);
	}


	if ((*start_arc).x >= 0)
		(*start_arc).x += rect_center->x;
	else
		(*start_arc).x = rect_center->x + (*start_arc).x;

	if ((*start_arc).y > 0)
		(*start_arc).y = rect_center->y + (*start_arc).y;		
	else
		(*start_arc).y = rect_center->y +(*start_arc).y;


	if ((*end_arc).x >= 0)
		(*end_arc).x += rect_center->x;
	else
		(*end_arc).x = rect_center->x + (*end_arc).x;

	if ((*end_arc).y > 0)
		(*end_arc).y = rect_center->y  + (*end_arc).y;
	else
		(*end_arc).y = rect_center->y + (*end_arc).y;
	return;

}


void SwapPoints(POINT PG_FAR * start_arc, POINT PG_FAR * end_arc)
{
	
	POINT tmp = {0,0};


	tmp.x = start_arc->x;
	tmp.y = start_arc->y;

	start_arc->x = end_arc->x;
	start_arc->y = end_arc->y;

	end_arc->x = tmp.x;
	end_arc->y = tmp.y;

	return;

}




void RotateAngles(short PG_FAR * startAngle, short angleRun, short PG_FAR * end_angle, short PG_FAR * bClockWise)
{
	*end_angle = (*startAngle + angleRun) % NUM_CIRCLE_DEGREES;

	
	if (*startAngle <= 0)
		*startAngle = abs(*startAngle) + 90;
	else if (*startAngle <= 90)
		*startAngle = 90 - *startAngle;
	else
		*startAngle = NUM_CIRCLE_DEGREES - (*startAngle - 90);


	if (*end_angle <= 0)
		*end_angle = abs(*end_angle) + 90;
	else if (*end_angle <= 90)
		*end_angle = 90 - *end_angle;
	else
		*end_angle = NUM_CIRCLE_DEGREES - (*end_angle - 90);


	if (*startAngle < 0)
		*startAngle += NUM_CIRCLE_DEGREES;

	*bClockWise = TRUE;

	return;

}

void DetermineSigns(short PG_FAR * bStartXPos, short PG_FAR * bStartYPos, short PG_FAR * bEndXPos, 
					short PG_FAR * bEndYPos, short startAngle, short end_angle)
{
	if (startAngle <= 90)
	{
		*bStartXPos = TRUE;
		*bStartYPos = FALSE;
	}
	else if (startAngle <= 180)
	{
		*bStartXPos = FALSE;
		*bStartYPos = FALSE;
	}
	else if (startAngle <= 270)
	{
		*bStartXPos = FALSE;
		*bStartYPos = TRUE;
	}
	else if (startAngle <= 360)
	{
		*bStartXPos = TRUE;
		*bStartYPos = TRUE;
	}

	if (end_angle <= 90)
	{
		*bEndXPos = TRUE;
		*bEndYPos = FALSE;
	}
	else if (end_angle <= 180)
	{
		*bEndXPos = FALSE;
		*bEndYPos = FALSE;
	}
	else if (end_angle <= 270)
	{
		*bEndXPos = FALSE;
		*bEndYPos = TRUE;
	}
	else if (end_angle <= 360)
	{
		*bEndXPos = TRUE;
		*bEndYPos = TRUE;
	}

	return;
}

int FrameArc(HDC hdc, mac_rect PG_FAR * mrect, POINT PG_FAR * rect_center, short startAngle, short angleRun, 
				  ppen_track pen_stat, pbrush_track brush_stat)
{
	double	semi_major_axis = 0, semi_minor_axis = 0;
	double	rad_start_angle = 0, rad_end_angle = 0;
	short	end_angle = 0;
	short	bClockWise = FALSE, bStartXPos = FALSE, bStartYPos = FALSE;
	short   bEndXPos = FALSE, bEndYPos = FALSE;
	POINT	start_arc = {0,0}, end_arc = {0,0};
	HBRUSH  hbrush_tmp = NULL;
	HPEN    hpen_tmp = NULL;
	short	old_brush_style = brush_stat->current_brush.lbStyle;
	

	brush_stat->current_brush.lbStyle = BS_NULL;
	if (CreateAndSelectBrush(hdc, brush_stat) == NULL)
	{
		return (NOSUCCESS);
	}
	brush_stat->current_brush.lbStyle = old_brush_style;

	if (CreateAndSelectPen(hdc, pen_stat) == NULL)
	{
		return (NOSUCCESS);
	}


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
		int bArcSucceeded = FALSE;
		bArcSucceeded = Arc(hdc, mrect->left, mrect->top, mrect->right, mrect->bottom, start_arc.x, start_arc.y, 
				end_arc.x, end_arc.y);
	
		if (CreateAndSelectBrush(hdc, brush_stat) == NULL)
		{
			return (NOSUCCESS);
		}

		if (bArcSucceeded)
		{
			return(SUCCESS);
		}
		else
		{
			return(NOSUCCESS);
		}
	}


	return (SUCCESS);
}


 
#endif

