/* A Bison parser, made by GNU Bison 2.3.  */

/* Skeleton interface for Bison's Yacc-like parsers in C

   Copyright (C) 1984, 1989, 1990, 2000, 2001, 2002, 2003, 2004, 2005, 2006
   Free Software Foundation, Inc.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street, Fifth Floor,
   Boston, MA 02110-1301, USA.  */

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




#if ! defined YYSTYPE && ! defined YYSTYPE_IS_DECLARED
typedef int YYSTYPE;
# define yystype YYSTYPE /* obsolescent; will be withdrawn */
# define YYSTYPE_IS_DECLARED 1
# define YYSTYPE_IS_TRIVIAL 1
#endif

extern YYSTYPE yylval;

