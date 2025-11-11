/*jWMF.h
*/
#ifndef PGWMF_H
#define PGWMF_H

#include "Paige.h"
#include "stdlib.h"
#include <QDOffscreen.h>
#include <String.h>
#include <Math.h>

#define WORD 		unsigned short
#define BYTE 		unsigned char
#define DWORD 		unsigned long

#define WIN_SCAN_LINE_PADDING 4
#define MAC_SCAN_LINE_PADDING 8

#define SUCCESS 0
#define	FAILED  1

#define MFCOMMENT	15

// OpenCPicParams defines
#define SEVENTY_TWO_DPI 	(Fixed)72
#define PIC_VERSION			-2


#define COLORREF_RED 				 0x0000000FF
#define COLORREF_GREEN				 0x00000FF00
#define COLORREF_BLUE				 0x000FF0000
#define COLORREF_RESERVED			 0x0FF000000

#define META_SETBKCOLOR              0x0201
#define META_SETBKMODE               0x0102
#define META_SETMAPMODE              0x0103
#define META_SETROP2                 0x0104
#define META_SETRELABS               0x0105
#define META_SETPOLYFILLMODE         0x0106
#define META_SETSTRETCHBLTMODE       0x0107
#define META_SETTEXTCHAREXTRA        0x0108
#define META_SETTEXTCOLOR            0x0209
#define META_SETTEXTJUSTIFICATION    0x020A
#define META_SETWINDOWORG            0x020B
#define META_SETWINDOWEXT            0x020C
#define META_SETVIEWPORTORG          0x020D
#define META_SETVIEWPORTEXT          0x020E
#define META_OFFSETWINDOWORG         0x020F
#define META_SCALEWINDOWEXT          0x0410
#define META_OFFSETVIEWPORTORG       0x0211
#define META_SCALEVIEWPORTEXT        0x0412
#define META_LINETO                  0x0213
#define META_MOVETO                  0x0214
#define META_EXCLUDECLIPRECT         0x0415
#define META_INTERSECTCLIPRECT       0x0416
#define META_ARC                     0x0817
#define META_ELLIPSE                 0x0418
#define META_FLOODFILL               0x0419
#define META_PIE                     0x081A
#define META_RECTANGLE               0x041B
#define META_ROUNDRECT               0x061C
#define META_PATBLT                  0x061D
#define META_SAVEDC                  0x001E
#define META_SETPIXEL                0x041F
#define META_OFFSETCLIPRGN           0x0220
#define META_TEXTOUT                 0x0521
#define META_BITBLT                  0x0922
#define META_STRETCHBLT              0x0B23
#define META_POLYGON                 0x0324
#define META_POLYLINE                0x0325
#define META_ESCAPE                  0x0626
#define META_RESTOREDC               0x0127
#define META_FILLREGION              0x0228
#define META_FRAMEREGION             0x0429
#define META_INVERTREGION            0x012A
#define META_PAINTREGION             0x012B
#define META_SELECTCLIPREGION        0x012C
#define META_SELECTOBJECT            0x012D
#define META_SETTEXTALIGN            0x012E
#define META_CHORD                   0x0830
#define META_SETMAPPERFLAGS          0x0231
#define META_EXTTEXTOUT              0x0a32
#define META_SETDIBTODEV             0x0d33
#define META_SELECTPALETTE           0x0234
#define META_REALIZEPALETTE          0x0035
#define META_ANIMATEPALETTE          0x0436
#define META_SETPALENTRIES           0x0037
#define META_POLYPOLYGON             0x0538
#define META_RESIZEPALETTE           0x0139
#define META_DIBBITBLT               0x0940
#define META_DIBSTRETCHBLT           0x0b41
#define META_DIBCREATEPATTERNBRUSH   0x0142
#define META_STRETCHDIB              0x0f43
#define META_EXTFLOODFILL            0x0548
#define META_DELETEOBJECT            0x01f0
#define META_CREATEPALETTE           0x00f7
#define META_CREATEPATTERNBRUSH      0x01F9
#define META_CREATEPENINDIRECT       0x02FA
#define META_CREATEFONTINDIRECT      0x02FB
#define META_CREATEBRUSHINDIRECT     0x02FC
#define META_CREATEREGION            0x06FF


//Dib color table defines
#define DIB_RGB_COLORS 		0
#define DIB_PAL_COLORS		1
#define DIB_PATTERN			3

