#include <stdlib.h> /* EXIT_SUCCESS, EXIT_FAILURE */
#include <windows.h> /* CreateFile, ReadFile, WriteFile, CloseHandle */
#include "../../include/fscc.h" /* FSCC_PURGE_RX, FSCC_PURGE_TX */
#include "utils.h" /* DisplayError */

int main(void)
{
	HANDLE port = 0;
	DWORD bytes_read = 0;
	DWORD bytes_written = 0;
	DWORD junk;
	char data[] = "Hello world!";
	char buffer[20] = {'\0'};

	port = CreateFile("\\\\.\\FSCC0", GENERIC_READ | GENERIC_WRITE, 0, NULL, 
	                  OPEN_EXISTING, 0, NULL);
 
	if (port == INVALID_HANDLE_VALUE) {
		DisplayError("CreateFile");
		  
		return EXIT_FAILURE; 
	}

	if (DeviceIoControl(port, FSCC_PURGE_RX, NULL, 0, NULL, 0, &junk, (LPOVERLAPPED)NULL) == FALSE) {
		DisplayError("DeviceIoControl");
		
		if (CloseHandle(port) == FALSE)
			DisplayError("CloseHandle");
			
		return EXIT_FAILURE;
	}

	if (DeviceIoControl(port, FSCC_PURGE_TX, NULL, 0, NULL, 0, &junk, (LPOVERLAPPED)NULL) == FALSE) {
		DisplayError("DeviceIoControl");
		
		if (CloseHandle(port) == FALSE)
			DisplayError("CloseHandle");
			
		return EXIT_FAILURE;
	}

   if (WriteFile(port, data, sizeof(data), &bytes_written, NULL) == FALSE) {
		DisplayError("WriteFile");
		
		if (CloseHandle(port) == FALSE)
			DisplayError("CloseHandle");
		
		return EXIT_FAILURE;
	}

	if(ReadFile(port, buffer, sizeof(buffer), &bytes_read, NULL) == FALSE)
	{
		DisplayError("ReadFile");
		  
		if (CloseHandle(port) == FALSE)
			DisplayError("CloseHandle");
		  
		return EXIT_FAILURE;
	}

		  
	if (CloseHandle(port) == FALSE) {
		DisplayError("CloseHandle");
			
		return EXIT_FAILURE;
	}
	
	return EXIT_SUCCESS;
}
