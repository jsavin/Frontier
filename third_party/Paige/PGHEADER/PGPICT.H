#ifndef PGPICT_H
#define PGPICT_H

#include "CPUDEFS.h"

#ifdef WINDOWS_PLATFORM

/*pgPict.h
 Functions used to translate a macintosh picture into a windows metafile.

	by John Ruble
	DataPak Software
	started: 06/19/97
	finished vs :
*/


//Pict opcodes
#define PICT_NOP 				0x0000
#define PICT_Clip 				0x0001
#define PICT_bkPat 				0x0002
#define PICT_txFont 			0x0003
#define PICT_txFace				0x0004
#define PICT_txMode				0x0005
#define PICT_spExtra			0x0006
#define PICT_PnSize				0x0007
#define PICT_PnMode				0x0008
#define PICT_PnPat				0x0009
#define PICT_FillPat			0x000A
#define PICT_OvSize				0x000B
#define PICT_Origin				0x000C
#define PICT_txSize				0x000D
#define PICT_FgColor			0x000E
#define PICT_BkColor			0x000F
#define PICT_txRatio			0x0010
#define PICT_VersionOp			0x0011
#define PICT_bkPixPat			0x0012
#define PICT_pnPixPat			0x0013
#define PICT_fillPixPat			0x0014
#define PICT_pnLocHFrac  		0x0015
#define PICT_chExtra			0x0016
#define PICT_RGBFgCol			0x001A
#define PICT_RGBBkCol			0x001B
#define PICT_hiliteMode			0x001C
#define PICT_HiliteColor		0x001D
#define PICT_defHilite			0x001E
#define PICT_opColor			0x001F
#define PICT_Line				0x0020
#define PICT_LineFrom			0x0021
#define PICT_ShortLine			0x0022
#define PICT_ShortLineFrom	 	0x0023
#define PICT_longText			0x0028
#define PICT_dhText				0x0029
#define PICT_dvText				0x002a
#define PICT_dhDvText			0x002b
#define PICT_fontName			0x002c
#define PICT_lineJustify		0x002d
#define PICT_glyphState			0x002e
#define PICT_frameRect			0x0030
#define PICT_paintRect			0x0031
#define PICT_eraseRect			0x0032
#define PICT_invertRect			0x0033
#define PICT_fillRect			0x0034
#define PICT_frameSameRect		0x0038
#define PICT_paintSameRect		0x0039
#define PICT_eraseSameRect		0x003a
#define PICT_invertSameRect		0x003b
#define PICT_fillSameRect		0x003c
#define PICT_frameRRect			0x0040
#define PICT_paintRRect			0x0041
#define PICT_eraseRRect			0x0042
#define PICT_invertRRect		0x0043
#define PICT_fillRRect			0x0044
#define PICT_frameSameRRect		0x0048
#define PICT_paintSameRRect		0x0049
#define PICT_eraseSameRRect		0x004A
#define PICT_invertSameRRect	0x004B
#define PICT_fillSameRRect		0x004C
#define PICT_frameOval			0x0050
#define PICT_paintOval			0x0051
#define PICT_eraseOval			0x0052
#define PICT_invertOval			0x0053
#define PICT_fillOval			0x0054
#define PICT_frameSameOval		0x0058
#define PICT_paintSameOval		0x0059
#define PICT_eraseSameOval		0x005A
#define PICT_invertSameOval		0x005B
#define PICT_fillSameOval		0x005C
#define PICT_frameArc			0x0060
#define PICT_paintArc			0x0061
#define PICT_eraseArc			0x0062
#define PICT_invertArc			0x0063
#define PICT_fillArc			0x0064
#define PICT_frameSameArc		0x0068
#define PICT_paintSameArc		0x0069
#define PICT_eraseSameArc		0x006A
#define PICT_invertSameArc		0x006B
#define PICT_fillSameArc		0x006C
#define PICT_framePoly			0x0070
#define PICT_paintPoly			0x0071
#define PICT_erasePoly			0x0072
#define PICT_invertPoly			0x0073
#define PICT_fillPoly			0x0074
#define PICT_frameSamePoly		0x0078
#define PICT_paintSamePoly		0x0079
#define PICT_eraseSamePoly		0x007A
#define PICT_invertSamePoly		0x007B
#define PICT_fillSamePoly		0x007C
#define PICT_frameRgn			0x0080
#define PICT_paintRgn			0x0081
#define PICT_eraseRgn			0x0082
#define PICT_invertRgn			0x0083
#define PICT_fillRgn			0x0084
#define PICT_frameSameRgn		0x0088
#define PICT_paintSameRgn		0x0089
#define PICT_eraseSameRgn		0x008a
#define PICT_invertSameRgn		0x008b
#define PICT_fillSameRgn		0x008c
#define PICT_BitsRect			0x0090
#define PICT_bitsRgn			0x0091
#define PICT_PackBitsRect		0x0098
#define PICT_packBitsRgn		0x0099
#define PICT_DirectBitsRect		0x009a
#define PICT_directBitsRgn		0x009b
#define PICT_ShortComment		0x00A0
#define PICT_LongComment		0x00A1
#define PICT_opEndPic			0x00FF
#define PICT_HeaderOp			0x0C00
#define PICT_Version			0x02FF
#define PICT_compressedQuicktime	0x8200
#define PICT_uncompressedQuicktime	0x8201

