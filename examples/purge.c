#include <fscc.h>

int main(void)
{
    HANDLE h = 0;
    DWORD tmp;

    h = CreateFile("\\\\.\\FSCC0", GENERIC_READ | GENERIC_WRITE, 0, NULL,
                   OPEN_EXISTING, 0, NULL);

    DeviceIoControl(h, FSCC_PURGE_TX,
                    NULL, 0,
                    NULL, 0,
                    &tmp, (LPOVERLAPPED)NULL);

    DeviceIoControl(h, FSCC_PURGE_RX,
                    NULL, 0,
                    NULL, 0,
                    &tmp, (LPOVERLAPPED)NULL);

    CloseHandle(h);

    return EXIT_SUCCESS;
}

