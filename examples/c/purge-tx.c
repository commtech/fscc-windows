#include <stdlib.h> /* EXIT_SUCCESS, EXIT_FAILURE */
#include <windows.h> /* CreateFile, DeviceIoControl, CloseHandle */
#include "../../include/fscc.h" /* FSCC_PURGE_TX */
#include "utils.h" /* DisplayError */

int main(void)
{
	HANDLE port = 0;
	DWORD junk;
	
	port = CreateFile("\\\\.\\FSCC0", GENERIC_READ | GENERIC_WRITE, 0, NULL, 
	                  OPEN_EXISTING, 0, NULL);
 
	if (port == INVALID_HANDLE_VALUE) { 
		DisplayError("CreateFile");
		  
		return -1; 
	}

	DeviceIoControl(port, FSCC_PURGE_TX, NULL, 0, NULL, 0, &junk, (LPOVERLAPPED)NULL);

	CloseHandle(port);
	
	return 0;
}