//apple reserved opcodes
#define PICT_appleRes0024		0x0024
#define PICT_appleRes0027		0x0027
#define PICT_appleRes002C       0x002C
#define PICT_appleRes002F		0x002F
#define	PICT_appleRes0017		0x0017
#define PICT_appleRes0019		0x0019
#define PICT_appleRes0035		0x0035
#define PICT_appleRes0037		0x0037
#define PICT_appleRes003d		0x003d
#define	PICT_appleRes003f		0x003f
#define PICT_appleRes0045		0x0045
#define PICT_appleRes0047		0x0047
#define PICT_appleRes004d		0x004d
#define	PICT_appleRes004f		0x004f
#define PICT_appleRes0055		0x0055
#define PICT_appleRes0057		0x0057
#define	PICT_appleRes005d		0x005d
#define PICT_appleRes005f		0x005f
#define PICT_appleRes0065		0x0065
#define	PICT_appleRes0067		0x0067
#define PICT_appleRes006d		0x006d
#define PICT_appleRes006f		0x006f
#define PICT_appleRes0075		0x0075
#define PICT_appleRes0077		0x0077
#define PICT_appleRes007d		0x007d
#define PICT_appleRes007f		0x007f
#define PICT_appleRes0085		0x0085
#define PICT_appleRes0087		0x0087
#define PICT_appleRes008d		0x008d
#define PICT_appleRes008f		0x008f
#define PICT_appleRes0092		0x0092
#define PICT_appleRes0097		0x0097
#define PICT_appleRes009C		0x009c
#define PICT_appleRes009F		0x009F
#define PICT_appleRes00A2		0x00a2
#define PICT_appleRes00AF		0x00af
#define PICT_appleRes00B0		0x00b0
#define PICT_appleRes00cf		0x00cf
#define PICT_appleRes00D0		0x00D0
#define PICT_appleRes00FE		0x00FE
#define PICT_appleRes0100		0x0100
#define PICT_appleRes01FF		0x01FF
#define PICT_appleRes0200		0x0200
#define PICT_appleRes0300		0x0300
#define PICT_appleRes0400		0x0400
#define	PICT_appleRes0500		0x0500
#define PICT_appleRes0600		0x0600
#define PICT_appleRes0700		0x0700
#define PICT_appleRes8000		0x8000
#define PICT_appleRes80ff		0x80ff
#define PICT_appleRes0A00		0x0A00
#define PICT_appleRes0BFE		0x0BFE
#define PICT_appleRes0BFF		0x0BFF
#define PICT_appleRes0C01		0x0C01
#define PICT_appleRes0D00		0x0D00
#define PICT_appleRes2306		0x2306
#define	PICT_appleRes2307		0x2307
#define PICT_appleRes230B		0x230B
#define PICT_appleRes230F		0x230F
#define PICT_appleRes2318		0x2318
#define PICT_appleRes23FC		0x23FC
#define PICT_appleRes23FD		0x23FD
#define PICT_appleRes23F6		0x23F6
#define PICT_appleRes23F7		0x23F7
#define PICT_appleRes23F8		0x23F8
#define PICT_appleRes23F9		0x23F9
#define PICT_appleRes23FE		0x23FE
#define PICT_appleRes23FF		0x23FF
#define PICT_appleRes7F00		0x7F00
#define PICT_appleRes7EFF		0x7EFF
#define PICT_appleRes7FFF		0x7FFF
#define PICT_appleRes8100		0x8100
#define PICT_appleResFFFF		0xFFFF
// end Pict opcodes


