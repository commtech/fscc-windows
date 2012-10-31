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

#include "card.h"
#include "card.tmh"

#include "port.h"
#include "utils.h"
#include "isr.h"
#include "driver.h"

#include <ntddser.h>
#include <ntstrsafe.h>

EVT_WDF_DEVICE_PREPARE_HARDWARE fscc_card_prepare_hardware;
EVT_WDF_DEVICE_RELEASE_HARDWARE fscc_card_release_hardware;

struct fscc_card *fscc_card_new(WDFDRIVER Driver, IN PWDFDEVICE_INIT DeviceInit)
{
	WDF_PNPPOWER_EVENT_CALLBACKS  pnpPowerCallbacks;
	WDF_OBJECT_ATTRIBUTES attributes;
	NTSTATUS status;
	WDFDEVICE device;
	struct fscc_card *card = 0;
	unsigned i = 0;
    WDF_DEVICE_PNP_CAPABILITIES pnpCaps;
	
	WDF_INTERRUPT_CONFIG  interruptConfig;
	WDF_OBJECT_ATTRIBUTES  interruptAttributes;

    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DEVICE, 
                "%!FUNC! DeviceInit 0x%p", 
				DeviceInit);
	
	WDF_OBJECT_ATTRIBUTES_INIT_CONTEXT_TYPE(&attributes, FSCC_CARD);
	
    WdfDeviceInitSetDeviceType(DeviceInit, FILE_DEVICE_SERIAL_PORT);
	WdfDeviceInitSetPowerPolicyOwnership(DeviceInit, TRUE);
	
	WDF_PNPPOWER_EVENT_CALLBACKS_INIT(&pnpPowerCallbacks);
	pnpPowerCallbacks.EvtDevicePrepareHardware = fscc_card_prepare_hardware;
	pnpPowerCallbacks.EvtDeviceReleaseHardware = fscc_card_release_hardware;
	WdfDeviceInitSetPnpPowerEventCallbacks(DeviceInit, &pnpPowerCallbacks);

	status = WdfDeviceCreate(&DeviceInit, &attributes, &device);  
	if(!NT_SUCCESS(status)) {
        TraceEvents(TRACE_LEVEL_ERROR, TRACE_DEVICE, 
			"WdfDeviceCreate failed %!STATUS!", status);
		return 0;
	}

	card = WdfObjectGet_FSCC_CARD(device);

	card->device = device;
	card->dma = 0;
	card->driver = Driver;

	WDF_DEVICE_PNP_CAPABILITIES_INIT(&pnpCaps);
    pnpCaps.Removable = WdfFalse;
	pnpCaps.UniqueID = WdfTrue;
    WdfDeviceSetPnpCapabilities(card->device, &pnpCaps);
	
	for (i = 0; i < 2; i++) {
		struct fscc_port *port = 0;
		
		port = fscc_port_new(card, i);
		
		if (!port)
			return 0;

		status = WdfFdoAddStaticChild(card->device, port->device);
		if (!NT_SUCCESS(status)) {
			WdfObjectDelete(port->device);
			TraceEvents(TRACE_LEVEL_ERROR, TRACE_DEVICE, 
				"WdfFdoAddStaticChild failed %!STATUS!", status);
		}

		card->ports[i] = port;
	};
	
	WDF_INTERRUPT_CONFIG_INIT(&interruptConfig, fscc_isr, NULL);
	interruptConfig.ShareVector = WdfTrue;
	WDF_OBJECT_ATTRIBUTES_INIT_CONTEXT_TYPE(&interruptAttributes, FSCC_CARD);

	status = WdfInterruptCreate(card->device, &interruptConfig, &interruptAttributes, &card->interrupt);
	if (!NT_SUCCESS(status)) {
        TraceEvents(TRACE_LEVEL_ERROR, TRACE_DEVICE, 
			"WdfInterruptCreate failed %!STATUS!", status);
		return 0;
	}
	
    TraceEvents(TRACE_LEVEL_VERBOSE, TRACE_DEVICE, "%!FUNC! Exit");
	
	return card;
}

