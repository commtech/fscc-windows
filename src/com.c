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
#include "public.h"
#include "port.h"

#include <ntddser.h>
#include <ntstrsafe.h>


void Bus_GetFsccInfo(IN WDFDEVICE Device, OUT PDO_EXTENSION *pdo_ext)
{
	struct com_port *com_port = 0;
	struct fscc_port *fscc_port = 0;
	struct fscc_card *card = 0;

	com_port = WdfObjectGet_COM_PORT(Device);
	fscc_port = com_port->fscc_port;
	card = fscc_port->card;

	pdo_ext->flags = card->bar[1].tr_descriptor->Flags;
	pdo_ext->UartAddress = (PULONG)((char *)card->bar[1].address + (8 * com_port->channel));
	pdo_ext->FcrAddress = card->bar[2].address;
	pdo_ext->DeviceID = fscc_port_get_PDEV(fscc_port);
	pdo_ext->Channel = com_port->channel;
}

BOOLEAN Bus_IsFsccOpen(IN WDFDEVICE Device)
{
	struct com_port *com_port = 0;
	struct fscc_port *fscc_port = 0;

	com_port = WdfObjectGet_COM_PORT(Device);
	fscc_port = com_port->fscc_port;

	return (fscc_port->open_counter > 0);
}

void Bus_EnableAsync(IN WDFDEVICE Device)
{
	struct com_port *com_port = 0;
	struct fscc_port *fscc_port = 0;
	struct fscc_card *card = 0;
	struct fscc_registers regs;

	com_port = WdfObjectGet_COM_PORT(Device);
	fscc_port = com_port->fscc_port;

	FSCC_REGISTERS_INIT(regs);
	regs.FCR = fscc_port->register_storage.FCR;

	switch (fscc_port->channel) {
	case 0:
		regs.FCR |= 0x01000000;
		break;

	case 1:
		regs.FCR |= 0x02000000;
		break;
	}

	fscc_port_set_registers(fscc_port, &regs);
}

void Bus_DisableAsync(IN WDFDEVICE Device)
{
	struct com_port *com_port = 0;
	struct fscc_port *fscc_port = 0;
	struct fscc_card *card = 0;
	struct fscc_registers regs;

	com_port = WdfObjectGet_COM_PORT(Device);
	fscc_port = com_port->fscc_port;

	FSCC_REGISTERS_INIT(regs);
	regs.FCR = fscc_port->register_storage.FCR;

	switch (fscc_port->channel) {
	case 0:
		regs.FCR &= ~(0x01000000);
		break;

	case 1:
		regs.FCR &= ~(0x02000000);
		break;
	}

	fscc_port_set_registers(fscc_port, &regs);
}

FsccEvtDeviceResourcesQuery(WDFDEVICE Device, WDFCMRESLIST res_list)
{
	struct com_port *com_port = 0;
	struct fscc_port *fscc_port = 0;
	struct fscc_card *card = 0;

	com_port = WdfObjectGet_COM_PORT(Device);
	fscc_port = com_port->fscc_port;
	card = fscc_port->card;

	TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DEVICE, "A");
	
	WdfCmResourceListAppendDescriptor(res_list, card->interrupt_raw_descriptor);
	WdfCmResourceListAppendDescriptor(res_list, card->bar[1].raw_descriptor);
  
	return STATUS_SUCCESS;
}

FsccEvtDeviceResourceRequirementsQuery(WDFDEVICE Device, WDFIORESREQLIST requirements_list)
{
	struct com_port *com_port = 0;
	struct fscc_port *fscc_port = 0;
	struct fscc_card *card = 0;

	WDFIORESLIST res_list;
	IO_RESOURCE_DESCRIPTOR ird;

	com_port = WdfObjectGet_COM_PORT(Device);
	fscc_port = com_port->fscc_port;
	card = fscc_port->card;

	TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DEVICE, "B");

	WdfIoResourceRequirementsListSetInterfaceType(requirements_list, PNPBus);
  
	WdfIoResourceListCreate(requirements_list, WDF_NO_OBJECT_ATTRIBUTES, &res_list);
#if 0
	RtlZeroMemory(&ird, sizeof(ird));
	ird.Option = 0;
	ird.Type = card->bar[1].raw_descriptor->Type;
	ird.ShareDisposition = CmResourceShareShared;
	ird.Flags = card->bar[1].raw_descriptor->Flags;
	ird.u.Port.Length = card->bar[1].raw_descriptor->u.Port.Length;
	ird.u.Port.MinimumAddress.LowPart = card->bar[1].raw_descriptor->u.Port.Start.LowPart; //xpdd->platform_mmio_addr.QuadPart;
	ird.u.Port.MaximumAddress.LowPart = ird.u.Port.MinimumAddress.LowPart + ird.u.Port.Length - 1; //xpdd->platform_mmio_addr.QuadPart;
	ird.u.Port.Alignment = 1; //PAGE_SIZE;
	WdfIoResourceListAppendDescriptor(res_list, &ird);
	TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DEVICE, "%i, 0x%x, 0x%0x", ird.u.Port.Length, ird.u.Port.MinimumAddress.LowPart, ird.u.Port.MaximumAddress.LowPart);
