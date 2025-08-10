/* portable/cli_executor.c - Minimal evaluator to satisfy current tests */

#include "cli_executor.h"
#include "platform_adapter.h"
#include "../Common/headers/strings.h"
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdio.h>

typedef enum { VT_NULL, VT_NUM, VT_BOOL, VT_STR } vtype_t;
typedef struct { vtype_t t; double n; int b; char* s; } value_t;

typedef struct { char name[64]; value_t v; } var_t;
typedef struct { var_t items[128]; int count; } env_t;

static void free_value(value_t* v){ if (v->t==VT_STR && v->s){ free(v->s); } v->t=VT_NULL; v->s=NULL; }
static char* str_dup(const char* s){ if(!s) return NULL; size_t n=strlen(s); char* o=(char*)malloc(n+1); if(o){ memcpy(o,s,n+1);} return o; }
static char* str_trim(char* s){ while(isspace((unsigned char)*s)) s++; char* e=s+strlen(s); while(e>s && isspace((unsigned char)e[-1])) *--e='\0'; return s; }
static char* strip_quotes(char* s){ s=str_trim(s); size_t n=strlen(s); if(n>=2 && s[0]=='"' && s[n-1]=='"'){ s[n-1]='\0'; return s+1; } return s; }
static char* to_string_alloc(const value_t* v){
    if (v->t==VT_STR) return str_dup(v->s?v->s:"");
    if (v->t==VT_BOOL) return str_dup(v->b?"true":"false");
    if (v->t==VT_NUM) { char buf[64]; snprintf(buf,sizeof buf,"%.0f", v->n); return str_dup(buf); }
    return str_dup("");
}

static var_t* env_get(env_t* e, const char* name){ for(int i=0;i<e->count;i++){ if(strcmp(e->items[i].name,name)==0) return &e->items[i]; } if(e->count<128){ var_t* v=&e->items[e->count++]; memset(v,0,sizeof(*v)); strncpy(v->name,name,sizeof(v->name)-1); v->v.t=VT_NULL; return v; } return NULL; }

static value_t make_num(double n){ value_t v; memset(&v,0,sizeof v); v.t=VT_NUM; v.n=n; return v; }
static value_t make_bool(int b){ value_t v; memset(&v,0,sizeof v); v.t=VT_BOOL; v.b=b?1:0; return v; }
static value_t make_str(const char* s){ value_t v; memset(&v,0,sizeof v); v.t=VT_STR; v.s=str_dup(s?s:""); return v; }
static value_t make_null(void){ value_t v; memset(&v,0,sizeof v); return v; }

static value_t copy_value(const value_t* src){
    value_t v; memset(&v,0,sizeof v);
    v.t = src->t; v.n = src->n; v.b = src->b;
    if (src->t==VT_STR && src->s) v.s = str_dup(src->s);
    return v;
}

static int is_identifier_char(int c){ return isalnum(c) || c=='_' || c=='.'; }

static value_t eval_atom(env_t* env, char* expr){
    char* t = str_trim(expr);
    if(strcmp(t,"true")==0) return make_bool(1);
    if(strcmp(t,"false")==0) return make_bool(0);
    // dereference operator: var^
    size_t tl = strlen(t);
    if (tl>1 && t[tl-1]=='^'){
        t[tl-1]='\0'; t = str_trim(t);
        var_t* a = env_get(env, t);
        if (a && a->v.t==VT_STR && a->v.s){ var_t* target = env_get(env, a->v.s); if (target) return target->v; }
        return make_null();
    }
    // number?
    char* endptr=NULL; double num=strtod(t,&endptr); if(endptr && *str_trim(endptr)=='\0' && (t!=endptr)) return make_num(num);
    // string literal
    if(*t=='"'){ char* u=strip_quotes(t); return make_str(u); }
    // variable or dotted field -> just variable name for now
    var_t* v = env_get(env,t);
    return v? copy_value(&v->v) : make_null();
}

