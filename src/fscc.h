/*
    Copyright (C) 2014  Commtech, Inc.

    This file is part of fscc-windows.

    fscc-windows is free software: you can redistribute it and/or modify it
    under the terms of the GNU General Public License as published bythe Free
    Software Foundation, either version 3 of the License, or (at your option)
    any later version.

    fscc-windows is distributed in the hope that it will be useful, but WITHOUT
    ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
    FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
    more details.

    You should have received a copy of the GNU General Public License along
    with fscc-windows.  If not, see <http://www.gnu.org/licenses/>.

*/


#ifndef FSCC_H
#define FSCC_H

#include <ntddk.h>
#include <wdf.h>

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

enum transmit_modifiers { XF=0, XREP=1, TXT=2, TXEXT=4 };

DRIVER_INITIALIZE DriverEntry;
EVT_WDF_DRIVER_UNLOAD  DriverUnload;

EVT_WDF_DRIVER_DEVICE_ADD FsccDeviceAdd;
EVT_WDF_OBJECT_CONTEXT_CLEANUP FsccDeviceRemove;
EVT_WDF_DEVICE_PREPARE_HARDWARE FsccDevicePrepareHardware;
EVT_WDF_DEVICE_RELEASE_HARDWARE FsccDeviceReleaseHardware;
EVT_WDF_DEVICE_D0_ENTRY FsccDeviceD0Entry;
EVT_WDF_DEVICE_D0_EXIT FsccDeviceD0Exit;
EVT_WDF_IO_QUEUE_IO_DEFAULT FsccIoDefault;
EVT_WDF_IO_QUEUE_IO_READ FsccIoRead;
EVT_WDF_IO_QUEUE_IO_WRITE FsccIoWrite;
EVT_WDF_IO_QUEUE_IO_DEVICE_CONTROL FsccIoDeviceControl;



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

#define FSCC_REGISTERS_INIT(registers) memset(&registers, -1, sizeof(registers))
#define FSCC_UPDATE_VALUE -2

#define FSCC_ID 0x000f
#define SFSCC_ID 0x0014
#define SFSCC_104_LVDS_ID 0x0015
#define FSCC_232_ID 0x0016
#define SFSCC_104_UA_ID 0x0017
#define SFSCC_4_UA_ID 0x0018
#define SFSCC_UA_ID 0x0019
#define SFSCC_LVDS_ID 0x001a
#define FSCC_4_UA_ID 0x001b
#define SFSCC_4_LVDS_ID 0x001c
#define FSCC_UA_ID 0x001d
#define SFSCCe_4_ID 0x001e
#define SFSCC_4_CPCI_ID 0x001f
#define SFSCCe_4_LVDS_UA_ID 0x0022
#define SFSCC_4_UA_CPCI_ID 0x0023
#define SFSCC_4_UA_LVDS_ID 0x0025
#define SFSCC_UA_LVDS_ID 0x0026
#define FSCCe_4_UA_ID 0x0027

#define STATUS_LENGTH 2

#endif
