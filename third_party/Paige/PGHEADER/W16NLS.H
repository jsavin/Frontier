#ifdef WIN16_COMPILE

#ifndef _W16NLS_H
#define _W16NLS_H

#if !defined(_GDI32_)
#define WINGDIAPI DECLSPEC_IMPORT
#else
#define WINGDIAPI
#endif

#ifdef __cplusplus
extern "C" {
#endif

#ifndef EXTERN_C
# ifdef __cplusplus
#  define EXTERN_C extern "C"
# else
#  define EXTERN_C extern
# endif
#endif

#ifdef _MAC
# ifndef FAR
#  define FAR
# endif
# ifdef _MSC_VER
#  define NLSAPI_(TYPE) EXTERN_C TYPE __pascal
# else
#  define NLSAPI_(TYPE) EXTERN_C pascal TYPE
# endif
#else
# define NLSAPI_(TYPE) EXTERN_C TYPE WINAPI
#endif


/***************************************************************************\
* Constants
*
* Define all constants for the NLS component here.
\***************************************************************************/

/*
 *  Character Type Flags.
 */
#define CT_CTYPE1            0x00000001     /* ctype 1 information */
#define CT_CTYPE2            0x00000002     /* ctype 2 information */
#define CT_CTYPE3            0x00000004     /* ctype 3 information */

/*
 *  CType 1 Flag Bits.
 */
#define C1_UPPER             0x0001         /* upper case */
#define C1_LOWER             0x0002         /* lower case */
#define C1_DIGIT             0x0004         /* decimal digits */
#define C1_SPACE             0x0008         /* spacing characters */
#define C1_PUNCT             0x0010         /* punctuation characters */
#define C1_CNTRL             0x0020         /* control characters */
#define C1_BLANK             0x0040         /* blank characters */
#define C1_XDIGIT            0x0080         /* other digits */
#define C1_ALPHA             0x0100         /* any letter */

/*
 *  CType 2 Flag Bits.
 */
#define C2_LEFTTORIGHT       0x1            /* left to right */
#define C2_RIGHTTOLEFT       0x2            /* right to left */

#define C2_EUROPENUMBER      0x3            /* European number, digit */
#define C2_EUROPESEPARATOR   0x4            /* European numeric separator */
#define C2_EUROPETERMINATOR  0x5            /* European numeric terminator */
#define C2_ARABICNUMBER      0x6            /* Arabic number */
#define C2_COMMONSEPARATOR   0x7            /* common numeric separator */

#define C2_BLOCKSEPARATOR    0x8            /* block separator */
#define C2_SEGMENTSEPARATOR  0x9            /* segment separator */
#define C2_WHITESPACE        0xA            /* white space */
#define C2_OTHERNEUTRAL      0xB            /* other neutrals */

#define C2_NOTAPPLICABLE     0x0            /* no implicit directionality */

/*
 *  CType 3 Flag Bits.
 */
#define C3_NONSPACING        0x0001         /* nonspacing character */
#define C3_DIACRITIC         0x0002         /* diacritic mark */
#define C3_VOWELMARK         0x0004         /* vowel mark */
#define C3_SYMBOL            0x0008         /* symbols */

#define C3_KATAKANA          0x0010
#define C3_HIRAGANA          0x0020
#define C3_HALFWIDTH         0x0040
#define C3_FULLWIDTH         0x0080
#define C3_IDEOGRAPH         0x0100
#define C3_JAPANACCENT       0x0200

#define C3_NOTAPPLICABLE     0x0            /* ctype 3 is not applicable */


/*
 *  String Flags.
 */
#define NORM_IGNORECASE         0x00000001  /* ignore case */
#define NORM_IGNORENONSPACE     0x00000002  /* ignore nonspacing chars */
#define NORM_IGNORESYMBOLS      0x00000004  /* ignore symbols */

#define NORM_IGNOREWIDTH        0x00000008  /* ignore width */
#define NORM_IGNOREKANATYPE     0x00000040  /* ignore kanatype */
#define NORM_IGNOREJAPANACCENT  0x00000080  /* ignore Japanese accent marks */

#define NORM_IGNOREKASHIDA      0x00040000  /* ignore Arabic kashida chars */


/*
 *  Locale Dependent Mapping Flags.
 */
#define LCMAP_LOWERCASE      0x00000100 /* lower case letters */
#define LCMAP_UPPERCASE      0x00000200 /* upper case letters */
#define LCMAP_SORTKEY        0x00000400 /* WC sort key (normalize) */

#define LCMAP_HALFWIDTH      0x00000800  /* narrow pitch case letters */
#define LCMAP_FULLWIDTH      0x00001000  /* wide picth case letters */ 
#define LCMAP_HIRAGANA       0x00002000  /* map katakana to hiragana */
#define LCMAP_KATAKANA       0x00004000  /* map hiragana to katakana */


/*
 *  Locale Types.
 *
 *  These types are used for the GetLocaleInfoA NLS API routine.
 */

#define LOCALE_NOUSEROVERRIDE   0x80000000    /* OR in to avoid user override */

#define LOCALE_ILANGUAGE            0x0001    /* language id */
#define LOCALE_SLANGUAGE            0x0002    /* localized name of language */
#define LOCALE_SENGLANGUAGE         0x1001    /* English name of language */
#define LOCALE_SABBREVLANGNAME      0x0003    /* abbreviated language name */
#define LOCALE_SNATIVELANGNAME      0x0004    /* native name of language */
#define LOCALE_ICOUNTRY             0x0005    /* country code */
#define LOCALE_SCOUNTRY             0x0006    /* localized name of country */  
#define LOCALE_SENGCOUNTRY          0x1002    /* English name of country */  
#define LOCALE_SABBREVCTRYNAME      0x0007    /* abbreviated country name */
#define LOCALE_SNATIVECTRYNAME      0x0008    /* native name of country */  
#define LOCALE_IDEFAULTLANGUAGE     0x0009    /* default language id */
#define LOCALE_IDEFAULTCOUNTRY      0x000A    /* default country code */
#define LOCALE_IDEFAULTCODEPAGE     0x000B    /* default code page */
                                            
#define LOCALE_SLIST                0x000C    /* list item separator */
#define LOCALE_IMEASURE             0x000D    /* 0 = metric, 1 = US */
                                            
#define LOCALE_SDECIMAL             0x000E    /* decimal separator */
#define LOCALE_STHOUSAND            0x000F    /* thousand separator */
#define LOCALE_SGROUPING            0x0010    /* digit grouping */
#define LOCALE_IDIGITS              0x0011    /* number of fractional digits */
#define LOCALE_ILZERO               0x0012    /* leading zeros for decimal */
#define LOCALE_SNATIVEDIGITS        0x0013    /* native ascii 0-9 */
                                            
#define LOCALE_SCURRENCY            0x0014    /* local monetary symbol */
#define LOCALE_SINTLSYMBOL          0x0015    /* intl monetary symbol */
#define LOCALE_SMONDECIMALSEP       0x0016    /* monetary decimal separator */
#define LOCALE_SMONTHOUSANDSEP      0x0017    /* monetary thousand separator */
#define LOCALE_SMONGROUPING         0x0018    /* monetary grouping */
#define LOCALE_ICURRDIGITS          0x0019    /* # local monetary digits */
#define LOCALE_IINTLCURRDIGITS      0x001A    /* # intl monetary digits */
#define LOCALE_ICURRENCY            0x001B    /* positive currency mode */
#define LOCALE_INEGCURR             0x001C    /* negative currency mode */
                                            
#define LOCALE_SDATE                0x001D    /* date separator */
#define LOCALE_STIME                0x001E    /* time separator */
#define LOCALE_SSHORTDATE           0x001F    /* short date-time separator */
#define LOCALE_SLONGDATE            0x0020    /* long date-time separator */
#define LOCALE_IDATE                0x0021    /* short date format ordering */
#define LOCALE_ILDATE               0x0022    /* long date format ordering */
#define LOCALE_ITIME                0x0023    /* time format specifier */
#define LOCALE_ICENTURY             0x0024    /* century format specifier */
#define LOCALE_ITLZERO              0x0025    /* leading zeros in time field */
#define LOCALE_IDAYLZERO            0x0026    /* leading zeros in day field */
#define LOCALE_IMONLZERO            0x0027    /* leading zeros in month field */
#define LOCALE_S1159                0x0028    /* AM designator */
#define LOCALE_S2359                0x0029    /* PM designator */
                                            
#define LOCALE_SDAYNAME1            0x002A    /* long name for Monday */
#define LOCALE_SDAYNAME2            0x002B    /* long name for Tuesday */
#define LOCALE_SDAYNAME3            0x002C    /* long name for Wednesday */
#define LOCALE_SDAYNAME4            0x002D    /* long name for Thursday */
#define LOCALE_SDAYNAME5            0x002E    /* long name for Friday */
#define LOCALE_SDAYNAME6            0x002F    /* long name for Saturday */
#define LOCALE_SDAYNAME7            0x0030    /* long name for Sunday */
#define LOCALE_SABBREVDAYNAME1      0x0031    /* abbreviated name for Monday */   
#define LOCALE_SABBREVDAYNAME2      0x0032    /* abbreviated name for Tuesday */  
#define LOCALE_SABBREVDAYNAME3      0x0033    /* abbreviated name for Wednesday */
#define LOCALE_SABBREVDAYNAME4      0x0034    /* abbreviated name for Thursday */ 
#define LOCALE_SABBREVDAYNAME5      0x0035    /* abbreviated name for Friday */   
#define LOCALE_SABBREVDAYNAME6      0x0036    /* abbreviated name for Saturday */ 
#define LOCALE_SABBREVDAYNAME7      0x0037    /* abbreviated name for Sunday */   
#define LOCALE_SMONTHNAME1          0x0038    /* long name for January */
#define LOCALE_SMONTHNAME2          0x0039    /* long name for February */
#define LOCALE_SMONTHNAME3          0x003A    /* long name for March */
#define LOCALE_SMONTHNAME4          0x003B    /* long name for April */
#define LOCALE_SMONTHNAME5          0x003C    /* long name for May */
#define LOCALE_SMONTHNAME6          0x003D    /* long name for June */
#define LOCALE_SMONTHNAME7          0x003E    /* long name for July */
#define LOCALE_SMONTHNAME8          0x003F    /* long name for August */
#define LOCALE_SMONTHNAME9          0x0040    /* long name for September */
#define LOCALE_SMONTHNAME10         0x0041    /* long name for October */
#define LOCALE_SMONTHNAME11         0x0042    /* long name for November */
#define LOCALE_SMONTHNAME12         0x0043    /* long name for December */
#define LOCALE_SABBREVMONTHNAME1    0x0044    /* abbreviated name for January */
#define LOCALE_SABBREVMONTHNAME2    0x0045    /* abbreviated name for February */
#define LOCALE_SABBREVMONTHNAME3    0x0046    /* abbreviated name for March */
#define LOCALE_SABBREVMONTHNAME4    0x0047    /* abbreviated name for April */
#define LOCALE_SABBREVMONTHNAME5    0x0048    /* abbreviated name for May */
#define LOCALE_SABBREVMONTHNAME6    0x0049    /* abbreviated name for June */
#define LOCALE_SABBREVMONTHNAME7    0x004A    /* abbreviated name for July */
#define LOCALE_SABBREVMONTHNAME8    0x004B    /* abbreviated name for August */
#define LOCALE_SABBREVMONTHNAME9    0x004C    /* abbreviated name for September */
#define LOCALE_SABBREVMONTHNAME10   0x004D    /* abbreviated name for October */
#define LOCALE_SABBREVMONTHNAME11   0x004E    /* abbreviated name for November */
#define LOCALE_SABBREVMONTHNAME12   0x004F    /* abbreviated name for December */
                                            
#define LOCALE_SPOSITIVESIGN        0x0050    /* positive sign */
#define LOCALE_SNEGATIVESIGN        0x0051    /* negative sign */
#define LOCALE_IPOSSIGNPOSN         0x0052    /* positive sign position */
#define LOCALE_INEGSIGNPOSN         0x0053    /* negative sign position */
#define LOCALE_IPOSSYMPRECEDES      0x0054    /* mon sym precedes pos amt */
#define LOCALE_IPOSSEPBYSPACE       0x0055    /* mon sym sep by space from pos */ 
#define LOCALE_INEGSYMPRECEDES      0x0056    /* mon sym precedes neg amt */
#define LOCALE_INEGSEPBYSPACE       0x0057    /* mon sym sep by space from neg */

/*
 *  Character Set Types.
 *
 */

#define JOHAB_CHARSET           130
#define HEBREW_CHARSET          177
#define ARABIC_CHARSET          178
#define GREEK_CHARSET           161
#define TURKISH_CHARSET         162
#define THAI_CHARSET            222
#define EASTEUROPE_CHARSET      238
#define RUSSIAN_CHARSET         204
#define MAC_CHARSET             77
#define BALTIC_CHARSET          186

/*
 *  Language Support.
 *
 */

#define MAKELCID(lgid, srtid)  ((DWORD)((((DWORD)((WORD  )(srtid))) << 16) |  \
                                         ((DWORD)((WORD  )(lgid)))))