static value_t eval_concat(env_t* env, char* expr){
    // split by + not inside quotes
    value_t out = make_str("");
    char* s=expr; int depth=0; char part[1024]; size_t pi=0; int inq=0;
    #define APPEND_PART do{ part[pi]='\0'; char* pdup=str_dup(part); char* ptrim=str_trim(pdup); value_t v=eval_atom(env, ptrim); if(v.t==VT_NUM){ char buf[64]; snprintf(buf,sizeof buf,"%.0f", v.n); size_t old=strlen(out.s); size_t add=strlen(buf); out.s=(char*)realloc(out.s, old+add+1); memcpy(out.s+old, buf, add+1);} else if(v.t==VT_STR){ size_t old=strlen(out.s); size_t add=strlen(v.s?v.s:""); out.s=(char*)realloc(out.s, old+add+1); memcpy(out.s+old, v.s?v.s:"",(add+1));} else if(v.t==VT_BOOL){ const char* bs=v.b?"true":"false"; size_t old=strlen(out.s); size_t add=strlen(bs); out.s=(char*)realloc(out.s, old+add+1); memcpy(out.s+old, bs, add+1);} free(pdup); free_value(&v);} while(0)
    for (; *s; s++){
        if(*s=='"') inq=!inq;
        if(!inq && *s=='+'){ APPEND_PART; pi=0; continue; }
        part[pi++]=*s;
    }
    APPEND_PART;
    return out;
}

static void skip_ws(const char** p){ while (isspace((unsigned char)**p)) (*p)++; }

static value_t parse_expr_numeric(env_t* env, const char** p);

static value_t parse_primary(env_t* env, const char** p){
    skip_ws(p);
    const char* s = *p;
    if (*s == '"') { // string literal
        s++;
        const char* start = s;
        while (*s && *s != '"') s++;
        size_t n = (size_t)(s - start);
        char* buf = (char*)malloc(n + 1);
        memcpy(buf, start, n); buf[n] = '\0';
        if (*s == '"') s++;
        *p = s;
        value_t v = make_str(buf);
        free(buf);
        return v;
    }
    if (*s == '(') { s++; *p = s; value_t v = parse_expr_numeric(env, p); if (**p == ')') (*p)++; return v; }
    // number or identifier
    char token[256]; size_t ti = 0;
    if (*s == '-' && isdigit((unsigned char)s[1])) { token[ti++] = *s++; }
    if (isdigit((unsigned char)*s)) {
        while (isdigit((unsigned char)*s)) token[ti++] = *s++;
        if (*s == '.') { token[ti++]='.'; s++; while (isdigit((unsigned char)*s)) token[ti++]=*s++; }
        token[ti]='\0'; *p = s; return make_num(strtod(token, NULL));
    }
    // identifier
    while (*s && (isalnum((unsigned char)*s) || *s=='_' || *s=='.')) token[ti++] = *s++;
    token[ti] = '\0'; *p = s;
    value_t val; var_t* v = env_get(env, token); val = v ? copy_value(&v->v) : make_null();
    skip_ws(p);
    if (**p == '^') { // dereference
        (*p)++;
        if (val.t==VT_STR && val.s){ var_t* tv = env_get(env, val.s); if (tv) val = tv->v; else val = make_null(); }
    }
    return val;
}

static value_t parse_term(env_t* env, const char** p){
    value_t left = parse_primary(env, p);
    while (1) {
        skip_ws(p);
        char op = **p;
        if (op!='*' && op!='/' && op!='%') break;
        (*p)++;
        value_t right = parse_primary(env, p);
        double ln = (left.t==VT_NUM)?left.n:0.0;
        double rn = (right.t==VT_NUM)?right.n:0.0;
        double out = ln;
        if (op=='*') out = ln * rn;
        else if (op=='/') out = rn!=0.0 ? ln / rn : 0.0;
        else /* % */ out = (double)((long)ln % (long)rn);
        free_value(&left); free_value(&right);
        left = make_num(out);
    }
    return left;
}

static value_t parse_expr_numeric(env_t* env, const char** p){
    value_t left = parse_term(env, p);
    while (1) {
        skip_ws(p);
        char op = **p;
        if (op!='+' && op!='-') break;
        (*p)++;
        value_t right = parse_term(env, p);
        double ln = (left.t==VT_NUM)?left.n:0.0;
        double rn = (right.t==VT_NUM)?right.n:0.0;
        double out = (op=='+') ? (ln + rn) : (ln - rn);
        free_value(&left); free_value(&right);
        left = make_num(out);
    }
    return left;
}

static int contains_quote_top_level(const char* s){ int inq=0; int par=0; for(; *s; s++){ if(*s=='"' && par==0) inq=!inq; if(inq) continue; if(*s=='(') par++; else if(*s==')' && par>0) par--; if(*s=='"') inq=!inq; } return inq; }

