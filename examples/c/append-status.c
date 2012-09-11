#include <stdio.h> /* fprintf */
#include <string.h> /* memset */
#include <stdlib.h> /* EXIT_SUCCESS, EXIT_FAILURE */
#include "../../include/fscc.h" /* FSCC_SET_APPEND_STATUS */
#include "utils.h" /* DisplayError */

int main(void)
{
	HANDLE port = 0;
	DWORD bytes_written = 0;
	DWORD bytes_read = 0;
	DWORD junk;
	char data[] = "Hello world!";
	char buffer[20] = {'\0'};
	unsigned status = 0;

	port = CreateFile("\\\\.\\FSCC0", GENERIC_READ | GENERIC_WRITE, 0, NULL, 
	                  OPEN_EXISTING, 0, NULL);
 
	if (port == INVALID_HANDLE_VALUE) { 
		DisplayError("CreateFile");
		  
		return EXIT_FAILURE; 
	}
	
	DeviceIoControl(port, FSCC_DISABLE_APPEND_STATUS, NULL, 0, NULL, 0, &junk, (LPOVERLAPPED)NULL);
	
	bytes_written = WriteFile(port, data, sizeof(data), &bytes_written, NULL);
	bytes_read = ReadFile(port, buffer, sizeof(buffer), &bytes_read, NULL);	
	
	fprintf(stdout, "append_status = off\n");
	fprintf(stdout, "bytes_read = %i\n", bytes_read);
	fprintf(stdout, "data = %s\n", buffer);
	fprintf(stdout, "status = 0x????\n\n");

	DeviceIoControl(port, FSCC_ENABLE_APPEND_STATUS, NULL, 0, NULL, 0, &junk, (LPOVERLAPPED)NULL);
	memset(&buffer, 0, sizeof(buffer));	
	
	bytes_written = WriteFile(port, data, sizeof(data), &bytes_written, NULL);
	bytes_read = ReadFile(port, buffer, sizeof(buffer), &bytes_read, NULL);	
	
	status = buffer[bytes_read - 2];
	
	fprintf(stdout, "append_status = on\n");
	fprintf(stdout, "bytes_read = %i\n", bytes_read);
	fprintf(stdout, "data = %s\n", buffer);
	fprintf(stdout, "status = 0x%04x\n", status);

	CloseHandle(port);
	
	return EXIT_SUCCESS;
}

/*

OUTPUT
-------------------------------------------------------------------------------

append_status = off
bytes_read = 13
data = Hello world!
status = 0x????

append_status = on
bytes_read = 15
data = Hello world!
status = 0x0004

*/