#endif
	RtlZeroMemory(&ird, sizeof(ird));
	ird.Option = 0;
	ird.Type = card->interrupt_raw_descriptor->Type;
	ird.ShareDisposition = CmResourceShareShared;
	ird.Flags = card->interrupt_raw_descriptor->Flags;
	ird.u.Interrupt.MinimumVector = card->interrupt_raw_descriptor->u.Interrupt.Vector;
	ird.u.Interrupt.MaximumVector = card->interrupt_raw_descriptor->u.Interrupt.Vector;
	WdfIoResourceListAppendDescriptor(res_list, &ird);

	WdfIoResourceRequirementsListAppendIoResList(requirements_list, res_list);

	return STATUS_SUCCESS;
}

NTSTATUS com_port_init(struct fscc_port *fscc_port, unsigned channel)
{
	NTSTATUS status = STATUS_SUCCESS;
	struct com_port *com_port = 0;
	WDF_OBJECT_ATTRIBUTES attributes;
	PWDFDEVICE_INIT DeviceInit;
	WDFDEVICE device;
	WDF_IO_QUEUE_CONFIG queue_config;
    COM_INTERFACE_STANDARD  ComInterface;
    WDF_QUERY_INTERFACE_CONFIG  qiConfig;
	WDF_PDO_EVENT_CALLBACKS pdo_callbacks;
	
	WCHAR instance_id_buffer[20];
	UNICODE_STRING instance_id;
	
	DECLARE_CONST_UNICODE_STRING(device_id, L"FSCC\\PNP0501\0"); // Shows up as 'Bus relations' in the device manager
	DECLARE_CONST_UNICODE_STRING(location, L"FSCC Card\0");
	DECLARE_CONST_UNICODE_STRING(compat_id, L"FSCC\\PNP0501\0");

	RtlInitEmptyUnicodeString(&instance_id, instance_id_buffer, sizeof(instance_id_buffer));
	status = RtlUnicodeStringPrintf(&instance_id, L"%02d", channel);
	if (!NT_SUCCESS(status)) {
        TraceEvents(TRACE_LEVEL_ERROR, TRACE_DEVICE, 
			"RtlUnicodeStringPrintf failed %!STATUS!", status);
		return status;
	}
	
	DeviceInit = WdfPdoInitAllocate(fscc_port->card->device); 
	if (DeviceInit == NULL) {
        TraceEvents(TRACE_LEVEL_ERROR, TRACE_DEVICE, 
			"WdfPdoInitAllocate failed %!STATUS!", status);
		return status;
	}

	WDF_PDO_EVENT_CALLBACKS_INIT(&pdo_callbacks);
	//pdo_callbacks.EvtDeviceResourcesQuery = FsccEvtDeviceResourcesQuery;
	pdo_callbacks.EvtDeviceResourceRequirementsQuery = FsccEvtDeviceResourceRequirementsQuery;
	WdfPdoInitSetEventCallbacks(DeviceInit, &pdo_callbacks);
	
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
	
	WDF_OBJECT_ATTRIBUTES_INIT_CONTEXT_TYPE(&attributes, COM_PORT);

	status = WdfDeviceCreate(&DeviceInit, &attributes, &device);  
	if (!NT_SUCCESS(status)) {
		WdfDeviceInitFree(DeviceInit);
        TraceEvents(TRACE_LEVEL_ERROR, TRACE_DEVICE, 
			"WdfDeviceCreate failed %!STATUS!", status);
		return status;
	}

	com_port = WdfObjectGet_COM_PORT(device);
	
	com_port->fscc_port = fscc_port;
	com_port->channel = channel;

    //
    // Create a custom interface so that other drivers can
    // query (IRP_MN_QUERY_INTERFACE) and use our callbacks directly.
    //
    RtlZeroMemory(&ComInterface, sizeof(ComInterface));

    ComInterface.InterfaceHeader.Size = sizeof(ComInterface);
    ComInterface.InterfaceHeader.Version = 1;
    ComInterface.InterfaceHeader.Context = device;

    //
    // Let the framework handle reference counting.
    //
    ComInterface.InterfaceHeader.InterfaceReference =
        WdfDeviceInterfaceReferenceNoOp;
    ComInterface.InterfaceHeader.InterfaceDereference =
        WdfDeviceInterfaceDereferenceNoOp;

    ComInterface.GetFsccInfo = Bus_GetFsccInfo;
    ComInterface.IsFsccOpen = Bus_IsFsccOpen;
    ComInterface.EnableAsync = Bus_EnableAsync;
    ComInterface.DisableAsync = Bus_DisableAsync;

    WDF_QUERY_INTERFACE_CONFIG_INIT(&qiConfig,
                                    (PINTERFACE) &ComInterface,
                                    &GUID_COM_INTERFACE_STANDARD,
                                    NULL);
    //
    // If you have multiple interfaces, you can call WdfDeviceAddQueryInterface
    // multiple times to add additional interfaces.
    //
    status = WdfDeviceAddQueryInterface(device, &qiConfig);
	if (!NT_SUCCESS(status)) {
		WdfObjectDelete(device);
        TraceEvents(TRACE_LEVEL_ERROR, TRACE_DEVICE, 
			"WdfDeviceAddQueryInterface failed %!STATUS!", status);
		return status;
	}

	status = WdfFdoAddStaticChild(fscc_port->card->device, device);
	if (!NT_SUCCESS(status)) {
		WdfObjectDelete(device);
        TraceEvents(TRACE_LEVEL_ERROR, TRACE_DEVICE, 
			"WdfFdoAddStaticChild failed %!STATUS!", status);
		return status;
	}

	return STATUS_SUCCESS;
}