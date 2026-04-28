/* A Bison parser, made by GNU Bison 2.3.  */

/*
    Skeleton implementation for Bison's Yacc-like parsers in C

    SPDX-License-Identifier: MIT

    Copyright (C) 1984, 1989, 1990, 2000, 2001, 2002, 2003, 2004, 2005, 2006
    Free Software Foundation, Inc. (parser skeleton)
    Copyright (c) 1992-2004 UserLand Software, Inc. (grammar and application code)
    Copyright (c) 2025-2026 Frontier contributors

    The Bison parser skeleton is distributed with a special exception
    permitting redistribution under any license; see the special exception
    notice below. The MIT license above governs the combined work.

    Permission is hereby granted, free of charge, to any person obtaining a
    copy of this software and associated documentation files (the "Software"),
    to deal in the Software without restriction, including without limitation
    the rights to use, copy, modify, merge, publish, distribute, sublicense,
    and/or sell copies of the Software, and to permit persons to whom the
    Software is furnished to do so, subject to the following conditions:

    The above copyright notice and this permission notice shall be included in
    all copies or substantial portions of the Software.

    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
    IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
    FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
    AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
    LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
    FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
    DEALINGS IN THE SOFTWARE.
*/

/* As a special exception, you may create a larger work that contains
   part or all of the Bison parser skeleton and distribute that work
   under terms of your choice, so long as that work isn't itself a
   parser generator using the skeleton or a modified version thereof
   as a parser skeleton.  Alternatively, if you modify or redistribute
   the parser skeleton itself, you may (at your option) remove this
   special exception, which will cause the skeleton and the resulting
   Bison output files to be licensed under the GNU General Public
   License without this special exception.

   This special exception was added by the Free Software Foundation in
   version 2.2 of Bison.  */

/* C LALR(1) parser skeleton written by Richard Stallman, by
   simplifying the original so-called "semantic" parser.  */

/* All symbols defined below should begin with yy or YY, to avoid
   infringing on user name space.  This should be done even for local
   variables, as they might otherwise be expanded by user macros.
   There are some unavoidable exceptions within include files to
   define necessary library symbols; they are noted "INFRINGES ON
   USER NAME SPACE" below.  */

/* Identify Bison output.  */
#define YYBISON 1

/* Bison version.  */
#define YYBISON_VERSION "2.3"

/* Skeleton name.  */
#define YYSKELETON_NAME "yacc.c"

/* Pure parsers.  */
#define YYPURE 0

/* Using locations.  */
#define YYLSP_NEEDED 0



/* Tokens.  */
#ifndef YYTOKENTYPE
# define YYTOKENTYPE
   /* Put the tokens into the symbol table, so that GDB and other debuggers
      know about them.  */
   enum yytokentype {
     EQtoken = 400,
     NEtoken = 401,
     GTtoken = 402,
     LTtoken = 403,
     GEtoken = 404,
     LEtoken = 405,
     nottoken = 406,
     andandtoken = 407,
     orortoken = 408,
     beginswithtoken = 409,
     endswithtoken = 410,
     containstoken = 411,
     bitandtoken = 412,
     bitortoken = 413,
     looptoken = 500,
     filelooptoken = 501,
     intoken = 502,
     breaktoken = 503,
     returntoken = 504,
     iftoken = 505,
     thentoken = 506,
     elsetoken = 507,
     bundletoken = 508,
     localtoken = 509,
     ontoken = 510,
     whiletoken = 511,
     casetoken = 512,
     kerneltoken = 513,
     fortoken = 514,
     totoken = 515,
     downtotoken = 516,
     continuetoken = 517,
     withtoken = 518,
     trytoken = 519,
     globaltoken = 520,
     errortoken = 292,
     eoltoken = 293,
     constanttoken = 294,
     identifiertoken = 295,
     othertoken = 296,
     assigntoken = 297,
     addtoken = 298,
     subtracttoken = 299,
     multiplytoken = 300,
     dividetoken = 301,
     modtoken = 302,
     plusplustoken = 303,
     minusminustoken = 304,
     unaryminus = 522
   };
#endif
/* Tokens.  */
#define EQtoken 400
#define NEtoken 401
#define GTtoken 402
#define LTtoken 403
#define GEtoken 404
#define LEtoken 405
#define nottoken 406
#define andandtoken 407
#define orortoken 408
#define beginswithtoken 409
#define endswithtoken 410
#define containstoken 411
#define bitandtoken 412
#define bitortoken 413
#define looptoken 500
#define filelooptoken 501
#define intoken 502
#define breaktoken 503
#define returntoken 504
#define iftoken 505
#define thentoken 506
#define elsetoken 507
#define bundletoken 508
#define localtoken 509
#define ontoken 510
#define whiletoken 511
#define casetoken 512
#define kerneltoken 513
#define fortoken 514
#define totoken 515
#define downtotoken 516
#define continuetoken 517
#define withtoken 518
#define trytoken 519
#define globaltoken 520
#define errortoken 292
#define eoltoken 293
#define constanttoken 294
#define identifiertoken 295
#define othertoken 296
#define assigntoken 297
#define addtoken 298
#define subtracttoken 299
#define multiplytoken 300
#define dividetoken 301
#define modtoken 302
#define plusplustoken 303
#define minusminustoken 304
#define unaryminus 522




/* Copy the first part of user declarations.  */
#line 29 "langparser.y"


/*
langparser.y generates langparser.c.  

use MPW Shell and the MPW tool MacYACC to create a LSC-compatible parser 
in langparser.c. the command line to generate langparser.c and yytab.h is:

	macyacc -d  "langparser.y"

*/

/*
these declarations make it possible to compile and run langparser.c 
with no modification to the source code as generated by MacYACC.

5/29/92 dmb: added try statement handling

5.1.2 dmb: added cleanandexit block to dispose everything on memory errors
*/

#include "frontier.h"
#include "standard.h"

#include "memory.h"
#include "strings.h"
#include "lang.h"
#include "langinternal.h"
#include "langparser.h"
#include "logging.h"

/* Ensure Bison uses hdltreenode as semantic value type */
#ifndef YYSTYPE
#define YYSTYPE hdltreenode
#define YYSTYPE_IS_DECLARED 1
#endif


//static FILE __file[1];

/*
	2006-04-01 kw & aradke: The following static function templates caused
		a compilation error with GCC 4.0 because we already picked up the
		functions from stdio.h and string.h via osincludes.h and frontier.h.
		Therefore, we commented out the following templates. The functions
		are never called in real life anyway because yytflag is never non-nil.
*/

/*
static int fprintf (FILE *f, const char *s, ...) {return (0);}

static FILE *fopen (const char *s1, const char *s2) {return (NULL);}

static int fclose (FILE *f) {return (0);}

static int strcmp (const char * str1, const char * str2) {return (0);}
*/

/*
10/2/91 dmb: a disturbing discovery -- our error handling doesn't deallocate 
any of the code tree that's been built so for.  searching through langparser.c, 
it appears that there are three data structures will need to go through: yyval, 
yylval, and the value stack, yyv.  Unfortunately, the value stack pointer, yypv, 
is a local in yyparse, so we have to be tricky to get at it.  so I'm replacing 
the yyerror routine with a macro that calls the real error routine, but passes 
the stack pointer as an additional parameter.

there's probably a much cleaner way to do this, but I don't have the 
documentation, nor much time...

11/22/91 dmb: well, there's little time indeed, but we now have MacYACC 3.0, and 
a little documentation.  The right was to deal with data structure disposal on 
errors is to make sure that there are reduction rules for every error situation, 
so that after yyerror is called the parser can ruduce the stack all the way down 
to our start token (module), where the tree can be safely disposed.  I've added a 
number of such reductions that should cover many error situations, but a much more 
thorough job needs to be done eventually.  It's hard to trap errors at a very atomic 
level without introducing ambiguities to the grammer (i.e. reduction conflicts).  
So, given the time to work on it, I would go through the definition of "statement" 
and add rules for every permutation of errors within the more complex contructs.  The 
key thing to understand is that "expr" is written to handle any error, so anything 
that depends on expr is covered.  It's unique contructs associated with some compound 
statements that need extra work.
*/

/* Global result handle for modern parsers */
hdltreenode langparser_result = nil;

/* Bison compatibility shims: map legacy MacYACC identifiers to Bison's */
#if defined(YYBISON) || defined(YYBISON_VERSION)
/* Provide yylex prototype to satisfy C99; matches static definition below */
static int yylex(void);
/* Provide yyerror prototype expected by Bison */
int yyerror(const char *s);
/* Simple trace for debugging parser actions (guarded) */
static void yytrace(const char *s) {
    (void)s;
#ifdef PARSER_TRACE
    log_trace(LOG_COMP_PARSE, "%s", s);
#endif
}
/* MacYACC err count symbol -> Bison's yynerrs */
#ifndef pcyyerrct
#define pcyyerrct yynerrs
#endif
/* Legacy semantic stack pointer/name -> Bison's */
#ifndef yypv
#define yypv yyvsp
#endif
#ifndef yyv
#define yyv yyvs
#endif
#endif /* Bison compatibility */



/* Enabling traces.  */
#ifndef YYDEBUG
# define YYDEBUG 0
#endif

/* Enabling verbose error messages.  */
#ifdef YYERROR_VERBOSE
# undef YYERROR_VERBOSE
# define YYERROR_VERBOSE 1
#else
# define YYERROR_VERBOSE 0
#endif

/* Enabling the token table.  */
#ifndef YYTOKEN_TABLE
# define YYTOKEN_TABLE 0
#endif

#if ! defined YYSTYPE && ! defined YYSTYPE_IS_DECLARED
typedef int YYSTYPE;
# define yystype YYSTYPE /* obsolescent; will be withdrawn */
# define YYSTYPE_IS_DECLARED 1
# define YYSTYPE_IS_TRIVIAL 1
#endif



/* Copy the second part of user declarations.  */


/* Line 216 of yacc.c.  */
#line 324 "langparser.c"

#ifdef short
# undef short
#endif

#ifdef YYTYPE_UINT8
typedef YYTYPE_UINT8 yytype_uint8;
#else
typedef unsigned char yytype_uint8;
#endif

#ifdef YYTYPE_INT8
typedef YYTYPE_INT8 yytype_int8;
#elif (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
typedef signed char yytype_int8;
#else
typedef short int yytype_int8;
#endif

#ifdef YYTYPE_UINT16
typedef YYTYPE_UINT16 yytype_uint16;
#else
typedef unsigned short int yytype_uint16;
#endif

#ifdef YYTYPE_INT16
typedef YYTYPE_INT16 yytype_int16;
#else
typedef short int yytype_int16;
#endif

#ifndef YYSIZE_T
# ifdef __SIZE_TYPE__
#  define YYSIZE_T __SIZE_TYPE__
# elif defined size_t
#  define YYSIZE_T size_t
# elif ! defined YYSIZE_T && (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
#  include <stddef.h> /* INFRINGES ON USER NAME SPACE */
#  define YYSIZE_T size_t
# else
#  define YYSIZE_T unsigned int
# endif
#endif

#define YYSIZE_MAXIMUM ((YYSIZE_T) -1)

#ifndef YY_
# if defined YYENABLE_NLS && YYENABLE_NLS
#  if ENABLE_NLS
#   include <libintl.h> /* INFRINGES ON USER NAME SPACE */
#   define YY_(msgid) dgettext ("bison-runtime", msgid)
#  endif
# endif
# ifndef YY_
#  define YY_(msgid) msgid
# endif
#endif

/* Suppress unused-variable warnings by "using" E.  */
#if ! defined lint || defined __GNUC__
# define YYUSE(e) ((void) (e))
#else
# define YYUSE(e) /* empty */
#endif

/* Identity function, used to suppress warnings about constant conditions.  */
#ifndef lint
# define YYID(n) (n)
#else
#if (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
static int
YYID (int i)
#else
static int
YYID (i)
    int i;
#endif
{
  return i;
}
#endif

#if ! defined yyoverflow || YYERROR_VERBOSE

/* The parser invokes alloca or malloc; define the necessary symbols.  */

# ifdef YYSTACK_USE_ALLOCA
#  if YYSTACK_USE_ALLOCA
#   ifdef __GNUC__
#    define YYSTACK_ALLOC __builtin_alloca
#   elif defined __BUILTIN_VA_ARG_INCR
#    include <alloca.h> /* INFRINGES ON USER NAME SPACE */
#   elif defined _AIX
#    define YYSTACK_ALLOC __alloca
#   elif defined _MSC_VER
#    include <malloc.h> /* INFRINGES ON USER NAME SPACE */
#    define alloca _alloca
#   else
#    define YYSTACK_ALLOC alloca
#    if ! defined _ALLOCA_H && ! defined _STDLIB_H && (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
#     include <stdlib.h> /* INFRINGES ON USER NAME SPACE */
#     ifndef _STDLIB_H
#      define _STDLIB_H 1
#     endif
#    endif
#   endif
#  endif
# endif

# ifdef YYSTACK_ALLOC
   /* Pacify GCC's `empty if-body' warning.  */
#  define YYSTACK_FREE(Ptr) do { /* empty */; } while (YYID (0))
#  ifndef YYSTACK_ALLOC_MAXIMUM
    /* The OS might guarantee only one guard page at the bottom of the stack,
       and a page size can be as small as 4096 bytes.  So we cannot safely
       invoke alloca (N) if N exceeds 4096.  Use a slightly smaller number
       to allow for a few compiler-allocated temporary stack slots.  */
#   define YYSTACK_ALLOC_MAXIMUM 4032 /* reasonable circa 2006 */
#  endif
# else
#  define YYSTACK_ALLOC YYMALLOC
#  define YYSTACK_FREE YYFREE
#  ifndef YYSTACK_ALLOC_MAXIMUM
#   define YYSTACK_ALLOC_MAXIMUM YYSIZE_MAXIMUM
#  endif
#  if (defined __cplusplus && ! defined _STDLIB_H \
       && ! ((defined YYMALLOC || defined malloc) \
	     && (defined YYFREE || defined free)))
#   include <stdlib.h> /* INFRINGES ON USER NAME SPACE */
#   ifndef _STDLIB_H
#    define _STDLIB_H 1
#   endif
#  endif
#  ifndef YYMALLOC
#   define YYMALLOC malloc
#   if ! defined malloc && ! defined _STDLIB_H && (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
void *malloc (YYSIZE_T); /* INFRINGES ON USER NAME SPACE */
#   endif
#  endif
#  ifndef YYFREE
#   define YYFREE free
#   if ! defined free && ! defined _STDLIB_H && (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
void free (void *); /* INFRINGES ON USER NAME SPACE */
#   endif
#  endif
# endif
#endif /* ! defined yyoverflow || YYERROR_VERBOSE */


#if (! defined yyoverflow \
     && (! defined __cplusplus \
	 || (defined YYSTYPE_IS_TRIVIAL && YYSTYPE_IS_TRIVIAL)))

/* A type that is properly aligned for any stack member.  */
union yyalloc
{
  yytype_int16 yyss;
  YYSTYPE yyvs;
  };

