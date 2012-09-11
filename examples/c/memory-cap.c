#include <stdlib.h> /* EXIT_SUCCESS, EXIT_FAILURE */
#include <windows.h> /* CreateFile, DeviceIoControl, CloseHandle */
#include "../../include/fscc.h" /* FSCC_SET_MEMORY_CAP */
#include "utils.h" /* DisplayError */

int main(void)
{
	HANDLE port = 0;
	DWORD junk;
    struct fscc_memory_cap memory_cap;
	
	port = CreateFile("\\\\.\\FSCC0", GENERIC_READ | GENERIC_WRITE, 0, NULL, 
	                  OPEN_EXISTING, 0, NULL);
 
	if (port == INVALID_HANDLE_VALUE) { 
		DisplayError("CreateFile");
		  
		return EXIT_FAILURE; 
	}

	memory_cap.input = 50000;
	memory_cap.output = 75000;

	DeviceIoControl(port, FSCC_SET_MEMORY_CAP, &memory_cap, sizeof(memory_cap), NULL, 0, &junk, (LPOVERLAPPED)NULL);

	CloseHandle(port);
	
	return EXIT_SUCCESS;
}