#define PICT_DirectBits_BaseAddr	0x000000ff
#define PICT_VersionOP1			0x1101
#define PICT_Version1_Version	1

// Old QuickDraw Colors
#define OQD_blackColor			33
#define OQD_whiteColor			30
#define OQD_redColor			205
#define OQD_greenColor			341
#define OQD_blueColor			409
#define OQD_cyanColor			273
#define OQD_magentaColor		137
#define OQD_yellowColor			69

// define Mac Pen Modes
#define PNMODE_patCopy			8
#define PNMODE_patOr			9
#define PNMODE_patXor			10
#define PNMODE_patBic			11
#define PNMODE_notPatCopy		12
#define PNMODE_notPatOr			13
#define PNMODE_notPatXor		14
#define PNMODE_notPatBic		15

// version 2 PICT size in pict header
#define PICT_vs2Size			-1

#define Mac_DPI					72

#define NOSUCCESS				1
#define SUCCESS					0


#define PI						3.14159
#define TWO_PI					(2.0 * PI)
#define NUM_CIRCLE_DEGREES		360
#define DEGREES_TO_RAD			TWO_PI / NUM_CIRCLE_DEGREES


// 16 Bit Pixmap masks
#define PIX_16_RED				0x7C00
#define PIX_16_GREEN			0x03E0
#define	PIX_16_BLUE				0x001f

#define SIZE_PIXMAP_DATA		46  // this is a guess, may be 50 if baseAddr is used.
#define MAC_DITHER_PAT			2
#define SIZE_COLORSPEC_ENTRY	8

typedef unsigned long	DWORD;


#define MAC_REGION_BREAK 0x7FFF
      
#define PICT_BOLD_TXT			1
#define PICT_ITALIC_TXT			2
#define PICT_UNDERLINE_TXT		4  
#define PICT_OUTLINE_TXT		8   // Outline, ...
#define PICT_SHADOW_TXT			16  //   Shadow,..
#define PICT_CONDENSE_TXT		32  //     Condense, and ...
#define PICT_EXTEND_TXT			64  //       Extended Font Styles are not currently supported.  


#define SHORT_POLYLINE_INCREASE 50
#define DV_FLAG					1
#define DH_FLAG					2
#define DVDH_FLAG				3
#define LONG_FLAG				4
typedef struct 
{
	short		mac_fixed_int;
	pg_word		mac_fixed_fract;
}mac_fixed;


typedef struct 
{
	mac_fixed	top;
	mac_fixed	left;
	mac_fixed	bottom;
	mac_fixed	right;
}mac_fpRect;



typedef struct 
{
	short		top;
	short		left;
	short		bottom;
	short		right;
}mac_rect, *pmac_rect;



typedef struct 
{
	pg_word		picSize;	// low word of picture size
	mac_rect	picFrame;
	pg_word		version_op;	// should be 0x0011 for version 2 PICTs;
	pg_word		version;	// should be 0x02FF for version 2 PICTs;
	pg_word		header_op;	// should be 0x0C00 for version 2 PICTs;
	long		pict_size;	// -1 for version 2
	mac_fpRect	fBBox;		// a fixed point bounding box
	DWORD		reserved1;
}mac_pict_header;




typedef struct 
{
	short rgnSize;
	mac_rect rgnBBox;

}mac_rgn, PG_FAR *pmac_rgn;

typedef struct 
{
	pg_word red;
	pg_word green;
	pg_word blue;
}rgbMac,  PG_FAR *prgbMac;


typedef struct
{
	short polySize;
	mac_rect polyBBox;
	memory_ref polyPoints;
}mac_poly,  PG_FAR *pmac_poly;

