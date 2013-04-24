/*
    Copyright (C) 2013  Commtech, Inc.

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


#define INITGUID

#include <ntddk.h>
#include <wdf.h>

#include "Trace.h"

//
// WDFDRIVER Events
//

DRIVER_INITIALIZE DriverEntry;
EVT_WDF_DRIVER_DEVICE_ADD FSCCEvtDeviceAdd;
EVT_WDF_OBJECT_CONTEXT_CLEANUP FSCCEvtDriverContextCleanup;

NTSTATUS fscc_driver_get_last_port_num(WDFDRIVER driver, int *port_num);
NTSTATUS fscc_driver_set_last_port_num(WDFDRIVER driver, int port_num);