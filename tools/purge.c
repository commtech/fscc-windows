#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <Windows.h>
#include <fscc.h>

int main(int argc, char *argv[])
{
    HANDLE h = 0;
    DWORD tmp, tx_return, rx_return;
    unsigned char handle_string[20];
	
	if(argc != 2)
	{
		printf("Usage: %s [port number]\n", argv[0]);
		return EXIT_FAILURE;
	}

	sprintf(handle_string, "\\\\.\\FSCC%d", atoi(argv[1]));
    h = CreateFile(handle_string, GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL);
    if (h == INVALID_HANDLE_VALUE) 
	{
        printf("%s: CreateFile failed with %d\n", handle_string, GetLastError());
        return EXIT_FAILURE;
    }
	
	// It's a good idea to purge the transmit and receive FIFOs before beginning any new operations.
	tx_return = DeviceIoControl(h, FSCC_PURGE_TX, NULL, 0, NULL, 0, &tmp, (LPOVERLAPPED)NULL);
	rx_return = DeviceIoControl(h, FSCC_PURGE_RX, NULL, 0, NULL, 0, &tmp, (LPOVERLAPPED)NULL);
	printf("PURGE_TX return value: %d\n", tx_return);
	printf("PURGE_RX return value: %d\n", rx_return);
    CloseHandle(h);
    return EXIT_SUCCESS;
}