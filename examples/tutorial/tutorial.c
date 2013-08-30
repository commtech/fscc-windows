#include <Windows.h> /* HANDLE, CreateFile, CloseHandle 
                        WriteFile, ReadFile*/
#include <stdio.h> /* fprintf */
#include <stdlib.h> /* EXIT_SUCCESS, EXIT_FAILURE */
#include <fscc.h> /* fscc_connect, fscc_disconnect, fscc_handle
                     fscc_write, fscc_read */

int main(void)
{
	HANDLE h;
	char odata[] = "Hello world!";
	char idata[20] = {0};
	DWORD tmp;

	/* Open port 0 in a blocking IO mode */
	h = CreateFile("\\\\.\\FSCC0", GENERIC_READ | GENERIC_WRITE, 0, NULL, 
                   OPEN_EXISTING, 0, NULL);

	/* Send "Hello world!" text */
	WriteFile(h, odata, sizeof(odata), &tmp, NULL);

	/* Read the data back in (with our loopback connector) */
	ReadFile(h, idata, sizeof(idata), &tmp, NULL);

	fprintf(stdout, "%s\n", idata);

	CloseHandle(h);

	return EXIT_SUCCESS;
}
