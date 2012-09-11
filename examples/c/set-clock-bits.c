#include <stdlib.h> /* EXIT_SUCCESS, EXIT_FAILURE */
#include <windows.h> /* CreateFile, DeviceIoControl, CloseHandle */
#include "../../include/fscc.h" /* FSCC_SET_CLOCK_BITS */
#include "examples.h" /* DisplayError */

int main(void)
{
	HANDLE port = 0;
	DWORD junk;
	char clock_bits[20] = {0xff, 0xff, 0xff, 0x01, 0x84, 0x01, 0x48, 0x72,
	                       0x9a, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	                       0x00, 0x04, 0xe0, 0x01};
	
	port = CreateFile("\\\\.\\FSCC0", GENERIC_READ | GENERIC_WRITE, 0, NULL, 
	                  OPEN_EXISTING, 0, NULL);
 
	if (port == INVALID_HANDLE_VALUE) { 
		DisplayError("CreateFile");
		  
		return EXIT_FAILURE; 
	}
	
	DeviceIoControl(port, FSCC_SET_CLOCK_BITS, &clock_bits, sizeof(clock_bits), NULL, 0, &junk, (LPOVERLAPPED)NULL);

	CloseHandle(port);
	
	return EXIT_SUCCESS;
}
