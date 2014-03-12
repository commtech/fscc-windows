#include <fscc.h>

/*
    This is a simple example showing how to change the transmit type for each
    port.

    Valid TX_MODIFIERS are:
    ---------------------------------------------------------------------------
    XF - Normal transmit - disable modifiers
    XREP - Transmit repeat
    TXT - Transmit on timer
    TXEXT - Transmit on external signal

*/

int main(void)
{
    HANDLE h = 0;
    unsigned modifiers;
    DWORD tmp;

    h = CreateFile("\\\\.\\FSCC0", GENERIC_READ | GENERIC_WRITE, 0, NULL,
                   OPEN_EXISTING, 0, NULL);

    /* Enable transmit repeat & transmit on timer */
    DeviceIoControl(h, FSCC_GET_TX_MODIFIERS,
                    NULL, 0,
                    &modifiers, sizeof(modifiers),
                    &tmp, (LPOVERLAPPED)NULL);

    /* Enable transmit repeat & transmit on timer */
    modifiers = TXT | XREP;
    DeviceIoControl(h, FSCC_SET_TX_MODIFIERS,
                    &modifiers, sizeof(modifiers),
                    NULL, 0,
                    &tmp, (LPOVERLAPPED)NULL);

    /* Revert to normal transmission */
    modifiers = XF;
    DeviceIoControl(h, FSCC_SET_TX_MODIFIERS,
                    &modifiers, sizeof(modifiers),
                    NULL, 0,
                    &tmp, (LPOVERLAPPED)NULL); /* disable modifiers */

    CloseHandle(h);

    return 0;
}