/* The size of the maximum gap between one aligned stack and the next.  */
# define YYSTACK_GAP_MAXIMUM (sizeof (union yyalloc) - 1)

/* The size of an array large to enough to hold all stacks, each with
   N elements.  */
# define YYSTACK_BYTES(N) \
     ((N) * (sizeof (yytype_int16) + sizeof (YYSTYPE)) \
      + YYSTACK_GAP_MAXIMUM)

/* Copy COUNT objects from FROM to TO.  The source and destination do
   not overlap.  */
# ifndef YYCOPY
#  if defined __GNUC__ && 1 < __GNUC__
#   define YYCOPY(To, From, Count) \
      __builtin_memcpy (To, From, (Count) * sizeof (*(From)))
#  else
#   define YYCOPY(To, From, Count)		\
      do					\
	{					\
	  YYSIZE_T yyi;				\
	  for (yyi = 0; yyi < (Count); yyi++)	\
	    (To)[yyi] = (From)[yyi];		\
	}					\
      while (YYID (0))
#  endif
# endif

/* Relocate STACK from its old location to the new one.  The
   local variables YYSIZE and YYSTACKSIZE give the old and new number of
   elements in the stack, and YYPTR gives the new location of the
   stack.  Advance YYPTR to a properly aligned location for the next
   stack.  */
# define YYSTACK_RELOCATE(Stack)					\
    do									\
      {									\
	YYSIZE_T yynewbytes;						\
	YYCOPY (&yyptr->Stack, Stack, yysize);				\
	Stack = &yyptr->Stack;						\
	yynewbytes = yystacksize * sizeof (*Stack) + YYSTACK_GAP_MAXIMUM; \
	yyptr += yynewbytes / sizeof (*yyptr);				\
      }									\
    while (YYID (0))

#endif

/* YYFINAL -- State number of the termination state.  */
#define YYFINAL  80
/* YYLAST -- Last index in YYTABLE.  */
#define YYLAST   1059

/* YYNTOKENS -- Number of terminals.  */
#define YYNTOKENS  64
/* YYNNTS -- Number of nonterminals.  */
#define YYNNTS  38
/* YYNRULES -- Number of rules.  */
#define YYNRULES  134
/* YYNRULES -- Number of states.  */
#define YYNSTATES  271

/* YYTRANSLATE(YYLEX) -- Bison symbol number corresponding to YYLEX.  */
#define YYUNDEFTOK  2
#define YYMAXUTOK   522

#define YYTRANSLATE(YYX)						\
  ((unsigned int) (YYX) <= YYMAXUTOK ? yytranslate[YYX] : YYUNDEFTOK)

/* YYTRANSLATE[YYLEX] -- Bison symbol number corresponding to YYLEX.  */
static const yytype_uint8 yytranslate[] =
{
       0,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
      58,    59,     2,     2,    51,     2,    55,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,    63,    60,
       2,     2,     2,     2,    52,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,    56,     2,    57,    54,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,    61,     2,    62,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     1,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,    38,    39,    40,    41,    42,    43,    44,    45,
      46,    47,    48,    49,    50,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       3,     4,     5,     6,     7,     8,     9,    10,    11,    12,
      13,    14,    15,    16,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
      17,    18,    19,    20,    21,    22,    23,    24,    25,    26,
      27,    28,    29,    30,    31,    32,    33,    34,    35,    36,
      37,     2,    53
};

#if YYDEBUG
/* YYPRHS[YYN] -- Index of the first RHS symbol of rule number YYN in
   YYRHS.  */
static const yytype_uint16 yyprhs[] =
{
       0,     0,     3,     6,     8,    10,    12,    16,    22,    27,
      28,    31,    33,    36,    38,    40,    44,    48,    50,    54,
      58,    60,    63,    66,    71,    75,    79,    83,    88,    93,
      98,   100,   102,   104,   106,   107,   109,   113,   116,   121,
     126,   131,   136,   141,   144,   147,   150,   153,   156,   161,
     164,   168,   170,   172,   175,   180,   187,   190,   193,   198,
     201,   206,   213,   222,   227,   236,   238,   243,   246,   253,
     260,   269,   276,   285,   291,   295,   300,   307,   310,   312,
     314,   317,   318,   320,   323,   328,   331,   333,   337,   339,
     343,   344,   346,   347,   349,   353,   355,   359,   363,   365,
     369,   371,   373,   377,   382,   384,   386,   389,   391,   394,
     397,   400,   403,   407,   411,   415,   419,   423,   427,   431,
     435,   439,   443,   447,   451,   455,   459,   463,   467,   471,
     474,   477,   484,   489,   493
};

/* YYRHS -- A `-1'-separated list of the rules' RHS.  */
static const yytype_int8 yyrhs[] =
{
      65,     0,    -1,    71,    39,    -1,    71,    -1,     1,    -1,
      41,    -1,    56,   101,    57,    -1,    27,    66,    58,    70,
      59,    -1,    27,    66,    58,    59,    -1,    -1,    43,   101,
      -1,     1,    -1,    66,    68,    -1,     1,    -1,    69,    -1,
      70,    51,    69,    -1,    70,    60,    69,    -1,    78,    -1,
      71,    60,    78,    -1,    61,    71,    62,    -1,     1,    -1,
      77,    54,    -1,   100,    54,    -1,    58,   101,    59,    54,
      -1,    77,    55,    66,    -1,    77,    55,     1,    -1,   101,
      32,   101,    -1,    77,    56,   101,    57,    -1,    77,    56,
      75,    57,    -1,    77,    56,    95,    57,    -1,    74,    -1,
      76,    -1,    66,    -1,    73,    -1,    -1,   101,    -1,    77,
      43,   101,    -1,    67,    72,    -1,    67,    61,    79,    62,
      -1,    26,    58,    70,    59,    -1,    26,    61,    70,    62,
      -1,    37,    58,    70,    59,    -1,    37,    61,    70,    62,
      -1,    80,    72,    -1,    81,    72,    -1,    82,    72,    -1,
      83,    72,    -1,    84,    72,    -1,    84,    72,    24,    72,
      -1,    86,    72,    -1,    20,    58,    59,    -1,    20,    -1,
      34,    -1,    21,    94,    -1,    87,    61,    89,    62,    -1,
      87,    61,    89,    62,    24,    72,    -1,    90,    72,    -1,
      85,    72,    -1,    85,    72,    24,    72,    -1,   101,     1,
      -1,    30,    58,    74,    59,    -1,    18,    58,    66,    19,
     101,    59,    -1,    18,    58,    66,    19,   101,    51,   101,
      59,    -1,    18,    58,    66,     1,    -1,    17,    58,    78,
      60,   101,    60,    78,    59,    -1,    17,    -1,    17,    58,
     101,    59,    -1,    28,   101,    -1,    17,    58,    78,    60,
     101,    59,    -1,    31,    77,    43,   101,    32,   101,    -1,
      31,    58,    77,    43,   101,    32,   101,    59,    -1,    31,
      77,    43,   101,    33,   101,    -1,    31,    58,    77,    43,
     101,    33,   101,    59,    -1,    31,    77,    43,   101,     1,
      -1,    31,    77,     1,    -1,    31,    77,    19,   101,    -1,
      31,    58,    77,    19,   101,    59,    -1,    22,   101,    -1,
      36,    -1,    25,    -1,    29,   101,    -1,    -1,    72,    -1,
     101,    88,    -1,    89,    60,   101,    88,    -1,    35,    91,
      -1,    77,    -1,    91,    51,    77,    -1,   101,    -1,    92,
      51,   101,    -1,    -1,    92,    -1,    -1,   101,    -1,   101,
      63,   101,    -1,    95,    -1,    96,    51,    95,    -1,    66,
      63,   101,    -1,    97,    -1,    98,    51,    97,    -1,    93,
      -1,    98,    -1,    92,    51,    98,    -1,    77,    58,    99,
      59,    -1,    40,    -1,    77,    -1,    52,    77,    -1,   100,
      -1,    49,    77,    -1,    77,    49,    -1,    50,    77,    -1,
      77,    50,    -1,    58,   101,    59,    -1,   101,    44,   101,
      -1,   101,    45,   101,    -1,   101,    46,   101,    -1,   101,
      47,   101,    -1,   101,    48,   101,    -1,   101,     3,   101,
      -1,   101,     4,   101,    -1,   101,     6,   101,    -1,   101,
       8,   101,    -1,   101,     5,   101,    -1,   101,     7,   101,
      -1,   101,    12,   101,    -1,   101,    13,   101,    -1,   101,
      14,   101,    -1,   101,    11,   101,    -1,   101,    10,   101,
      -1,    45,   101,    -1,     9,   101,    -1,    22,   101,    23,
     101,    24,   101,    -1,    22,   101,    23,   101,    -1,    61,
      93,    62,    -1,    61,    96,    62,    -1
};

/* YYRLINE[YYN] -- source line where rule number YYN was defined.  */
static const yytype_uint16 yyrline[] =
{
       0,   274,   274,   290,   302,   312,   319,   330,   338,   356,
     363,   370,   380,   393,   403,   410,   418,   429,   436,   449,
     456,   466,   474,   482,   493,   501,   511,   522,   530,   538,
     549,   556,   563,   570,   580,   588,   595,   603,   611,   619,
     627,   635,   643,   651,   661,   671,   681,   691,   701,   711,
     721,   729,   737,   745,   753,   763,   773,   783,   793,   803,
     821,   852,   860,   868,   879,   887,   895,   903,   911,   922,
     930,   938,   946,   954,   962,   973,   981,   992,  1003,  1014,
    1025,  1036,  1043,  1053,  1061,  1077,  1088,  1095,  1108,  1115,
    1129,  1136,  1168,  1175,  1186,  1197,  1204,  1217,  1228,  1235,
    1248,  1255,  1262,  1276,  1286,  1293,  1300,  1308,  1315,  1323,
    1331,  1339,  1347,  1354,  1362,  1370,  1378,  1386,  1394,  1402,
    1410,  1418,  1426,  1434,  1442,  1450,  1458,  1466,  1474,  1482,
    1490,  1498,  1506,  1514,  1522
};
#endif

#if YYDEBUG || YYERROR_VERBOSE || YYTOKEN_TABLE
/* YYTNAME[SYMBOL-NUM] -- String name of the symbol SYMBOL-NUM.
   First, the terminals, then, starting at YYNTOKENS, nonterminals.  */
static const char *const yytname[] =
{
  "$end", "error", "$undefined", "EQtoken", "NEtoken", "GTtoken",
  "LTtoken", "GEtoken", "LEtoken", "nottoken", "andandtoken", "orortoken",
  "beginswithtoken", "endswithtoken", "containstoken", "bitandtoken",
  "bitortoken", "looptoken", "filelooptoken", "intoken", "breaktoken",
  "returntoken", "iftoken", "thentoken", "elsetoken", "bundletoken",
  "localtoken", "ontoken", "whiletoken", "casetoken", "kerneltoken",
  "fortoken", "totoken", "downtotoken", "continuetoken", "withtoken",
  "trytoken", "globaltoken", "errortoken", "eoltoken", "constanttoken",
  "identifiertoken", "othertoken", "assigntoken", "addtoken",
  "subtracttoken", "multiplytoken", "dividetoken", "modtoken",
  "plusplustoken", "minusminustoken", "','", "'@'", "unaryminus", "'^'",
  "'.'", "'['", "']'", "'('", "')'", "';'", "'{'", "'}'", "':'", "$accept",
  "module", "bracketedidentifier", "handlerheader", "optionalinit",
  "namelistid", "namelist", "statementlist", "bracketedstatementlist",
  "derefid", "dottedid", "rangeref", "arrayref", "term", "statement",
  "kernelcall", "fileloopheader", "loopheader", "forloopheader",
  "forinloopheader", "ifheader", "tryheader", "bundleheader", "caseheader",
  "optionalstatementlist", "casebody", "withheader", "termlist",
  "exprlist", "optionalexprlist", "optionalexpr", "fieldspec", "fieldlist",
  "namedvalue", "namedvaluelist", "parameterlist", "functionref", "expr", 0
};
#endif

# ifdef YYPRINT
/* YYTOKNUM[YYLEX-NUM] -- Internal token number corresponding to
   token YYLEX-NUM.  */
static const yytype_uint16 yytoknum[] =
{
       0,   256,   521,   400,   401,   402,   403,   404,   405,   406,
     407,   408,   409,   410,   411,   412,   413,   500,   501,   502,
     503,   504,   505,   506,   507,   508,   509,   510,   511,   512,
     513,   514,   515,   516,   517,   518,   519,   520,   292,   293,
     294,   295,   296,   297,   298,   299,   300,   301,   302,   303,
     304,    44,    64,   522,    94,    46,    91,    93,    40,    41,
      59,   123,   125,    58
};
# endif

/* YYR1[YYN] -- Symbol number of symbol that rule YYN derives.  */
static const yytype_uint8 yyr1[] =
{
       0,    64,    65,    65,    65,    66,    66,    67,    67,    68,
      68,    68,    69,    69,    70,    70,    70,    71,    71,    72,
      72,    73,    73,    73,    74,    74,    75,    76,    76,    76,
      77,    77,    77,    77,    78,    78,    78,    78,    78,    78,
      78,    78,    78,    78,    78,    78,    78,    78,    78,    78,
      78,    78,    78,    78,    78,    78,    78,    78,    78,    78,
      79,    80,    80,    80,    81,    81,    81,    81,    81,    82,
      82,    82,    82,    82,    82,    83,    83,    84,    85,    86,
      87,    88,    88,    89,    89,    90,    91,    91,    92,    92,
      93,    93,    94,    94,    95,    96,    96,    97,    98,    98,
      99,    99,    99,   100,   101,   101,   101,   101,   101,   101,
     101,   101,   101,   101,   101,   101,   101,   101,   101,   101,
     101,   101,   101,   101,   101,   101,   101,   101,   101,   101,
     101,   101,   101,   101,   101
};

/* YYR2[YYN] -- Number of symbols composing right hand side of rule YYN.  */
static const yytype_uint8 yyr2[] =
{
       0,     2,     2,     1,     1,     1,     3,     5,     4,     0,
       2,     1,     2,     1,     1,     3,     3,     1,     3,     3,
       1,     2,     2,     4,     3,     3,     3,     4,     4,     4,
       1,     1,     1,     1,     0,     1,     3,     2,     4,     4,
       4,     4,     4,     2,     2,     2,     2,     2,     4,     2,
       3,     1,     1,     2,     4,     6,     2,     2,     4,     2,
       4,     6,     8,     4,     8,     1,     4,     2,     6,     6,
       8,     6,     8,     5,     3,     4,     6,     2,     1,     1,
       2,     0,     1,     2,     4,     2,     1,     3,     1,     3,
       0,     1,     0,     1,     3,     1,     3,     3,     1,     3,
       1,     1,     3,     4,     1,     1,     2,     1,     2,     2,
       2,     2,     3,     3,     3,     3,     3,     3,     3,     3,
       3,     3,     3,     3,     3,     3,     3,     3,     3,     2,
       2,     6,     4,     3,     3
};

