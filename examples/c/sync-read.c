#include <stdio.h> /* fprintf */
#include <stdlib.h> /* EXIT_SUCCESS, EXIT_FAILURE */
#include <windows.h> /* CreateFile, ReadFile, CloseHandle */
#include "examples.h" /* DisplayError */

int main(void)
{
	HANDLE port = 0;
	DWORD bytes_read = 0;
	char data[20] = {'\0'};

	port = CreateFile("\\\\.\\FSCC0", GENERIC_READ | GENERIC_WRITE, 0, NULL, 
	                  OPEN_EXISTING, 0, NULL);
 
	if (port == INVALID_HANDLE_VALUE) { 
		DisplayError("CreateFile");
		  	  
		return EXIT_FAILURE;
	}

	if(ReadFile(port, data, sizeof(data), &bytes_read, NULL) == FALSE)
	{
		DisplayError("ReadFile");
		  
		CloseHandle(port);	
	  
		return EXIT_FAILURE;
	}

	fprintf(stdout, "%s\n", data);

	CloseHandle(port);
	
	return EXIT_SUCCESS;
}
