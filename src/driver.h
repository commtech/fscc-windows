/*++

Module Name:

    driver.h

Abstract:

    This file contains the driver definitions.

Environment:

    Kernel-mode Driver Framework

--*/

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