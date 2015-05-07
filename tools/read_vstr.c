#include <stdio.h>
#include <windows.h>
#include <time.h>
#include <fscc.h>

int main(int argc, char *argv[])
{
    HANDLE h = 0;
    DWORD tmp, reg_return;
    unsigned char handle_string[20];
	struct fscc_registers regs;

    if(argc != 2)
	{
		printf("Usage: %s [port number]\n", argv[0]);
		return EXIT_FAILURE;
	}

	sprintf(handle_string, "\\\\.\\FSCC%d", atoi(argv[1]));
    h = CreateFile(handle_string, GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL);
    if (h == INVALID_HANDLE_VALUE) 
	{
        fprintf(stderr, "%s: CreateFile failed with %d\n", handle_string, GetLastError());
        return EXIT_FAILURE;
    }
	
    FSCC_REGISTERS_INIT(regs);
	regs.VSTR = FSCC_UPDATE_VALUE;
	reg_return = DeviceIoControl(h, FSCC_GET_REGISTERS, &regs, sizeof(regs), &regs, sizeof(regs), &tmp, (LPOVERLAPPED)NULL);
	printf("VSTR value: 0x%8.8x\n", regs.VSTR);
    CloseHandle(h);
	
    return EXIT_SUCCESS;
}