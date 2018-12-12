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
	regs.FIFOT = 0x08001000;
	regs.CCR0  = 0x0011201c;
	regs.CCR1  = 0x00000018;
	regs.CCR2  = 0x00000000;
	regs.BGR   = 0x00000000;
	regs.SSR   = 0x0000007e;
	regs.SMR   = 0x00000000;
	regs.TSR   = 0x0000007e;
	regs.TMR   = 0x00000000;
	regs.RAR   = 0x00000000;
	regs.RAMR  = 0x00000000;
	regs.PPR   = 0x00000000;
	regs.TCR   = 0x00000000;
	regs.IMR   = 0x0f000000;
	regs.DPLLR = 0x00000004;
	regs.FCR   = 0x00000000;
	reg_return = DeviceIoControl(h, FSCC_SET_REGISTERS, &regs, sizeof(regs), NULL, 0, &tmp, (LPOVERLAPPED)NULL);
	printf("SET_REGISTERS return value: %d\n", reg_return);
	DeviceIoControl(h, FSCC_DISABLE_APPEND_STATUS, NULL, 0, NULL, 0, &tmp, (LPOVERLAPPED)NULL);
	regs.CCR0  = 0x00112004;
	reg_return = DeviceIoControl(h, FSCC_SET_REGISTERS, &regs, sizeof(regs), NULL, 0, &tmp, (LPOVERLAPPED)NULL);
	printf("SET_REGISTERS return value: %d\n", reg_return);
	
    CloseHandle(h);
    return EXIT_SUCCESS;
}