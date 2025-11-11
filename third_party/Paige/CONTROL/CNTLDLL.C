/* This file is used as a PAIGE control "shell" to create a DLL for Windows. */

#include <WINDOWS.H>
#include "pgCntl.h"
#include "pgCtlUtl.h"

int far pascal LibMain(HINSTANCE hinst, WORD wDataSeg, WORD cbHeapSize, LPSTR cmdline);
int far pascal WEP (int nParameter);


int far pascal LibMain(HINSTANCE hinst, WORD wDataSeg, WORD cbHeapSize, LPSTR cmdline)
{
	libInstance = hinst;
 	return	1;			/* No initialization required for PAIGE DLL */
}

int far pascal WEP (int nParameter)
{
	ClosePaigeLibs();
	
  	return	1;
}



