%option noinput
%option nounput
%option noyywrap
%option yylineno

%{
#include <stdlib.h>
#include <string.h>
#include "strings_parser.h"
#include "strings_compiler.h"

static char *lex_dup(const char *s) {
    size_t len = strlen(s);
    char *copy = (char *)malloc(len + 1);
    if (!copy) {
        strings_report_error("out of memory while duplicating token");
        exit(EXIT_FAILURE);
    }
    memcpy(copy, s, len + 1);
    return copy;
}

#define YY_USER_ACTION /* no-op */
%}

ws      [ \t\r\n]+
ident   [A-Za-z_][A-Za-z0-9_.-]*
number  [0-9]+
string  \"([^\"\\]|\\\\.)*\"
comment \#[^\n]*

%%
{ws}            ;
{comment}       ;
"-"             { return DASH; }
":"             { return COLON; }
{number}        { yylval.number = strtol(yytext, NULL, 10); return NUMBER; }
{string}        { yylval.str = strings_unescape_quoted(yytext); return STRING; }
{ident}         { yylval.str = lex_dup(yytext); return IDENT; }
.               { strings_report_error("unexpected character '%c'", yytext[0]); }
%%