/* YYDEFACT[STATE-NAME] -- Default rule to reduce with in state
   STATE-NUM when YYTABLE doesn't specify something else to do.  Zero
   means the default is an error.  */
static const yytype_uint8 yydefact[] =
{
       0,     4,     0,    65,     0,    51,    92,     0,    79,     0,
       0,     0,     0,     0,    52,     0,    78,     0,   104,     5,
       0,     0,     0,     0,     0,     0,    90,     0,    32,     0,
       3,    33,    30,    31,   105,    17,     0,     0,     0,     0,
       0,     0,     0,     0,     0,   107,     0,     0,   105,   130,
      34,     0,     0,    53,    93,    77,     0,     0,     0,    67,
      80,     0,     0,     0,     0,    86,    85,     0,     0,   129,
     108,   110,   106,     0,     0,    91,     0,    95,     0,    88,
       1,    20,    34,    37,     2,    34,     0,   109,   111,    21,
       0,     0,    90,    34,    43,    44,    45,    46,    47,    57,
      49,     0,    56,    22,    59,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,    50,     0,    13,     0,    14,
       0,     0,     0,   105,     0,    74,     0,     0,     0,     0,
       0,     6,   112,     0,   133,     0,   134,     0,     0,     0,
       0,    18,    36,    25,    24,     0,     0,     0,    32,    91,
     100,    98,   101,     0,    88,     0,     0,     0,     0,   118,
     119,   122,   120,   123,   121,   128,   127,   124,   125,   126,
     113,   114,   115,   116,   117,     0,    66,    63,     0,   132,
      11,     0,    12,     0,    39,     0,    40,     8,     0,     0,
       0,     0,    75,     0,    87,    41,    42,    23,    89,    96,
       0,    94,     0,    19,    38,    28,    29,     0,    27,     0,
       0,     0,   103,    48,    58,     0,    54,    82,    83,     0,
       0,     0,    10,    15,    16,     7,     0,     0,    73,     0,
       0,    30,     0,    26,    97,   102,     0,    99,     0,     0,
      68,    34,     0,    61,   131,    76,     0,     0,    69,    71,
      60,    84,    55,     0,     0,     0,     0,    64,    62,    70,
      72
};

/* YYDEFGOTO[NTERM-NUM].  */
static const yytype_int16 yydefgoto[] =
{
      -1,    27,    28,    29,   192,   129,   130,   149,   227,    31,
      32,   155,    33,    48,    35,   150,    36,    37,    38,    39,
      40,    41,    42,    43,   228,   167,    44,    66,    75,    76,
      53,    77,    78,   161,   162,   163,    45,    46
};

/* YYPACT[STATE-NUM] -- Index in YYTABLE of the portion describing
   STATE-NUM.  */
#define YYPACT_NINF -73
static const yytype_int16 yypact[] =
{
     343,   -73,   274,   -37,   -24,   -15,   274,   274,   -73,    61,
     -16,   274,   274,    -3,   -73,     0,   -73,    69,   -73,   -73,
     274,     0,     0,     0,   274,   274,   274,    47,   -73,     2,
     -23,   -73,   -73,   -73,   193,   -73,    10,    10,    10,    10,
      10,    10,    10,    -1,    10,    20,   294,   274,   231,   -73,
     814,   -16,    37,   -73,   965,   936,    13,    13,    17,   965,
     965,   274,   202,    20,   274,   126,    74,    13,    13,   -73,
     126,   126,   126,   739,   541,    75,    26,   -73,    36,   265,
     -73,   -73,   772,   -73,   -73,   814,   274,   -73,   -73,   -73,
      38,   274,   274,   814,   -73,   -73,   -73,   -73,    97,   108,
     -73,   274,   -73,   -73,   -73,   274,   274,   274,   274,   274,
     274,   274,   274,   274,   274,   274,   274,   274,   274,   274,
     274,   936,    73,   416,     7,   -73,   274,   -73,     6,   -73,
      21,    16,     5,   589,   557,   -73,   274,   274,     0,    87,
      35,   -73,    82,   274,   -73,   274,   -73,   274,    79,   -45,
      86,   -73,   965,   -73,   -73,    92,    96,   402,    81,   103,
     -73,   -73,   114,   115,   965,    10,    10,    98,   165,   717,
     717,   301,   301,   301,   301,   187,  1011,   301,   301,   301,
      54,    54,   -73,   -73,   -73,   274,   -73,   -73,   274,   950,
     -73,   274,   -73,    13,   -73,    13,   -73,   -73,    91,   274,
     274,    82,   965,   873,   126,   -73,   -73,   -73,   965,   -73,
     265,   965,     0,   -73,   -73,   -73,   -73,   274,   -73,   274,
     274,   -16,   -73,   -73,   -73,   274,   161,   -73,   -73,   474,
     491,   274,   965,   -73,   -73,   -73,   607,   890,   -73,   274,
     274,   128,   126,   965,   965,   114,    81,   -73,   165,    10,
     -73,   814,   274,   -73,   965,   -73,   274,   274,   965,   965,
     -73,   -73,   -73,   129,   623,   673,   689,   -73,   -73,   -73,
     -73
};

/* YYPGOTO[NTERM-NUM].  */
static const yytype_int16 yypgoto[] =
{
     -73,   -73,    72,   -73,   -73,   -34,   -55,   196,    -9,   -73,
      -7,   -73,   -73,    70,   -49,   -73,   -73,   -73,   -73,   -73,
     -73,   -73,   -73,   -73,   -42,   -73,   -73,   -73,   122,   124,
     -73,   -72,   -73,   -14,     4,   -73,    29,    -2
};

/* YYTABLE[YYPACT[STATE-NUM]].  What to do in state STATE-NUM.  If
   positive, shift that token.  If negative, reduce the rule which
   number is the opposite.  If zero, do what YYDEFACT says.
   If YYTABLE_NINF, syntax error.  */
#define YYTABLE_NINF -82
static const yytype_int16 yytable[] =
{
      49,   122,   131,    81,    54,    55,   127,   190,   187,    59,
      60,    81,   139,   140,   127,    85,    84,   213,    69,   156,
      83,    50,    73,    74,    79,    19,   188,    94,    95,    96,
      97,    98,    99,   100,    51,   102,   151,    85,    19,   153,
      24,    19,    63,    52,    63,   121,    19,    80,   123,   191,
      63,    63,    63,    24,    19,    61,    24,    -9,    64,   134,
     101,    24,   134,    82,   197,    -9,    -9,   193,    -9,    24,
      34,    93,   193,   209,   103,   132,   195,   198,   196,    19,
     194,   195,    58,    62,   152,    65,   193,   145,   144,   157,
     164,    70,    71,    72,    24,   195,   125,   206,   146,   168,
     118,   119,   120,   169,   170,   171,   172,   173,   174,   175,
     176,   177,   178,   179,   180,   181,   182,   183,   184,    56,
      34,   165,    57,   124,   189,   138,   143,    67,   128,   128,
      68,   133,   166,   185,   202,   203,   207,   212,   193,   128,
     128,   208,   193,   210,   219,   211,   205,   195,   214,   215,
     235,   195,    34,   216,   220,    34,   223,   224,   225,   233,
     226,   234,   154,    34,   158,   221,    81,    63,   105,   106,
     107,   108,   109,   110,   222,   111,   112,   113,   114,   115,
      89,    90,    91,   229,    92,   249,   230,   260,   267,   232,
     105,   106,   107,   108,   109,   110,    30,   236,   237,   113,
     114,   115,   263,   135,   128,   241,   261,   247,   204,   116,
     117,   118,   119,   120,   159,   243,   160,   244,   208,     0,
       0,   136,     0,   248,   245,   -81,    93,   -81,     0,   254,
       0,   116,   117,   118,   119,   120,    86,   258,   259,     0,
     262,    63,    87,    88,     0,   137,     0,    89,    90,    91,
     264,    92,     0,     0,   265,   266,    89,    90,    91,     0,
      92,     0,     0,     0,     0,   128,     0,   128,   105,   106,
     107,   108,   109,   110,     0,   111,   112,   113,   114,   115,
      87,    88,   242,     2,     0,    89,    90,    91,     0,    92,
       0,     0,   158,   246,   -35,   104,    47,   105,   106,   107,
     108,   109,   110,     0,   111,   112,   113,   114,   115,   116,
     117,   118,   119,   120,    18,    19,     0,     0,     0,    20,
       0,    34,     0,    21,    22,     0,    23,     0,   147,     0,
      24,     0,    25,   -35,     0,    26,     0,     0,   116,   117,
     118,   119,   120,   -34,     1,   116,   117,   118,   119,   120,
       0,     0,     2,   -35,   -35,     0,   -35,     0,     0,     0,
       3,     4,     0,     5,     6,     7,     0,     0,     8,     9,
      10,    11,    12,     0,    13,     0,     0,    14,    15,    16,
      17,     0,   -34,    18,    19,     0,     0,     0,    20,     0,
       0,     0,    21,    22,     0,    23,     0,     0,     0,    24,
       0,    25,     0,   -34,    26,   105,   106,   107,   108,   109,
     110,     0,   111,   112,   113,   114,   115,   104,     0,   105,
     106,   107,   108,   109,   110,     0,   111,   112,   113,   114,
     115,     0,     0,     0,   217,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,   116,   117,   118,   119,
     120,     0,     0,     0,     0,     0,     0,     0,     0,   218,
     116,   117,   118,   119,   120,   147,     0,     0,     0,     0,
       0,     0,     0,     0,     0,   186,   -35,   105,   106,   107,
     108,   109,   110,     0,   111,   112,   113,   114,   115,     0,
       0,     0,     0,     0,   105,   106,   107,   108,   109,   110,
       0,   111,   112,   113,   114,   115,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,   116,   117,
     118,   119,   120,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,   250,   251,   116,   117,   118,   119,   120,
       0,     0,   252,     0,   105,   106,   107,   108,   109,   110,
     253,   111,   112,   113,   114,   115,     0,     0,     0,     0,
     105,   106,   107,   108,   109,   110,     0,   111,   112,   113,
     114,   115,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,   116,   117,   118,   119,   120,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     142,   116,   117,   118,   119,   120,     0,     0,   199,     0,
     105,   106,   107,   108,   109,   110,   201,   111,   112,   113,
     114,   115,     0,     0,     0,     0,   105,   106,   107,   108,
     109,   110,   200,   111,   112,   113,   114,   115,    87,    88,
       0,     0,     0,    89,    90,    91,     0,    92,     0,     0,
       0,   116,   117,   118,   119,   120,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,   255,   116,   117,   118,
     119,   120,     0,     0,     0,     0,   105,   106,   107,   108,
     109,   110,   268,   111,   112,   113,   114,   115,     0,     0,
       0,     0,   105,   106,   107,   108,   109,   110,     0,   111,
     112,   113,   114,   115,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,   116,   117,   118,
     119,   120,   107,   108,   109,   110,     0,     0,     0,   113,
     114,   115,   269,   116,   117,   118,   119,   120,     0,     0,
       0,     0,   105,   106,   107,   108,   109,   110,   270,   111,
     112,   113,   114,   115,     0,     0,     0,     0,     0,     0,
       0,   116,   117,   118,   119,   120,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     2,     0,   116,   117,   118,   119,   120,     0,     3,
       4,     0,     5,     6,     7,     0,   141,     8,     9,    10,
      11,    12,   148,    13,     0,     0,    14,    15,    16,    17,
       0,     0,    18,    19,     0,     0,     0,    20,     0,     0,
       0,    21,    22,     2,    23,     0,     0,     0,    24,     0,
      25,     3,     4,    26,     5,     6,     7,     0,     0,     8,
       9,    10,    11,    12,     0,    13,     0,     0,    14,    15,
      16,    17,     0,     0,    18,    19,     0,     0,     0,    20,
       0,     0,     0,    21,    22,     0,    23,     0,     0,     0,
      24,     0,    25,     0,   238,    26,   105,   106,   107,   108,
     109,   110,     0,   111,   112,   113,   114,   115,     0,     0,
       0,     0,     0,   105,   106,   107,   108,   109,   110,     0,
     111,   112,   113,   114,   115,   239,   240,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,   116,   117,   118,
     119,   120,   256,   257,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,   116,   117,   118,   119,   120,   105,
     106,   107,   108,   109,   110,     0,   111,   112,   113,   114,
     115,     0,     0,   105,   106,   107,   108,   109,   110,   126,
     111,   112,   113,   114,   115,     0,     0,     0,   105,   106,
     107,   108,   109,   110,   231,   111,   112,   113,   114,   115,
     116,   117,   118,   119,   120,     0,     0,     0,     0,     0,
       0,     0,     0,     0,   116,   117,   118,   119,   120,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,   116,
     117,   118,   119,   120,   105,   106,   107,   108,   109,   110,
       0,   111,     0,   113,   114,   115,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,   116,   117,   118,   119,   120
};