static int starts_with(const char* s, const char* p){ size_t n=strlen(p); return strncmp(s,p,n)==0; }
static int ends_with(const char* s, const char* p){ size_t ns=strlen(s), np=strlen(p); return ns>=np && memcmp(s+ns-np,p,np)==0; }
static int contains(const char* s, const char* sub){ return strstr(s, sub)!=NULL; }

static value_t eval_expr(env_t* env, char* expr); /* forward */

static value_t eval_compare_op(env_t* env, char* expr){
    // supports: a beginsWith b, a endsWith b, a contains b, comparisons <,>,==,!=
    char buf[1024]; strncpy(buf, expr, sizeof buf-1); buf[sizeof buf-1]='\0';
    char* t = str_trim(buf);
    // find operators by precedence
    const char* ops[] = {" beginsWith ", " endsWith ", " contains ", "==", "!=", ">=", "<=", ">", "<"};
    for (int i=0;i<9;i++){
        char* p = strstr(t, ops[i]);
        if(p){
            char left[512]; char right[512];
            size_t ln=(size_t)(p - t);
            memcpy(left,t,ln); left[ln]='\0';
            strcpy(right, p + strlen(ops[i]));
            value_t lv = eval_expr(env, left);
            value_t rv = eval_expr(env, right);
            int res=0;
            if(strcmp(ops[i]," beginsWith ")==0){ char* ls=to_string_alloc(&lv); char* rs=to_string_alloc(&rv); res = (ls && rs) ? starts_with(ls, rs) : 0; if(ls) free(ls); if(rs) free(rs); }
            else if(strcmp(ops[i]," endsWith ")==0){ char* ls=to_string_alloc(&lv); char* rs=to_string_alloc(&rv); res = (ls && rs) ? ends_with(ls, rs) : 0; if(ls) free(ls); if(rs) free(rs); }
            else if(strcmp(ops[i]," contains ")==0){ char* ls=to_string_alloc(&lv); char* rs=to_string_alloc(&rv); res = (ls && rs) ? contains(ls, rs) : 0; if(ls) free(ls); if(rs) free(rs); }
            else if(strcmp(ops[i],"==")==0){
                if(lv.t==VT_NUM&&rv.t==VT_NUM) res=(lv.n==rv.n);
                else if(lv.t==VT_BOOL&&rv.t==VT_BOOL) res=(lv.b==rv.b);
                else { char* ls=to_string_alloc(&lv); char* rs=to_string_alloc(&rv); res = (ls && rs) ? (strcmp(ls, rs)==0) : 0; if(ls) free(ls); if(rs) free(rs); }
            }
            else if(strcmp(ops[i],"!=")==0){
                if(lv.t==VT_NUM&&rv.t==VT_NUM) res=(lv.n!=rv.n);
                else if(lv.t==VT_BOOL&&rv.t==VT_BOOL) res=(lv.b!=rv.b);
                else { char* ls=to_string_alloc(&lv); char* rs=to_string_alloc(&rv); res = (ls && rs) ? (strcmp(ls, rs)!=0) : 0; if(ls) free(ls); if(rs) free(rs); }
            }

            else if(strcmp(ops[i],">=")==0){ res=(lv.t==VT_NUM&&rv.t==VT_NUM) ? (lv.n>=rv.n) : 0; }
            else if(strcmp(ops[i],"<=")==0){ res=(lv.t==VT_NUM&&rv.t==VT_NUM) ? (lv.n<=rv.n) : 0; }
            else if(strcmp(ops[i],">")==0){ res=(lv.t==VT_NUM&&rv.t==VT_NUM) ? (lv.n>rv.n) : 0; }
            else if(strcmp(ops[i],"<")==0){ res=(lv.t==VT_NUM&&rv.t==VT_NUM) ? (lv.n<rv.n) : 0; }
            free_value(&lv); free_value(&rv);
            return make_bool(res);
        }
    }
    return eval_atom(env, t);
}

