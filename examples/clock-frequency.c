#include <fscc.h>

int main(void)
{
    HANDLE h = 0;
    DWORD tmp;
    char clock_bits[20] = {0xff, 0xff, 0xff, 0x01, 0x84, 0x01, 0x48, 0x72,
                           0x9a, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                           0x00, 0x04, 0xe0, 0x01};
    
    h = CreateFile("\\\\.\\FSCC0", GENERIC_READ | GENERIC_WRITE, 0, NULL, 
                      OPEN_EXISTING, 0, NULL);
    
    DeviceIoControl(h, FSCC_SET_CLOCK_BITS, 
                    &clock_bits, sizeof(clock_bits), 
                    NULL, 0, 
                    &tmp, (LPOVERLAPPED)NULL);

    CloseHandle(h);
    
    return 0;
}
