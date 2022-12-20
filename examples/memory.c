#include <fscc.h>
#include <stdio.h>

int main(void)
{
	/*
    HANDLE h = 0;
    DWORD tmp;
    struct fscc_memory memory;

    h = CreateFile("\\\\.\\FSCC0", GENERIC_READ | GENERIC_WRITE, 0, NULL,
                   OPEN_EXISTING, 0, NULL);

	FSCC_MEMORY_INIT(memory);
    memory.rx_size = 256; 
	memory.rx_num = 200; 
	memory.tx_size = 256; 
	memory.tx_num = 200; 

    DeviceIoControl(h, FSCC_SET_MEMORY,
                    &memory, sizeof(memory),
                    NULL, 0,
                    &tmp, (LPOVERLAPPED)NULL);
	DeviceIoControl(h, FSCC_GET_MEMORY,
                    NULL, 0,
                    &memory, sizeof(memory),
                    &tmp, (LPOVERLAPPED)NULL);

    CloseHandle(h);
	*/
	printf("Not yet implemented!\n");
    return 0;
}

