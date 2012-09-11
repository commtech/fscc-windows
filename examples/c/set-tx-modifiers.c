#include <stdlib.h> /* EXIT_SUCCESS, EXIT_FAILURE */
#include <windows.h> /* CreateFile, DeviceIoControl, CloseHandle */
#include "../../include/fscc.h" /* FSCC_SET_TX_MODIFIERS */
#include "examples.h" /* DisplayError */

/*
    This is a simple example showing how to change the transmit type for each 
    port.

    Valid TX_MODIFIERS are:
    ---------------------------------------------------------------------------
    XF - Normal transmit - disable modifiers
    XREP - Transmit repeat
    TXT - Transmit on timer
    TXEXT - Transmit on external signal
    
*/

int main(void)
{
	HANDLE port = 0;
	unsigned modifiers;
	DWORD junk;
	
	port = CreateFile("\\\\.\\FSCC0", GENERIC_READ | GENERIC_WRITE, 0, NULL, 
	                  OPEN_EXISTING, 0, NULL);
 
	if (port == INVALID_HANDLE_VALUE) { 
		DisplayError("CreateFile");
		  
		return EXIT_FAILURE; 
	}
	
	modifiers = TXT | XREP;
	DeviceIoControl(port, FSCC_SET_TX_MODIFIERS, &modifiers, sizeof(modifiers), NULL, 0, &junk, (LPOVERLAPPED)NULL);
	
	modifiers = XF;
	DeviceIoControl(port, FSCC_SET_TX_MODIFIERS, &modifiers, sizeof(modifiers), NULL, 0, &junk, (LPOVERLAPPED)NULL); /* disable modifiers */

	CloseHandle(port);
	
	return EXIT_SUCCESS;
}