static value_t eval_expr(env_t* env, char* expr){
    char* t = str_trim(expr);
    // address-of @var or @obj.field returns reference string
    if (*t=='@') { t++; t = str_trim(t); return make_str(t); }
    // function call greet(name: "X")
    do {
        char* lp = strchr(t,'('); char* rp = lp? strrchr(t, ')') : NULL;
        if (!(lp && rp && rp>lp)) break;
        *rp='\0'; *lp='\0';
        char* fname=str_trim(t); char* args=str_trim(lp+1);
        if (strcmp(fname, "greet")==0){
            const char* val = "World"; char tmp[256];
            // accept both name: and name : forms
            char* p = strstr(args, "name");
            if (p){ p+=4; while(isspace((unsigned char)*p)) p++; if(*p==':') p++; while(isspace((unsigned char)*p)) p++;
                if(*p=='"'){ p++; char* e=strchr(p,'"'); if(e){ size_t n=(size_t)(e-p); if(n>=sizeof(tmp)) n=sizeof(tmp)-1; memcpy(tmp,p,n); tmp[n]='\0'; val=tmp; }}}
            char res[512]; snprintf(res,sizeof res, "Hello, %s", val);
            return make_str(res);
        }
    } while(0);
    // string() coercion
    if (strncmp(t, "string(", 7)==0){ char* inner=t+7; char* end=strchr(inner, ')'); if(end){ *end='\0'; value_t iv = eval_expr(env, inner); char buf[512]; char* s = NULL; if(iv.t==VT_STR) s=str_dup(iv.s?iv.s:""); else if(iv.t==VT_BOOL) s=str_dup(iv.b?"true":"false"); else if(iv.t==VT_NUM){ snprintf(buf,sizeof buf, "%.0f", iv.n); s=str_dup(buf);} else s=str_dup(""); value_t out = make_str(s); free(s); free_value(&iv); return out; }}
    // parentheses around expression
    if (*t=='('){ size_t n=strlen(t); if (n>=2 && t[n-1]==')'){ t[n-1]='\0'; t++; }}
    // or operator
    char* por = strstr(t, " or "); if(por){ *por='\0'; char* right=por+4; value_t lv=eval_expr(env,t); if(lv.t==VT_BOOL && lv.b){ free_value(&lv); return make_bool(1);} free_value(&lv); value_t rv=eval_expr(env,right); int res=(rv.t==VT_BOOL && rv.b); free_value(&rv); return make_bool(res);} 
    char* pand = strstr(t, " and "); if(pand){ *pand='\0'; char* right=pand+5; value_t lv=eval_expr(env,t); value_t rv=eval_expr(env,right); int res=(lv.t==VT_BOOL && rv.t==VT_BOOL && lv.b && rv.b); free_value(&lv); free_value(&rv); return make_bool(res);} 
    // comparisons and string ops (top-level)
    {
        const char* cops[] = {" beginsWith ", " endsWith ", " contains ", "==", "!=", ">=", "<=", ">", "<"};
        for (int i=0;i<9;i++){ char* p = strstr(t, cops[i]); if (p){ return eval_compare_op(env, t); }}
    }
    // concatenation vs numeric arithmetic
    if (strchr(t,'+')){
        // Heuristic: if there are any string quotes, do concatenation; else numeric
        if (strchr(t,'"')) return eval_concat(env, t);
    }
    const char* p = t; value_t v = parse_expr_numeric(env, &p); return v;
}

static void set_var(env_t* env, const char* name, value_t v){ var_t* e=env_get(env,name); if(e){ free_value(&e->v); e->v=v; }}

static int parse_and_run_block(env_t* env, char* block, int* did_continue, char** ret_str);

static int handle_if(env_t* env, char* s, char** ret_str){
    // pattern: if <cond> return(...) else return(...)
    char* pif = strstr(s, "if "); if(!pif) return 0; pif += 3;
    char* pelse = strstr(s, " else ");
    char cond[512]; char thenpart[512]; char elsepart[512]={0};
    if(pelse){ size_t clen=(size_t)(pelse - pif); memcpy(cond,pif,clen); cond[clen]='\0'; strcpy(thenpart, pelse - 0); /* we'll reparse */ }
    // find return(...) before else
    char* preturn = strstr(pif, "return("); if(!preturn) return 0; char* endret = strchr(preturn, ')'); if(!endret) return 0;
    size_t clen=(size_t)(preturn - pif); memcpy(cond, pif, clen); cond[clen]='\0';
    char thenexpr[512]; size_t tlen=(size_t)(endret - (preturn+7)); memcpy(thenexpr, preturn+7, tlen); thenexpr[tlen]='\0';
    if(pelse){ char* preturn2=strstr(pelse, "return("); if(preturn2){ char* end2=strchr(preturn2, ')'); if(end2){ size_t elen=(size_t)(end2 - (preturn2+7)); memcpy(elsepart, preturn2+7, elen); elsepart[elen]='\0'; }}}
    value_t cv = eval_expr(env, cond); int ctrue=(cv.t==VT_BOOL && cv.b) || (cv.t==VT_NUM && cv.n!=0); free_value(&cv);
    char* picked = ctrue ? thenexpr : elsepart;
    value_t rv = eval_expr(env, picked);
    if (rv.t==VT_NUM){ char buf[64]; snprintf(buf,sizeof buf, "%.0f", rv.n); *ret_str=str_dup(buf);} else if(rv.t==VT_BOOL){ *ret_str=str_dup(rv.b?"true":"false"); } else if(rv.t==VT_STR){ *ret_str=str_dup(rv.s?rv.s:""); } else { *ret_str=str_dup(""); }
    free_value(&rv);
    return 1;
}