NTSTATUS fscc_card_prepare_hardware(IN WDFDEVICE Device, IN WDFCMRESLIST ResourcesRaw, IN WDFCMRESLIST ResourcesTranslated)
{
	struct fscc_card *card = 0;
	unsigned bar_counter = 0;
	unsigned i = 0;

    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DEVICE, 
                "%!FUNC! Device 0x%p, ResourcesRaw 0x%p, ResourcesTranslated 0x%p", 
                Device, ResourcesRaw, ResourcesTranslated);
	
	card = WdfObjectGet_FSCC_CARD(Device);

	for (i = 0; i < WdfCmResourceListGetCount(ResourcesTranslated); i++) {
		PCM_PARTIAL_RESOURCE_DESCRIPTOR descriptor;
		
		descriptor = WdfCmResourceListGetDescriptor(ResourcesTranslated, i);
		
		if (!descriptor)
			return STATUS_DEVICE_CONFIGURATION_ERROR;
			
		switch (descriptor->Type) {
		case CmResourceTypePort:
			card->bar[bar_counter].address = ULongToPtr(descriptor->u.Port.Start.LowPart);
			card->bar[bar_counter].memory_mapped = FALSE;
			bar_counter++;
			break;
			
		case CmResourceTypeMemory:
			card->bar[bar_counter].address = MmMapIoSpace(descriptor->u.Memory.Start, descriptor->u.Memory.Length, MmNonCached);
			card->bar[bar_counter].memory_mapped = TRUE;
			bar_counter++;
			break;
		}		
	}

	for (i = 0; i < 2; i++)
		fscc_port_prepare_hardware(card->ports[i]);

    TraceEvents(TRACE_LEVEL_VERBOSE, TRACE_DEVICE, "%!FUNC! Exit");

	return STATUS_SUCCESS;
}

NTSTATUS fscc_card_release_hardware(WDFDEVICE Device, WDFCMRESLIST ResourcesTranslated)
{
	unsigned bar_counter = 0;
	unsigned i = 0;
	struct fscc_card *card = 0;
	
	card = WdfObjectGet_FSCC_CARD(Device);

    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DEVICE, 
                "%!FUNC! Device 0x%p, ResourcesTranslated 0x%p", 
                Device, ResourcesTranslated);

	//WdfInterruptDisable(card->interrupt);

	for (i = 0; i < 2; i++)
		fscc_port_release_hardware(card->ports[i]);

	for (i = 0; i < WdfCmResourceListGetCount(ResourcesTranslated); i++) {		
		PCM_PARTIAL_RESOURCE_DESCRIPTOR descriptor;
		
		descriptor = WdfCmResourceListGetDescriptor(ResourcesTranslated, i);
		
		if (!descriptor)
			return STATUS_DEVICE_CONFIGURATION_ERROR;
		
		switch (descriptor->Type) {
		case CmResourceTypePort:
			bar_counter++;
			break;
			
		case CmResourceTypeMemory:
			MmUnmapIoSpace(card->bar[bar_counter].address, descriptor->u.Memory.Length);
			bar_counter++;
			break;
		}
	}

    TraceEvents(TRACE_LEVEL_VERBOSE, TRACE_DEVICE, "%!FUNC! Exit");

	return STATUS_SUCCESS;
}

NTSTATUS fscc_card_registry_open(struct fscc_card *card, WDFKEY *key)
{
	return WdfDeviceOpenRegistryKey(card->device, PLUGPLAY_REGKEY_DEVICE, STANDARD_RIGHTS_ALL, 
		                            WDF_NO_OBJECT_ATTRIBUTES, key);
}

void *fscc_card_get_BAR(struct fscc_card *card, unsigned number)
{
	if (number > 2)
		return 0;

	return card->bar[number].address;
}

UINT32 fscc_card_get_register(struct fscc_card *card, unsigned bar,
							  unsigned offset)
{
	void *address = 0;
	UINT32 value = 0;

	address = fscc_card_get_BAR(card, bar);

	value = READ_PORT_ULONG((ULONG *)((char *)address + offset));

	return value;
}

void fscc_card_set_register(struct fscc_card *card, unsigned bar,
							unsigned offset, UINT32 value)
{
	void *address = 0;

	address = fscc_card_get_BAR(card, bar);

	WRITE_PORT_ULONG((ULONG *)((char *)address + offset), value);
}

