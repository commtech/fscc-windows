/*! \file */ 

#ifndef FSCC_H
#define FSCC_H

#ifdef __cplusplus
extern "C" 
{
#endif

#include <Windows.h>

#define FSCC_IOCTL_MAGIC 0x8018

#define FSCC_GET_REGISTERS CTL_CODE(FSCC_IOCTL_MAGIC, 0x800, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define FSCC_SET_REGISTERS CTL_CODE(FSCC_IOCTL_MAGIC, 0x801, METHOD_BUFFERED, FILE_ANY_ACCESS)

#define FSCC_PURGE_TX CTL_CODE(FSCC_IOCTL_MAGIC, 0x802, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define FSCC_PURGE_RX CTL_CODE(FSCC_IOCTL_MAGIC, 0x803, METHOD_BUFFERED, FILE_ANY_ACCESS)

#define FSCC_ENABLE_APPEND_STATUS CTL_CODE(FSCC_IOCTL_MAGIC, 0x804, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define FSCC_DISABLE_APPEND_STATUS CTL_CODE(FSCC_IOCTL_MAGIC, 0x805, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define FSCC_GET_APPEND_STATUS CTL_CODE(FSCC_IOCTL_MAGIC, 0x80D, METHOD_BUFFERED, FILE_ANY_ACCESS)

#define FSCC_SET_MEMORY_CAP CTL_CODE(FSCC_IOCTL_MAGIC, 0x806, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define FSCC_GET_MEMORY_CAP CTL_CODE(FSCC_IOCTL_MAGIC, 0x807, METHOD_BUFFERED, FILE_ANY_ACCESS)

#define FSCC_SET_CLOCK_BITS CTL_CODE(FSCC_IOCTL_MAGIC, 0x808, METHOD_BUFFERED, FILE_ANY_ACCESS)

#define FSCC_ENABLE_IGNORE_TIMEOUT CTL_CODE(FSCC_IOCTL_MAGIC, 0x80A, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define FSCC_DISABLE_IGNORE_TIMEOUT CTL_CODE(FSCC_IOCTL_MAGIC, 0x80B, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define FSCC_GET_IGNORE_TIMEOUT CTL_CODE(FSCC_IOCTL_MAGIC, 0x80F, METHOD_BUFFERED, FILE_ANY_ACCESS)

#define FSCC_SET_TX_MODIFIERS CTL_CODE(FSCC_IOCTL_MAGIC, 0x80C, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define FSCC_GET_TX_MODIFIERS CTL_CODE(FSCC_IOCTL_MAGIC, 0x80E, METHOD_BUFFERED, FILE_ANY_ACCESS)

enum transmit_modifiers { XF=0, XREP=1, TXT=2, TXEXT=4 };
		
typedef INT64 fscc_register;

struct fscc_registers {
	/* BAR 0 */
	fscc_register __reserved1[2];
	
	fscc_register FIFOT;
	
	fscc_register __reserved2[2];
	
	fscc_register CMDR;
	fscc_register STAR; /* Read-only */
	fscc_register CCR0;
	fscc_register CCR1;
	fscc_register CCR2;
	fscc_register BGR;
	fscc_register SSR;
	fscc_register SMR;
	fscc_register TSR;
	fscc_register TMR;
	fscc_register RAR;
	fscc_register RAMR;
	fscc_register PPR;
	fscc_register TCR;
	fscc_register VSTR; /* Read-only */
	
	fscc_register __reserved4[1];
	
	fscc_register IMR;
	fscc_register DPLLR;

	/* BAR 2 */
	fscc_register FCR;
};

struct fscc_memory_cap {
    int input;
    int output;
};

/******************************************************************************/
/*!

  Initializes an fscc_registers structure for use in other functions.

  \def FSCC_REGISTERS_INIT(registers)

*/
/******************************************************************************/
#define FSCC_REGISTERS_INIT(registers) memset(&registers, -1, sizeof(registers))

#define FSCC_UPDATE_VALUE -2

__declspec(dllexport) int fscc_connect(unsigned port_num, BOOL overlapped, HANDLE *h);
__declspec(dllexport) int fscc_set_tx_modifiers(HANDLE h, unsigned modifiers);
__declspec(dllexport) int fscc_get_tx_modifiers(HANDLE h, unsigned *modifiers);
__declspec(dllexport) int fscc_set_memory_cap(HANDLE h, const struct fscc_memory_cap *memcap);
__declspec(dllexport) int fscc_get_memory_cap(HANDLE h, struct fscc_memory_cap *memcap);
__declspec(dllexport) int fscc_set_registers(HANDLE h, const struct fscc_registers *regs);
__declspec(dllexport) int fscc_get_registers(HANDLE h, struct fscc_registers *regs);
__declspec(dllexport) int fscc_get_append_status(HANDLE h, BOOL *status);
__declspec(dllexport) int fscc_enable_append_status(HANDLE h);
__declspec(dllexport) int fscc_disable_append_status(HANDLE h);
__declspec(dllexport) int fscc_get_ignore_timeout(HANDLE h, BOOL *status);
__declspec(dllexport) int fscc_enable_ignore_timeout(HANDLE h);
__declspec(dllexport) int fscc_disable_ignore_timeout(HANDLE h);
__declspec(dllexport) int fscc_purge(HANDLE h, BOOL tx, BOOL rx);
__declspec(dllexport) int fscc_write(HANDLE h, char *buf, unsigned size, unsigned *bytes_written, OVERLAPPED *o);
__declspec(dllexport) int fscc_read(HANDLE h, char *buf, unsigned size, unsigned *bytes_read, OVERLAPPED *o);
__declspec(dllexport) int fscc_read_with_timeout(HANDLE h, char *buf, unsigned size, unsigned *bytes_read, unsigned timeout);
__declspec(dllexport) int fscc_disconnect(HANDLE h);
__declspec(dllexport) int fscc_set_clock_frequency(HANDLE h, unsigned frequency, unsigned ppm);

#ifdef __cplusplus
}
#endif

#endif