static const yytype_int16 yycheck[] =
{
       2,    50,    57,     1,     6,     7,     1,     1,     1,    11,
      12,     1,    67,    68,     1,    60,    39,    62,    20,    91,
      29,    58,    24,    25,    26,    41,    19,    36,    37,    38,
      39,    40,    41,    42,    58,    44,    85,    60,    41,     1,
      56,    41,    13,    58,    15,    47,    41,     0,    50,    43,
      21,    22,    23,    56,    41,    58,    56,    51,    58,    61,
      61,    56,    64,    61,    59,    59,    60,    51,    62,    56,
       0,    61,    51,   145,    54,    58,    60,   132,    62,    41,
      59,    60,    10,    13,    86,    15,    51,    51,    62,    91,
      92,    21,    22,    23,    56,    60,    59,    62,    62,   101,
      46,    47,    48,   105,   106,   107,   108,   109,   110,   111,
     112,   113,   114,   115,   116,   117,   118,   119,   120,    58,
      50,    24,    61,    51,   126,    51,    51,    58,    56,    57,
      61,    61,    24,    60,   136,   137,    54,    58,    51,    67,
      68,   143,    51,   145,    63,   147,    59,    60,    62,    57,
      59,    60,    82,    57,    51,    85,   165,   166,    60,   193,
      62,   195,    90,    93,    92,    51,     1,   138,     3,     4,
       5,     6,     7,     8,    59,    10,    11,    12,    13,    14,
      54,    55,    56,   185,    58,    24,   188,    59,    59,   191,
       3,     4,     5,     6,     7,     8,     0,   199,   200,    12,
      13,    14,   251,     1,   132,   212,   248,   221,   138,    44,
      45,    46,    47,    48,    92,   217,    92,   219,   220,    -1,
      -1,    19,    -1,   225,   220,    60,    61,    62,    -1,   231,
      -1,    44,    45,    46,    47,    48,    43,   239,   240,    -1,
     249,   212,    49,    50,    -1,    43,    -1,    54,    55,    56,
     252,    58,    -1,    -1,   256,   257,    54,    55,    56,    -1,
      58,    -1,    -1,    -1,    -1,   193,    -1,   195,     3,     4,
       5,     6,     7,     8,    -1,    10,    11,    12,    13,    14,
      49,    50,   212,     9,    -1,    54,    55,    56,    -1,    58,
      -1,    -1,   220,   221,     0,     1,    22,     3,     4,     5,
       6,     7,     8,    -1,    10,    11,    12,    13,    14,    44,
      45,    46,    47,    48,    40,    41,    -1,    -1,    -1,    45,
      -1,   251,    -1,    49,    50,    -1,    52,    -1,    63,    -1,
      56,    -1,    58,    39,    -1,    61,    -1,    -1,    44,    45,
      46,    47,    48,     0,     1,    44,    45,    46,    47,    48,
      -1,    -1,     9,    59,    60,    -1,    62,    -1,    -1,    -1,
      17,    18,    -1,    20,    21,    22,    -1,    -1,    25,    26,
      27,    28,    29,    -1,    31,    -1,    -1,    34,    35,    36,
      37,    -1,    39,    40,    41,    -1,    -1,    -1,    45,    -1,
      -1,    -1,    49,    50,    -1,    52,    -1,    -1,    -1,    56,
      -1,    58,    -1,    60,    61,     3,     4,     5,     6,     7,
       8,    -1,    10,    11,    12,    13,    14,     1,    -1,     3,
       4,     5,     6,     7,     8,    -1,    10,    11,    12,    13,
      14,    -1,    -1,    -1,    32,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    44,    45,    46,    47,
      48,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    57,
      44,    45,    46,    47,    48,    63,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    59,    60,     3,     4,     5,
       6,     7,     8,    -1,    10,    11,    12,    13,    14,    -1,
      -1,    -1,    -1,    -1,     3,     4,     5,     6,     7,     8,
      -1,    10,    11,    12,    13,    14,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    44,    45,
      46,    47,    48,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    59,    60,    44,    45,    46,    47,    48,
      -1,    -1,    51,    -1,     3,     4,     5,     6,     7,     8,
      59,    10,    11,    12,    13,    14,    -1,    -1,    -1,    -1,
       3,     4,     5,     6,     7,     8,    -1,    10,    11,    12,
      13,    14,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    44,    45,    46,    47,    48,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      59,    44,    45,    46,    47,    48,    -1,    -1,    19,    -1,
       3,     4,     5,     6,     7,     8,    59,    10,    11,    12,
      13,    14,    -1,    -1,    -1,    -1,     3,     4,     5,     6,
       7,     8,    43,    10,    11,    12,    13,    14,    49,    50,
      -1,    -1,    -1,    54,    55,    56,    -1,    58,    -1,    -1,
      -1,    44,    45,    46,    47,    48,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    59,    44,    45,    46,
      47,    48,    -1,    -1,    -1,    -1,     3,     4,     5,     6,
       7,     8,    59,    10,    11,    12,    13,    14,    -1,    -1,
      -1,    -1,     3,     4,     5,     6,     7,     8,    -1,    10,
      11,    12,    13,    14,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    44,    45,    46,
      47,    48,     5,     6,     7,     8,    -1,    -1,    -1,    12,
      13,    14,    59,    44,    45,    46,    47,    48,    -1,    -1,
      -1,    -1,     3,     4,     5,     6,     7,     8,    59,    10,
      11,    12,    13,    14,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    44,    45,    46,    47,    48,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,     9,    -1,    44,    45,    46,    47,    48,    -1,    17,
      18,    -1,    20,    21,    22,    -1,    57,    25,    26,    27,
      28,    29,    30,    31,    -1,    -1,    34,    35,    36,    37,
      -1,    -1,    40,    41,    -1,    -1,    -1,    45,    -1,    -1,
      -1,    49,    50,     9,    52,    -1,    -1,    -1,    56,    -1,
      58,    17,    18,    61,    20,    21,    22,    -1,    -1,    25,
      26,    27,    28,    29,    -1,    31,    -1,    -1,    34,    35,
      36,    37,    -1,    -1,    40,    41,    -1,    -1,    -1,    45,
      -1,    -1,    -1,    49,    50,    -1,    52,    -1,    -1,    -1,
      56,    -1,    58,    -1,     1,    61,     3,     4,     5,     6,
       7,     8,    -1,    10,    11,    12,    13,    14,    -1,    -1,
      -1,    -1,    -1,     3,     4,     5,     6,     7,     8,    -1,
      10,    11,    12,    13,    14,    32,    33,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    44,    45,    46,
      47,    48,    32,    33,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    44,    45,    46,    47,    48,     3,
       4,     5,     6,     7,     8,    -1,    10,    11,    12,    13,
      14,    -1,    -1,     3,     4,     5,     6,     7,     8,    23,
      10,    11,    12,    13,    14,    -1,    -1,    -1,     3,     4,
       5,     6,     7,     8,    24,    10,    11,    12,    13,    14,
      44,    45,    46,    47,    48,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    44,    45,    46,    47,    48,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    44,
      45,    46,    47,    48,     3,     4,     5,     6,     7,     8,
      -1,    10,    -1,    12,    13,    14,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    44,    45,    46,    47,    48
};

/* YYSTOS[STATE-NUM] -- The (internal number of the) accessing
   symbol of state STATE-NUM.  */
static const yytype_uint8 yystos[] =
{
       0,     1,     9,    17,    18,    20,    21,    22,    25,    26,
      27,    28,    29,    31,    34,    35,    36,    37,    40,    41,
      45,    49,    50,    52,    56,    58,    61,    65,    66,    67,
      71,    73,    74,    76,    77,    78,    80,    81,    82,    83,
      84,    85,    86,    87,    90,   100,   101,    22,    77,   101,
      58,    58,    58,    94,   101,   101,    58,    61,    66,   101,
     101,    58,    77,   100,    58,    77,    91,    58,    61,   101,
      77,    77,    77,   101,   101,    92,    93,    95,    96,   101,
       0,     1,    61,    72,    39,    60,    43,    49,    50,    54,
      55,    56,    58,    61,    72,    72,    72,    72,    72,    72,
      72,    61,    72,    54,     1,     3,     4,     5,     6,     7,
       8,    10,    11,    12,    13,    14,    44,    45,    46,    47,
      48,   101,    78,   101,    66,    59,    23,     1,    66,    69,
      70,    70,    58,    77,   101,     1,    19,    43,    51,    70,
      70,    57,    59,    51,    62,    51,    62,    63,    30,    71,
      79,    78,   101,     1,    66,    75,    95,   101,    66,    92,
      93,    97,    98,    99,   101,    24,    24,    89,   101,   101,
     101,   101,   101,   101,   101,   101,   101,   101,   101,   101,
     101,   101,   101,   101,   101,    60,    59,     1,    19,   101,
       1,    43,    68,    51,    59,    60,    62,    59,    70,    19,
      43,    59,   101,   101,    77,    59,    62,    54,   101,    95,
     101,   101,    58,    62,    62,    57,    57,    32,    57,    63,
      51,    51,    59,    72,    72,    60,    62,    72,    88,   101,
     101,    24,   101,    69,    69,    59,   101,   101,     1,    32,
      33,    74,    77,   101,   101,    98,    66,    97,   101,    24,
      59,    60,    51,    59,   101,    59,    32,    33,   101,   101,
      59,    88,    72,    78,   101,   101,   101,    59,    59,    59,
      59
};

#define yyerrok		(yyerrstatus = 0)
#define yyclearin	(yychar = YYEMPTY)
#define YYEMPTY		(-2)
#define YYEOF		0

#define YYACCEPT	goto yyacceptlab
#define YYABORT		goto yyabortlab
#define YYERROR		goto yyerrorlab


/* Like YYERROR except do call yyerror.  This remains here temporarily
   to ease the transition to the new meaning of YYERROR, for GCC.
   Once GCC version 2 has supplanted version 1, this can go.  */

#define YYFAIL		goto yyerrlab

#define YYRECOVERING()  (!!yyerrstatus)

#define YYBACKUP(Token, Value)					\
do								\
  if (yychar == YYEMPTY && yylen == 1)				\
    {								\
      yychar = (Token);						\
      yylval = (Value);						\
      yytoken = YYTRANSLATE (yychar);				\
      YYPOPSTACK (1);						\
      goto yybackup;						\
    }								\
  else								\
    {								\
      yyerror (YY_("syntax error: cannot back up")); \
      YYERROR;							\
    }								\
while (YYID (0))


#define YYTERROR	1
#define YYERRCODE	256


/* YYLLOC_DEFAULT -- Set CURRENT to span from RHS[1] to RHS[N].
   If N is 0, then set CURRENT to the empty location which ends
   the previous symbol: RHS[0] (always defined).  */

#define YYRHSLOC(Rhs, K) ((Rhs)[K])
#ifndef YYLLOC_DEFAULT
# define YYLLOC_DEFAULT(Current, Rhs, N)				\
    do									\
      if (YYID (N))                                                    \
	{								\
	  (Current).first_line   = YYRHSLOC (Rhs, 1).first_line;	\
	  (Current).first_column = YYRHSLOC (Rhs, 1).first_column;	\
	  (Current).last_line    = YYRHSLOC (Rhs, N).last_line;		\
	  (Current).last_column  = YYRHSLOC (Rhs, N).last_column;	\
	}								\
      else								\
	{								\
	  (Current).first_line   = (Current).last_line   =		\
	    YYRHSLOC (Rhs, 0).last_line;				\
	  (Current).first_column = (Current).last_column =		\
	    YYRHSLOC (Rhs, 0).last_column;				\
	}								\
    while (YYID (0))
#endif


/* YY_LOCATION_PRINT -- Print the location on the stream.
   This macro was not mandated originally: define only if we know
   we won't break user code: when these are the locations we know.  */

#ifndef YY_LOCATION_PRINT
# if defined YYLTYPE_IS_TRIVIAL && YYLTYPE_IS_TRIVIAL
#  define YY_LOCATION_PRINT(File, Loc)			\
     fprintf (File, "%d.%d-%d.%d",			\
	      (Loc).first_line, (Loc).first_column,	\
	      (Loc).last_line,  (Loc).last_column)
# else
#  define YY_LOCATION_PRINT(File, Loc) ((void) 0)
# endif
#endif


/* YYLEX -- calling `yylex' with the right arguments.  */

#ifdef YYLEX_PARAM
# define YYLEX yylex (YYLEX_PARAM)
#else
# define YYLEX yylex ()
#endif

/* Enable debugging if requested.  */
#if YYDEBUG

# ifndef YYFPRINTF
#  include <stdio.h> /* INFRINGES ON USER NAME SPACE */
#  define YYFPRINTF fprintf
# endif

# define YYDPRINTF(Args)			\
do {						\
  if (yydebug)					\
    YYFPRINTF Args;				\
} while (YYID (0))

# define YY_SYMBOL_PRINT(Title, Type, Value, Location)			  \
do {									  \
  if (yydebug)								  \
    {									  \
      YYFPRINTF (stderr, "%s ", Title);					  \
      yy_symbol_print (stderr,						  \
		  Type, Value); \
      YYFPRINTF (stderr, "\n");						  \
    }									  \
} while (YYID (0))


/*--------------------------------.
| Print this symbol on YYOUTPUT.  |
`--------------------------------*/

/*ARGSUSED*/
#if (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
static void
yy_symbol_value_print (FILE *yyoutput, int yytype, YYSTYPE const * const yyvaluep)
#else
static void
yy_symbol_value_print (yyoutput, yytype, yyvaluep)
    FILE *yyoutput;
    int yytype;
    YYSTYPE const * const yyvaluep;
#endif
{
  if (!yyvaluep)
    return;
# ifdef YYPRINT
  if (yytype < YYNTOKENS)
    YYPRINT (yyoutput, yytoknum[yytype], *yyvaluep);
# else
  YYUSE (yyoutput);
# endif
  switch (yytype)
    {
      default:
	break;
    }
}


/*--------------------------------.
| Print this symbol on YYOUTPUT.  |
`--------------------------------*/

#if (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
static void
yy_symbol_print (FILE *yyoutput, int yytype, YYSTYPE const * const yyvaluep)
#else
static void
yy_symbol_print (yyoutput, yytype, yyvaluep)
    FILE *yyoutput;
    int yytype;
    YYSTYPE const * const yyvaluep;
#endif
{
  if (yytype < YYNTOKENS)
    YYFPRINTF (yyoutput, "token %s (", yytname[yytype]);
  else
    YYFPRINTF (yyoutput, "nterm %s (", yytname[yytype]);

  yy_symbol_value_print (yyoutput, yytype, yyvaluep);
  YYFPRINTF (yyoutput, ")");
}

/*------------------------------------------------------------------.
| yy_stack_print -- Print the state stack from its BOTTOM up to its |
| TOP (included).                                                   |
`------------------------------------------------------------------*/

#if (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
static void
yy_stack_print (yytype_int16 *bottom, yytype_int16 *top)
#else
static void
yy_stack_print (bottom, top)
    yytype_int16 *bottom;
    yytype_int16 *top;
#endif
{
  YYFPRINTF (stderr, "Stack now");
  for (; bottom <= top; ++bottom)
    YYFPRINTF (stderr, " %d", *bottom);
  YYFPRINTF (stderr, "\n");
}

# define YY_STACK_PRINT(Bottom, Top)				\
do {								\
  if (yydebug)							\
    yy_stack_print ((Bottom), (Top));				\
} while (YYID (0))


/*------------------------------------------------.
| Report that the YYRULE is going to be reduced.  |
`------------------------------------------------*/

#if (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
static void
yy_reduce_print (YYSTYPE *yyvsp, int yyrule)
#else
static void
yy_reduce_print (yyvsp, yyrule)
    YYSTYPE *yyvsp;
    int yyrule;
#endif
{
  int yynrhs = yyr2[yyrule];
  int yyi;
  unsigned long int yylno = yyrline[yyrule];
  YYFPRINTF (stderr, "Reducing stack by rule %d (line %lu):\n",
	     yyrule - 1, yylno);
  /* The symbols being reduced.  */
  for (yyi = 0; yyi < yynrhs; yyi++)
    {
      fprintf (stderr, "   $%d = ", yyi + 1);
      yy_symbol_print (stderr, yyrhs[yyprhs[yyrule] + yyi],
		       &(yyvsp[(yyi + 1) - (yynrhs)])
		       		       );
      fprintf (stderr, "\n");
    }
}

# define YY_REDUCE_PRINT(Rule)		\
do {					\
  if (yydebug)				\
    yy_reduce_print (yyvsp, Rule); \
} while (YYID (0))

/* Nonzero means print parse trace.  It is left uninitialized so that
   multiple parsers can coexist.  */
int yydebug;
#else /* !YYDEBUG */
# define YYDPRINTF(Args)
# define YY_SYMBOL_PRINT(Title, Type, Value, Location)
# define YY_STACK_PRINT(Bottom, Top)
# define YY_REDUCE_PRINT(Rule)
#endif /* !YYDEBUG */


/* YYINITDEPTH -- initial size of the parser's stacks.  */
#ifndef	YYINITDEPTH
# define YYINITDEPTH 200
#endif