/* 
    At the card level there is no offset manipulation to get to the second port
    on each card. If you would like to pass in a register offset and get the
    appropriate address on a port basis use the fscc_port_* functions.
*/
void fscc_card_get_register_rep(struct fscc_card *card, unsigned bar,
                                unsigned offset, char *buf,
                                unsigned byte_count)
{
    void *address = 0;
    unsigned leftover_count = 0;
    UINT32 incoming_data = 0;
    unsigned chunks = 0;
	unsigned i = 0;

    return_if_untrue(card);
    return_if_untrue(bar <= 2);
    return_if_untrue(buf);
    return_if_untrue(byte_count > 0);

    address = fscc_card_get_BAR(card, bar);
    leftover_count = byte_count % 4;
    chunks = (byte_count - leftover_count) / 4;

	for (i = 0; i < chunks; i++) {
		UINT32 value = 0;

		value = READ_PORT_ULONG((ULONG *)((char *)address + offset));

		memcpy(&buf[i * 4], &value, sizeof(value));
	}

    if (leftover_count) {
        incoming_data = READ_PORT_ULONG((ULONG *)((char *)address + offset));

        memmove(buf + (byte_count - leftover_count),
                (char *)(&incoming_data), leftover_count);
    }

#ifdef __BIG_ENDIAN
    {
        unsigned i = 0;

        for (i = 0; i < (int)(byte_count / 2); i++) {
            char first, last;

            first = buf[i];
            last = buf[byte_count - i - 1];

            buf[i] = last;
            buf[byte_count - i - 1] = first;
        }
    }
#endif
}

/* 
    At the card level there is no offset manipulation to get to the second port
    on each card. If you would like to pass in a register offset and get the
    appropriate address on a port basis use the fscc_port_* functions.
*/
void fscc_card_set_register_rep(struct fscc_card *card, unsigned bar,
                                unsigned offset, const char *data,
                                unsigned byte_count)
{
    void *address = 0;
    unsigned leftover_count = 0;
    unsigned chunks = 0;
    char *reversed_data = 0;
    const char *outgoing_data = 0;
	unsigned i = 0;

    return_if_untrue(card);
    return_if_untrue(bar <= 2);
    return_if_untrue(data);
    return_if_untrue(byte_count > 0);

    address = fscc_card_get_BAR(card, bar);
    leftover_count = byte_count % 4;
    chunks = (byte_count - leftover_count) / 4;

    outgoing_data = data;

#ifdef __BIG_ENDIAN
    {
        unsigned i = 0;
		
		reversed_data = (char *)ExAllocatePoolWithTag(NonPagedPool, byte_count, 'ataD');

        for (i = 0; i < byte_count; i++)
            reversed_data[i] = data[byte_count - i - 1];

        outgoing_data = reversed_data;
    }
#endif

	for (i = 0; i < chunks; i++)
		WRITE_PORT_ULONG((ULONG *)((char *)address + offset), chars_to_u32(outgoing_data + (i * 4)));

    if (leftover_count)
		WRITE_PORT_ULONG((ULONG *)((char *)address + offset), chars_to_u32(outgoing_data + (byte_count - leftover_count)));

    if (reversed_data)
		ExFreePoolWithTag (reversed_data, 'ataD');
}

char *fscc_card_get_name(struct fscc_card *card)
{
	UNREFERENCED_PARAMETER(card);

	return "TODO2";
	
	/*
	switch (card->pci_dev->device) {
	case FSCC_ID:
		return "FSCC PCI";
	case SFSCC_ID:
		return "SuperFSCC PCI";
	case FSCC_232_ID:
		return "FSCC-232 PCI";
	case SFSCC_4_ID:
		return "SuperFSCC/4 PCI";
	case FSCC_4_ID:
		return "FSCC/4 PCI";
	case SFSCC_4_LVDS_ID:
		return "SuperFSCC/4 LVDS PCI";
	case SFSCCe_4_ID:
		return "SuperFSCC/4 PCIe";
	}

	return "Unknown Device";
	*/
}