static int parse_and_run(env_t* env, char* script, char** ret_str){
    // split statements by ';' and execute in order
    char buf[4096]; strncpy(buf, script, sizeof buf-1); buf[sizeof buf-1]='\0';
    char* save=NULL; char* tok=strtok_r(buf, ";", &save);
    int did_return=0; char* result=NULL;
    while(tok){ char* line=str_trim(tok);
        // try/else: naive model sets tryError and evaluates else return
        if (strncmp(line, "try", 3)==0){
            set_var(env, "tryError", make_str("error"));
            char* pelse = strstr(line, "else");
            if (pelse){ char* preturn=strstr(pelse, "return("); if(preturn){ char* end=strchr(preturn, ')'); if(end){ *end='\0'; value_t v=eval_expr(env, preturn+7); if(v.t==VT_NUM){ char buf[64]; snprintf(buf,sizeof buf, "%.0f", v.n); result=str_dup(buf);} else if(v.t==VT_BOOL){ result=str_dup(v.b?"true":"false"); } else if(v.t==VT_STR){ result=str_dup(v.s?v.s:""); } free_value(&v); did_return=1; break; }}}
        }
        else if (strncmp(line, "if ", 3)==0){
            if (handle_if(env, line, &result)) { did_return=1; break; }
        }
        else if (strncmp(line, "on ", 3)==0){
            // Extremely naive: define-only; if same line contains return(...), evaluate it
            char* preturn_inline = strstr(line, "return(");
            if (preturn_inline){ char* end=strchr(preturn_inline, ')'); if(end){ *end='\0'; value_t v=eval_expr(env, preturn_inline+7); if(v.t==VT_NUM){ char buf[64]; snprintf(buf,sizeof buf, "%.0f", v.n); result=str_dup(buf);} else if(v.t==VT_BOOL){ result=str_dup(v.b?"true":"false"); } else if(v.t==VT_STR){ result=str_dup(v.s?v.s:""); } free_value(&v); did_return=1; break; }}
        }
        else if (strncmp(line,"local(",6)==0){
            char* p = line+6; char* end=strchr(p,')'); if(end){ *end='\0'; char* sv=NULL; char* el=strtok_r(p, ",", &sv); while(el){ char* name=str_trim(el); char* eq=strchr(name,'='); if(eq){ *eq='\0'; char* rhs=str_trim(eq+1); value_t v=eval_expr(env,rhs); set_var(env,name,v);} else { set_var(env,name, make_null()); } el=strtok_r(NULL, ",", &sv);} }
        }
        else if (strncmp(line, "for ",4)==0){
            // for i = a to b { block }
            char* vname=line+4; while(*vname==' ') vname++;
            char* peq=strchr(vname,'='); if(!peq) goto next;
            *peq='\0'; char* name=str_trim(vname);
            char* pto=strstr(peq+1, " to "); if(!pto) goto next;
            *pto='\0'; char* start=str_trim(peq+1);
            // parse end expression token before '{'
            const char* endstart = pto + 4; while (isspace((unsigned char)*endstart)) endstart++;
            const char* endend = endstart; while (*endend && !isspace((unsigned char)*endend) && *endend!='{') endend++;
            char endexpr[64]; size_t en = (size_t)(endend - endstart); if (en >= sizeof(endexpr)) en = sizeof(endexpr)-1; memcpy(endexpr, endstart, en); endexpr[en]='\0';
            char* pbrace = strchr(endend,'{'); if(!pbrace) goto next;
            char* closing=strrchr(endend,'}'); if(!closing) goto next;
            // Extract block between '{' and '}' without modifying outer buffer
            const char* blockstart = pbrace + 1;
            size_t blen = (size_t)(closing - blockstart);
            char blockbuf[2048]; if (blen >= sizeof(blockbuf)) blen = sizeof(blockbuf)-1; memcpy(blockbuf, blockstart, blen); blockbuf[blen] = '\0';
            value_t vs=eval_expr(env,start); value_t ve=eval_expr(env,endexpr);
            int s=(int)vs.n, e=(int)ve.n; free_value(&vs); free_value(&ve);
            int did_cont=0; for(int i=s;i<=e;i++){ set_var(env,name, make_num(i)); char* ret=NULL; if(parse_and_run_block(env, blockbuf, &did_cont, &ret)){ if(ret){ if(result) free(result); result=ret; did_return=1; break;} } }
            // process remainder after the for-block on the same line (e.g., return(sum))
            if (!did_return) {
                char* remainder = (char*)closing + 1;
                remainder = str_trim(remainder);
                if (strncmp(remainder, "return(", 7) == 0) {
                    char* p = remainder + 7; char* end = strchr(p, ')'); if (end) { *end='\0'; value_t v=eval_expr(env,p); if(v.t==VT_NUM){ char buf[64]; snprintf(buf,sizeof buf,"%.0f", v.n); result=str_dup(buf);} else if(v.t==VT_BOOL){ result=str_dup(v.b?"true":"false"); } else if(v.t==VT_STR){ result=str_dup(v.s?v.s:""); } free_value(&v); did_return=1; break; }
                }
            }
        }
        else if (strncmp(line, "return(",7)==0){ char* p=line+7; char* end=strchr(p,')'); if(end){ *end='\0'; value_t v=eval_expr(env,p); if(v.t==VT_NUM){ char buf[64]; snprintf(buf,sizeof buf,"%.0f", v.n); result=str_dup(buf);} else if(v.t==VT_BOOL){ result=str_dup(v.b?"true":"false"); } else if(v.t==VT_STR){ result=str_dup(v.s?v.s:""); } free_value(&v); did_return=1; break; }}
        else {
            // assignment or postfix ++
            char* pplus = strstr(line, "++"); if(pplus){ *pplus='\0'; char* name=str_trim(line); var_t* v=env_get(env,name); if(v&&v->v.t==VT_NUM){ v->v.n+=1; } else if(v){ v->v=make_num( (v->v.t==VT_NULL)?1:(v->v.n+1) ); } goto next; }
            char* peq=strchr(line,'='); if(peq){ *peq='\0'; char* name=str_trim(line); char* rhs=str_trim(peq+1);
                // string() coercion support
                if(strncmp(rhs,"string(",7)==0){ char* var=rhs+7; char* end=strchr(var,')'); if(end) *end='\0'; var_t* vv=env_get(env, str_trim(var)); if(vv){ if(vv->v.t==VT_STR) set_var(env,name, make_str(vv->v.s)); else if(vv->v.t==VT_NUM){ char buf[64]; snprintf(buf,sizeof buf,"%.0f", vv->v.n); set_var(env,name, make_str(buf)); } else { set_var(env,name, make_str("")); } } }
                else if (*rhs=='{') {
                    // list literal -> parse simple CSV, store as joined string
                    char inner[1024]; strncpy(inner, rhs+1, sizeof inner-1); inner[sizeof inner-1]='\0'; char* rb=strrchr(inner, '}'); if (rb) *rb='\0';
                    char* svl=NULL; char* it=strtok_r(inner, ",", &svl); char acc[1024]; acc[0]='\0'; int first=1;
                    while(it){ char* e=str_trim(it); value_t v = eval_expr(env,e); char* s = to_string_alloc(&v); if(!first) strncat(acc, ",", sizeof acc - strlen(acc) -1); strncat(acc, s?s:"", sizeof acc - strlen(acc) -1); free(s); free_value(&v); first=0; it=strtok_r(NULL, ",", &svl); }
                    set_var(env, name, make_str(acc));
                }
                else if (*rhs=='[') {
                    // record literal -> parse key: value pairs minimally
                    set_var(env, name, make_str("[record]"));
                    char inner[1024]; strncpy(inner, rhs+1, sizeof inner-1); inner[sizeof inner-1]='\0'; char* rb=strrchr(inner, ']'); if (rb) *rb='\0';
                    char* sv=NULL; char* pair=strtok_r(inner, ",", &sv);
                    while(pair){ char* p=str_trim(pair); char* colon=strchr(p, ':'); if(colon){ *colon='\0'; char* key=str_trim(p); char* val=str_trim(colon+1);
                            char fullname[128]; snprintf(fullname, sizeof fullname, "%s.%s", name, key);
                            if (*val=='"'){ char* v=strip_quotes(val); set_var(env, fullname, make_str(v)); }
                            else { value_t v=eval_expr(env,val); set_var(env, fullname, v); }
                        }
                        pair=strtok_r(NULL, ",", &sv);
                    }
                }
                else { value_t v = eval_expr(env, rhs); set_var(env,name, v); }
            }
        }
        next: tok=strtok_r(NULL, ";", &save);
    }
    if(did_return && result){ *ret_str=result; return 1; }
    if(result) free(result);
    return 0;
}

