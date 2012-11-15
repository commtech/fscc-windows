/*++

Module Name:

    driver.c

Abstract:

    This file contains the driver entry points and callbacks.

Environment:

    Kernel-mode Driver Framework

--*/

#include "driver.h"
#include "driver.tmh"

#include "card.h"
#include "utils.h"

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
	
    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DRIVER, 
                "%!FUNC! DriverObject 0x%p, RegistryPath 0x%p", 
				DriverObject, RegistryPath);

    //
    // Register a cleanup callback so that we can call WPP_CLEANUP when
    // the framework driver object is deleted during driver unload.
    //
    WDF_OBJECT_ATTRIBUTES_INIT(&attributes);
    attributes.EvtCleanupCallback = FSCCEvtDriverContextCleanup;

    WDF_DRIVER_CONFIG_INIT(&config, FSCCEvtDeviceAdd);

    status = WdfDriverCreate(DriverObject, RegistryPath, &attributes, &config, WDF_NO_HANDLE);
	
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
	struct fscc_card *card = 0;

    PAGED_CODE();
	
	card = fscc_card_new(Driver, DeviceInit);

	if (!card)
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

NTSTATUS fscc_driver_registry_get_portnum(WDFDRIVER driver, unsigned *port_num)
{
	NTSTATUS status;
	WDFKEY driver_key;
	ULONG value = *port_num;
	UNICODE_STRING key_str;

	RtlInitUnicodeString(&key_str, L"StartingPortNumber");

	status = WdfDriverOpenParametersRegistryKey(driver, STANDARD_RIGHTS_ALL, WDF_NO_OBJECT_ATTRIBUTES, &driver_key);
	if (!NT_SUCCESS(status)) {
		TraceEvents(TRACE_LEVEL_ERROR, TRACE_DEVICE, 
			"WdfDriverOpenParametersRegistryKey failed %!STATUS!", status);
		WdfRegistryClose(driver_key);
		return status;
	}
	
	/* Start by getting the driver's next available port number. */
	status = registry_get_or_create_ulong(driver_key, &key_str, &value, 0);
	if (!NT_SUCCESS(status)) {
		TraceEvents(TRACE_LEVEL_ERROR, TRACE_DEVICE, 
			"WdfRegistryQueryULong failed %!STATUS!", status);
	}

	WdfRegistryClose(driver_key);

	*port_num = (unsigned)value;

	return status;
}

NTSTATUS fscc_driver_registry_set_portnum(WDFDRIVER driver, unsigned port_num)
{	
	NTSTATUS status;
	WDFKEY key;
	UNICODE_STRING key_str;

	RtlInitUnicodeString(&key_str, L"StartingPortNumber");

	status = WdfDriverOpenParametersRegistryKey(driver, STANDARD_RIGHTS_ALL, WDF_NO_OBJECT_ATTRIBUTES, &key);
	if (!NT_SUCCESS(status)) {
		TraceEvents(TRACE_LEVEL_ERROR, TRACE_DEVICE, 
			"WdfDriverOpenParametersRegistryKey failed %!STATUS!", status);
		WdfRegistryClose(key);
		return status;
	}
	
	/* Start by getting the driver's next available port number. */
	status = registry_set_ulong(key, &key_str, (ULONG)port_num);
	if (!NT_SUCCESS(status)) {
		TraceEvents(TRACE_LEVEL_ERROR, TRACE_DEVICE, 
			"WdfRegistryQueryULong failed %!STATUS!", status);
	}

	WdfRegistryClose(key);

	return status;
}