/* Brush Styles */
#define BS_SOLID            0
#define BS_NULL             1
#define BS_HOLLOW           BS_NULL
#define BS_HATCHED          2
#define BS_PATTERN          3
#define BS_INDEXED          4
#define BS_DIBPATTERN       5
#define BS_DIBPATTERNPT     6
#define BS_PATTERN8X8       7
#define BS_DIBPATTERN8X8    8
#define BS_MONOPATTERN      9

/* Hatch Styles */
#define HS_HORIZONTAL       0       /* ----- */
#define HS_VERTICAL         1       /* ||||| */
#define HS_FDIAGONAL        2       /* \\\\\ */
#define HS_BDIAGONAL        3       /* ///// */
#define HS_CROSS            4       /* +++++ */
#define HS_DIAGCROSS        5       /* xxxxx */

/* Pen Styles */
#define PS_SOLID            0
#define PS_DASH             1       /* -------  */
#define PS_DOT              2       /* .......  */
#define PS_DASHDOT          3       /* _._._._  */
#define PS_DASHDOTDOT       4       /* _.._.._  */
#define PS_NULL             5
#define PS_INSIDEFRAME      6
#define PS_USERSTYLE        7
#define PS_ALTERNATE        8
#define PS_STYLE_MASK       0x0000000F

#define PS_ENDCAP_ROUND     0x00000000
#define PS_ENDCAP_SQUARE    0x00000100
#define PS_ENDCAP_FLAT      0x00000200
#define PS_ENDCAP_MASK      0x00000F00

#define PS_JOIN_ROUND       0x00000000
#define PS_JOIN_BEVEL       0x00001000
#define PS_JOIN_MITER       0x00002000
#define PS_JOIN_MASK        0x0000F000

#define PS_COSMETIC         0x00000000
#define PS_GEOMETRIC        0x00010000
#define PS_TYPE_MASK        0x000F0000

#define AD_COUNTERCLOCKWISE 1
#define AD_CLOCKWISE        2


// Windows Mapping Mode defines
#define MM_TEXT 			1
#define MM_LOMETRIC			2
#define MM_HIMETRIC			3
#define MM_LOENGLISH		4
#define MM_HIENGLISH		5
#define MM_TWIPS			6
#define MM_ISOTROPIC		7
#define	MM_ANISOTROPIC		8

// Min and Max Mapping Mode values for Windows
#define MM_MIN				MM_TEXT
#define MM_MAX				MM_ANISOTROPIC
#define MM_MAX_FIXEDSCALE	MM_TWIPS

//ExtTextOut fuOptions  defines
#define ETO_CLIPPED 		0x0004
#define ETO_OPAQUE			0x0002


/* Text Alignment Options */
#define TA_NOUPDATECP                0
#define TA_UPDATECP                  1

#define TA_LEFT                      0
#define TA_RIGHT                     2
#define TA_CENTER                    6

#define TA_TOP                       0
#define TA_BOTTOM                    8
#define TA_BASELINE                  24


//Font Defines
#define OUT_DEFAULT_PRECIS      0
#define OUT_STRING_PRECIS       1
#define OUT_CHARACTER_PRECIS    2
#define OUT_STROKE_PRECIS       3
#define OUT_TT_PRECIS           4
#define OUT_DEVICE_PRECIS       5
#define OUT_RASTER_PRECIS       6
#define OUT_TT_ONLY_PRECIS      7
#define OUT_OUTLINE_PRECIS      8

#define CLIP_DEFAULT_PRECIS     0
#define CLIP_CHARACTER_PRECIS   1
#define CLIP_STROKE_PRECIS      2
#define CLIP_MASK               0xf
#define CLIP_LH_ANGLES          (1<<4)
#define CLIP_TT_ALWAYS          (2<<4)
#define CLIP_EMBEDDED           (8<<4)

#define DEFAULT_QUALITY         0
#define DRAFT_QUALITY           1
#define PROOF_QUALITY           2

#define DEFAULT_PITCH           0
#define FIXED_PITCH             1
#define VARIABLE_PITCH          2

#define ANSI_CHARSET            0
#define DEFAULT_CHARSET         1
#define SYMBOL_CHARSET          2
#define SHIFTJIS_CHARSET        128
#define HANGEUL_CHARSET         129
#define GB2312_CHARSET          134
#define CHINESEBIG5_CHARSET     136
#define OEM_CHARSET             255