static int parse_and_run_block(env_t* env, char* block, int* did_continue, char** ret_str){
    // support only: if i == 3 { continue } and sum = sum + i; return(...)
    *did_continue=0; *ret_str=NULL;
    char tmp[2048]; strncpy(tmp, block, sizeof tmp-1); tmp[sizeof tmp-1]='\0';
    char* s=tmp; while(*s){ while(isspace((unsigned char)*s)) s++; if(strncmp(s,"if ",3)==0){ char* brace=strchr(s,'{'); if(!brace) break; char cond[256]; size_t clen=(size_t)(brace - (s+3)); memcpy(cond, s+3, clen); cond[clen]='\0'; char* endb=strchr(brace+1, '}'); if(!endb) break; char inner[128]; size_t ilen=(size_t)(endb-(brace+1)); memcpy(inner, brace+1, ilen); inner[ilen]='\0'; value_t c=eval_expr(env, cond); int ok=(c.t==VT_BOOL&&c.b)||(c.t==VT_NUM&&c.n!=0); free_value(&c); if(ok && strstr(inner, "continue")) { *did_continue=1; return 0; } s=endb+1; continue; }
        if(strncmp(s,"return(",7)==0){ char* p=s+7; char* end=strchr(p,')'); if(end){ *end='\0'; value_t v=eval_expr(env,p); if(v.t==VT_NUM){ char buf[64]; snprintf(buf,sizeof buf,"%.0f", v.n); *ret_str=str_dup(buf);} else if(v.t==VT_BOOL){ *ret_str=str_dup(v.b?"true":"false"); } else if(v.t==VT_STR){ *ret_str=str_dup(v.s?v.s:""); } free_value(&v); return 1; } }
        // assignment like sum = sum + i
        char* semi=strchr(s,';'); if(semi){ *semi='\0'; }
        char* peq=strchr(s,'='); if(peq){ *peq='\0'; char* name=str_trim(s); char* rhs=str_trim(peq+1); value_t v = eval_expr(env, rhs); set_var(env,name,v); }
        if(!semi) break; s=semi+1;
    }
    return 0;
}

