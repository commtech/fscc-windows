/*
    Copyright (C) 2018  Commtech, Inc.

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


#include "driver.h"
#include "port.h"
#include "utils.h"
#include "debug.h"

#if defined(EVENT_TRACING)
#include "driver.tmh"
#endif


#ifdef ALLOC_PRAGMA
#pragma alloc_text (INIT, DriverEntry)
#pragma alloc_text (PAGE, FSCCEvtDeviceAdd)
#pragma alloc_text (PAGE, FSCCEvtDriverContextCleanup)
#endif

/******************************************************************************/

NTSTATUS
DriverEntry(PDRIVER_OBJECT  DriverObject, PUNICODE_STRING RegistryPath)
/*++

Routine Description:
    DriverEntry initializes the driver and is the first routine called by the
    system after the driver is loaded. DriverEntry specifies the other entry
    points in the function driver, such as EvtDevice and DriverUnload.

Parameters Description:

    DriverObject - represents the instance of the function driver that is loaded
    into memory. DriverEntry must initialize members of DriverObject before it
    returns to the caller. DriverObject is allocated by the system before the
    driver is loaded, and it is released by the system after the system unloads
    the function driver from memory.

    RegistryPath - represents the driver specific path in the Registry.
    The function driver can use the path to store driver related data between
    reboots. The path does not store hardware instance specific data.

Return Value:

    STATUS_SUCCESS if successful,
    STATUS_UNSUCCESSFUL otherwise.

--*/
{
    WDF_DRIVER_CONFIG config;
    NTSTATUS status;
    WDF_OBJECT_ATTRIBUTES attributes;

    //
    // Initialize WPP Tracing
    //
    WPP_INIT_TRACING(DriverObject, RegistryPath);

    //
    // Register a cleanup callback so that we can call WPP_CLEANUP when
    // the framework driver object is deleted during driver unload.
    //
    WDF_OBJECT_ATTRIBUTES_INIT(&attributes);
    attributes.EvtCleanupCallback = FSCCEvtDriverContextCleanup;

    WDF_DRIVER_CONFIG_INIT(&config, FSCCEvtDeviceAdd);

    status = WdfDriverCreate(DriverObject, RegistryPath, &attributes, &config,
                             WDF_NO_HANDLE);

    if (!NT_SUCCESS(status)) {
        TraceEvents(TRACE_LEVEL_ERROR, TRACE_DRIVER,
                    "WdfDriverCreate failed %!STATUS!", status);

        WPP_CLEANUP(DriverObject);
        return status;
    }

    return status;
}

NTSTATUS
FSCCEvtDeviceAdd(WDFDRIVER Driver, PWDFDEVICE_INIT DeviceInit)
/*++
Routine Description:

    EvtDeviceAdd is called by the framework in response to AddDevice
    call from the PnP manager. We create and initialize a device object to
    represent a new instance of the device.

Arguments:

    Driver - Handle to a framework driver object created in DriverEntry

    DeviceInit - Pointer to a framework-allocated WDFDEVICE_INIT structure.

Return Value:

    NTSTATUS

--*/
{
    struct fscc_port *port = 0;

    PAGED_CODE();

    port = fscc_port_new(Driver, DeviceInit);

    if (!port)
        return STATUS_INTERNAL_ERROR;

    return STATUS_SUCCESS;
}

VOID
FSCCEvtDriverContextCleanup(WDFOBJECT DriverObject)
/*++
Routine Description:

    Free all the resources allocated in DriverEntry.

Arguments:

    DriverObject - handle to a WDF Driver object.

Return Value:

    VOID.

--*/
{
    UNREFERENCED_PARAMETER(DriverObject);

    PAGED_CODE ();

    //
    // Stop WPP Tracing
    //
    WPP_CLEANUP(WdfDriverWdmGetDriverObject(DriverObject));
}

NTSTATUS fscc_driver_get_last_port_num(WDFDRIVER driver, int *port_num)
{
    NTSTATUS status;
    WDFKEY driverkey;
    UNICODE_STRING key_str;
    ULONG port_num_long;

    RtlInitUnicodeString(&key_str, L"LastPortNumber");

    status = WdfDriverOpenParametersRegistryKey(driver, STANDARD_RIGHTS_ALL,
                                    WDF_NO_OBJECT_ATTRIBUTES, &driverkey);
    if (!NT_SUCCESS(status)) {
        TraceEvents(TRACE_LEVEL_ERROR, TRACE_DEVICE,
            "WdfDeviceOpenRegistryKey failed %!STATUS!", status);
        return status;
    }

    status = WdfRegistryQueryULong(driverkey, &key_str, &port_num_long);
    if (!NT_SUCCESS(status)) {
        TraceEvents(TRACE_LEVEL_ERROR, TRACE_DEVICE,
            "WdfRegistryQueryULong failed %!STATUS!", status);
        return status;
    }

    *port_num = (int)port_num_long;

    WdfRegistryClose(driverkey);

    return status;
}

NTSTATUS fscc_driver_set_last_port_num(WDFDRIVER driver, int value)
{
    NTSTATUS status;
    WDFKEY driverkey;
    UNICODE_STRING key_str;

    RtlInitUnicodeString(&key_str, L"LastPortNumber");

    status = WdfDriverOpenParametersRegistryKey(driver, STANDARD_RIGHTS_ALL,
                                    WDF_NO_OBJECT_ATTRIBUTES, &driverkey);
    if (!NT_SUCCESS(status)) {
        TraceEvents(TRACE_LEVEL_ERROR, TRACE_DEVICE,
            "WdfDeviceOpenRegistryKey failed %!STATUS!", status);
        return status;
    }

    status = WdfRegistryAssignULong(driverkey, &key_str, value);
    if (!NT_SUCCESS(status)) {
        TraceEvents(TRACE_LEVEL_ERROR, TRACE_DEVICE,
            "WdfRegistryAssignULong failed %!STATUS!", status);
        return status;
    }

    WdfRegistryClose(driverkey);

    return status;
}