//Special define for ExtTextOut Opcode
#define MINIMAL_EXTTEXTOUT		20 	/*If number of data bytes for opcode is less than this then
										opcode is running minus rect and character spacing array,
										most likely it is a one character draw.
									*/
/* Font Families */
#define FF_DONTCARE         (0<<4)  /* Don't care or don't know. */
#define FF_ROMAN            (1<<4)  /* Variable stroke width, serifed. */
                                    /* Times Roman, Century Schoolbook, etc. */
#define FF_SWISS            (2<<4)  /* Variable stroke width, sans-serifed. */
                                    /* Helvetica, Swiss, etc. */
#define FF_MODERN           (3<<4)  /* Constant stroke width, serifed or sans-serifed. */
                                    /* Pica, Elite, Courier, etc. */
#define FF_SCRIPT           (4<<4)  /* Cursive, etc. */
#define FF_DECORATIVE       (5<<4)  /* Old English, etc. */

/* Font Weights */
#define FW_DONTCARE         0
#define FW_THIN             100
#define FW_EXTRALIGHT       200
#define FW_LIGHT            300
#define FW_NORMAL           400
#define FW_MEDIUM           500
#define FW_SEMIBOLD         600
#define FW_BOLD             700
#define FW_EXTRABOLD        800
#define FW_HEAVY            900

#define FW_ULTRALIGHT       FW_EXTRALIGHT
#define FW_REGULAR          FW_NORMAL
#define FW_DEMIBOLD         FW_SEMIBOLD
#define FW_ULTRABOLD        FW_EXTRABOLD
#define FW_BLACK            FW_HEAVY


// Windows Bitmap Compression choices
#define	WIN_COMPRESS_RGB	0
#define WIN_COMPRESS_RLE8   1
#define WIN_COMPRESS_RLE4	2

#define least_nibble_in_byte(value)		((value) & 0x0f)
#define	highest_nibble_in_byte(value)	(((value) & 0xf0) >> 4)


//windows gdi object types
#define WIN_GDI_PEN 		0
#define WIN_GDI_BRUSH 		1	
#define WIN_GDI_FONT 		2
#define WIN_GDI_BITMAP		3
#define WIN_GDI_REGION		4
#define WIN_GDI_PALETTE   	5

// gdi table resize value
#define GDI_OBJECT_MIN_RESIZE 5

#define LF_FACESIZE 		32

//BKMODE values
#define OPAQUE				2
#define TRANSPARENT 		1


/* Ternary raster operations */
#define SRCCOPY             (DWORD)0x00CC0020 /* dest = source                   */
#define SRCPAINT            (DWORD)0x00EE0086 /* dest = source OR dest           */
#define SRCAND              (DWORD)0x008800C6 /* dest = source AND dest          */
#define SRCINVERT           (DWORD)0x00660046 /* dest = source XOR dest          */
#define SRCERASE            (DWORD)0x00440328 /* dest = source AND (NOT dest )   */
#define NOTSRCCOPY          (DWORD)0x00330008 /* dest = (NOT source)             */
#define NOTSRCERASE         (DWORD)0x001100A6 /* dest = (NOT src) AND (NOT dest) */
#define MERGECOPY           (DWORD)0x00C000CA /* dest = (source AND pattern)     */
#define MERGEPAINT          (DWORD)0x00BB0226 /* dest = (NOT source) OR dest     */
#define PATCOPY             (DWORD)0x00F00021 /* dest = pattern                  */
#define PATPAINT            (DWORD)0x00FB0A09 /* dest = DPSnoo                   */
#define PATINVERT           (DWORD)0x005A0049 /* dest = pattern XOR dest         */
#define DSTINVERT           (DWORD)0x00550009 /* dest = (NOT dest)               */
#define BLACKNESS           (DWORD)0x00000042 /* dest = BLACK                    */
#define WHITENESS           (DWORD)0x00FF0062 /* dest = WHITE                    */


//#define SCALE_RES			1.33333333 // res differences btn MAC(72dpi) & PC(96dpi)
#define META_HEADER_SIZE 		9  //size in WORDs
#define META_HEADER_SIZE_BYTES	18
#define META_VERSION 			0x0300
#define DEFAULT_FONT_NAME      "Geneva"