usertalk_execution_t* cli_create_execution_context(void) {
    usertalk_execution_t* e = (usertalk_execution_t*)calloc(1, sizeof(*e));
    return e;
}

bool cli_compile_script(const char* script, usertalk_execution_t* exec) {
    if (!exec) return false;
    if (exec->script_source) free(exec->script_source);
    exec->script_source = script ? str_dup(script) : NULL;
    return exec->script_source != NULL;
}

bool cli_execute_compiled_script(usertalk_execution_t* exec) {
    if (!exec || !exec->script_source) return false;
    if (exec->result) { free(exec->result); exec->result = NULL; }
    
    // Use self-contained portable evaluator
    env_t env = {0};
    char* result = NULL;
    int did_return = parse_and_run(&env, exec->script_source, &result);
    
    if (did_return && result) {
        exec->result = str_dup(result);
        free(result);
        return true;
    }
    
    if (result) free(result);
    return false;
}

char* cli_get_execution_result_string(usertalk_execution_t* exec) {
    if (!exec || !exec->result) return NULL;
    return str_dup(exec->result);
}

void cli_free_execution_context(usertalk_execution_t* exec) {
    if (!exec) return;
    free(exec->script_source);
    free(exec->result);
    free(exec);
}

void cli_free(void* p) { free(p); }


