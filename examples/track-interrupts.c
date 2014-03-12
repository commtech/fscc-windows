#include <fscc.h>

int main(void)
{
    HANDLE h = 0;
    DWORD tmp;
    unsigned interrupts;
    unsigned matches;
    OVERLAPPED ol;

    memset(&ol, 0, sizeof(ol));

    h = CreateFile("\\\\.\\FSCC0", GENERIC_READ | GENERIC_WRITE, 0, NULL,
                   OPEN_EXISTING, 0, NULL);

    /* TIN interrupt */
    interrupts = 0x00000400;
    DeviceIoControl(h, FSCC_TRACK_INTERRUPTS,
                    &interrupts, sizeof(interrupts),
                    &matches, sizeof(matches),
                    &tmp, &ol);

    /* Use OVERLAPPED IO structure to monitor interrupt */

    CloseHandle(h);

    return 0;
}