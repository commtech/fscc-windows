#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <Windows.h>
#include <fscc.h>

int main(int argc, char *argv[])
{
    HANDLE h = 0;
    DWORD size_read, size_written, tmp;
    unsigned char odata[4096], idata[4096], handle_string[20];
	size_t i, j, num_loops;
	int failures = 0, total_failures = 0;
	size_t max_size = (size_t)-1;
	
	if(argc != 3)
	{
		printf("Usage: %s [port number] [number of loops]\n", argv[0]);
		return EXIT_FAILURE;
	}
	num_loops = atoi(argv[2]);
	if(num_loops < 1) num_loops = 1;
	if(num_loops > max_size)
	{
		printf("Maximum loops = %d!\nChanging number of loops to %d.\n", max_size, max_size);
		num_loops = max_size;
	}
	sprintf(handle_string, "\\\\.\\FSCC%d", atoi(argv[1]));
    h = CreateFile(handle_string, GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL);
    if (h == INVALID_HANDLE_VALUE) 
	{
        fprintf(stderr, "%s: CreateFile failed with %d\n", handle_string, GetLastError());
        return EXIT_FAILURE;
    }
	
	// It's a good idea to purge the transmit and receive FIFOs before beginning any new operations.
	DeviceIoControl(h, FSCC_PURGE_TX, NULL, 0, NULL, 0, &tmp, (LPOVERLAPPED)NULL);
	DeviceIoControl(h, FSCC_PURGE_RX, NULL, 0, NULL, 0, &tmp, (LPOVERLAPPED)NULL);
	
	// Time to do a loop-test.
    for(i = 0; i < num_loops; i++)
	{
		failures = 0;
		printf("Loop: %d\r", i+1);
		// Generate random data to transmit.
		for(j = 0; j < sizeof(odata); j++) odata[j] = rand()%255;
		// Clear the received data buffer.
		memset(idata, 0, sizeof(idata));
		// Attempt to write.
		WriteFile(h, odata, sizeof(odata), &size_written, NULL);
		// Make sure we actually write something.
		if(size_written < 1) 
		{
			// If we're getting a return 0 on our write, something has gone wrong. Better purge!
			printf("\nLoop %d: size_written == %d", i, size_written);
			total_failures += 1;
			DeviceIoControl(h, FSCC_PURGE_TX, NULL, 0, NULL, 0, &tmp, (LPOVERLAPPED)NULL);
			DeviceIoControl(h, FSCC_PURGE_RX, NULL, 0, NULL, 0, &tmp, (LPOVERLAPPED)NULL);
			continue;
		}
		// Attempt to read.
		ReadFile(h, idata, sizeof(idata), &size_read, NULL);
		// Make sure we actually read something.
		if(size_read < 1) 
		{
			// If we're getting a return 0 on our read, something has gone wrong. Better purge!
			printf("\nLoop %d: size_read == %d", i, size_read);
			total_failures += 1;
			DeviceIoControl(h, FSCC_PURGE_TX, NULL, 0, NULL, 0, &tmp, (LPOVERLAPPED)NULL);
			DeviceIoControl(h, FSCC_PURGE_RX, NULL, 0, NULL, 0, &tmp, (LPOVERLAPPED)NULL);
			continue;
		}
		if(size_read != size_written)
		{
			// Size mismatch? This is unlikely. I would verify that append_status is disabled.
			printf("\nSize mismatch! Wrote %d bytes, read %d bytes.", size_written, size_read);
			total_failures += 1;
			continue;
		}
		// Verify that we read the same data we wrote.
		for(j = 0; j < size_read; j++)
			if(idata[j] != odata[j])
			{
				// Only printing 5 data mismatches - don't want to flood the screen TOO much.
				printf("\nLoop %d: Data mismatch - tx_data[%4.4d]: 0x%2.2x != rx_data[%4.4d]: 0x%2.2x", i, j, odata[j], j, idata[j]); 
				failures++;
				if(failures > 4) break;
			}
		total_failures += failures;
		if(failures) printf("\n");
	}
	printf("Loops %d: total failures: %d\n", num_loops, total_failures);
    CloseHandle(h);
    return EXIT_SUCCESS;
}