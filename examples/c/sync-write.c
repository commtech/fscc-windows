#include <stdlib.h> /* EXIT_SUCCESS, EXIT_FAILURE */
#include <windows.h> /* CreateFile WriteFile, CloseHandle */
#include "examples.h" /* DisplayError */

int main(void)
{
	HANDLE port = 0;
	DWORD bytes_written = 0;
	char data[] = "Hello world!";
	
	//port = FsccOpen(0);
	//test();
	
	port = CreateFile("\\\\.\\FSCC0", GENERIC_READ | GENERIC_WRITE, 0, NULL, 
	                  OPEN_EXISTING, 0, NULL);

	if (port == INVALID_HANDLE_VALUE) { 
		DisplayError("CreateFile");
		   
		return EXIT_FAILURE; 
	}
	
   if (WriteFile(port, data, sizeof(data), &bytes_written, NULL) == FALSE) {
		DisplayError("WriteFile");
		
		CloseHandle(port);		
		return EXIT_FAILURE;
	}
	
	CloseHandle(port);
	
	return EXIT_SUCCESS;
}
