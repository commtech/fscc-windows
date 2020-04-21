#include <fscc.h>

int main(void)
{
    HANDLE h = 0;
    DWORD tmp;
    unsigned status = 0;

    h = CreateFile("\\\\.\\FSCC0", GENERIC_READ | GENERIC_WRITE, 0, NULL,
                   OPEN_EXISTING, 0, NULL);

    DeviceIoControl(h, FSCC_GET_FORCE_FIFO,
                    NULL, 0,
                    &status, sizeof(status),
                    &tmp, (LPOVERLAPPED)NULL);

    DeviceIoControl(h, FSCC_ENABLE_FORCE_FIFO,
                    NULL, 0,
                    NULL, 0,
                    &tmp, (LPOVERLAPPED)NULL);

    DeviceIoControl(h, FSCC_DISABLE_FORCE_FIFO,
                    NULL, 0,
                    NULL, 0,
                    &tmp, (LPOVERLAPPED)NULL);

    CloseHandle(h);

    return 0;
}