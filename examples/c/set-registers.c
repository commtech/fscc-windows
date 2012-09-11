#include <stdlib.h> /* EXIT_SUCCESS, EXIT_FAILURE */
#include <windows.h> /* CreateFile, DeviceIoControl, CloseHandle */
#include "../../include/fscc.h" /* FSCC_REGISTERS_INIT, FSCC_GET_REGISTERS */
#include "utils.h" /* DisplayError */

int main(void)
{
	HANDLE port = 0;
	DWORD junk;
	struct fscc_registers regs;
	
	port = CreateFile("\\\\.\\FSCC0", GENERIC_READ | GENERIC_WRITE, 0, NULL, 
	                  OPEN_EXISTING, 0, NULL);
 
	if (port == INVALID_HANDLE_VALUE) { 
		DisplayError("CreateFile");
		  
		return EXIT_FAILURE; 
	}

	FSCC_REGISTERS_INIT(regs);

	regs.CCR0 = 0x00000000;
	regs.CCR1 = 0x00000000;
	regs.CCR2 = 0x00000000;
	
	DeviceIoControl(port, FSCC_SET_REGISTERS, NULL, 0, &regs, sizeof(regs), &junk, (LPOVERLAPPED)NULL);

	CloseHandle(port);
	
	return EXIT_SUCCESS;
}