typedef struct _WindowsMetaHeader
{
	WORD FileType;					// Type of metafile (1=memory, 2=disk)
	WORD HeaderSize;				// Size of header in WORDs (ALWAYS 9)
	WORD Version;					// Version of Micrososft Windows Used
	DWORD FileSize;					// Total size of the metafile in WORDs
	WORD NumOfObjects;				// Number of objects in the file		
	DWORD MaxRecordSize;			// size of largest record in WORDs
	WORD NoParamaters;				// Not used, Always 0
}WMFHEAD;

typedef struct _WindowsMetaRecord
{
	DWORD Size;						// Total size of the record in WORDs
	WORD Function;					// Function number (defined above)
	memory_ref Parameters;				// Parameter values passed to function, is WORD*;
}WMFRECORD;


// Probably will never use this as most of the information is already given in ConvertMetaFile
typedef struct _WmfSpecialHeader
{
	DWORD Key;						// Magic number, always 9AC6CDD7h
	WORD Handle;					// Metafile HANDLE number, always 0
	short Left;						// Left coordinate in metafile units
	short Top;						// Top coordinate in metafile units
	short Right;					// Right coordinate in metafile units
	short Bottom;					// Bottom coordinate in metafile units
	WORD Inch;						// Number of metafile units per inch
	DWORD Reserved;					// Always 0
	WORD Checksum;					// Checksum value for previous 10 WORDs
}WMFSPECIAL;

typedef struct _Win3xBitmapHeader
{
	DWORD Size; 					// Size of this header in bytes
	long  Width;					// Image Width in pixels
	long Height;					// IMage height inpixels
	WORD Planes;					// Number of color planes
	WORD BitsPerPixel;				// Number of bits per pixel
	DWORD Compression;				// Compression Methods Used
	DWORD SizeOfBitmap;				// Size of Bitmap in bytes
	long HorzResolution;			// Horizontal resolution in pixles per meter
	long VertResolution;			// Vertical resolution in pixels per meter
	DWORD ColorsUsed;				// Number of colors in image
	DWORD ColorsImportant;			// Minimum number of important colors
}BITMAPINFOHEADER;

typedef struct _Win3xPaletteEntry
{
	BYTE Blue;						
	BYTE Green;
	BYTE Red;
	BYTE Reserved;					// Always 0
}RGBQUAD;



typedef struct _BitmapInfo
{
	BITMAPINFOHEADER bmiHeader;
	memory_ref bmiColors; //RGBQUAD*
}BITMAPINFO;


typedef struct _StretchDibParams
{
	short XDest;
	short YDest;
	short nDestWidth;
	short nDestHeight;
	short XSrc;
	short YSrc;
	short nSrcWidth;
	short nSrcHeight;
	memory_ref ScanTable; //BYTE*
	memory_ref bmi;       //BITMAPINFO*
	unsigned short iUsage;
	DWORD dwRop;
}STRETCHDIBPARAMS;

typedef struct _LogPen
{
	unsigned short 	lopnStyle;
	Point			lopnWidth;
	RGBColor		lopnColor;  //COLORREF
}LOGPEN;


typedef struct _LogBrush
{

	unsigned short 	lbStyle;
	RGBColor	 	lbColor; //COLORREF
	DWORD 		 	lbHatch;
}LOGBRUSH;


typedef struct _LogRegion
{
	short 		left;
	short 		top;
	short 		right;
	short 		bottom;
}LOGREGION;


// following structure is used in when dib is not stored in metafile.
typedef struct _LogBitmap
{
	short 		bmType;
	short 		bmWidth;
	short 		bmHeight;
	short 		bmWidthBytes;
	BYTE 		bmPlanes;
	BYTE 		bmBitsPerPixel;
	void* 		bmBits;
}LOGBITMAP;

typedef struct
{
	memory_ref  mf_stretchDib;
}BITMAPOBJECT;
	
	
typedef struct _LogFont
{
	short 		lfHeight;
	short 		lfWidth;
	short 		lfEscapement;
	short 		lfOrientation;
	short 		lfWeight;
	BYTE 		lfItalic;
	BYTE 		lfUnderline;
	BYTE		lfStrikeOut;
	BYTE 		lfCharSet;
	BYTE		lfOutPrecision;
	BYTE		lfClipPrecision;
	BYTE		lfQuality;
	BYTE		lfPitchAndFamily;
	char		lfFaceName[LF_FACESIZE];
}LOGFONT;


