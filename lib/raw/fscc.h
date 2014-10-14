/*
    Copyright (C) 2014 Commtech, Inc.

    This file is part of fscc-windows.

    fscc-windows is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    fscc-windows is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with fscc-windows.  If not, see <http://www.gnu.org/licenses/>.

*/

#ifndef FSCC_H
#define FSCC_H

#ifdef __cplusplus
extern "C"
{
#endif

#include <Windows.h>
#include <string.h>

#define FSCC_REGISTERS_INIT(regs) memset(&regs, -1, sizeof(regs))
#define FSCC_MEMORY_CAP_INIT(memcap) memset(&memcap, -1, sizeof(memcap))
#define FSCC_UPDATE_VALUE -2

enum transmit_type { XF=0, XREP=1, TXT=2, TXEXT=4 };

typedef INT64 fscc_register;

struct fscc_registers {
    /* BAR 0 */
    fscc_register reserved1[2];

    fscc_register FIFOT;

    fscc_register reserved2[2];

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

    fscc_register reserved3[1];

    fscc_register IMR;
    fscc_register DPLLR;

    /* BAR 2 */
    fscc_register FCR;
};

struct fscc_memory_cap {
    int input;
    int output;
};

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

#define FSCC_ENABLE_RX_MULTIPLE CTL_CODE(FSCC_IOCTL_MAGIC, 0x810, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define FSCC_DISABLE_RX_MULTIPLE CTL_CODE(FSCC_IOCTL_MAGIC, 0x811, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define FSCC_GET_RX_MULTIPLE CTL_CODE(FSCC_IOCTL_MAGIC, 0x812, METHOD_BUFFERED, FILE_ANY_ACCESS)

#define FSCC_ENABLE_APPEND_TIMESTAMP CTL_CODE(FSCC_IOCTL_MAGIC, 0x813, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define FSCC_DISABLE_APPEND_TIMESTAMP CTL_CODE(FSCC_IOCTL_MAGIC, 0x814, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define FSCC_GET_APPEND_TIMESTAMP CTL_CODE(FSCC_IOCTL_MAGIC, 0x815, METHOD_BUFFERED, FILE_ANY_ACCESS)

#define FSCC_ENABLE_WAIT_ON_WRITE CTL_CODE(FSCC_IOCTL_MAGIC, 0x816, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define FSCC_DISABLE_WAIT_ON_WRITE CTL_CODE(FSCC_IOCTL_MAGIC, 0x817, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define FSCC_GET_WAIT_ON_WRITE CTL_CODE(FSCC_IOCTL_MAGIC, 0x818, METHOD_BUFFERED, FILE_ANY_ACCESS)

#define FSCC_TRACK_INTERRUPTS CTL_CODE(FSCC_IOCTL_MAGIC, 0x819, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define FSCC_GET_MEM_USAGE CTL_CODE(FSCC_IOCTL_MAGIC, 0x81A, METHOD_BUFFERED, FILE_ANY_ACCESS)

#ifdef __cplusplus
}
#endif

#endif
