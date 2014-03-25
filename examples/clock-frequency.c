#include <fscc.h>
#include "calculate-clock-bits.h"

int main(void)
{
    HANDLE h = 0;
    DWORD tmp;
    unsigned char clock_bits[20];

    h = CreateFile("\\\\.\\FSCC0", GENERIC_READ | GENERIC_WRITE, 0, NULL,
                   OPEN_EXISTING, 0, NULL);

    /* 18.432 MHz */
    calculate_clock_bits(18432000, 10, clock_bits);

    DeviceIoControl(h, FSCC_SET_CLOCK_BITS,
                    &clock_bits, sizeof(clock_bits),
                    NULL, 0,
                    &tmp, (LPOVERLAPPED)NULL);

    CloseHandle(h);

    return 0;
}
