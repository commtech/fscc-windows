#include <stdio.h> /* fprintf */
#include <stdlib.h> /* EXIT_SUCCESS, EXIT_FAILURE */
#include <windows.h> /* CreateFile, DeviceIoControl, CloseHandle */
#include "../../include/fscc.h" /* FSCC_REGISTERS_INIT, FSCC_UPDATE_VALUE, FSCC_GET_REGISTERS */
#include "utils.h" /* DisplayError */

int main(void)
{
	HANDLE port = 0;
	DWORD tmp;
	struct fscc_registers regs;
	struct fscc_memory_cap memcap;
	unsigned append_status;

	port = CreateFile("\\\\.\\FSCC0", GENERIC_READ | GENERIC_WRITE, 0, NULL, 
	                  OPEN_EXISTING, 0, NULL);
	
	if (port == INVALID_HANDLE_VALUE) { 
		DisplayError("CreateFile");
		  
		return EXIT_FAILURE; 
	}
	
	FSCC_REGISTERS_INIT(regs);

	regs.CCR0 = FSCC_UPDATE_VALUE;
	regs.CCR1 = FSCC_UPDATE_VALUE;
	regs.CCR2 = FSCC_UPDATE_VALUE;
	
	DeviceIoControl(port, FSCC_GET_REGISTERS, &regs, sizeof(regs), &regs, sizeof(regs), &tmp, (LPOVERLAPPED)NULL);
	
	fprintf(stdout, "CCR0 = 0x%08x\n", (unsigned)regs.CCR0);
	fprintf(stdout, "CCR1 = 0x%08x\n", (unsigned)regs.CCR1);
	fprintf(stdout, "CCR2 = 0x%08x\n", (unsigned)regs.CCR2);

	CloseHandle(port);	
	
	return EXIT_SUCCESS;
}