/* This file is used as a PAIGE "shell" to create a DLL for Windows. */

#include <WINDOWS.H>

int far pascal LibMain(HINSTANCE hinst, WORD wDataSeg, WORD cbHeapSize, LPSTR cmdline);
int far pascal WEP (int nParameter);


int far pascal LibMain(HINSTANCE hinst, WORD wDataSeg, WORD cbHeapSize, LPSTR cmdline)
{
	return	1;			/* No initialization required for PAIGE DLL */
}

int far pascal WEP (int nParameter)
{
  	return	1;
}