/* YYMAXDEPTH -- maximum size the stacks can grow to (effective only
   if the built-in stack extension method is used).

   Do not make this value too large; the results are undefined if
   YYSTACK_ALLOC_MAXIMUM < YYSTACK_BYTES (YYMAXDEPTH)
   evaluated with infinite-precision integer arithmetic.  */

#ifndef YYMAXDEPTH
# define YYMAXDEPTH 10000
#endif



#if YYERROR_VERBOSE

# ifndef yystrlen
#  if defined __GLIBC__ && defined _STRING_H
#   define yystrlen strlen
#  else
/* Return the length of YYSTR.  */
#if (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
static YYSIZE_T
yystrlen (const char *yystr)
#else
static YYSIZE_T
yystrlen (yystr)
    const char *yystr;
#endif
{
  YYSIZE_T yylen;
  for (yylen = 0; yystr[yylen]; yylen++)
    continue;
  return yylen;
}
#  endif
# endif

# ifndef yystpcpy
#  if defined __GLIBC__ && defined _STRING_H && defined _GNU_SOURCE
#   define yystpcpy stpcpy
#  else
/* Copy YYSRC to YYDEST, returning the address of the terminating '\0' in
   YYDEST.  */
#if (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
static char *
yystpcpy (char *yydest, const char *yysrc)
#else
static char *
yystpcpy (yydest, yysrc)
    char *yydest;
    const char *yysrc;
#endif
{
  char *yyd = yydest;
  const char *yys = yysrc;

  while ((*yyd++ = *yys++) != '\0')
    continue;

  return yyd - 1;
}
#  endif
# endif

# ifndef yytnamerr
/* Copy to YYRES the contents of YYSTR after stripping away unnecessary
   quotes and backslashes, so that it's suitable for yyerror.  The
   heuristic is that double-quoting is unnecessary unless the string
   contains an apostrophe, a comma, or backslash (other than
   backslash-backslash).  YYSTR is taken from yytname.  If YYRES is
   null, do not copy; instead, return the length of what the result
   would have been.  */
static YYSIZE_T
yytnamerr (char *yyres, const char *yystr)
{
  if (*yystr == '"')
    {
      YYSIZE_T yyn = 0;
      char const *yyp = yystr;

      for (;;)
	switch (*++yyp)
	  {
	  case '\'':
	  case ',':
	    goto do_not_strip_quotes;

	  case '\\':
	    if (*++yyp != '\\')
	      goto do_not_strip_quotes;
	    /* Fall through.  */
	  default:
	    if (yyres)
	      yyres[yyn] = *yyp;
	    yyn++;
	    break;

	  case '"':
	    if (yyres)
	      yyres[yyn] = '\0';
	    return yyn;
	  }
    do_not_strip_quotes: ;
    }

  if (! yyres)
    return yystrlen (yystr);

  return yystpcpy (yyres, yystr) - yyres;
}
# endif

/* Copy into YYRESULT an error message about the unexpected token
   YYCHAR while in state YYSTATE.  Return the number of bytes copied,
   including the terminating null byte.  If YYRESULT is null, do not
   copy anything; just return the number of bytes that would be
   copied.  As a special case, return 0 if an ordinary "syntax error"
   message will do.  Return YYSIZE_MAXIMUM if overflow occurs during
   size calculation.  */
static YYSIZE_T
yysyntax_error (char *yyresult, int yystate, int yychar)
{
  int yyn = yypact[yystate];

  if (! (YYPACT_NINF < yyn && yyn <= YYLAST))
    return 0;
  else
    {
      int yytype = YYTRANSLATE (yychar);
      YYSIZE_T yysize0 = yytnamerr (0, yytname[yytype]);
      YYSIZE_T yysize = yysize0;
      YYSIZE_T yysize1;
      int yysize_overflow = 0;
      enum { YYERROR_VERBOSE_ARGS_MAXIMUM = 5 };
      char const *yyarg[YYERROR_VERBOSE_ARGS_MAXIMUM];
      int yyx;

# if 0
      /* This is so xgettext sees the translatable formats that are
	 constructed on the fly.  */
      YY_("syntax error, unexpected %s");
      YY_("syntax error, unexpected %s, expecting %s");
      YY_("syntax error, unexpected %s, expecting %s or %s");
      YY_("syntax error, unexpected %s, expecting %s or %s or %s");
      YY_("syntax error, unexpected %s, expecting %s or %s or %s or %s");
# endif
      char *yyfmt;
      char const *yyf;
      static char const yyunexpected[] = "syntax error, unexpected %s";
      static char const yyexpecting[] = ", expecting %s";
      static char const yyor[] = " or %s";
      char yyformat[sizeof yyunexpected
		    + sizeof yyexpecting - 1
		    + ((YYERROR_VERBOSE_ARGS_MAXIMUM - 2)
		       * (sizeof yyor - 1))];
      char const *yyprefix = yyexpecting;

      /* Start YYX at -YYN if negative to avoid negative indexes in
	 YYCHECK.  */
      int yyxbegin = yyn < 0 ? -yyn : 0;

      /* Stay within bounds of both yycheck and yytname.  */
      int yychecklim = YYLAST - yyn + 1;
      int yyxend = yychecklim < YYNTOKENS ? yychecklim : YYNTOKENS;
      int yycount = 1;

      yyarg[0] = yytname[yytype];
      yyfmt = yystpcpy (yyformat, yyunexpected);

      for (yyx = yyxbegin; yyx < yyxend; ++yyx)
	if (yycheck[yyx + yyn] == yyx && yyx != YYTERROR)
	  {
	    if (yycount == YYERROR_VERBOSE_ARGS_MAXIMUM)
	      {
		yycount = 1;
		yysize = yysize0;
		yyformat[sizeof yyunexpected - 1] = '\0';
		break;
	      }
	    yyarg[yycount++] = yytname[yyx];
	    yysize1 = yysize + yytnamerr (0, yytname[yyx]);
	    yysize_overflow |= (yysize1 < yysize);
	    yysize = yysize1;
	    yyfmt = yystpcpy (yyfmt, yyprefix);
	    yyprefix = yyor;
	  }

      yyf = YY_(yyformat);
      yysize1 = yysize + yystrlen (yyf);
      yysize_overflow |= (yysize1 < yysize);
      yysize = yysize1;

      if (yysize_overflow)
	return YYSIZE_MAXIMUM;

      if (yyresult)
	{
	  /* Avoid sprintf, as that infringes on the user's name space.
	     Don't have undefined behavior even if the translation
	     produced a string with the wrong number of "%s"s.  */
	  char *yyp = yyresult;
	  int yyi = 0;
	  while ((*yyp = *yyf) != '\0')
	    {
	      if (*yyp == '%' && yyf[1] == 's' && yyi < yycount)
		{
		  yyp += yytnamerr (yyp, yyarg[yyi++]);
		  yyf += 2;
		}
	      else
		{
		  yyp++;
		  yyf++;
		}
	    }
	}
      return yysize;
    }
}
#endif /* YYERROR_VERBOSE */


/*-----------------------------------------------.
| Release the memory associated to this symbol.  |
`-----------------------------------------------*/

/*ARGSUSED*/
#if (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
static void
yydestruct (const char *yymsg, int yytype, YYSTYPE *yyvaluep)
#else
static void
yydestruct (yymsg, yytype, yyvaluep)
    const char *yymsg;
    int yytype;
    YYSTYPE *yyvaluep;
#endif
{
  YYUSE (yyvaluep);

  if (!yymsg)
    yymsg = "Deleting";
  YY_SYMBOL_PRINT (yymsg, yytype, yyvaluep, yylocationp);

  switch (yytype)
    {

      default:
	break;
    }
}


/* Prevent warnings from -Wmissing-prototypes.  */

#ifdef YYPARSE_PARAM
#if defined __STDC__ || defined __cplusplus
int yyparse (void *YYPARSE_PARAM);
#else
int yyparse ();
#endif
#else /* ! YYPARSE_PARAM */
#if defined __STDC__ || defined __cplusplus
int yyparse (void);
#else
int yyparse ();
#endif
#endif /* ! YYPARSE_PARAM */



/* The look-ahead symbol.  */
int yychar;

/* The semantic value of the look-ahead symbol.  */
YYSTYPE yylval;

/* Number of syntax errors so far.  */
int yynerrs;



/*----------.
| yyparse.  |
`----------*/

#ifdef YYPARSE_PARAM
#if (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
int
yyparse (void *YYPARSE_PARAM)
#else
int
yyparse (YYPARSE_PARAM)
    void *YYPARSE_PARAM;
#endif
#else /* ! YYPARSE_PARAM */
#if (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
int
yyparse (void)
#else
int
yyparse ()