#define MAKELANGID(p, s)       ((((WORD  )(s)) << 10) | (WORD  )(p))
#define PRIMARYLANGID(lgid)    ((WORD  )(lgid) & 0x3ff)
#define SUBLANGID(lgid)        ((WORD  )(lgid) >> 10)
#define LANG_NEUTRAL                     0x00

#define LANG_ARABIC                      0x01
#define LANG_AFRIKAANS                   0x36
#define LANG_ALBANIAN                    0x1c
#define LANG_BASQUE                      0x2d
#define LANG_BULGARIAN                   0x02
#define LANG_BYELORUSSIAN                0x23
#define LANG_CATALAN                     0x03
#define LANG_CHINESE                     0x04
#define LANG_CROATIAN                    0x1a
#define LANG_CZECH                       0x05
#define LANG_DANISH                      0x06
#define LANG_DUTCH                       0x13
#define LANG_ENGLISH                     0x09
#define LANG_ESTONIAN                    0x25
#define LANG_FAEROESE                    0x38
#define LANG_FARSI                       0x29
#define LANG_FINNISH                     0x0b
#define LANG_FRENCH                      0x0c
#define LANG_GERMAN                      0x07
#define LANG_GREEK                       0x08
#define LANG_HEBREW                      0x0D
#define LANG_HUNGARIAN                   0x0e
#define LANG_ICELANDIC                   0x0f
#define LANG_INDONESIAN                  0x21
#define LANG_ITALIAN                     0x10
#define LANG_JAPANESE                    0x11
#define LANG_KOREAN                      0x12
#define LANG_LATVIAN                     0x26
#define LANG_LITHUANIAN                  0x27
#define LANG_NORWEGIAN                   0x14
#define LANG_POLISH                      0x15
#define LANG_PORTUGUESE                  0x16
#define LANG_RHAETO_ROMAN                0x17
#define LANG_ROMANIAN                    0x18
#define LANG_RUSSIAN                     0x19
#define LANG_SERBO_CROATIAN              0x1a
#define LANG_SLOVAK                      0x1b
#define LANG_SLOVENIAN                   0x24
#define LANG_SORBIAN                     0x2e
#define LANG_SPANISH                     0x0a
#define LANG_SWEDISH                     0x1d
#define LANG_THAI                        0x1e
#define LANG_TURKISH                     0x1f
#define LANG_UKRAINIAN                   0x22
#define LANG_URDU                        0x20

