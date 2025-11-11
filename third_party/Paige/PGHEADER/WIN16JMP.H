
#ifndef _WIN_SETJMP

extern "C" {

#if (_MSC_VER <= 600)
#define __cdecl     _cdecl
#define __far       _far
#endif 

/* define the buffer type for holding the state information */

#define _JBLEN  9  /* bp, di, si, sp, ret addr, ds */

#ifndef _JMP_BUF_DEFINED
typedef  int  jmp_buf[_JBLEN];
#define _JMP_BUF_DEFINED
#endif 

/* ANSI requires setjmp be a macro */

#define setjmp  _setjmp

/* function prototypes */

int  __cdecl _setjmp(jmp_buf);
void __cdecl longjmp(jmp_buf, int);


#define _WIN_SETJMP

};

#endif 
