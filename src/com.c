/*
	Copyright (C) 2010  Commtech, Inc.
	
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


#include "com.h"
#include "com.tmh"

#include <ntddser.h>
#include <ntstrsafe.h>



NTSTATUS com_port_init(struct fscc_card *card, unsigned channel)
{
	NTSTATUS status = STATUS_SUCCESS;
	struct fscc_port *port = 0;
	WDF_OBJECT_ATTRIBUTES attributes;
	PWDFDEVICE_INIT DeviceInit;
	WDFDEVICE device;
	WDF_IO_QUEUE_CONFIG queue_config;
	
	WCHAR instance_id_buffer[20];
	UNICODE_STRING instance_id;
	
	DECLARE_CONST_UNICODE_STRING(device_id, L"FSCC\\PNP0501\0"); // Shows up as 'Bus relations' in the device manager
	DECLARE_CONST_UNICODE_STRING(location, L"FSCC Card\0");
	DECLARE_CONST_UNICODE_STRING(compat_id, L"FSCC\\PNP0501\0");

    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DEVICE, 
                "%!FUNC! card 0x%p", card);

	RtlInitEmptyUnicodeString(&instance_id, instance_id_buffer, sizeof(instance_id_buffer));
	status = RtlUnicodeStringPrintf(&instance_id, L"%02d", channel);
	if (!NT_SUCCESS(status)) {
        TraceEvents(TRACE_LEVEL_ERROR, TRACE_DEVICE, 
			"RtlUnicodeStringPrintf failed %!STATUS!", status);
		return status;
	}
	
	DeviceInit = WdfPdoInitAllocate(card->device); 
	if (DeviceInit == NULL) {
        TraceEvents(TRACE_LEVEL_ERROR, TRACE_DEVICE, 
			"WdfPdoInitAllocate failed %!STATUS!", status);
		return status;
	}
	
	WdfDeviceInitSetDeviceType(DeviceInit, FILE_DEVICE_SERIAL_PORT);	

	status = WdfPdoInitAssignDeviceID(DeviceInit, &device_id); 
	if (!NT_SUCCESS(status)) {
		WdfDeviceInitFree(DeviceInit);
        TraceEvents(TRACE_LEVEL_ERROR, TRACE_DEVICE, 
			"WdfPdoInitAssignDeviceID failed %!STATUS!", status);
		return status;
	}

	status = WdfPdoInitAssignInstanceID(DeviceInit, &instance_id);
	if (!NT_SUCCESS(status)) {
		WdfDeviceInitFree(DeviceInit);
        TraceEvents(TRACE_LEVEL_ERROR, TRACE_DEVICE, 
			"WdfPdoInitAssignInstanceID failed %!STATUS!", status);
		return status;
	}

	status = WdfPdoInitAddHardwareID(DeviceInit, &device_id);
	if (!NT_SUCCESS(status)) {
		WdfDeviceInitFree(DeviceInit);
        TraceEvents(TRACE_LEVEL_ERROR, TRACE_DEVICE, 
			"WdfPdoInitAddHardwareID failed %!STATUS!", status);
		return status;
	}

	status = WdfPdoInitAddCompatibleID(DeviceInit, &compat_id);
	if (!NT_SUCCESS(status)) {
		WdfDeviceInitFree(DeviceInit);
        TraceEvents(TRACE_LEVEL_ERROR, TRACE_DEVICE, 
			"WdfPdoInitAddCompatibleID failed %!STATUS!", status);
		return status;
	}

	status = WdfDeviceCreate(&DeviceInit, WDF_NO_OBJECT_ATTRIBUTES, &device);  
	if (!NT_SUCCESS(status)) {
		WdfDeviceInitFree(DeviceInit);
        TraceEvents(TRACE_LEVEL_ERROR, TRACE_DEVICE, 
			"WdfDeviceCreate failed %!STATUS!", status);
		return 0;
	}

	status = WdfFdoAddStaticChild(card->device, device);
	if (!NT_SUCCESS(status)) {
		WdfObjectDelete(device);
        TraceEvents(TRACE_LEVEL_ERROR, TRACE_DEVICE, 
			"WdfFdoAddStaticChild failed %!STATUS!", status);
	}

	return STATUS_SUCCESS;
}