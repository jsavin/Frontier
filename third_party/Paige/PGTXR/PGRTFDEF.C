/* RTF definitions table,  used by RTF Import & Export. This file contains all the
keywords for RTF. */

#include "CPUDEFS.H"
#include "pgrtfdef.h"

#ifdef UNICODE

pg_char PG_FAR special_char_commands[] = {
	L"bullet ldblquote line lquote page par rdblquote rquote sect tab \0"
};

pg_char PG_FAR group_avoid_commands[] = {
	L"comment footer footerl footerf footerr footnote ftncn ftnsep ftnsepc header headerl headerf headerr rxe \0"
};


pg_char PG_FAR destination_commands[] = {
	L"ansi bin colortbl deff fonttbl mac macpict object objh objw pict pich pichgoal picscalex picscaley picw picwgoal rtf sbasedon stylesheet wmetafile \0"
};

pg_char PG_FAR document_commands[] = {
	L"margb margl margr margt paperh paperw \0"
};

pg_char PG_FAR paragraph_commands[] = {
	L"box brdrb brdrcf brdrdb brdrdot brdrl brdrr brdrs brdrsh brdrt brdrth brdrw brsp fi keepn li pagebb pard qc qj ql qr ri sa sb shading sl tldot tlhyph tqc tqdec tqr tx \0"
};

pg_char PG_FAR style_commands[] = {
	"b bk blue caps cchs cf dn expnd f fcharset fs green i outl plain red s scaps shad strike sub super tc tcl ul uld uldb ulw up v xe \0"
};

pg_char PG_FAR info_commands[] = {
	L"author buptim creatim doccomm dy edmins hr id keywords min mo nextfile noofchars nofpages nofwords operator printtim revtim sec subject title verno version yr \0"
};

pg_char PG_FAR table_commands[] = {
	L"cell cellx clbrdrb clbrdrl clbrdrr clbrdrt clmgf clmrg clshdng intbl row trbrdrb trbrdrh trbrdrl trbrdrr trbrdrv trbrdrt trgaph trhdr trkeep trleft trqc trql trqr trrh \0"
};

// Default stylesheet name:

pg_char PG_FAR def_stylesheet_name[] = {L"Normal Text\0"};

#else

pg_char PG_FAR special_char_commands[] = {
	"bullet ldblquote line lquote page par rdblquote rquote sect tab \0"
};

pg_char PG_FAR group_avoid_commands[] = {
	"comment footer footerl footerf footerr footnote ftncn ftnsep ftnsepc header headerl headerf headerr rxe \0"
};


pg_char PG_FAR destination_commands[] = {
	"ansi bin colortbl deff fonttbl mac macpict objdata object objh objw pict pich pichgoal picscalex picscaley picw picwgoal result rtf sbasedon stylesheet trowd wmetafile \0"
};


pg_char PG_FAR document_commands[] = {
	"margb margl margr margt paperh paperw \0"
};

pg_char PG_FAR paragraph_commands[] = {
	"box brdrb brdrcf brdrdb brdrdot brdrl brdrr brdrs brdrsh brdrt brdrth brdrw brsp fi keepn li pagebb pard qc qj ql qr ri sa sb shading sl tldot tlhyph tqc tqdec tqr tx \0"
};

pg_char PG_FAR style_commands[] = {
	"b bk blue caps cchs cf dn expnd f fcharset fs green i outl plain red s scaps shad strike sub super tc tcl ul uld uldb ulw up v xe \0"
};

pg_char PG_FAR info_commands[] = {
	"author buptim creatim doccomm dy edmins hr id keywords min mo nextfile noofchars nofpages nofwords operator printtim revtim sec subject title verno version yr \0"
};

pg_char PG_FAR table_commands[] = {
	"cell cellx clbrdrb clbrdrl clbrdrr clbrdrt clmgf clmrg clshdng intbl row trbrdrb trbrdrh trbrdrl trbrdrr trbrdrv trbrdrt trgaph trhdr trkeep trleft trqc trql trqr trrh \0"
};



// Default stylesheet name:

pg_char PG_FAR def_stylesheet_name[] = {"Normal Text\0"};

#endif