typedef struct
{
	short numberEntries;
}PALOBJECT;


typedef struct _gdiObject
{
	short   index_gdi_object;
	short	  gdi_object_type;
}GDIOBJECT;

typedef struct _tagEscape
{
	short nEscape;
	short cbInput;
	memory_ref InData;
	memory_ref OutData;
}ESCAPEPARAMS;



//color conversion
RGBColor ConvertCOLORREFtoRGBColor(DWORD ColorRef);
RGBColor ConvertRGBQuadToRGBColor(RGBQUAD rgbq);		
RGBColor GetCOLORREF(pg_bits8_ptr* DataBytes);

//Basic parsing functions		
short GetMetaRecordParams(pg_bits8_ptr* DataBytes, WMFRECORD* metaRecord);		
WORD GetWordFromByteArray(const pg_bits8_ptr DataBytes);
DWORD GetDWordFromByteArray(const pg_bits8_ptr DataBytes);
		
int GetMetaHeaderFromByteArray(pg_bits8_ptr* DataBytes, WMFHEAD* metaHeader);
void GetMetaRecordFromByteArray(pg_bits8_ptr* DataBytes, WMFRECORD* metaRecord);




//Main record translation function
short TranslateMetaRecords(pg_bits8_ptr* DataBytes, Rect localframe, GrafPtr currentPort, short NumObjects, Rect frame);


// Dibs in WMF functions
short GetBitmapInfo(pg_bits8_ptr* DataBytes, memory_ref* bitmapInfo, memory_ref* ScanTableEntries);	
short GetStretchDibParams(pg_bits8_ptr* DataBytes, memory_ref mf_stretchDibParams);
short GetWidthInBytes(short width_pixels, short bits_per_pixel);
short bNeedsToBeFlipped(Point* WINExt, Point* WINOrigin);  //JWR 04/03/98: added function
short FlipDibScanTable(memory_ref* stretchDibParams);
short CalculateWINScanLineLength(short Width);
short CalculateMACScanLineLength(short Width);
short AddMACScanLinePadding(memory_ref* stretchDibParams);
ColorTable** CreateNewPixMapCTab(memory_ref pStretchDibParams);
short ConvertWIN24toMAC32(memory_ref stretchDibParams);
long DecodeRLEScanTable(pg_bits8_ptr ptr_ScanTable, short ScanlineLength, long SizeScanTable, short compress_method, pg_bits8_ptr* DataBytes);
short MacStretchDib(memory_ref* mfptr_stretchDibParams, Rect frame, Rect* rectangle, GrafPtr currentPort, Point* WINOrigin, Point* WINExt);



//Vector graphic functions
short GetWINOrigin (pg_bits8_ptr* DataBytes, Point* WINOrigin);
void GetFontParams(pg_bits8_ptr* DataBytes, LOGFONT* font, WMFRECORD* metaRecord);
void GetPenParams(pg_bits8_ptr* DataBytes, LOGPEN* pen, WMFRECORD* metaRecord);
void GetBrushParams (pg_bits8_ptr* DataBytes, LOGBRUSH* brush, WMFRECORD* metaRecord);
void GetRect(pg_bits8_ptr* DataBytes, Rect* rectangle, Point WINOrigin, Point WINExt, Rect localframe, WMFRECORD* metaRecord);
memory_ref GetPolygonParams(pg_bits8_ptr* DataBytes,short *NumPoints, Point WINOrigin, Point WINExt, Rect localframe, WMFRECORD* metaRecord);
void GetRoundRectParams(pg_bits8_ptr* DataBytes, Rect* rectangle, short* ovalWidth, short* ovalHeight, Point* WINOrigin,
	 Point* WINExt, Rect* localframe, WMFRECORD* metaRecord);



// My Drawing Functions
void DrawPie(LOGBRUSH* current_brush, LOGPEN* current_pen, Rect* rectangle, short start_angle, short stop_angle, Point* start_point,
				 Point* stop_point, Point* rect_center, short BKMode, RGBColor* rgbBackColor);
void DrawRectangle(LOGBRUSH* current_brush, LOGPEN* current_pen, Rect* rectangle, RGBColor* rgbBackColor, short BKMode);
void DrawRoundRect (LOGBRUSH* current_brush, LOGPEN* current_pen, Rect* rectangle, short ovalWidth, short ovalHeight, 
		RGBColor* rgbBackColor, short BKMode);