/*
 *  Sublanguage IDs.
 *
 *  The name immediately following SUBLANG_ dictates which primary
 *  language ID that sublanguage ID can be combined with to form a
 *  valid language ID.
 */
#define SUBLANG_NEUTRAL                  0x00    /* language neutral */
#define SUBLANG_DEFAULT                  0x01    /* user default */
#define SUBLANG_SYS_DEFAULT              0x02    /* system default */

#define SUBLANG_ARABIC                   0x01    /* Arabic (Saudi Arabia) */
#define SUBLANG_ARABIC_IRAQ              0x02    /* Arabic (Iraq) */
#define SUBLANG_ARABIC_EGYPT             0x03    /* Arabic (Egypt) */
#define SUBLANG_ARABIC_LIBYA             0x04    /* Arabic (Libya) */
#define SUBLANG_ARABIC_ALGERIA           0x05    /* Arabic (Algeria) */
#define SUBLANG_ARABIC_MOROCCO           0x06    /* Arabic (Morocco) */
#define SUBLANG_ARABIC_TUNISIA           0x07    /* Arabic (Tunisia) */
#define SUBLANG_ARABIC_OMAN              0x08    /* Arabic (Oman) */
#define SUBLANG_ARABIC_YEMEN             0x09    /* Arabic (Yemen) */
#define SUBLANG_ARABIC_SYRIA             0x10    /* Arabic (Syria) */
#define SUBLANG_ARABIC_JORDAN            0x11    /* Arabic (Jordan) */
#define SUBLANG_ARABIC_LEBANON           0x12    /* Arabic (Lebanon) */
#define SUBLANG_ARABIC_KUWAIT            0x13    /* Arabic (Kuwait) */
#define SUBLANG_ARABIC_UAE               0x14    /* Arabic (United Arab Emirates) */
#define SUBLANG_ARABIC_BAHRAIN           0x15    /* Arabic (Bahrain) */
#define SUBLANG_ARABIC_QATAR             0x16    /* Arabic (Qatar) */
#define SUBLANG_CHINESE_TRADITIONAL      0x01    /* Chinese (Taiwan) */
#define SUBLANG_CHINESE_SIMPLIFIED       0x02    /* Chinese (PR China) */
#define SUBLANG_CHINESE_HONGKONG         0x03    /* Chinese (Hong Kong) */
#define SUBLANG_CHINESE_SINGAPORE        0x04    /* Chinese (Singapore) */
#define SUBLANG_DUTCH                    0x01    /* Dutch */
#define SUBLANG_DUTCH_BELGIAN            0x02    /* Dutch (Belgian) */
#define SUBLANG_ENGLISH_US               0x01    /* English (USA) */
#define SUBLANG_ENGLISH_UK               0x02    /* English (UK) */
#define SUBLANG_ENGLISH_AUS              0x03    /* English (Australian) */
#define SUBLANG_ENGLISH_CAN              0x04    /* English (Canadian) */
#define SUBLANG_ENGLISH_NZ               0x05    /* English (New Zealand) */
#define SUBLANG_ENGLISH_EIRE             0x06    /* English (Irish) */
#define SUBLANG_ENGLISH_SAFRICA          0x07    /* English (South Africa) */
#define SUBLANG_ENGLISH_JAMAICA          0x08    /* English (Jamaica) */
#define SUBLANG_ENGLISH_CARRIBEAN        0x09    /* English (Carribean) */
#define SUBLANG_FRENCH                   0x01    /* French */
#define SUBLANG_FRENCH_BELGIAN           0x02    /* French (Belgian) */
#define SUBLANG_FRENCH_CANADIAN          0x03    /* French (Canadian) */
#define SUBLANG_FRENCH_SWISS             0x04    /* French (Swiss) */
#define SUBLANG_FRENCH_LUXEMBOURG        0x05    /* French (Luxembourg) */
#define SUBLANG_GERMAN                   0x01    /* German */
#define SUBLANG_GERMAN_SWISS             0x02    /* German (Swiss) */
#define SUBLANG_GERMAN_AUSTRIAN          0x03    /* German (Austrian) */
#define SUBLANG_GERMAN_LUXEMBOURG        0x04    /* German (Luxembourg) */
#define SUBLANG_GERMAN_LIECHTENSTEIN     0x05    /* German (Liechtenstein) */
#define SUBLANG_ITALIAN                  0x01    /* Italian */
#define SUBLANG_ITALIAN_SWISS            0x02    /* Italian (Swiss) */
#define SUBLANG_KOREAN                   0x01    /* Korean */
#define SUBLANG_KOREAN_JOHAB             0x02    /* Korean (Johab) */
#define SUBLANG_NORWEGIAN_BOKMAL         0x01    /* Norwegian (Bokmal) */
#define SUBLANG_NORWEGIAN_NYNORSK        0x02    /* Norwegian (Nynorsk) */
#define SUBLANG_PORTUGUESE               0x02    /* Portuguese */
#define SUBLANG_PORTUGUESE_BRAZILIAN     0x01    /* Portuguese (Brazilian) */
#define SUBLANG_SERBO_CROATIAN_CYRILLIC  0x02    /* Serbo-Croatian (Cyrillic) */
#define SUBLANG_SERBO_CROATIAN_LATIN     0x01    /* Croato-Serbian (Latin) */
#define SUBLANG_SPANISH                  0x01    /* Spanish (Castilian) */
#define SUBLANG_SPANISH_MEXICAN          0x02    /* Spanish (Mexican) */
#define SUBLANG_SPANISH_MODERN           0x03    /* Spanish (Modern) */
#define SUBLANG_SPANISH_GUATEMALA        0x04    /* Spanish (Guatemala) */
#define SUBLANG_SPANISH_COSTARICA        0x05    /* Spanish (Costa Rica) */
#define SUBLANG_SPANISH_PANAMA           0x06    /* Spanish (Panama) */
#define SUBLANG_SPANISH_DOMINICAN        0x07    /* Spanish (Dominican Republic) */
#define SUBLANG_SPANISH_VENEZUELA        0x08    /* Spanish (Venezuela) */
#define SUBLANG_SPANISH_COLOMBIA         0x09    /* Spanish (Colombia) */
#define SUBLANG_SPANISH_PERU             0x10    /* Spanish (Peru) */
#define SUBLANG_SPANISH_ARGENTINA        0x11    /* Spanish (Argentina) */
#define SUBLANG_SPANISH_ECUADOR          0x12    /* Spanish (Ecuador) */
#define SUBLANG_SPANISH_CHILE            0x13    /* Spanish (Chile) */
#define SUBLANG_SPANISH_URUGUAY          0x14    /* Spanish (Uruguay) */
#define SUBLANG_SPANISH_PARAGUAY         0x15    /* Spanish (Paraguay) */
#define SUBLANG_SPANISH_BOLIVIA          0x16    /* Spanish (Bolivia) */
/*
 *  Sorting IDs.
 *
 */