typedef struct
{
	memory_ref polyPoints;
	int		   index;
	int	   numPoints;
}short_polyline, PG_FAR *pshort_polyline;
	
typedef struct
{
	short	value; // index or other value
	rgbMac	mac_rgb; // true color;

}mac_colorSpec,  PG_FAR *pmac_colorSpec;

typedef struct
{
	long			ctSeed;
	pg_word			ctFlags;
	pg_word			ctSize;
	memory_ref		cSpecArray; // array of mac_colorSpec records

}mac_colorTable,  PG_FAR *pmac_colorTable;


typedef struct
{
	long				baseAddr;
	short				rowBytes;
	mac_rect			bounds;
}mac_bitmap,  PG_FAR *pmac_bitmap;

typedef struct
{
	short				rowBytes;
	mac_rect			bounds;
	short				pmVersion;
	short				packType;
	long				packSize;
	mac_fixed			hRes;
	mac_fixed			vRes;
	short				pixelType;
	short				pixelSize;
	short				cmpCount;
	short				cmpSize;
	long				planeBytes;

}mac_corePixData,  PG_FAR *pmac_corePixData;

typedef struct
{
	memory_ref			baseAddr; // pixel data
	mac_corePixData		std_pm_data;
	memory_ref			pmTable; // mac_color table
	long				pmReserved;
}mac_pixmap,  PG_FAR *pmac_pixmap;

typedef struct
{
	mac_corePixData		std_pm_data;
	long				pmTable;
	long				pmReserved;
}mac_cb_pixmapData,  PG_FAR *pmac_cb_pixmapData;


typedef struct
{
	mac_cb_pixmapData	cb_pixmapData;
	memory_ref			mf_colorTable;
	mac_rect			srcRect;
	mac_rect			dstRect;
	pg_word				mode;
	memory_ref			mf_PixData;

}mac_packBitsRect,  PG_FAR *pmac_packBitsRect;

typedef struct
{
	mac_cb_pixmapData	cb_pixmapData;
	memory_ref			mf_colorTable;
	mac_rect			srcRect;
	mac_rect			dstRect;
	pg_word				mode;        
	mac_rect			mask_rect;
	memory_ref			mf_PixData;

}mac_packBitsRgn,  PG_FAR *pmac_packBitsRgn;

typedef struct
{
	mac_bitmap			m_bitmap;
	mac_rect			srcRect;
	mac_rect			dstRect;
	pg_word				mode;
	memory_ref			mf_bitData;
}mac_packBitmapRect,  PG_FAR *pmac_packBitmapRect;      

typedef struct
{
	mac_bitmap			m_bitmap;
	mac_rect			srcRect;
	mac_rect			dstRect;
	pg_word				mode;
	mac_rect			mask_rect;
	memory_ref			mf_bitData;
}mac_packBitmapRgn,  PG_FAR *pmac_packBitmapRgn;


typedef struct
{
	LOGPEN current_pen;
	HPEN	hpen_current;
//	HPEN	hpen_previous;

}pen_track,  PG_FAR *ppen_track;

typedef struct
{
	LOGBRUSH current_brush;
	HBRUSH hbrush_current;
//	HBRUSH hbrush_previous;
}brush_track,  PG_FAR *pbrush_track;

typedef struct
{
	LOGFONT current_font;
	HFONT	hfont_current;
}font_track, PG_FAR *pfont_track;