void DrawEllipse (LOGBRUSH* current_brush, LOGPEN* current_pen, Rect* rectangle, RGBColor* rgbBackColor, short BKMode);
void DrawPolygon(LOGBRUSH* current_brush, LOGPEN* current_pen, memory_ref mf_PolyPoints, long NumPoints, 
		short BKMode, RGBColor* rgbBackColor);


// Arc, Wedge, Chord functions (Note: Chord's not currently supported)
void CalculateRectCenter (Rect* rectangle, Point* rect_center);
void GetArcPoints(pg_bits8_ptr* DataBytes, Rect* rectangle, Point WINOrigin, Point WINExt, Rect localframe);
void FrameWedge (const Rect* r, short start_angle, short arc_angle, Point* start_point, Point* stop_point, Point* rect_center);
void GetWedgeParams(pg_bits8_ptr* DataBytes, Rect* rectangle, short* start_angle, short* stop_angle, Point* start_point, 
	Point* stop_point, Point* rect_center, Point WINOrigin, Point WINExt, Rect localframe, WMFRECORD* metaRecord);
void GetArcParams(pg_bits8_ptr* DataBytes, Rect* rectangle, short* start_angle, short* stop_angle, Point WINOrigin, Point WINExt,
		 Rect localframe, WMFRECORD* metaRecord);


// Managing GDI Objects
//short InitializeSelectedObjectsTable(memory_ref* selected_objects, short NumObjects);
short AddBrushToBrushTable(short* currentBrushIndex, short* CurrentNumElements,
		 memory_ref* brush_table, LOGBRUSH* brush);
short AddFontToFontTable(short* currentFontIndex, short* currentNumElements, memory_ref* font_table, LOGFONT* font);
short AddPenToPenTable(short* currentPenIndex, short* CurrentNumElements, memory_ref* pen_table, LOGPEN* pen);
short SelectGDIObject(short ObjectIndex, memory_ref* mfptr_object_table,/* memory_ref* mfptr_selected_objects,*/ BITMAPOBJECT* ptr_current_bitmap, LOGPEN* ptr_current_pen, 
	LOGBRUSH* ptr_current_brush, LOGFONT* ptr_current_font, memory_ref mf_bitmap_table, memory_ref mf_pen_table, memory_ref mf_brush_table, memory_ref mf_font_table);
void DeleteGDIObject(short ObjectIndex, memory_ref* mfptr_object_table);
short AddObjectToGDIObjectTable(short object_type, short* CurrentObjectIndex, short* CurrentNumElements, memory_ref* object_table, short specific_object_index);
short AddBitmapToBitmapTable(short* currentBitmapIndex, short* CurrentNumElements, memory_ref* mfptr_bitmap_table, memory_ref mf_stretchDibParams);
memory_ref MakeCopyStretchDib(memory_ref mf_stretchDibParams);
void DisposeStretchDibRef(memory_ref* mfptr_stretchDib);
short AddPaletteToPaletteTable(short* currentPaletteIndex, short* CurrentNumElements, memory_ref* mfptr_palette_table, PALOBJECT* ptr_pal);
short CreateMacPattern(DWORD fnColorSpec, memory_ref patternInfo, memory_ref scanBits, memory_ref* mfptr_mac_pattern);


// Text Functions
int ExtTextOut (pg_bits8_ptr* DataBytes, WMFRECORD* ptr_metaRecord, int current_mapmode, Point* ptr_WINOrigin, Point* ptr_WINExt, Rect* ptr_localframe,
					LOGFONT* ptr_current_font, GrafPtr currentPort, short text_align);
void myDrawText(char* ptr_string_to_display, short* ptr_character_spacing,int firstByteLoc, int ByteCount, Point* WINExt, Rect* localframe);
void GetPatBltParams(pg_bits8_ptr* DataBytes, Point* ptr_nLeftTopRect, short* ptr_width, short* ptr_height, DWORD* ptr_fdwRop, WMFRECORD* ptr_metarecord);
void ScalePatBltParams (Point* nTopLeftRect, short* width, short* height, Point* WINOrigin, Point* WINExt, Rect* localframe);
int MyPatBlt(Point* nTopLeftRect, short* width, short* height, DWORD* fdwRop, LOGBRUSH* current_brush, LOGPEN* current_pen);

#endif
