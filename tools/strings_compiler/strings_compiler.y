%{
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "strings_compiler.h"

extern int yylex(void);
void yyerror(const char *s);
extern int yylineno;

%}

%union {
    char *str;
    long number;
    struct {
        int kind;
        char *string;
        long number;
    } holder;
}

%token <str> IDENT
%token <str> STRING
%token <number> NUMBER
%token DASH
%token COLON

%type <holder> value

%%

document
    : sections
    | /* empty */
    ;

sections
    : sections section
    | section
    ;

section
    : IDENT COLON { strings_begin_table($1); free($1); } entries_opt { strings_end_table(); }
    ;

entries_opt
    : entries
    | /* empty */
    ;

entries
    : entries entry
    | entry
    ;

entry
    : DASH { strings_begin_entry(); } kv_pairs { strings_finish_entry(); }
    ;

kv_pairs
    : kv_pairs kv_pair
    | kv_pair
    ;

kv_pair
    : IDENT COLON value {
        parsed_value pv;
        if ($3.kind == 0)
            pv = parsed_value_string($3.string);
        else
            pv = parsed_value_number($3.number);
        strings_set_entry_field($1, &pv);
        parsed_value_dispose(&pv);
        free($1);
      }
    ;

value
    : STRING { $$.kind = 0; $$.string = $1; }
    | NUMBER { $$.kind = 1; $$.number = $1; }
    | IDENT  { $$.kind = 0; $$.string = $1; }
    ;

%%

void yyerror(const char *msg) {
    strings_report_error("parse error on line %d: %s", yylineno, msg);
}