#define SORT_DEFAULT                     0x0     /* sorting default */

#define SORT_JAPANESE_XJIS               0x0     /* Japanese XJIS order */
#define SORT_JAPANESE_UNICODE            0x1     /* Japanese Unicode order */

#define SORT_CHINESE_BIG5                0x0     /* Chinese BIG5 order */
#define SORT_CHINESE_UNICODE             0x1     /* Chinese Unicode order */

#define SORT_KOREAN_KSC                  0x0     /* Korean KSC order */
#define SORT_KOREAN_UNICODE              0x1     /* Korean Unicode order */

#define CP_WINANSI      1004    /* default codepage for windows & old DDE convs. */
#define CP_WINUNICODE   1200

#define MAX_LEADBYTES        12             /* 5 ranges, 2 bytes ea., 0 term. */
#define MAX_DEFAULTCHAR      2              /* single or double byte */

typedef struct _cpinfo {
    UINT    MaxCharSize;                    /* max length (bytes) of a char */
    BYTE    DefaultChar[MAX_DEFAULTCHAR];   /* default character */
    BYTE    LeadByte[MAX_LEADBYTES];        /* lead byte ranges */
} CPINFO, *LPCPINFO;


/***************************************************************************\
* Function Prototypes
*
* Only prototypes for the NLS APIs should go here.
\***************************************************************************/
typedef unsigned long  LCID;                  /* locale ID */
typedef unsigned short LANGID;                /* language ID */
typedef unsigned long  LCTYPE;                /* locale type constant */

 
NLSAPI_(int)
CompareStringA(LCID, unsigned long, const char FAR*, int, const char FAR*, int);

NLSAPI_(int)
LCMapStringA(LCID, unsigned long, const char FAR*, int, char FAR*, int);

NLSAPI_(int)
GetLocaleInfoA(LCID, LCTYPE, char FAR*, int);

NLSAPI_(int)
GetStringTypeA(LCID, unsigned long, const char FAR*, int, unsigned short FAR*);

NLSAPI_(LANGID)
GetSystemDefaultLangID(void);

NLSAPI_(LANGID)
GetUserDefaultLangID(void);

NLSAPI_(LCID)
GetSystemDefaultLCID(void);

NLSAPI_(LCID)
GetUserDefaultLCID(void);


#ifdef __cplusplus
}
#endif

#endif /* _W16NLS */
#endif
