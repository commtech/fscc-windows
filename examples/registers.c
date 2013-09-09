#include <fscc.h>

int main(void)
{
    HANDLE h = 0;
    DWORD tmp;
    struct fscc_registers regs;

    h = CreateFile("\\\\.\\FSCC0", GENERIC_READ | GENERIC_WRITE, 0, NULL, 
                      OPEN_EXISTING, 0, NULL);
    
    FSCC_REGISTERS_INIT(regs);

    /* Change the CCR0 and BGR elements to our desired values */
    regs.CCR0 = 0x0011201c;
    regs.BGR = 10;
    
    /* Set the CCR0 and BGR register values */
    DeviceIoControl(h, FSCC_SET_REGISTERS, 
                    &regs, sizeof(regs), 
                    NULL, 0, 
                    &tmp, (LPOVERLAPPED)NULL);
    
    /* Re-initialize our registers structure */
    FSCC_REGISTERS_INIT(regs);

    /* Mark the CCR0 and CCR1 elements to retrieve values */
    regs.CCR1 = FSCC_UPDATE_VALUE;
    regs.CCR2 = FSCC_UPDATE_VALUE;
    
    /* Get the CCR1 and CCR2 register values */
    DeviceIoControl(h, FSCC_GET_REGISTERS, 
                    &regs, sizeof(regs), 
                    &regs, sizeof(regs), 
                    &tmp, (LPOVERLAPPED)NULL);

    CloseHandle(h);	
    
    return 0;
}