#ifdef __cplusplus
extern	"C" {
#endif



// returns byte in data array and increments ptr to next byte
pg_byte GetByteInc(pg_bits8_ptr PG_FAR * ptr2_data_bytes);


// reads and returns a macintosh word, and incremetns ptr to next byte
pg_word GetMacWordInc (pg_bits8_ptr PG_FAR * ptr2_data_bytes);



// retrieves a DWORD in the Macintosh MSB byte order and increments pointer
// to next byte
DWORD GetMacDWORDInc(pg_bits8_ptr PG_FAR * ptr2_data_bytes);



// fills a mac_rect structure from array of data bytes and 
// increments the ptr.
void GetMacRectInc(pg_bits8_ptr PG_FAR * ptr2_data_bytes, mac_rect PG_FAR * mrect);


//fills a mac_fixed structure with data from an array of bytes and
// increments the ptr
void GetMacFixedInc(pg_bits8_ptr PG_FAR * ptr2_data_bytes, mac_fixed PG_FAR * tmp_fixed);


pg_word GetOpCodeInc(pg_bits8_ptr PG_FAR * ptr2_data_bytes);

//Fills mac_pict_header structure from byte array and icrements ptr. Returns 0 if 
// succeeds, 1 if fails.
int GetMacPictHeaderInc(pg_bits8_ptr PG_FAR * ptr2_data_bytes, mac_pict_header PG_FAR * pic_header);

// Check the information in the PICT header to see if its valid
// if it is return 0, else return 1.
int IsBadPictHeader(mac_pict_header PG_FAR * pic_header, long data_size_bytes);


// retrieves a mac_rgn structure from byte array and increments ptr.
int GetMacRgn(pg_bits8_ptr PG_FAR * ptr2_data_bytes, mac_rgn PG_FAR * clip_rgn);

void RetrievePt(pg_bits8_ptr PG_FAR * ptr2_data_bytes, POINT PG_FAR * pt);


//Gets the parameters passed with the PICT_Clip opcode
// and calls associated metafile commands.
// returns 0 on success and 1 on failure
int DealWithClip(pg_bits8_ptr PG_FAR * ptr2_data_bytes, HDC hdc_metafile);


//retrieves the parameters for ShortLine opcode and performs associated
// metafile calls, returns 0 if succes else 1.
int DealWithShortLine(pg_bits8_ptr PG_FAR * ptr2_data_bytes, short_polyline PG_FAR * shortline_poly, double XScaleFactor, double YScaleFactor, HDC hdc_metafile);


// Handles shortLineFrom opcode, returns 0 if success else 1
int DealShortLineFrom(pg_bits8_ptr PG_FAR * ptr2_data_bytes, short_polyline PG_FAR * shortline_poly, double XScaleFactor, double YScaleFactor, HDC hdc_metafile);

//Skips long comments in opcodes, returns 0 if succeeds, 1 if fails.
int SkipLongComment(pg_bits8_ptr PG_FAR * ptr2_data_bytes);

//retrieves rgbMac color information from byte stream, converts it to Windows
// RGBTRIPLE and passes it back.
void RetrieveForeColor(pg_bits8_ptr PG_FAR * ptr2_data_bytes, RGBTRIPLE PG_FAR * current_forecolor);


//retrieves a macintosh PenPat from byte array
//returns 0 if successful, 1 if fails.
int RetrievePenPat(pg_bits8_ptr PG_FAR * ptr2_data_bytes, memory_ref PG_FAR * mfptr_pnpat);

// retrieves frameRect opcode parameters
void RetrieveFrameRect(pg_bits8_ptr ptr2_data_bytes, mac_rect PG_FAR * mrect);

//returns 0 on succees, 1 if failed
int RetrievePolyParams(pg_bits8_ptr PG_FAR * ptr2_data_bytes, pmac_poly mpoly, long PG_FAR * numPoints);

void DisposePackBitsRectParams(memory_ref PG_FAR * mfptr_pbr_params);  

void DisposePackBitsRgnParams(memory_ref PG_FAR * mfptr_pbr_params); 

void DisposeBMPackBitsRect(memory_ref PG_FAR * mfptr_pbr_data);

void DisposeBMPackBitsRgn(memory_ref PG_FAR * mfptr_pbr_data);



int UnpackPixBitsThree(pg_bits8_ptr PG_FAR * ptr2_data_bytes, pg_bits8_ptr ptr_pixbits, int bWordByteCount, 
					   pmac_corePixData ptr_core_pix, long pms_size, short rowBytes);

int UnpackPixBitsFour (pg_bits8_ptr PG_FAR * ptr2_data_bytes, pg_bits8_ptr ptr_pixbits, int bWordByteCount,
					pmac_corePixData ptr_core_pix, long pm_size, short rowBytes);

int DefaultUnpackBMbits(pg_bits8_ptr PG_FAR * ptr2_data_bytes, pg_bits8_ptr ptr_BMbits, 
						int bWordByteCount, pmac_packBitmapRect ptr_bm, short rowBytes, mac_rect PG_FAR * dstRect, int bBitsRect);

int DefaultUnpackBMbitsRgn(pg_bits8_ptr PG_FAR * ptr2_data_bytes, pg_bits8_ptr ptr_BMbits, 
						int bWordByteCount, pmac_packBitmapRgn ptr_bm, short rowBytes, mac_rect PG_FAR * dstRect, int bBitsRect);

int DefaultUnpackPixBits(pg_bits8_ptr PG_FAR * ptr2_data_bytes, pg_bits8_ptr ptr_pixbits, 
						 int bWordByteCount, pmac_corePixData ptr_core_pix, long pm_size, short rowBytes, mac_rect PG_FAR * dstRect);


int UnpackPixData(pg_bits8_ptr PG_FAR * ptr2_data_bytes, pg_bits8_ptr ptr_pixbits, 
				  pmac_corePixData ptr_core_pix, short rowBytes, long pm_size, mac_rect PG_FAR * dstRect);


int RetrievePixelBits(pg_bits8_ptr PG_FAR * ptr2_data_bytes, pg_bits8_ptr ptr_pixbits,pmac_corePixData ptr_core_pix, mac_rect PG_FAR * dstRect);

int RetrieveMacBmData(pg_bits8_ptr PG_FAR * ptr2_data_bytes, pmac_bitmap ptr_mac_bitmap);

int RetrieveBMBits(pg_bits8_ptr PG_FAR * ptr2_data_bytes, pg_bits8_ptr ptr_BMbits, pmac_packBitmapRect ptr_bm, mac_rect PG_FAR * dstRect, int bBitsRect);

int RetrieveBMBitsRgn(pg_bits8_ptr PG_FAR * ptr2_data_bytes, pg_bits8_ptr ptr_BMbits, pmac_packBitmapRgn ptr_bm, mac_rect PG_FAR * dstRect, int bBitsRect);


int RetrieveCorePixData(pg_bits8_ptr PG_FAR * ptr2_data_bytes, pmac_corePixData ptr_core_pm);


int RetrieveMacCbPixmapData(pg_bits8_ptr PG_FAR * ptr2_data_bytes, pmac_cb_pixmapData ptr_cb_pm);


int RetrieveColorSpecEntries(pg_bits8_ptr PG_FAR * ptr2_data_bytes, pmac_colorSpec ptr_colSpec, int NumCtEntries);

int RetrieveMacColorTable(pg_bits8_ptr PG_FAR * ptr2_data_bytes, pmac_colorTable ptr_ct);

int RetrieveBMPackBitsRectParams(pg_bits8_ptr PG_FAR * ptr2_data_bytes, memory_ref PG_FAR * mfptr_pbr_data, int bBitsRect);

 int RetrieveBMPackBitsRgnParams(pg_bits8_ptr PG_FAR * ptr2_data_bytes, memory_ref PG_FAR * mfptr_pbr_data, int bBitsRect);


int RetrievePackBitsRectParams(pg_bits8_ptr PG_FAR * ptr2_data_bytes, memory_ref PG_FAR * mfptr_pbr_data);

int RetrievePackBitsRgnParams(pg_bits8_ptr PG_FAR * ptr2_data_bytes, memory_ref PG_FAR * mfptr_pbr_data);

//Returns valid COLORREF if successful, else returns -1 in high byte.
COLORREF ConvertRGBTtoColorRef(RGBTRIPLE PG_FAR * color);

void ScalePt(POINT PG_FAR * pt, double XScaleFactor, double YScaleFactor);

void MyScaleRect(mac_rect PG_FAR * rectangle, double XScaleFactor, double YScaleFactor);

void AdjustRect(mac_rect PG_FAR * mrect);

void GetMacRectAndScaleInc(pg_bits8_ptr PG_FAR * ptr2_data_bytes, mac_rect PG_FAR * mrect, double XScaleFactor, double YScaleFactor);

int GetMacRgnAndScaleRect(pg_bits8_ptr PG_FAR * ptr2_data_bytes, mac_rgn PG_FAR * mrgn, mac_rect PG_FAR * mrect, double XScaleFactor, double YScaleFactor);

void GetRectCenter(mac_rect PG_FAR * mrect, POINT PG_FAR * center);

void DegreesToRadians(short startAngle, double PG_FAR * rad_start_angle);

void CalculateEndAngle (short startAngle, short angleRun, short PG_FAR * end_angle, int PG_FAR * bClockWise);

int IsHorizontalEllipse(mac_rect PG_FAR * mrect, double PG_FAR * semi_major_axis, double PG_FAR * semi_minor_axis);

void CalculateArcPoints(mac_rect PG_FAR * mrect, POINT PG_FAR * rect_center, double semi_major_axis, double semi_minor_axis, 
						double rad_start_angle, double rad_end_angle, POINT PG_FAR * start_arc, POINT PG_FAR * end_arc, int bHorizontal,
						short bStartXPos, short bStartYPos, short bEndXPos, short bEndYPos);

void SwapPoints(POINT PG_FAR * start_arc, POINT PG_FAR * end_arc);


void RotateAngles(short PG_FAR * startAngle, short angleRun, short PG_FAR * end_angle, short PG_FAR * bClockWise);

void DetermineSigns(short PG_FAR * bStartXPos, short PG_FAR * bStartYPos, short PG_FAR * bEndXPos, 
					short  PG_FAR *bEndYPos, short startAngle, short end_angle);

int FrameArc(HDC hdc, mac_rect PG_FAR * mrect, POINT PG_FAR * rect_center, short startAngle, short angleRun, 
				  ppen_track pen_stat, pbrush_track brush_stat);


int PaintPieWedge(HDC hdc, mac_rect PG_FAR * mrect, POINT PG_FAR * rect_center, short startAngle, short angleRun, 
				  ppen_track pen_stat, pbrush_track brush_stat);

int OldQDColorToRGB(RGBTRIPLE PG_FAR * rgbcolor, long old_qd_color);

void MacRgnToMacRect(mac_rgn PG_FAR * mrgn, mac_rect PG_FAR * rectangle);

void ScalePolyPts(mac_poly PG_FAR * mpoly, long numPoints, double XScaleFactor, double YScaleFactor);



int DrawBMPackBitsRect(HDC hdc, memory_ref PG_FAR * mfptr_pbr_data, double XScaleFactor, double YScaleFactor, POINT PG_FAR * winExt);
 
int DrawBMPackBitsRgn(HDC hdc, memory_ref PG_FAR * mfptr_pbr_data, double XScaleFactor, double YScaleFactor, POINT PG_FAR * winExt);
 
int Expand2to4bits(pg_bits8_ptr ptr_Dest, pg_bits8_ptr ptr_Src, long numScanLines, 
				   long dstBytesPadding, long srcRowBytes);


int Expand16to24Bits(pg_bits8_ptr ptr_Dest, pg_bits8_ptr ptr_Src, long numScanLines, 
					 long dstBytesPadding, long srcRowBytes);


int DrawPackBitsRect(HDC hdc, memory_ref PG_FAR * mf_pbr_data, double XScaleFactor, double YScaleFactor, POINT PG_FAR * winExt);

int DrawPackBitsRgn(HDC hdc, memory_ref PG_FAR * mf_pbr_data, double XScaleFactor, double YScaleFactor, POINT PG_FAR * winExt);


pg_word GetNewPenWidth(pg_bits8_ptr PG_FAR * ptr2_data_bytes, ppen_track pen_stat, double XScaleFactor, double YScaleFactor);

HPEN CreateAndSelectPen(HDC hdc, ppen_track pen_stat);
 
HFONT CreateAndSelectFont(HDC hdc, pfont_track font_stat);

 
HBRUSH CreateAndSelectBrush(HDC hdc, pbrush_track brush_stat);

int PictPaintOval(HDC hdc, ppen_track pen_stat, pbrush_track brush_stat, mac_rect PG_FAR * mrect);

int PictFrameOval(HDC hdc, ppen_track pen_stat, pbrush_track brush_stat, mac_rect PG_FAR * mrect);

void GetArcAngles (pg_bits8_ptr PG_FAR * ptr2_data_bytes, short PG_FAR * startAngle, short PG_FAR * angleRun);


void GetArcParams(pg_bits8_ptr PG_FAR * ptr2_data_bytes, mac_rect PG_FAR * mrect, short PG_FAR * startAngle, 
				  short PG_FAR * angleRun, POINT PG_FAR * rect_center, double XScaleFactor, double YScaleFactor);

int RetrieveAndDrawPixmapRgn(pg_bits8_ptr PG_FAR * ptr2_data_bytes, HDC hdc, double XScaleFactor,
					double YScaleFactor, POINT PG_FAR * winExt, int bBitsRect);

int RetrieveAndDrawPixmap(pg_bits8_ptr PG_FAR * ptr2_data_bytes, HDC hdc, double XScaleFactor,
					double YScaleFactor, POINT PG_FAR * winExt, int bBitsRect);

void SkipDefaultPackedPixData(pg_bits8_ptr PG_FAR * ptr2_data_bytes, long numScanlines, short rowBytes);


int SkipOpCode(pg_bits8_ptr PG_FAR * ptr2_data_bytes, pg_word current_opcode);

int PictSetPnMode(HDC hdc, pg_word current_pen_mode, ppen_track pen_stat);


void RetrieveAdjustedScaledPt(pg_bits8_ptr PG_FAR * ptr2_data_bytes, POINT PG_FAR * pt, double XScaleFactor,
							  double YScaleFactor);


int PictPaintRect(HDC hdc, mac_rect PG_FAR * mrect, ppen_track pen_stat, pbrush_track brush_stat);


int PictFrameRect(HDC hdc, mac_rect PG_FAR * mrect, ppen_track pen_stat, pbrush_track brush_stat);


int PictPaintRRect(HDC hdc, mac_rect PG_FAR * mrect, POINT PG_FAR * ovSize, ppen_track pen_stat, pbrush_track brush_stat);


int PictFrameRRect(HDC hdc, mac_rect PG_FAR * mrect, POINT PG_FAR * ovSize, ppen_track pen_stat, pbrush_track brush_stat);


int PictFramePoly(HDC hdc, pmac_poly mpoly, long numPoints, ppen_track pen_stat, pbrush_track brush_stat, 
				  double XScaleFactor, double YScaleFactor);

int PictPaintPoly(HDC hdc, pmac_poly mpoly, long numPoints, ppen_track pen_stat, pbrush_track brush_stat, 
				  double XScaleFactor, double YScaleFactor);

int RetrieveDHDVText(pg_bits8_ptr PG_FAR* ptr2_data_bytes, int PG_FAR* string_length, int PG_FAR* horizontal_offset, int PG_FAR* vertical_offset, memory_ref PG_FAR* mfptr_string_to_display, pg_bits8 flDhDv);


int DhDvDrawText(HDC hdc, int PG_FAR* last_vertOffset, int PG_FAR* last_horzOffset, memory_ref PG_FAR* mfptr_string_to_display, int string_length, int vertical_offset, 
					int horizontal_offset, POINT PG_FAR* winExt, pg_bits8 flDvDh);

void DisposeTranslateOpcodesData(memory_ref PG_FAR * mfp_current_pnpat, pmac_poly mpoly, ppen_track pen_stat, pbrush_track brush_stat, pfont_track font_stat, pshort_polyline shortline_poly);

// reads PICT opcodes and calls associated metafile commands
// returns 1 if fails, otherwise returns 0. Increments ptr.
int TranslatePictOpCodes(pg_bits8_ptr PG_FAR * ptr2_data_bytes, HDC hdc_metafile, 
						 POINT PG_FAR * winExt, HDC target_dc);


// Main function to translate a Pict to a metafile
// returns null if fails, otherwise returns a handle to the new metafile. 
// Increments ptr
PG_PASCAL (HMETAFILE) TranslatePictToWMFInc (pgm_globals_ptr mem, pg_bits8_ptr PG_FAR * ptr2_data_bytes, long data_size_bytes, POINT PG_FAR * winExt, 
								  HDC target_dc);
    
    
#ifdef __cplusplus
};
#endif
    
    
#endif
#endif

