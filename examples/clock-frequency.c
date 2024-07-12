#include <fscc.h>
#include "calculate-clock-bits.h"

int main(void)
{
    HANDLE h = 0;
    DWORD tmp;
	clock_data_fscc clock_data;

    h = CreateFile("\\\\.\\FSCC0", GENERIC_READ | GENERIC_WRITE, 0, NULL,
                   OPEN_EXISTING, 0, NULL);

	clock_data.frequency = 18432000;
	calculate_clock_bits_fscc(&clock_data, 10);
	DeviceIoControl(h, FSCC_SET_CLOCK_BITS,
			&clock_data, sizeof(clock_data),
			NULL, 0,
			&tmp, (LPOVERLAPPED)NULL);


    CloseHandle(h);

    return 0;
}