#endif
#endif
{
  
  int yystate;
  int yyn;
  int yyresult;
  /* Number of tokens to shift before error messages enabled.  */
  int yyerrstatus;
  /* Look-ahead token as an internal (translated) token number.  */
  int yytoken = 0;
#if YYERROR_VERBOSE
  /* Buffer for error messages, and its allocated size.  */
  char yymsgbuf[128];
  char *yymsg = yymsgbuf;
  YYSIZE_T yymsg_alloc = sizeof yymsgbuf;
#endif

  /* Three stacks and their tools:
     `yyss': related to states,
     `yyvs': related to semantic values,
     `yyls': related to locations.

     Refer to the stacks thru separate pointers, to allow yyoverflow
     to reallocate them elsewhere.  */

  /* The state stack.  */
  yytype_int16 yyssa[YYINITDEPTH];
  yytype_int16 *yyss = yyssa;
  yytype_int16 *yyssp;

  /* The semantic value stack.  */
  YYSTYPE yyvsa[YYINITDEPTH];
  YYSTYPE *yyvs = yyvsa;
  YYSTYPE *yyvsp;



#define YYPOPSTACK(N)   (yyvsp -= (N), yyssp -= (N))

  YYSIZE_T yystacksize = YYINITDEPTH;

  /* The variables used to return semantic value and location from the
     action routines.  */
  YYSTYPE yyval;


  /* The number of symbols on the RHS of the reduced rule.
     Keep to zero when no symbol should be popped.  */
  int yylen = 0;

  YYDPRINTF ((stderr, "Starting parse\n"));

  yystate = 0;
  yyerrstatus = 0;
  yynerrs = 0;
  yychar = YYEMPTY;		/* Cause a token to be read.  */

  /* Initialize stack pointers.
     Waste one element of value and location stack
     so that they stay on the same level as the state stack.
     The wasted elements are never initialized.  */

  yyssp = yyss;
  yyvsp = yyvs;

  goto yysetstate;

/*------------------------------------------------------------.
| yynewstate -- Push a new state, which is found in yystate.  |
`------------------------------------------------------------*/
 yynewstate:
  /* In all cases, when you get here, the value and location stacks
     have just been pushed.  So pushing a state here evens the stacks.  */
  yyssp++;

 yysetstate:
  *yyssp = yystate;

  if (yyss + yystacksize - 1 <= yyssp)
    {
      /* Get the current used size of the three stacks, in elements.  */
      YYSIZE_T yysize = yyssp - yyss + 1;

#ifdef yyoverflow
      {
	/* Give user a chance to reallocate the stack.  Use copies of
	   these so that the &'s don't force the real ones into
	   memory.  */
	YYSTYPE *yyvs1 = yyvs;
	yytype_int16 *yyss1 = yyss;


	/* Each stack pointer address is followed by the size of the
	   data in use in that stack, in bytes.  This used to be a
	   conditional around just the two extra args, but that might
	   be undefined if yyoverflow is a macro.  */
	yyoverflow (YY_("memory exhausted"),
		    &yyss1, yysize * sizeof (*yyssp),
		    &yyvs1, yysize * sizeof (*yyvsp),

		    &yystacksize);

	yyss = yyss1;
	yyvs = yyvs1;
      }
#else /* no yyoverflow */
# ifndef YYSTACK_RELOCATE
      goto yyexhaustedlab;
# else
      /* Extend the stack our own way.  */
      if (YYMAXDEPTH <= yystacksize)
	goto yyexhaustedlab;
      yystacksize *= 2;
      if (YYMAXDEPTH < yystacksize)
	yystacksize = YYMAXDEPTH;

      {
	yytype_int16 *yyss1 = yyss;
	union yyalloc *yyptr =
	  (union yyalloc *) YYSTACK_ALLOC (YYSTACK_BYTES (yystacksize));
	if (! yyptr)
	  goto yyexhaustedlab;
	YYSTACK_RELOCATE (yyss);
	YYSTACK_RELOCATE (yyvs);

#  undef YYSTACK_RELOCATE
	if (yyss1 != yyssa)
	  YYSTACK_FREE (yyss1);
      }
# endif
#endif /* no yyoverflow */

      yyssp = yyss + yysize - 1;
      yyvsp = yyvs + yysize - 1;


      YYDPRINTF ((stderr, "Stack size increased to %lu\n",
		  (unsigned long int) yystacksize));

      if (yyss + yystacksize - 1 <= yyssp)
	YYABORT;
    }

  YYDPRINTF ((stderr, "Entering state %d\n", yystate));

  goto yybackup;

/*-----------.
| yybackup.  |
`-----------*/
yybackup:

  /* Do appropriate processing given the current state.  Read a
     look-ahead token if we need one and don't already have one.  */

  /* First try to decide what to do without reference to look-ahead token.  */
  yyn = yypact[yystate];
  if (yyn == YYPACT_NINF)
    goto yydefault;

  /* Not known => get a look-ahead token if don't already have one.  */

  /* YYCHAR is either YYEMPTY or YYEOF or a valid look-ahead symbol.  */
  if (yychar == YYEMPTY)
    {
      YYDPRINTF ((stderr, "Reading a token: "));
      yychar = YYLEX;
    }

  if (yychar <= YYEOF)
    {
      yychar = yytoken = YYEOF;
      YYDPRINTF ((stderr, "Now at end of input.\n"));
    }
  else
    {
      yytoken = YYTRANSLATE (yychar);
      YY_SYMBOL_PRINT ("Next token is", yytoken, &yylval, &yylloc);
    }

  /* If the proper action on seeing token YYTOKEN is to reduce or to
     detect an error, take that action.  */
  yyn += yytoken;
  if (yyn < 0 || YYLAST < yyn || yycheck[yyn] != yytoken)
    goto yydefault;
  yyn = yytable[yyn];
  if (yyn <= 0)
    {
      if (yyn == 0 || yyn == YYTABLE_NINF)
	goto yyerrlab;
      yyn = -yyn;
      goto yyreduce;
    }

  if (yyn == YYFINAL)
    YYACCEPT;

  /* Count tokens shifted since error; after three, turn off error
     status.  */
  if (yyerrstatus)
    yyerrstatus--;

  /* Shift the look-ahead token.  */
  YY_SYMBOL_PRINT ("Shifting", yytoken, &yylval, &yylloc);

  /* Discard the shifted token unless it is eof.  */
  if (yychar != YYEOF)
    yychar = YYEMPTY;

  yystate = yyn;
  *++yyvsp = yylval;

  goto yynewstate;


/*-----------------------------------------------------------.
| yydefault -- do the default action for the current state.  |
`-----------------------------------------------------------*/
yydefault:
  yyn = yydefact[yystate];
  if (yyn == 0)
    goto yyerrlab;
  goto yyreduce;


/*-----------------------------.
| yyreduce -- Do a reduction.  |
`-----------------------------*/
yyreduce:
  /* yyn is the number of a rule to reduce with.  */
  yylen = yyr2[yyn];

  /* If YYLEN is nonzero, implement the default value of the action:
     `$$ = $1'.

     Otherwise, the following line sets YYVAL to garbage.
     This behavior is undocumented and Bison
     users should not rely upon it.  Assigning to YYVAL
     unconditionally makes the parser a bit smaller, and it avoids a
     GCC warning that YYVAL may be used uninitialized.  */
  yyval = yyvsp[1-yylen];


  YY_REDUCE_PRINT (yyn);
  switch (yyn)
    {
        case 2:
#line 274 "langparser.y"
    {
		
		yytrace ("module | statementlist eoltoken");
		
		if (pcyyerrct) {
			(yyval) = (yyvsp[(1) - (2)]);
			YYABORT; /* parse error */
		}
		
		if (!pushbinaryoperation (moduleop, (yyvsp[(1) - (2)]), nil, &(yyval)))
			YYABORT; /* memory or internal error */
		/* expose final result to callers not accessing Bison internals */
		langparser_result = (yyval);
		YYACCEPT;
		;}
    break;

  case 3:
#line 290 "langparser.y"
    {
        yytrace ("module | statementlist (no eol)");
        if (pcyyerrct) {
            (yyval) = (yyvsp[(1) - (1)]);
            YYABORT; /* parse error */
        }
        if (!pushbinaryoperation (moduleop, (yyvsp[(1) - (1)]), nil, &(yyval)))
            YYABORT; /* memory or internal error */
        langparser_result = (yyval);
        YYACCEPT;
        ;}
    break;

  case 4:
#line 302 "langparser.y"
    {
        
        yytrace ("module | error");
        
        YYABORT;
        ;}
    break;

  case 5:
#line 312 "langparser.y"
    {
		
		yytrace ("bracketedidentifier : identifiertoken");
		
		(yyval) = (yyvsp[(1) - (1)]);
		;}
    break;

  case 6:
#line 319 "langparser.y"
    {
		
		yytrace ("bracketedidentifier | '[' expr ']'");
		
		if (!pushunaryoperation (bracketop, (yyvsp[(2) - (3)]), &(yyval)))
			YYABORT;
		;}
    break;

  case 7:
#line 330 "langparser.y"
    {
		
		yytrace ("handlerheader | ontoken bracketedidentifier '(' namelist ')'");
		
		if (!pushbinaryoperation (procop, (yyvsp[(2) - (5)]), (yyvsp[(4) - (5)]), &(yyval)))
			YYABORT;
		;}
    break;

  case 8:
#line 338 "langparser.y"
    {
		
		yytrace ("handlerheader | ontoken bracketedidentifier '(' ')'");
		
		if (!pushbinaryoperation (procop, (yyvsp[(2) - (4)]), nil, &(yyval)))
			YYABORT;
		;}
    break;

  case 9:
#line 356 "langparser.y"
    {
		
		yytrace ("optionalinit | empty");
		
		(yyval) = nil;
		;}
    break;

  case 10:
#line 363 "langparser.y"
    {
		
		yytrace ("optionalinit | assigntoken expr");
		
		(yyval) = (yyvsp[(2) - (2)]);
		;}
    break;

  case 11:
#line 370 "langparser.y"
    {
		
		yytrace ("optionalinit | error");
		
		(yyval) = nil;
		;}
    break;

  case 12:
#line 380 "langparser.y"
    {
		
		yytrace ("namelistid | optionalinit");
		
		if ((yyvsp[(2) - (2)]) == nil)
			(yyval) = (yyvsp[(1) - (2)]); /*just return the id*/
			
		else {
		if (!pushbinaryoperation (assignlocalop, (yyvsp[(1) - (2)]), (yyvsp[(2) - (2)]), &(yyval)))
			YYABORT;
			}
		;}
    break;

  case 13:
#line 393 "langparser.y"
    {
		
		yytrace ("namelistid | error");
		
		(yyval) = nil;
		;}
    break;

  case 14:
#line 403 "langparser.y"
    {
		
		yytrace ("namelist | namelistid");
		
		(yyval) = (yyvsp[(1) - (1)]); /*start the name list off with our address*/
		;}
    break;

  case 15:
#line 410 "langparser.y"
    {
		
		yytrace ("namelist | namelist ',' namelistid");
		
		if (!pushlastlink ((yyvsp[(3) - (3)]), (yyvsp[(1) - (3)]))) /*add new name to end of list*/
			YYABORT;
		;}
    break;

  case 16:
#line 418 "langparser.y"
    {
		
		yytrace ("namelist | namelist ';' namelistid");
		
		if (!pushlastlink ((yyvsp[(3) - (3)]), (yyvsp[(1) - (3)]))) /*add new name to end of list*/
			YYABORT;
		;}
    break;

  case 17:
#line 429 "langparser.y"
    {
		
		yytrace ("statementlist: statement");
		
		(yyval) = (yyvsp[(1) - (1)]); /*start the statement list off with our address*/
		;}
    break;

  case 18:
#line 436 "langparser.y"
    {
		
		yytrace ("statementlist | statementlist ';' statement");
		
		if (!pushlastlink ((yyvsp[(3) - (3)]), (yyvsp[(1) - (3)]))) /*add new statement to end of list*/
			YYABORT;
		
		(yyval) = (yyvsp[(1) - (3)]); 
		;}
    break;

  case 19:
#line 449 "langparser.y"
    {
		
		yytrace ("bracketedstatementlist: '{' statementlist '}'");
		
		(yyval) = (yyvsp[(2) - (3)]);
		;}
    break;

  case 20:
#line 456 "langparser.y"
    {
		
		yytrace ("bracketedstatementlist | error");
		
		(yyval) = nil;
		;}
    break;

  case 21:
#line 466 "langparser.y"
    {
		
		yytrace ("derefid : term '^'");
		
		if (!pushunaryoperation (dereferenceop, (yyvsp[(1) - (2)]), &(yyval)))
			YYABORT;
		;}
    break;

  case 22:
#line 474 "langparser.y"
    {
		
		yytrace ("derefid | functionref '^'");
		
		if (!pushunaryoperation (dereferenceop, (yyvsp[(1) - (2)]), &(yyval)))
			YYABORT;
		;}
    break;

  case 23:
#line 482 "langparser.y"
    {
		
		yytrace ("derefid | '(' expr ')' '^'");
		
		if (!pushunaryoperation (dereferenceop, (yyvsp[(2) - (4)]), &(yyval)))
			YYABORT;
		;}
    break;

  case 24:
#line 493 "langparser.y"
    {
		
		yytrace ("dottedid : term '.' bracketedidentifier");
		
		if (!pushbinaryoperation (dotop, (yyvsp[(1) - (3)]), (yyvsp[(3) - (3)]), &(yyval)))
			YYABORT;
		;}
    break;

  case 25:
#line 501 "langparser.y"
    {
		
		yytrace ("dottedid : term '.' error");
		
		(yyval) = (yyvsp[(1) - (3)]);
		;}
    break;

  case 26:
#line 511 "langparser.y"
    {
		
		yytrace ("rangeref: expr totoken expr");
		
		if (!pushbinaryoperation (rangeop, (yyvsp[(1) - (3)]), (yyvsp[(3) - (3)]), &(yyval)))
			YYABORT;
		;}
    break;

  case 27:
#line 522 "langparser.y"
    {
		
		yytrace ("arrayref: term '[' expr ']'");
		
		if (!pushbinaryoperation (arrayop, (yyvsp[(1) - (4)]), (yyvsp[(3) - (4)]), &(yyval)))
			YYABORT;
		;}
    break;

  case 28:
#line 530 "langparser.y"
    {
		
		yytrace ("arrayref | term '[' rangeref ']'");
		
		if (!pushbinaryoperation (arrayop, (yyvsp[(1) - (4)]), (yyvsp[(3) - (4)]), &(yyval)))
			YYABORT;
		;}
    break;

  case 29:
#line 538 "langparser.y"
    {
		
		yytrace ("arrayref | term '[' fieldspec ']'");
		
		if (!pushbinaryoperation (arrayop, (yyvsp[(1) - (4)]), (yyvsp[(3) - (4)]), &(yyval)))
			YYABORT;
		;}
    break;

  case 30:
#line 549 "langparser.y"
    {
		
		yytrace ("term: dottedid");
		
		(yyval) = (yyvsp[(1) - (1)]);
		;}
    break;

  case 31:
#line 556 "langparser.y"
    {
		
		yytrace ("term | arrayref");
		
		(yyval) = (yyvsp[(1) - (1)]);
		;}
    break;

  case 32:
#line 563 "langparser.y"
    {
		
		yytrace ("term | bracketedidentifier");
		
		(yyval) = (yyvsp[(1) - (1)]);
		;}
    break;

  case 33:
#line 570 "langparser.y"
    {
		
		yytrace ("term | derefid");
		
		(yyval) = (yyvsp[(1) - (1)]);
		;}
    break;

  case 34:
#line 580 "langparser.y"
    {
		
		yytrace ("statement: <empty statement>");
		
		if (!pushoperation (noop, &(yyval))) /*a place for the debugger to stop*/
			YYABORT;
		;}
    break;

  case 35:
#line 588 "langparser.y"
    {
		
		yytrace ("statement: expr");
		
		(yyval) = (yyvsp[(1) - (1)]);
		;}
    break;

  case 36:
#line 595 "langparser.y"
    {
		
		yytrace ("statement | term assigntoken expr");
		
		if (!pushbinaryoperation (assignop, (yyvsp[(1) - (3)]), (yyvsp[(3) - (3)]), &(yyval)))
			YYABORT;
		;}
    break;

  case 37:
#line 603 "langparser.y"
    {
		
		yytrace ("statement : handlerheader bracketedstatementlist");
		
		if (!pushbinaryoperation (moduleop, (yyvsp[(2) - (2)]), (yyvsp[(1) - (2)]), &(yyval)))
			YYABORT;		
		;}
    break;

  case 38:
#line 611 "langparser.y"
    {
		
		yytrace ("statement : handlerheader '{' kernelcall '}'");
		
		if (!pushbinaryoperation (moduleop, (yyvsp[(3) - (4)]), (yyvsp[(1) - (4)]), &(yyval)))
			YYABORT;		
		;}
    break;

  case 39:
#line 619 "langparser.y"
    {
		
		yytrace ("statement | localtoken '(' namelist ')'");
		
		if (!pushunaryoperation (localop, (yyvsp[(3) - (4)]), &(yyval)))
			YYABORT;
		;}
    break;

  case 40:
#line 627 "langparser.y"
    {
		
		yytrace ("statement | localtoken '{' namelist '}'");
		
		if (!pushunaryoperation (localop, (yyvsp[(3) - (4)]), &(yyval)))
			YYABORT;
		;}
    break;

  case 41:
#line 635 "langparser.y"
    {
		
		yytrace ("statement | globaltoken '(' namelist ')'");
		
		if (!pushunaryoperation (globalop, (yyvsp[(3) - (4)]), &(yyval)))
			YYABORT;
		;}
    break;

  case 42:
#line 643 "langparser.y"
    {
		
		yytrace ("statement | globaltoken '{' namelist '}'");
		
		if (!pushunaryoperation (globalop, (yyvsp[(3) - (4)]), &(yyval)))
			YYABORT;
		;}
    break;

  case 43:
#line 651 "langparser.y"
    {
		
		yytrace ("statement | fileloopheader bracketedstatementlist");
		
		if (!pushtripletstatementlists (nil, (yyvsp[(2) - (2)]), (yyvsp[(1) - (2)])))
			YYABORT;
		
		(yyval) = (yyvsp[(1) - (2)]);
		;}
    break;

  case 44:
#line 661 "langparser.y"
    {
		
		yytrace ("statement | loopheader bracketedstatementlist");
		
		if (!pushloopbody ((yyvsp[(2) - (2)]), (yyvsp[(1) - (2)])))
			YYABORT;
		
		(yyval) = (yyvsp[(1) - (2)]);
		;}
    break;

  case 45:
#line 671 "langparser.y"
    {
		
		yytrace ("statement | forloopheader bracketedstatementlist");
		
		if (!pushloopbody ((yyvsp[(2) - (2)]), (yyvsp[(1) - (2)])))
			YYABORT;
		
		(yyval) = (yyvsp[(1) - (2)]);
		;}
    break;

  case 46:
#line 681 "langparser.y"
    {
		
		yytrace ("statement | forinloopheader bracketedstatementlist");
		
		if (!pushloopbody ((yyvsp[(2) - (2)]), (yyvsp[(1) - (2)])))
			YYABORT;
		
		(yyval) = (yyvsp[(1) - (2)]);
		;}
    break;

  case 47:
#line 691 "langparser.y"
    {
		
		yytrace ("statement | ifheader bracketedstatementlist");
		
		if (!pushtripletstatementlists ((yyvsp[(2) - (2)]), nil, (yyvsp[(1) - (2)])))
			YYABORT;
		
		(yyval) = (yyvsp[(1) - (2)]);
		;}
    break;

  case 48:
#line 701 "langparser.y"
    {
		
		yytrace ("statement | ifheader bracketedstatementlist elsetoken bracketedstatementlist");
		
		if (!pushtripletstatementlists ((yyvsp[(2) - (4)]), (yyvsp[(4) - (4)]), (yyvsp[(1) - (4)])))
			YYABORT;
		
		(yyval) = (yyvsp[(1) - (4)]);
		;}
    break;

  case 49:
#line 711 "langparser.y"
    {
		
		yytrace ("statement | bundleheader bracketedstatementlist");
		
		if (!pushunarystatementlist ((yyvsp[(2) - (2)]), (yyvsp[(1) - (2)])))
			YYABORT;
		
		(yyval) = (yyvsp[(1) - (2)]);
		;}
    break;

  case 50:
#line 721 "langparser.y"
    {
		
		yytrace ("statement | breaktoken '(' ')'");
		
		if (!pushoperation (breakop, &(yyval)))
			YYABORT;
		;}
    break;

  case 51:
#line 729 "langparser.y"
    {
		
		yytrace ("statement | breaktoken");
		
		if (!pushoperation (breakop, &(yyval)))
			YYABORT;
		;}
    break;

  case 52:
#line 737 "langparser.y"
    {
		
		yytrace ("statement | continuetoken");
		
		if (!pushoperation (continueop, &(yyval)))
			YYABORT;
		;}
    break;

  case 53:
#line 745 "langparser.y"
    {
		
		yytrace ("statement | returntoken");
		
		if (!pushunaryoperation (returnop, (yyvsp[(2) - (2)]), &(yyval)))
			YYABORT;
		;}
    break;

  case 54:
#line 753 "langparser.y"
    {
		
		yytrace ("statement | caseheader casebody");
		
		if (!pushtripletstatementlists ((yyvsp[(3) - (4)]), nil, (yyvsp[(1) - (4)])))
			YYABORT;
		
		(yyval) = (yyvsp[(1) - (4)]);
		;}
    break;

  case 55:
#line 763 "langparser.y"
    {
		
		yytrace ("statement | caseheader casebody elsetoken bracketedstatementlist");
		
		if (!pushtripletstatementlists ((yyvsp[(3) - (6)]), (yyvsp[(6) - (6)]), (yyvsp[(1) - (6)])))
	YYABORT;
		
		(yyval) = (yyvsp[(1) - (6)]);
		;}
    break;

  case 56:
#line 773 "langparser.y"
    {
		
		yytrace ("statement | withheader bracketedstatementlist");
		
		if (!pushtripletstatementlists ((yyvsp[(2) - (2)]), nil, (yyvsp[(1) - (2)])))
			YYABORT;
		
		(yyval) = (yyvsp[(1) - (2)]);
		;}
    break;

  case 57:
#line 783 "langparser.y"
    {
		
		yytrace ("statement | tryheader bracketedstatementlist");
		
		if (!pushtripletstatementlists ((yyvsp[(2) - (2)]), nil, (yyvsp[(1) - (2)])))
			YYABORT;
		
		(yyval) = (yyvsp[(1) - (2)]);
		;}
    break;

  case 58:
#line 793 "langparser.y"
    {
		
		yytrace ("statement | tryheader bracketedstatementlist elsetoken bracketedstatementlist");
		
		if (!pushtripletstatementlists ((yyvsp[(2) - (4)]), (yyvsp[(4) - (4)]), (yyvsp[(1) - (4)])))
			YYABORT;
		
		(yyval) = (yyvsp[(1) - (4)]);
		;}
    break;

  case 59:
#line 803 "langparser.y"
    {
		
		yytrace ("statement | expr error");
		
		(yyval) = (yyvsp[(1) - (2)]);
		;}
    break;

  case 60:
#line 821 "langparser.y"
    {
		
		yytrace ("kernelcall: kerneltoken '(' dottedid ')'");
		
		if (!pushkernelcall ((yyvsp[(3) - (4)]), &(yyval)))
			YYABORT;
		;}
    break;

  case 61:
#line 852 "langparser.y"
    {
		
		yytrace ("fileloopheader: filelooptoken '(' bracketedidentifier intoken expr ')'");
		
		if (!pushquadruplet (fileloopop, (yyvsp[(3) - (6)]), (yyvsp[(5) - (6)]), nil, nil, &(yyval)))
			YYABORT;
		;}
    break;

  case 62:
#line 860 "langparser.y"
    {
		
		yytrace ("fileloopheader | filelooptoken '(' bracketedidentifier intoken expr ',' expr ')'");
		
		if (!pushquadruplet (fileloopop, (yyvsp[(3) - (8)]), (yyvsp[(5) - (8)]), nil, (yyvsp[(7) - (8)]), &(yyval)))
			YYABORT;
		;}
    break;

  case 63:
#line 868 "langparser.y"
    {
		
		yytrace ("fileloopheader | filelooptoken '(' bracketedidentifier error");
		
		if (!pushquadruplet (fileloopop, (yyvsp[(3) - (4)]), nil, nil, nil, &(yyval)))
			YYABORT;
		;}
    break;

  case 64:
#line 879 "langparser.y"
    {
		
		yytrace ("loopheader: looptoken '(' statement ';' expr ';' statement ')'");
		
		if (!pushloop ((yyvsp[(3) - (8)]), (yyvsp[(5) - (8)]), (yyvsp[(7) - (8)]), &(yyval)))
			YYABORT;
		;}
    break;

  case 65:
#line 887 "langparser.y"
    {
		
		yytrace ("loopheader | looptoken");
		
		if (!pushloop (nil, nil, nil, &(yyval)))
			YYABORT;
		;}
    break;

  case 66:
#line 895 "langparser.y"
    {
		
		yytrace ("loopheader | looptoken '(' expr ')'");
		
		if (!pushloop ((yyvsp[(3) - (4)]), nil, nil, &(yyval)))
			YYABORT;
		;}
    break;

  case 67:
#line 903 "langparser.y"
    {
		
		yytrace ("loopheader | whiletoken expr");
		
		if (!pushloop (nil, (yyvsp[(2) - (2)]), nil, &(yyval)))
			YYABORT;
		;}
    break;

  case 68:
#line 911 "langparser.y"
    {
		
		yytrace ("loopheader | looptoken '(' statement ';' expr ')'");
		
		if (!pushloop ((yyvsp[(3) - (6)]), (yyvsp[(5) - (6)]), nil, &(yyval)))
			YYABORT;
		;}
    break;

  case 69:
#line 922 "langparser.y"
    {
		
		yytrace ("forloopheader: fortoken term assigntoken expr totoken expr");
		
		if (!pushquadruplet (forloopop, (yyvsp[(4) - (6)]), (yyvsp[(6) - (6)]), (yyvsp[(2) - (6)]), nil, &(yyval)))
			YYABORT;
		;}
    break;

  case 70:
#line 930 "langparser.y"
    {
		
		yytrace ("forloopheader | fortoken '(' term assigntoken expr totoken expr ')' ");
		
		if (!pushquadruplet (forloopop, (yyvsp[(5) - (8)]), (yyvsp[(7) - (8)]), (yyvsp[(3) - (8)]), nil, &(yyval)))
			YYABORT;
		;}
    break;

  case 71:
#line 938 "langparser.y"
    {
		
		yytrace ("forloopheader | fortoken term assigntoken expr downtotoken expr");
		
		if (!pushquadruplet (fordownloopop, (yyvsp[(4) - (6)]), (yyvsp[(6) - (6)]), (yyvsp[(2) - (6)]), nil, &(yyval)))
			YYABORT;
		;}
    break;

  case 72:
#line 946 "langparser.y"
    {
		
		yytrace ("forloopheader | fortoken '(' term assigntoken expr downtotoken expr ')' ");
		
		if (!pushquadruplet (fordownloopop, (yyvsp[(5) - (8)]), (yyvsp[(7) - (8)]), (yyvsp[(3) - (8)]), nil, &(yyval)))
			YYABORT;
		;}
    break;

  case 73:
#line 954 "langparser.y"
    {
		
		yytrace ("forloopheader: fortoken term assigntoken expr error");
		
		if (!pushquadruplet (noop, (yyvsp[(2) - (5)]), (yyvsp[(4) - (5)]), nil, nil, &(yyval)))
			YYABORT;
		;}
    break;

  case 74:
#line 962 "langparser.y"
    {
		
		yytrace ("forloopheader: fortoken term error");
		
		if (!pushquadruplet (noop, (yyvsp[(2) - (3)]), nil, nil, nil, &(yyval)))
			YYABORT;
		;}
    break;

  case 75:
#line 973 "langparser.y"
    {
		
		yytrace ("forinloopheader: fortoken term intoken expr");
		
		if (!pushquadruplet (forinloopop, (yyvsp[(4) - (4)]), (yyvsp[(2) - (4)]), nil, nil, &(yyval)))
			YYABORT;
		;}
    break;

  case 76:
#line 981 "langparser.y"
    {
		
		yytrace ("forinloopheader | fortoken '(' term intoken expr ')' ");
		
		if (!pushquadruplet (forinloopop, (yyvsp[(5) - (6)]), (yyvsp[(3) - (6)]), nil, nil, &(yyval)))
			YYABORT;
		;}
    break;

  case 77:
#line 992 "langparser.y"
    {
		
		yytrace ("ifheader: iftoken expr");
		
		if (!pushtriplet (ifop, (yyvsp[(2) - (2)]), nil, nil, &(yyval)))
			YYABORT;
		;}
    break;

  case 78:
#line 1003 "langparser.y"
    {
		
		yytrace ("tryheader | trytoken");
		
		if (!pushtriplet (tryop, nil, nil, nil, &(yyval))) /*it's really just a binary*/
			YYABORT;
		;}
    break;

  case 79:
#line 1014 "langparser.y"
    {
		
		yytrace ("bundleheader: bundletoken");
		
		if (!pushunaryoperation (bundleop, nil, &(yyval)))
			YYABORT;
		;}
    break;

  case 80:
#line 1025 "langparser.y"
    {
		
		yytrace ("caseheader | casetoken expr");
		
		if (!pushtriplet (caseop, (yyvsp[(2) - (2)]), nil, nil, &(yyval)))
			YYABORT;
		;}
    break;

  case 81:
#line 1036 "langparser.y"
    {
	
		yytrace ("optionalstatementlist : (empty list)");
		
		(yyval) = nil;
		;}
    break;

  case 82:
#line 1043 "langparser.y"
    {
		
		yytrace ("optionalstatementlist | bracketedstatementlist");
		
		(yyval) = (yyvsp[(1) - (1)]);
		;}
    break;

  case 83:
#line 1053 "langparser.y"
    {
		
		yytrace ("casebody: expr optionalstatementlist");
		
		if (!pushbinaryoperation (casebodyop, (yyvsp[(1) - (2)]), (yyvsp[(2) - (2)]), &(yyval)))
			YYABORT;
		;}
    break;

  case 84:
#line 1061 "langparser.y"
    {
		
		yytrace ("casebody | casebody ';' expr optionalstatementlist");
		
		if (!pushbinaryoperation (casebodyop, (yyvsp[(3) - (4)]), (yyvsp[(4) - (4)]), &(yyval)))
			YYABORT;
		
		if (!pushlastlink ((yyval), (yyvsp[(1) - (4)])))
			YYABORT;
		
		(yyval) = (yyvsp[(1) - (4)]);
		;}
    break;

  case 85:
#line 1077 "langparser.y"
    {
		
		yytrace ("withheader: withtoken termlist");
		
		if (!pushbinaryoperation (withop, (yyvsp[(2) - (2)]), nil, &(yyval)))
			YYABORT;
		;}
    break;

  case 86:
#line 1088 "langparser.y"
    {
		
		yytrace ("termlist: term");
		
		(yyval) = (yyvsp[(1) - (1)]);
		;}
    break;

  case 87:
#line 1095 "langparser.y"
    {
		
		yytrace ("termlist | termlist ',' term");
		
		if (!pushlastlink ((yyvsp[(3) - (3)]), (yyvsp[(1) - (3)])))
			YYABORT;
			
		(yyval) = (yyvsp[(1) - (3)]);
		;}
    break;

  case 88:
#line 1108 "langparser.y"
    {
		
		yytrace ("exprlist: expr");
		
		(yyval) = (yyvsp[(1) - (1)]);
		;}
    break;

  case 89:
#line 1115 "langparser.y"
    {
		
		yytrace ("exprlist | exprlist ',' expr");
		
		if (!pushlastlink ((yyvsp[(3) - (3)]), (yyvsp[(1) - (3)])))
			YYABORT;
			
		(yyval) = (yyvsp[(1) - (3)]);
		;}
    break;

  case 90:
#line 1129 "langparser.y"
    {
	
		yytrace ("optionalexprlist : (empty list)");
		
		(yyval) = nil;
		;}
    break;

  case 91:
#line 1136 "langparser.y"
    {
		
		yytrace ("optionalexprlist | exprlist");
		
		(yyval) = (yyvsp[(1) - (1)]);
		;}
    break;

  case 92:
#line 1168 "langparser.y"
    {
		
		yytrace ("optionalexpr : (empty expr)");
		
		(yyval) = nil;
		;}
    break;

  case 93:
#line 1175 "langparser.y"
    {
		
		yytrace ("optionalexpr | expr");
		
		(yyval) = (yyvsp[(1) - (1)]);
		;}
    break;

  case 94:
#line 1186 "langparser.y"
    {
		
		yytrace ("fieldspec: expr : expr ");
		
		if (!pushbinaryoperation (fieldop, (yyvsp[(1) - (3)]), (yyvsp[(3) - (3)]), &(yyval)))
			YYABORT;
		;}
    break;

  case 95:
#line 1197 "langparser.y"
    {
		
		yytrace ("fieldlist: fieldspec ");
		
		(yyval) = (yyvsp[(1) - (1)]);
		;}
    break;

  case 96:
#line 1204 "langparser.y"
    {
		
		yytrace ("fieldlist | fieldlist ',' fieldspec");
		
		if (!pushlastlink ((yyvsp[(3) - (3)]), (yyvsp[(1) - (3)])))
			YYABORT;
		
		(yyval) = (yyvsp[(1) - (3)]);
		;}
    break;

  case 97:
#line 1217 "langparser.y"
    {
		
		yytrace ("namedvalue: bracketedidentifier : expr ");
		
		if (!pushbinaryoperation (fieldop, (yyvsp[(1) - (3)]), (yyvsp[(3) - (3)]), &(yyval)))
			YYABORT;
		;}
    break;

  case 98:
#line 1228 "langparser.y"
    {
		
		yytrace ("namedvaluelist: namedvalue ");
		
		(yyval) = (yyvsp[(1) - (1)]);
		;}
    break;

  case 99:
#line 1235 "langparser.y"
    {
		
		yytrace ("namedvaluelist | namedvaluelist ',' namedvalue");
		
		if (!pushlastlink ((yyvsp[(3) - (3)]), (yyvsp[(1) - (3)])))
			YYABORT;
		
		(yyval) = (yyvsp[(1) - (3)]);
		;}
    break;

  case 100:
#line 1248 "langparser.y"
    {
		
		yytrace ("parameterlist : optionalexprlist");
		
		(yyval) = (yyvsp[(1) - (1)]);
		;}
    break;

  case 101:
#line 1255 "langparser.y"
    {
		
		yytrace ("parameterlist | namedvaluelist");
		
		(yyval) = (yyvsp[(1) - (1)]);
		;}
    break;

  case 102:
#line 1262 "langparser.y"
    {
		
		yytrace ("parameterlist | exprlist ',' namedvaluelist");
		
		if (!pushlastlink ((yyvsp[(3) - (3)]), (yyvsp[(1) - (3)])))
			YYABORT;
		
		(yyval) = (yyvsp[(1) - (3)]);
		;}
    break;

  case 103:
#line 1276 "langparser.y"
    {
		
		yytrace ("functionref: term '(' parameterlist ')'");
		
		if (!pushfunctioncall ((yyvsp[(1) - (4)]), (yyvsp[(3) - (4)]), &(yyval)))
			YYABORT;
		;}
    break;

  case 104:
#line 1286 "langparser.y"
    {
		
		yytrace ("expr | constanttoken");
		
		(yyval) = (yyvsp[(1) - (1)]);
		;}
    break;

  case 105:
#line 1293 "langparser.y"
    {
		
		yytrace ("expr | term");
		
		(yyval) = (yyvsp[(1) - (1)]);
		;}
    break;

  case 106:
#line 1300 "langparser.y"
    {
		
		yytrace ("expr | '@' term");
		
		if (!pushunaryoperation (addressofop, (yyvsp[(2) - (2)]), &(yyval)))
			YYABORT;
		;}
    break;

  case 107:
#line 1308 "langparser.y"
    {
		
		yytrace ("expr | functionref");
		
		(yyval) = (yyvsp[(1) - (1)]);
		;}
    break;

  case 108:
#line 1315 "langparser.y"
    {
		
		yytrace ("expr | plusplustoken term"); 
		
		if (!pushunaryoperation (incrpreop, (yyvsp[(2) - (2)]), &(yyval)))
			YYABORT;
		;}
    break;

  case 109:
#line 1323 "langparser.y"
    {
		
		yytrace ("expr | term plusplustoken");
		
		if (!pushunaryoperation (incrpostop, (yyvsp[(1) - (2)]), &(yyval)))
			YYABORT;
		;}
    break;

  case 110:
#line 1331 "langparser.y"
    {
		
		yytrace ("expr | minusminustoken term");
		
		if (!pushunaryoperation (decrpreop, (yyvsp[(2) - (2)]), &(yyval)))
			YYABORT;
		;}
    break;

  case 111:
#line 1339 "langparser.y"
    {
		
		yytrace ("expr | term minusminustoken");
		
		if (!pushunaryoperation (decrpostop, (yyvsp[(1) - (2)]), &(yyval)))
			YYABORT;
		;}
    break;

  case 112:
#line 1347 "langparser.y"
    {
		
		yytrace ("expr | '(' expr ')'");
		
		(yyval) = (yyvsp[(2) - (3)]);
		;}
    break;

  case 113:
#line 1354 "langparser.y"
    {
		
		yytrace ("expr | expr addtoken expr");
		
		if (!pushbinaryoperation (addop, (yyvsp[(1) - (3)]), (yyvsp[(3) - (3)]), &(yyval)))
			YYABORT;
		;}
    break;

  case 114:
#line 1362 "langparser.y"
    {
		
		yytrace ("expr | expr subtracttoken expr");
		
		if (!pushbinaryoperation (subtractop, (yyvsp[(1) - (3)]), (yyvsp[(3) - (3)]), &(yyval)))
			YYABORT;
		;}
    break;

  case 115:
#line 1370 "langparser.y"
    {
		
		yytrace ("expr | expr multiplytoken expr");
		
		if (!pushbinaryoperation (multiplyop, (yyvsp[(1) - (3)]), (yyvsp[(3) - (3)]), &(yyval)))
			YYABORT;
		;}
    break;

  case 116:
#line 1378 "langparser.y"
    {
		
		yytrace ("expr | expr dividetoken expr");
		
		if (!pushbinaryoperation (divideop, (yyvsp[(1) - (3)]), (yyvsp[(3) - (3)]), &(yyval)))
			YYABORT;
		;}
    break;

  case 117:
#line 1386 "langparser.y"
    {
		
		yytrace ("expr | expr modtoken expr");
		
		if (!pushbinaryoperation (modop, (yyvsp[(1) - (3)]), (yyvsp[(3) - (3)]), &(yyval)))
			YYABORT;
		;}
    break;

  case 118:
#line 1394 "langparser.y"
    {
		
		yytrace ("expr | expr EQtoken expr");
		
		if (!pushbinaryoperation (EQop, (yyvsp[(1) - (3)]), (yyvsp[(3) - (3)]), &(yyval)))
			YYABORT;
		;}
    break;

  case 119:
#line 1402 "langparser.y"
    {
		
		yytrace ("expr | expr NEtoken expr");
		
		if (!pushbinaryoperation (NEop, (yyvsp[(1) - (3)]), (yyvsp[(3) - (3)]), &(yyval)))
			YYABORT;
		;}
    break;

  case 120:
#line 1410 "langparser.y"
    {
		
		yytrace ("expr | expr LTtoken expr");
		
		if (!pushbinaryoperation (LTop, (yyvsp[(1) - (3)]), (yyvsp[(3) - (3)]), &(yyval)))
			YYABORT;
		;}
    break;

  case 121:
#line 1418 "langparser.y"
    {
		
		yytrace ("expr | expr LEtoken expr");
		
		if (!pushbinaryoperation (LEop, (yyvsp[(1) - (3)]), (yyvsp[(3) - (3)]), &(yyval)))
			YYABORT;
		;}
    break;

  case 122:
#line 1426 "langparser.y"
    {
		
		yytrace ("expr | expr GTtoken expr");
		
		if (!pushbinaryoperation (GTop, (yyvsp[(1) - (3)]), (yyvsp[(3) - (3)]), &(yyval)))
			YYABORT;
		;}
    break;

  case 123:
#line 1434 "langparser.y"
    {
		
		yytrace ("expr | expr GEtoken expr");
		
		if (!pushbinaryoperation (GEop, (yyvsp[(1) - (3)]), (yyvsp[(3) - (3)]), &(yyval)))
			YYABORT;
		;}
    break;

  case 124:
#line 1442 "langparser.y"
    {
		
		yytrace ("expr | expr beginswithtoken expr");
		
		if (!pushbinaryoperation (beginswithop, (yyvsp[(1) - (3)]), (yyvsp[(3) - (3)]), &(yyval)))
			YYABORT;
		;}
    break;

  case 125:
#line 1450 "langparser.y"
    {
		
		yytrace ("expr | expr endswithtoken expr");
		
		if (!pushbinaryoperation (endswithop, (yyvsp[(1) - (3)]), (yyvsp[(3) - (3)]), &(yyval)))
			YYABORT;
		;}
    break;

  case 126:
#line 1458 "langparser.y"
    {
		
		yytrace ("expr | expr containstoken expr");
		
		if (!pushbinaryoperation (containsop, (yyvsp[(1) - (3)]), (yyvsp[(3) - (3)]), &(yyval)))
			YYABORT;
		;}
    break;

  case 127:
#line 1466 "langparser.y"
    {
		
		yytrace ("expr | expr orortoken expr");
		
		if (!pushbinaryoperation (ororop, (yyvsp[(1) - (3)]), (yyvsp[(3) - (3)]), &(yyval)))
			YYABORT;
		;}
    break;

  case 128:
#line 1474 "langparser.y"
    {
		
		yytrace ("expr | expr andandtoken expr");
		
		if (!pushbinaryoperation (andandop, (yyvsp[(1) - (3)]), (yyvsp[(3) - (3)]), &(yyval)))
			YYABORT;
		;}
    break;

  case 129:
#line 1482 "langparser.y"
    {
		
		yytrace ("expr | subtracttoken expr %prec unaryminus");
		
		if (!pushunaryoperation (unaryop, (yyvsp[(2) - (2)]), &(yyval)))
			YYABORT;
		;}
    break;

  case 130:
#line 1490 "langparser.y"
    {
		
		yytrace ("expr | nottoken expr");
		
		if (!pushunaryoperation (notop, (yyvsp[(2) - (2)]), &(yyval)))
			YYABORT;
		;}
    break;

  case 131:
#line 1498 "langparser.y"
    {
		
		yytrace ("expr | iftoken expr thentoken expr elsetoken expr");
		
		if (!pushtriplet (ifop, (yyvsp[(2) - (6)]), (yyvsp[(4) - (6)]), (yyvsp[(6) - (6)]), &(yyval)))
			YYABORT;
		;}
    break;

  case 132:
#line 1506 "langparser.y"
    {
		
		yytrace ("expr | iftoken expr thentoken expr");
		
		if (!pushtriplet (ifop, (yyvsp[(2) - (4)]), (yyvsp[(4) - (4)]), nil, &(yyval)))
			YYABORT;
		;}
    break;

  case 133:
#line 1514 "langparser.y"
    {
		
		yytrace ("expr | '{' exprlist '}'");
		
		if (!pushunaryoperation (listop, (yyvsp[(2) - (3)]), &(yyval)))
			YYABORT;
		;}
    break;

  case 134:
#line 1522 "langparser.y"
    {
		
		yytrace ("expr | '{' fieldlist '}'");
		
		if (!pushunaryoperation (recordop, (yyvsp[(2) - (3)]), &(yyval)))
			YYABORT;
		;}
    break;


/* Line 1267 of yacc.c.  */
#line 3440 "langparser.c"
      default: break;
    }
  YY_SYMBOL_PRINT ("-> $$ =", yyr1[yyn], &yyval, &yyloc);

  YYPOPSTACK (yylen);
  yylen = 0;
  YY_STACK_PRINT (yyss, yyssp);

  *++yyvsp = yyval;


  /* Now `shift' the result of the reduction.  Determine what state
     that goes to, based on the state we popped back to and the rule
     number reduced by.  */

  yyn = yyr1[yyn];

  yystate = yypgoto[yyn - YYNTOKENS] + *yyssp;
  if (0 <= yystate && yystate <= YYLAST && yycheck[yystate] == *yyssp)
    yystate = yytable[yystate];
  else
    yystate = yydefgoto[yyn - YYNTOKENS];

  goto yynewstate;


/*------------------------------------.
| yyerrlab -- here on detecting error |
`------------------------------------*/
yyerrlab:
  /* If not already recovering from an error, report this error.  */
  if (!yyerrstatus)
    {
      ++yynerrs;
#if ! YYERROR_VERBOSE
      yyerror (YY_("syntax error"));
#else
      {
	YYSIZE_T yysize = yysyntax_error (0, yystate, yychar);
	if (yymsg_alloc < yysize && yymsg_alloc < YYSTACK_ALLOC_MAXIMUM)
	  {
	    YYSIZE_T yyalloc = 2 * yysize;
	    if (! (yysize <= yyalloc && yyalloc <= YYSTACK_ALLOC_MAXIMUM))
	      yyalloc = YYSTACK_ALLOC_MAXIMUM;
	    if (yymsg != yymsgbuf)
	      YYSTACK_FREE (yymsg);
	    yymsg = (char *) YYSTACK_ALLOC (yyalloc);
	    if (yymsg)
	      yymsg_alloc = yyalloc;
	    else
	      {
		yymsg = yymsgbuf;
		yymsg_alloc = sizeof yymsgbuf;
	      }
	  }

	if (0 < yysize && yysize <= yymsg_alloc)
	  {
	    (void) yysyntax_error (yymsg, yystate, yychar);
	    yyerror (yymsg);
	  }
	else
	  {
	    yyerror (YY_("syntax error"));
	    if (yysize != 0)
	      goto yyexhaustedlab;
	  }
      }
#endif
    }



  if (yyerrstatus == 3)
    {
      /* If just tried and failed to reuse look-ahead token after an
	 error, discard it.  */

      if (yychar <= YYEOF)
	{
	  /* Return failure if at end of input.  */
	  if (yychar == YYEOF)
	    YYABORT;
	}
      else
	{
	  yydestruct ("Error: discarding",
		      yytoken, &yylval);
	  yychar = YYEMPTY;
	}
    }

  /* Else will try to reuse look-ahead token after shifting the error
     token.  */
  goto yyerrlab1;


/*---------------------------------------------------.
| yyerrorlab -- error raised explicitly by YYERROR.  |
`---------------------------------------------------*/
yyerrorlab:

  /* Pacify compilers like GCC when the user code never invokes
     YYERROR and the label yyerrorlab therefore never appears in user
     code.  */
  if (/*CONSTCOND*/ 0)
     goto yyerrorlab;

  /* Do not reclaim the symbols of the rule which action triggered
     this YYERROR.  */
  YYPOPSTACK (yylen);
  yylen = 0;
  YY_STACK_PRINT (yyss, yyssp);
  yystate = *yyssp;
  goto yyerrlab1;


/*-------------------------------------------------------------.
| yyerrlab1 -- common code for both syntax error and YYERROR.  |
`-------------------------------------------------------------*/
yyerrlab1:
  yyerrstatus = 3;	/* Each real token shifted decrements this.  */

  for (;;)
    {
      yyn = yypact[yystate];
      if (yyn != YYPACT_NINF)
	{
	  yyn += YYTERROR;
	  if (0 <= yyn && yyn <= YYLAST && yycheck[yyn] == YYTERROR)
	    {
	      yyn = yytable[yyn];
	      if (0 < yyn)
		break;
	    }
	}

      /* Pop the current state because it cannot handle the error token.  */
      if (yyssp == yyss)
	YYABORT;


      yydestruct ("Error: popping",
		  yystos[yystate], yyvsp);
      YYPOPSTACK (1);
      yystate = *yyssp;
      YY_STACK_PRINT (yyss, yyssp);
    }

  if (yyn == YYFINAL)
    YYACCEPT;

  *++yyvsp = yylval;


  /* Shift the error token.  */
  YY_SYMBOL_PRINT ("Shifting", yystos[yyn], yyvsp, yylsp);

  yystate = yyn;
  goto yynewstate;


/*-------------------------------------.
| yyacceptlab -- YYACCEPT comes here.  |
`-------------------------------------*/
yyacceptlab:
  yyresult = 0;
  goto yyreturn;

/*-----------------------------------.
| yyabortlab -- YYABORT comes here.  |
`-----------------------------------*/
yyabortlab:
  yyresult = 1;
  goto yyreturn;

#ifndef yyoverflow
/*-------------------------------------------------.
| yyexhaustedlab -- memory exhaustion comes here.  |
`-------------------------------------------------*/
yyexhaustedlab:
  yyerror (YY_("memory exhausted"));
  yyresult = 2;
  /* Fall through.  */
#endif

yyreturn:
  if (yychar != YYEOF && yychar != YYEMPTY)
     yydestruct ("Cleanup: discarding lookahead",
		 yytoken, &yylval);
  /* Do not reclaim the symbols of the rule which action triggered
     this YYABORT or YYACCEPT.  */
  YYPOPSTACK (yylen);
  YY_STACK_PRINT (yyss, yyssp);
  while (yyssp != yyss)
    {
      yydestruct ("Cleanup: popping",
		  yystos[*yyssp], yyvsp);
      YYPOPSTACK (1);
    }
#ifndef yyoverflow
  if (yyss != yyssa)
    YYSTACK_FREE (yyss);
#endif
#if YYERROR_VERBOSE
  if (yymsg != yymsgbuf)
    YYSTACK_FREE (yymsg);
#endif
  /* Make sure YYID is used.  */
  return YYID (yyresult);
}


#line 1540 "langparser.y"


#if defined(fldebug) && !defined(YYBISON)

	static void yytrace (const char * s) {
		
		bigstring bs;
		
		copyctopstring (s, bs);
		
		langtrace (bs);
		} /*yytrace*/

#else

	#define yytrace(s)

#endif


static int yylex (void) {
	
	/*
	get the next token from the input stream.  return the token number, and
	set the global yylval to the value of the token, if it has one.
	*/
	
    int t = parsegettoken (&yylval);
#ifdef PARSER_TRACE
    log_trace(LOG_COMP_PARSE, "lex tok=%d", t);
#endif
    return t;
    } /*yylex*/


int yyerror (const char *s) {
	
	/*
	langdisposetree (yyval);
	
	langdisposetree (yylval);
	*/
	
	/*
	clearbytes (&parseresult, (long) sizeof (parseresult));
	*/
	
	parseerror ((ptrstring) s);
	return 0; 
	} /*yyerror*/

