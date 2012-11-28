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


#include "port.h"
#include "port.tmh"

#include "frame.h"
#include "stream.h"
#include "utils.h"
#include "isr.h"
#include "public.h"
#include "driver.h"
#include "com.h"

#include <ntddser.h>
#include <ntstrsafe.h>

#define NUM_CLOCK_BYTES 20
#define TIMER_DELAY_MS 250

EVT_WDF_IO_QUEUE_IO_DEVICE_CONTROL fscc_port_ioctl;
EVT_WDF_IO_QUEUE_IO_WRITE port_write_handler;
EVT_WDF_IO_QUEUE_IO_READ read_event_handler;
EVT_WDF_DPC user_read_worker;
EVT_WDF_REQUEST_CANCEL EvtRequestCancel;

void empty_frame_list(LIST_ENTRY *frames);

void fscc_port_clear_oframes(struct fscc_port *port, unsigned lock);
void fscc_port_clear_iframes(struct fscc_port *port, unsigned lock);
unsigned fscc_port_timed_out(struct fscc_port *port);
int fscc_port_write(struct fscc_port *port, const char *data, unsigned length);
unsigned fscc_port_is_streaming(struct fscc_port *port);

NTSTATUS fscc_port_registry_registers_create(struct fscc_port *port, WDFKEY *key);
NTSTATUS fscc_port_registry_create_pre(struct fscc_card *card, unsigned channel, WDFKEY *key);
NTSTATUS fscc_port_registry_open_pre(struct fscc_card *card, unsigned channel, WDFKEY *key);
NTSTATUS fscc_port_registry_open(struct fscc_port *port, WDFKEY *key);
NTSTATUS fscc_port_registry_get_ulong_pre(struct fscc_card *card, unsigned channel, PUNICODE_STRING key_str, ULONG *value);
NTSTATUS fscc_port_registry_set_ulong_pre(struct fscc_card *card, unsigned channel, PUNICODE_STRING key_str, ULONG value);
NTSTATUS fscc_port_registry_get_portnum_pre(struct fscc_card *card, unsigned channel, unsigned *portnum);
NTSTATUS fscc_port_registry_set_portnum_pre(struct fscc_card *card, unsigned channel, unsigned portnum);

struct fscc_port *fscc_port_new(struct fscc_card *card, unsigned channel)
{
	NTSTATUS status = STATUS_SUCCESS;
	struct fscc_port *port = 0;
	WDF_OBJECT_ATTRIBUTES attributes;
	PWDFDEVICE_INIT DeviceInit;
	WDFDEVICE device;
	WDF_IO_QUEUE_CONFIG queue_config;
	
	WCHAR instance_id_buffer[20];
	UNICODE_STRING instance_id;
	
	WCHAR device_name_buffer[20];
	UNICODE_STRING device_name;
	
	WCHAR description_buffer[30];
	UNICODE_STRING description;
	
	WCHAR dos_name_buffer[30];
	UNICODE_STRING dos_name;

	WDF_DEVICE_STATE    device_state;
    WDF_DEVICE_PNP_CAPABILITIES pnpCaps;

	WDF_PNPPOWER_EVENT_CALLBACKS  pnpPowerCallbacks;

	WDF_DPC_CONFIG dpcConfig;
	WDF_OBJECT_ATTRIBUTES dpcAttributes;

	unsigned port_number = 0;
	
	DECLARE_CONST_UNICODE_STRING(device_id, L"{0b99e9a4-1460-4856-9c13-72fca9fa92a4}\\FSCC\0"); // Shows up as 'Bus relations' in the device manager
	DECLARE_CONST_UNICODE_STRING(location, L"FSCC Port\0");
	DECLARE_CONST_UNICODE_STRING(compat_id, L"{0b99e9a4-1460-4856-9c13-72fca9fa92a4}\\FSCC\0");

    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DEVICE, 
                "%!FUNC! card 0x%p, channel %d", card, channel);


	/* Creates the channel's registry key if it doesn't already exist */
	fscc_port_registry_create_pre(card, channel, NULL);

	/* Grab the channel's port number if it exists */
	status = fscc_port_registry_get_portnum_pre(card, channel, &port_number);
	if (!NT_SUCCESS(status) && status != STATUS_OBJECT_NAME_NOT_FOUND) {
		TraceEvents(TRACE_LEVEL_ERROR, TRACE_DEVICE, 
			"fscc_card_get_reg_portnum failed %!STATUS!", status);
	}

	if (status == STATUS_OBJECT_NAME_NOT_FOUND) {
		/* Get the driver's next available port number */
		status = fscc_driver_registry_get_portnum(card->driver, &port_number);
		if (!NT_SUCCESS(status)) {
			TraceEvents(TRACE_LEVEL_ERROR, TRACE_DEVICE, 
				"fscc_driver_get_reg_portnum failed %!STATUS!", status);
		}

		/* Set the channels's port number based on the driver's next available port number */
		status = fscc_port_registry_set_portnum_pre(card, channel, port_number);
		if (!NT_SUCCESS(status)) {
			TraceEvents(TRACE_LEVEL_ERROR, TRACE_DEVICE, 
				"fscc_card_set_reg_portnum failed %!STATUS!", status);
		}
	}




	RtlInitEmptyUnicodeString(&device_name, device_name_buffer, sizeof(device_name_buffer));
	status = RtlUnicodeStringPrintf(&device_name, L"\\Device\\FSCC%i", port_number);
	if (!NT_SUCCESS(status)) {
        TraceEvents(TRACE_LEVEL_ERROR, TRACE_DEVICE, 
			"RtlUnicodeStringPrintf failed %!STATUS!", status);
		return 0;
	}
	
	RtlInitEmptyUnicodeString(&instance_id, instance_id_buffer, sizeof(instance_id_buffer));
	status = RtlUnicodeStringPrintf(&instance_id, L"%02d", port_number);
	if (!NT_SUCCESS(status)) {
        TraceEvents(TRACE_LEVEL_ERROR, TRACE_DEVICE, 
			"RtlUnicodeStringPrintf failed %!STATUS!", status);
		return 0;
	}
	
	RtlInitEmptyUnicodeString(&description, description_buffer, sizeof(description_buffer));
	status = RtlUnicodeStringPrintf(&description, L"FSCC Port (FSCC%i)", port_number);
	if (!NT_SUCCESS(status)) {
        TraceEvents(TRACE_LEVEL_ERROR, TRACE_DEVICE, 
			"RtlUnicodeStringPrintf failed %!STATUS!", status);
		return 0;
	}
	
	RtlInitEmptyUnicodeString(&dos_name, dos_name_buffer, sizeof(dos_name_buffer));
	status = RtlUnicodeStringPrintf(&dos_name, L"\\DosDevices\\FSCC%i", port_number);
	if (!NT_SUCCESS(status)) {
        TraceEvents(TRACE_LEVEL_ERROR, TRACE_DEVICE, 
			"RtlUnicodeStringPrintf failed %!STATUS!", status);
		return 0;
	}
	
	DeviceInit = WdfPdoInitAllocate(card->device); 
	if (DeviceInit == NULL) {
        TraceEvents(TRACE_LEVEL_ERROR, TRACE_DEVICE, 
			"WdfPdoInitAllocate failed %!STATUS!", status);
		return 0;
	}

	status = WdfPdoInitAssignRawDevice(DeviceInit, (const LPGUID)&GUID_DEVCLASS_FSCC);
	if (!NT_SUCCESS(status)) {
		WdfDeviceInitFree(DeviceInit);
        TraceEvents(TRACE_LEVEL_ERROR, TRACE_DEVICE, 
			"WdfPdoInitAssignRawDevice failed %!STATUS!", status);
		return 0;
	}

	status = WdfDeviceInitAssignName(DeviceInit, &device_name);
	if (!NT_SUCCESS(status)) {
		WdfDeviceInitFree(DeviceInit);
        TraceEvents(TRACE_LEVEL_ERROR, TRACE_DEVICE, 
			"WdfDeviceInitAssignName failed %!STATUS!", status);
		return 0;
	}
	
    status = WdfDeviceInitAssignSDDLString(DeviceInit, &SDDL_DEVOBJ_SYS_ALL_ADM_RWX_WORLD_RWX_RES_RWX);
	if (!NT_SUCCESS(status)) {
		WdfDeviceInitFree(DeviceInit);
        TraceEvents(TRACE_LEVEL_ERROR, TRACE_DEVICE, 
			"WdfDeviceInitAssignSDDLString failed %!STATUS!", status);
		return 0;
	}
	
	WdfDeviceInitSetDeviceType(DeviceInit, FILE_DEVICE_SERIAL_PORT);
	WdfDeviceInitSetExclusive(DeviceInit, TRUE);
	WdfDeviceInitSetDeviceClass(DeviceInit, (LPGUID)&GUID_DEVCLASS_FSCC);

	status = WdfPdoInitAssignDeviceID(DeviceInit, &device_id); 
	if (!NT_SUCCESS(status)) {
		WdfDeviceInitFree(DeviceInit);
        TraceEvents(TRACE_LEVEL_ERROR, TRACE_DEVICE, 
			"WdfPdoInitAssignDeviceID failed %!STATUS!", status);
		return 0;
	}

	status = WdfPdoInitAssignInstanceID(DeviceInit, &instance_id);
	if (!NT_SUCCESS(status)) {
		WdfDeviceInitFree(DeviceInit);
        TraceEvents(TRACE_LEVEL_ERROR, TRACE_DEVICE, 
			"WdfPdoInitAssignInstanceID failed %!STATUS!", status);
		return 0;
	}

	status = WdfPdoInitAddHardwareID(DeviceInit, &device_id);
	if (!NT_SUCCESS(status)) {
		WdfDeviceInitFree(DeviceInit);
        TraceEvents(TRACE_LEVEL_ERROR, TRACE_DEVICE, 
			"WdfPdoInitAddHardwareID failed %!STATUS!", status);
		return 0;
	}

	status = WdfPdoInitAddCompatibleID(DeviceInit, &compat_id);
	if (!NT_SUCCESS(status)) {
		WdfDeviceInitFree(DeviceInit);
        TraceEvents(TRACE_LEVEL_ERROR, TRACE_DEVICE, 
			"WdfPdoInitAddCompatibleID failed %!STATUS!", status);
		return 0;
	}
	
	status = WdfPdoInitAddDeviceText(DeviceInit, &description, &location, 0x409);
	if (!NT_SUCCESS(status)) {
		WdfDeviceInitFree(DeviceInit);
        TraceEvents(TRACE_LEVEL_ERROR, TRACE_DEVICE, 
			"WdfPdoInitAddDeviceText failed %!STATUS!", status);
		return 0;
	}

    WdfPdoInitSetDefaultLocale(DeviceInit, 0x409);


	WDF_OBJECT_ATTRIBUTES_INIT_CONTEXT_TYPE(&attributes, FSCC_PORT);
	
	status = WdfDeviceCreate(&DeviceInit, &attributes, &device);  
	if (!NT_SUCCESS(status)) {
		WdfDeviceInitFree(DeviceInit);
        TraceEvents(TRACE_LEVEL_ERROR, TRACE_DEVICE, 
			"WdfDeviceCreate failed %!STATUS!", status);
		return 0;
	}

	port = WdfObjectGet_FSCC_PORT(device);
	
	port->card = card;
	port->channel = channel;
	port->device = device;
	port->istream = fscc_stream_new();
	
	WDF_IO_QUEUE_CONFIG_INIT(&queue_config, WdfIoQueueDispatchSequential);
	queue_config.EvtIoDeviceControl = fscc_port_ioctl;
		
	status = WdfIoQueueCreate(port->device, &queue_config, 
		                      WDF_NO_OBJECT_ATTRIBUTES, &port->ioctl_queue);
	if(!NT_SUCCESS(status)) {
        TraceEvents(TRACE_LEVEL_ERROR, TRACE_DEVICE, 
			"WdfIoQueueCreate failed %!STATUS!", status);
		return 0;
	}
	status = WdfDeviceConfigureRequestDispatching(port->device, port->ioctl_queue, 
		                                          WdfRequestTypeDeviceControl);
	if(!NT_SUCCESS(status)) {
        TraceEvents(TRACE_LEVEL_ERROR, TRACE_DEVICE, 
			"WdfDeviceConfigureRequestDispatching failed %!STATUS!", status);
		return 0;
	}
	
	WDF_IO_QUEUE_CONFIG_INIT(&queue_config, WdfIoQueueDispatchSequential);
	queue_config.EvtIoWrite = port_write_handler;
		
	status = WdfIoQueueCreate(port->device, &queue_config, 
		                      WDF_NO_OBJECT_ATTRIBUTES, &port->write_queue);
	if(!NT_SUCCESS(status)) {
        TraceEvents(TRACE_LEVEL_ERROR, TRACE_DEVICE, 
			"WdfIoQueueCreate failed %!STATUS!", status);
		return 0;
	}
	
	status = WdfDeviceConfigureRequestDispatching(port->device, port->write_queue, 
	                                              WdfRequestTypeWrite);
	if(!NT_SUCCESS(status)) {
        TraceEvents(TRACE_LEVEL_ERROR, TRACE_DEVICE, 
			"WdfDeviceConfigureRequestDispatching failed %!STATUS!", status);
		return 0;
	}
	
	WDF_IO_QUEUE_CONFIG_INIT(&queue_config, WdfIoQueueDispatchSequential);
	queue_config.EvtIoRead = read_event_handler;
		
	status = WdfIoQueueCreate(port->device, &queue_config, 
		                      WDF_NO_OBJECT_ATTRIBUTES, &port->read_queue);
	if(!NT_SUCCESS(status)) {
        TraceEvents(TRACE_LEVEL_ERROR, TRACE_DEVICE, 
			"WdfIoQueueCreate failed %!STATUS!", status);
		return 0;
	}
	
	status = WdfDeviceConfigureRequestDispatching(port->device, port->read_queue, 
	                                              WdfRequestTypeRead);
	if(!NT_SUCCESS(status)) {
        TraceEvents(TRACE_LEVEL_ERROR, TRACE_DEVICE, 
			"WdfDeviceConfigureRequestDispatching failed %!STATUS!", status);
		return 0;
	}
	

	WDF_IO_QUEUE_CONFIG_INIT(&queue_config, WdfIoQueueDispatchManual);
		
	status = WdfIoQueueCreate(port->device, &queue_config, 
		                      WDF_NO_OBJECT_ATTRIBUTES, &port->read_queue2);
	if(!NT_SUCCESS(status)) {
        TraceEvents(TRACE_LEVEL_ERROR, TRACE_DEVICE, 
			"WdfIoQueueCreate failed %!STATUS!", status);
		return 0;
	}

	//
    // Set some properties for the child device.
    //
    WDF_DEVICE_PNP_CAPABILITIES_INIT(&pnpCaps);

    pnpCaps.Removable         = WdfFalse;
    pnpCaps.NoDisplayInUI     = WdfFalse;

    pnpCaps.Address  = port_number;
    pnpCaps.UINumber = port_number;

    WdfDeviceSetPnpCapabilities(port->device, &pnpCaps);

    //
    // In addition to setting NoDisplayInUI in DeviceCaps, we
    // have to do the following to hide the device. Following call
    // tells the framework to report the device state in
    // IRP_MN_QUERY_DEVICE_STATE request.
    //
    WDF_DEVICE_STATE_INIT(&device_state);
    device_state.DontDisplayInUI = WdfFalse;
    WdfDeviceSetDeviceState(port->device, &device_state);
	
	status = WdfDeviceCreateDeviceInterface(port->device, (LPGUID)&GUID_DEVINTERFACE_FSCC, NULL);
	if (!NT_SUCCESS(status)) {
		WdfObjectDelete(port->device);
        TraceEvents(TRACE_LEVEL_ERROR, TRACE_DEVICE, 
			"WdfDeviceCreateDeviceInterface failed %!STATUS!", status);
		return 0;
	}

	status = WdfDeviceCreateSymbolicLink(port->device, &dos_name);
	if (!NT_SUCCESS(status)) {
        TraceEvents(TRACE_LEVEL_WARNING, TRACE_DEVICE, 
			"WdfDeviceCreateSymbolicLink failed %!STATUS!", status);
	}

	WDF_OBJECT_ATTRIBUTES_INIT(&attributes);
	attributes.ParentObject = port->device;

	status = WdfSpinLockCreate(&attributes, &port->oframe_spinlock);
	if (!NT_SUCCESS(status)) {
		WdfObjectDelete(port->device);
        TraceEvents(TRACE_LEVEL_ERROR, TRACE_DEVICE, 
			"WdfSpinLockCreate failed %!STATUS!", status);
		return 0;
	}

	status = WdfSpinLockCreate(&attributes, &port->iframe_spinlock);
	if (!NT_SUCCESS(status)) {
		WdfObjectDelete(port->device);
        TraceEvents(TRACE_LEVEL_ERROR, TRACE_DEVICE, 
			"WdfSpinLockCreate failed %!STATUS!", status);
		return 0;
	}

	InitializeListHead(&port->oframes);
	InitializeListHead(&port->iframes);

	WDF_DPC_CONFIG_INIT(&dpcConfig, &oframe_worker);
	dpcConfig.AutomaticSerialization = TRUE;

	WDF_OBJECT_ATTRIBUTES_INIT(&dpcAttributes);
	dpcAttributes.ParentObject = port->device;
	
	status = WdfDpcCreate(&dpcConfig, &dpcAttributes, &port->oframe_dpc);
	if (!NT_SUCCESS(status)) {
		WdfObjectDelete(port->device);
        TraceEvents(TRACE_LEVEL_ERROR, TRACE_DEVICE, 
			"WdfDpcCreate failed %!STATUS!", status);
		return 0;
	}

	WDF_DPC_CONFIG_INIT(&dpcConfig, &iframe_worker);
	dpcConfig.AutomaticSerialization = TRUE;

	WDF_OBJECT_ATTRIBUTES_INIT(&dpcAttributes);
	dpcAttributes.ParentObject = port->device;
	
	status = WdfDpcCreate(&dpcConfig, &dpcAttributes, &port->iframe_dpc);
	if (!NT_SUCCESS(status)) {
		WdfObjectDelete(port->device);
        TraceEvents(TRACE_LEVEL_ERROR, TRACE_DEVICE, 
			"WdfDpcCreate failed %!STATUS!", status);
		return 0;
	}

	WDF_DPC_CONFIG_INIT(&dpcConfig, &istream_worker);
	dpcConfig.AutomaticSerialization = TRUE;

	WDF_OBJECT_ATTRIBUTES_INIT(&dpcAttributes);
	dpcAttributes.ParentObject = port->device;

	status = WdfDpcCreate(&dpcConfig, &dpcAttributes, &port->istream_dpc);
	if (!NT_SUCCESS(status)) {
		WdfObjectDelete(port->device);
        TraceEvents(TRACE_LEVEL_ERROR, TRACE_DEVICE, 
			"WdfDpcCreate failed %!STATUS!", status);
		return 0;
	}

	WDF_DPC_CONFIG_INIT(&dpcConfig, &user_read_worker);
	dpcConfig.AutomaticSerialization = TRUE;

	WDF_OBJECT_ATTRIBUTES_INIT(&dpcAttributes);
	dpcAttributes.ParentObject = port->device;

	status = WdfDpcCreate(&dpcConfig, &dpcAttributes, &port->user_read_dpc);
	if (!NT_SUCCESS(status)) {
		WdfObjectDelete(port->device);
        TraceEvents(TRACE_LEVEL_ERROR, TRACE_DEVICE, 
			"WdfDpcCreate failed %!STATUS!", status);
		return 0;
	}


	
	/*
	{
		WDF_WMI_PROVIDER_CONFIG  providerConfig;
		WDF_WMI_INSTANCE_CONFIG  instanceConfig;
		NTSTATUS  status;
		DECLARE_CONST_UNICODE_STRING(mofRsrcName, L"FSCC");

		status = WdfDeviceAssignMofResourceName(port->device, &mofRsrcName);
		if (!NT_SUCCESS(status)) {
			TraceEvents(TRACE_LEVEL_ERROR, TRACE_DEVICE, 
				"WdfDeviceAssignMofResourceName failed %!STATUS!", status);
			return 0;
		}

		WDF_WMI_PROVIDER_CONFIG_INIT(&providerConfig, &GUID_WMI_FSCC);
		providerConfig.MinInstanceBufferSize = sizeof(GUID_WMI_FSCC);

		WDF_WMI_INSTANCE_CONFIG_INIT_PROVIDER_CONFIG(
													 &instanceConfig,
													 &providerConfig
													 );
		instanceConfig.Register = TRUE;
		//instanceConfig.EvtWmiInstanceQueryInstance = EvtWmiDeviceInfoQueryInstance;
		//instanceConfig.EvtWmiInstanceSetInstance = EvtWmiDeviceInfoSetInstance;

		status = WdfWmiInstanceCreate(port->device, &instanceConfig, WDF_NO_OBJECT_ATTRIBUTES, WDF_NO_HANDLE);
		if (!NT_SUCCESS(status)) {
			TraceEvents(TRACE_LEVEL_ERROR, TRACE_DEVICE, 
				"WdfWmiInstanceCreate failed %!STATUS!", status);
			return 0;
		}
	}
	*/


	
	/* Update the driver's next available port number if this port was successfully created */
	status = fscc_driver_registry_set_portnum(card->driver, port_number + 1);
	if (!NT_SUCCESS(status)) {
		TraceEvents(TRACE_LEVEL_ERROR, TRACE_DEVICE, 
			"fscc_card_set_reg_portnum failed %!STATUS!", status);
	}

	status = com_port_init(port, channel);
	if (!NT_SUCCESS(status)) {
		TraceEvents(TRACE_LEVEL_ERROR, TRACE_DEVICE, 
			"com_port_init failed %!STATUS!", status);
	}
	
    TraceEvents(TRACE_LEVEL_VERBOSE, TRACE_DEVICE, "%!FUNC! Exit");

	return port;
}

NTSTATUS fscc_port_prepare_hardware(struct fscc_port *port)
{
    unsigned char clock_bits[20] = DEFAULT_CLOCK_BITS;

	WDF_TIMER_CONFIG  timerConfig;
	WDF_OBJECT_ATTRIBUTES  timerAttributes;
	NTSTATUS  status;

    TraceEvents(TRACE_LEVEL_VERBOSE, TRACE_DEVICE, 
                "%!FUNC! port 0x%p", port);

    fscc_port_set_append_status(port, DEFAULT_APPEND_STATUS_VALUE);
    fscc_port_set_ignore_timeout(port, DEFAULT_IGNORE_TIMEOUT_VALUE);
    fscc_port_set_tx_modifiers(port, DEFAULT_TX_MODIFIERS_VALUE);

    port->memory_cap.input = DEFAULT_INPUT_MEMORY_CAP_VALUE;
    port->memory_cap.output = DEFAULT_OUTPUT_MEMORY_CAP_VALUE;

	port->pending_oframe = 0;
	port->pending_iframe = 0;
	
    port->last_isr_value = 0;

	//status = fscc_port_registry_registers_create(port, NULL);
	//if (!NT_SUCCESS(status)) {
	//	TraceEvents(TRACE_LEVEL_ERROR, TRACE_DEVICE, 
	//		"fscc_port_registry_registers_create failed %!STATUS!", status);
	//}

	FSCC_REGISTERS_INIT(port->register_storage);

	port->register_storage.FIFOT = DEFAULT_FIFOT_VALUE;
	port->register_storage.CCR0 = DEFAULT_CCR0_VALUE;
	port->register_storage.CCR1 = DEFAULT_CCR1_VALUE;
	port->register_storage.CCR2 = DEFAULT_CCR2_VALUE;
	port->register_storage.BGR = DEFAULT_BGR_VALUE;
	port->register_storage.SSR = DEFAULT_SSR_VALUE;
	port->register_storage.SMR = DEFAULT_SMR_VALUE;
	port->register_storage.TSR = DEFAULT_TSR_VALUE;
	port->register_storage.TMR = DEFAULT_TMR_VALUE;
	port->register_storage.RAR = DEFAULT_RAR_VALUE;
	port->register_storage.RAMR = DEFAULT_RAMR_VALUE;
	port->register_storage.PPR = DEFAULT_PPR_VALUE;
	port->register_storage.TCR = DEFAULT_TCR_VALUE;
	port->register_storage.IMR = DEFAULT_IMR_VALUE;
	port->register_storage.DPLLR = DEFAULT_DPLLR_VALUE;
	port->register_storage.FCR = DEFAULT_FCR_VALUE;

	fscc_port_set_registers(port, &port->register_storage);

    fscc_port_set_clock_bits(port, clock_bits);

    /* Locks both iframe_spinlock & oframe_spinlock. */
    fscc_port_execute_RRES(port);
    fscc_port_execute_TRES(port);

	WDF_TIMER_CONFIG_INIT(&timerConfig, timer_handler);

	timerConfig.Period = TIMER_DELAY_MS;
	timerConfig.TolerableDelay = TIMER_DELAY_MS;

	WDF_OBJECT_ATTRIBUTES_INIT(&timerAttributes);
	timerAttributes.ParentObject = port->device;

	status = WdfTimerCreate(&timerConfig, &timerAttributes, &port->timer);
	if (!NT_SUCCESS(status)) {
        TraceEvents(TRACE_LEVEL_ERROR, TRACE_DEVICE, 
			"WdfTimerCreate failed %!STATUS!", status);
		return status;
	}

	WdfTimerStart(port->timer, WDF_ABS_TIMEOUT_IN_MS(TIMER_DELAY_MS));

    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DEVICE, "%s (%x.%02x)",
		        fscc_card_get_name(port->card), fscc_port_get_PREV(port), fscc_port_get_FREV(port));

    TraceEvents(TRACE_LEVEL_VERBOSE, TRACE_DEVICE, "%!FUNC! Exit");

	return STATUS_SUCCESS;
}

NTSTATUS fscc_port_release_hardware(struct fscc_port *port)
{
    TraceEvents(TRACE_LEVEL_VERBOSE, TRACE_DEVICE, 
                "%!FUNC! port 0x%p", port);

	WdfTimerStop(port->timer, FALSE);

#if 0
    if (fscc_port_has_dma(port)) {
        fscc_port_execute_STOP_T(port);
        fscc_port_execute_STOP_R(port);
        fscc_port_execute_RST_T(port);
        fscc_port_execute_RST_R(port);

        fscc_port_set_register(port, 2, DMACCR_OFFSET, 0x00000000);
        fscc_port_set_register(port, 2, DMA_TX_BASE_OFFSET, 0x00000000);
    }
#endif
	
	WdfSpinLockAcquire(port->iframe_spinlock);
    fscc_stream_delete(port->istream);
	WdfSpinLockRelease(port->iframe_spinlock);

    fscc_port_clear_iframes(port, 1);
    fscc_port_clear_oframes(port, 1);

#ifdef DEBUG
    debug_interrupt_tracker_delete(port->interrupt_tracker);
#endif

    TraceEvents(TRACE_LEVEL_VERBOSE, TRACE_DEVICE, "%!FUNC! Exit");

	return STATUS_SUCCESS;
}
	
/******************************************************************************/
VOID fscc_port_ioctl(IN WDFQUEUE Queue, IN WDFREQUEST Request, 
	IN size_t OutputBufferLength, IN size_t InputBufferLength, 
	IN ULONG IoControlCode)
{
	NTSTATUS status = STATUS_SUCCESS;
	struct fscc_port *port = 0;
	size_t bytes_returned = 0;

    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DEVICE, 
                "%!FUNC! Queue 0x%p, Request 0x%p, OutputBufferLength %d, InputBufferLength %d, IoControlCode %d", 
                Queue, Request, (int)OutputBufferLength, (int)InputBufferLength, IoControlCode);
	
	port = WdfObjectGet_FSCC_PORT(WdfIoQueueGetDevice(Queue));

	switch(IoControlCode) {
	case FSCC_GET_REGISTERS: {
			struct fscc_registers *input_regs = 0;
			struct fscc_registers *output_regs = 0;

			TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DEVICE, "FSCC_GET_REGISTERS");
			
			status = WdfRequestRetrieveInputBuffer(Request, sizeof(*input_regs), (PVOID *)&input_regs, NULL);
			if (!NT_SUCCESS(status)) {
				TraceEvents(TRACE_LEVEL_WARNING, TRACE_DEVICE, 
					"WdfRequestRetrieveInputBuffer failed %!STATUS!", status);
				break;
			}
			
			status = WdfRequestRetrieveOutputBuffer(Request, sizeof(*output_regs), (PVOID *)&output_regs, NULL);
			if (!NT_SUCCESS(status)) {
				TraceEvents(TRACE_LEVEL_WARNING, TRACE_DEVICE, 
					"WdfRequestRetrieveOutputBuffer failed %!STATUS!", status);
				break;
			}

			memcpy(output_regs, input_regs, sizeof(struct fscc_registers));
			
			fscc_port_get_registers(port, output_regs);
			
			bytes_returned = sizeof(*output_regs);
		}

		break;

	case FSCC_SET_REGISTERS: {
			struct fscc_registers *input_regs = 0;
			
			status = WdfRequestRetrieveInputBuffer(Request, sizeof(*input_regs), (PVOID *)&input_regs, NULL);
			if (!NT_SUCCESS(status)) {
				TraceEvents(TRACE_LEVEL_WARNING, TRACE_DEVICE, 
					"WdfRequestRetrieveInputBuffer failed %!STATUS!", status);
				break;
			}
			
			status = fscc_port_set_registers(port, input_regs);
			if (!NT_SUCCESS(status)) {
				TraceEvents(TRACE_LEVEL_WARNING, TRACE_DEVICE, 
					"fscc_port_set_registers failed %!STATUS!", status);
				break;
			}
		}

		break;

	case FSCC_PURGE_TX:
		TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DEVICE, "FSCC_PURGE_TX");

		status = fscc_port_purge_tx(port);
		if (!NT_SUCCESS(status)) {
			TraceEvents(TRACE_LEVEL_WARNING, TRACE_DEVICE, 
				"fscc_port_purge_tx failed %!STATUS!", status);
			break;
		}

		break;

	case FSCC_PURGE_RX:
		TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DEVICE, "FSCC_PURGE_RX");

		status = fscc_port_purge_rx(port);
		if (!NT_SUCCESS(status)) {
			TraceEvents(TRACE_LEVEL_WARNING, TRACE_DEVICE, 
				"fscc_port_purge_rx failed %!STATUS!", status);
			break;
		}

		break;

	case FSCC_ENABLE_APPEND_STATUS:
		fscc_port_set_append_status(port, 1);
		break;

	case FSCC_DISABLE_APPEND_STATUS:
		fscc_port_set_append_status(port, 0);
		break;

    case FSCC_GET_APPEND_STATUS: {
			unsigned *append_status = 0;

			status = WdfRequestRetrieveOutputBuffer(Request, sizeof(*append_status), (PVOID *)&append_status, NULL);
			if (!NT_SUCCESS(status)) {
				TraceEvents(TRACE_LEVEL_WARNING, TRACE_DEVICE, 
					"WdfRequestRetrieveOutputBuffer failed %!STATUS!", status);
				break;
			}

			*append_status = fscc_port_get_append_status(port);
			
			bytes_returned = sizeof(*append_status);
		}

        break;

    case FSCC_SET_MEMORY_CAP: {
			struct fscc_memory_cap *memcap = 0;

			status = WdfRequestRetrieveInputBuffer(Request, sizeof(*memcap), (PVOID *)&memcap, NULL);
			if (!NT_SUCCESS(status)) {
				TraceEvents(TRACE_LEVEL_WARNING, TRACE_DEVICE, 
					"WdfRequestRetrieveInputBuffer failed %!STATUS!", status);
				break;
			}

			fscc_port_set_memory_cap(port, memcap);
		}

        break;

    case FSCC_GET_MEMORY_CAP: {
			struct fscc_memory_cap *memcap = 0;

			status = WdfRequestRetrieveOutputBuffer(Request, sizeof(*memcap), (PVOID *)&memcap, NULL);
			if (!NT_SUCCESS(status)) {
				TraceEvents(TRACE_LEVEL_WARNING, TRACE_DEVICE, 
					"WdfRequestRetrieveInputBuffer failed %!STATUS!", status);
				break;
			}

			memcap->input = fscc_port_get_input_memory_cap(port);
			memcap->output = fscc_port_get_output_memory_cap(port);
			
			bytes_returned = sizeof(*memcap);
		}

        break;

    case FSCC_SET_CLOCK_BITS: {
			char *clock_bits = 0;

			status = WdfRequestRetrieveInputBuffer(Request, NUM_CLOCK_BYTES, (PVOID *)&clock_bits, NULL);
			if (!NT_SUCCESS(status)) {
				TraceEvents(TRACE_LEVEL_WARNING, TRACE_DEVICE, 
					"WdfRequestRetrieveInputBuffer failed %!STATUS!", status);
				break;
			}

			fscc_port_set_clock_bits(port, clock_bits);
		}

        break;

    case FSCC_ENABLE_IGNORE_TIMEOUT:
        fscc_port_set_ignore_timeout(port, 1);
        break;

    case FSCC_DISABLE_IGNORE_TIMEOUT:
        fscc_port_set_ignore_timeout(port, 0);
        break;

    case FSCC_GET_IGNORE_TIMEOUT: {
			unsigned *ignore_timeout = 0;

			status = WdfRequestRetrieveOutputBuffer(Request, sizeof(*ignore_timeout), (PVOID *)&ignore_timeout, NULL);
			if (!NT_SUCCESS(status)) {
				TraceEvents(TRACE_LEVEL_WARNING, TRACE_DEVICE, 
					"WdfRequestRetrieveOutputBuffer failed %!STATUS!", status);
				break;
			}

			*ignore_timeout = fscc_port_get_ignore_timeout(port);
			
			bytes_returned = sizeof(*ignore_timeout);
		}

        break;
        
    case FSCC_SET_TX_MODIFIERS: {
			int *tx_modifiers = 0;

			status = WdfRequestRetrieveInputBuffer(Request, sizeof(*tx_modifiers), (PVOID *)&tx_modifiers, NULL);
			if (!NT_SUCCESS(status)) {
				TraceEvents(TRACE_LEVEL_WARNING, TRACE_DEVICE, 
					"WdfRequestRetrieveInputBuffer failed %!STATUS!", status);
				break;
			}

			status = fscc_port_set_tx_modifiers(port, *tx_modifiers);
			if (!NT_SUCCESS(status)) {
				TraceEvents(TRACE_LEVEL_WARNING, TRACE_DEVICE, 
					"fscc_port_set_tx_modifiers failed %!STATUS!", status);
				break;
			}
		}
			
        break;
        
    case FSCC_GET_TX_MODIFIERS: {
			unsigned *tx_modifiers = 0;

			status = WdfRequestRetrieveOutputBuffer(Request, sizeof(*tx_modifiers), (PVOID *)&tx_modifiers, NULL);
			if (!NT_SUCCESS(status)) {
				TraceEvents(TRACE_LEVEL_WARNING, TRACE_DEVICE, 
					"WdfRequestRetrieveOutputBuffer failed %!STATUS!", status);
				break;
			}

			*tx_modifiers = fscc_port_get_tx_modifiers(port);
			
			bytes_returned = sizeof(*tx_modifiers);
		}

        break;
		
	default:
		TraceEvents(TRACE_LEVEL_WARNING, TRACE_DEVICE, 
			"Unknown DeviceIoControl 0x%x", IoControlCode);
		break;
	}

    WdfRequestCompleteWithInformation(Request, status, bytes_returned);

    TraceEvents(TRACE_LEVEL_VERBOSE, TRACE_DEVICE, "%!FUNC! Exit");
}

/* 
    Handles taking the frames already retrieved from the card and giving them
    to the user. This is purely a helper for the fscc_port_read function.
*/    
int fscc_port_frame_read(struct fscc_port *port, char *buf, size_t buf_length, size_t *out_length)
{
    struct fscc_frame *frame = 0;
	
    TraceEvents(TRACE_LEVEL_VERBOSE, TRACE_DEVICE, 
                "%!FUNC! port 0x%p, buf 0x%p, buf_length %Iu, out_length 0x%p", 
                port, buf, buf_length, out_length);

    return_val_if_untrue(port, 0);

    frame = fscc_port_peek_front_frame(port, &port->iframes);

    if (!frame)
        return 0;
	
    *out_length = fscc_frame_get_target_length(frame);
    *out_length -= (port->append_status) ? 0 : STATUS_LENGTH;

    if (buf_length < *out_length)
        return STATUS_BUFFER_TOO_SMALL;

	memcpy(buf, fscc_frame_get_remaining_data(frame), *out_length);
	
	RemoveHeadList(&port->iframes);

    fscc_frame_delete(frame);

    TraceEvents(TRACE_LEVEL_VERBOSE, TRACE_DEVICE, "%!FUNC! Exit");

    return STATUS_SUCCESS;
}

/* 
    Handles taking the streams already retrieved from the card and giving them
    to the user. This is purely a helper for the fscc_port_read function.
*/ 
int fscc_port_stream_read(struct fscc_port *port, char *buf, size_t buf_length, size_t *out_length)
{
    TraceEvents(TRACE_LEVEL_VERBOSE, TRACE_DEVICE, 
                "%!FUNC! port 0x%p, buf 0x%p, buf_length %Iu, out_length 0x%p", 
                port, buf, buf_length, out_length);

    return_val_if_untrue(port, 0);

    *out_length = min(buf_length, (size_t)fscc_stream_get_length(port->istream));
	
	memcpy(buf, fscc_stream_get_data(port->istream), *out_length);

    fscc_stream_remove_data(port->istream, (unsigned)(*out_length));

    TraceEvents(TRACE_LEVEL_VERBOSE, TRACE_DEVICE, "%!FUNC! Exit");

    return STATUS_SUCCESS;
}

VOID read_event_handler(IN WDFQUEUE Queue, IN WDFREQUEST Request, IN size_t Length)
{	
	NTSTATUS status = STATUS_SUCCESS;
	PCHAR data_buffer = NULL;
	struct fscc_port *port = 0;
	size_t read_count = 0;

    TraceEvents(TRACE_LEVEL_VERBOSE, TRACE_DEVICE, 
                "%!FUNC! Queue 0x%p, Request 0x%p, Length %Iu", 
                Queue, Request, Length);

	port = WdfObjectGet_FSCC_PORT(WdfIoQueueGetDevice(Queue));
	
	if (Length == 0) {
		WdfRequestCompleteWithInformation(Request, STATUS_SUCCESS, Length);
		return;
	}

    if (fscc_port_using_async(port)) {
		TraceEvents(TRACE_LEVEL_WARNING, TRACE_DEVICE, 
					"use COMx nodes while in async mode");
		WdfRequestComplete(Request, STATUS_NOT_SUPPORTED);
		return;
    }

	WdfRequestForwardToIoQueue(Request, port->read_queue2);
	WdfDpcEnqueue(port->user_read_dpc);
}

void user_read_worker(WDFDPC Dpc)
{
	struct fscc_port *port = 0;
	NTSTATUS status = STATUS_SUCCESS;
	PCHAR data_buffer = NULL;
	size_t read_count = 0;
	WDFREQUEST request;
	unsigned length = 0;
	WDF_REQUEST_PARAMETERS params;

    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DEVICE, "%!FUNC! Dpc 0x%p", Dpc);
	
	port = WdfObjectGet_FSCC_PORT(WdfDpcGetParentObject(Dpc));

	if (!fscc_port_has_incoming_data(port))
		return;

	WdfSpinLockAcquire(port->iframe_spinlock);

	status = WdfIoQueueRetrieveNextRequest(port->read_queue2, &request);
	if (!NT_SUCCESS(status) && status != STATUS_NO_MORE_ENTRIES) {
		TraceEvents(TRACE_LEVEL_ERROR, TRACE_DEVICE, 
					"WdfIoQueueRetrieveNextRequest failed %!STATUS!", status);
	}
	else if (status == STATUS_NO_MORE_ENTRIES) {
		WdfSpinLockRelease(port->iframe_spinlock);
		return;
	}
	
	WDF_REQUEST_PARAMETERS_INIT(&params);
	WdfRequestGetParameters(request, &params);
	length = params.Parameters.Read.Length;
	
	status = WdfRequestRetrieveOutputBuffer(request, length, (PVOID*)&data_buffer, NULL);
	if (!NT_SUCCESS(status)) {
		TraceEvents(TRACE_LEVEL_ERROR, TRACE_DEVICE, 
					"WdfRequestRetrieveOutputBuffer failed %!STATUS!", status);
		WdfRequestComplete(request, status);
		WdfSpinLockRelease(port->iframe_spinlock);
		return;
	}

    if (fscc_port_is_streaming(port))
        status = fscc_port_stream_read(port, data_buffer, length, &read_count);
    else
        status = fscc_port_frame_read(port, data_buffer, length, &read_count);

	if (!NT_SUCCESS(status)) {
		TraceEvents(TRACE_LEVEL_WARNING, TRACE_DEVICE, 
					"fscc_port_{frame,stream}_read failed %!STATUS!", status);
		WdfRequestComplete(request, status);
		WdfSpinLockRelease(port->iframe_spinlock);
		return;
	}

	WdfRequestCompleteWithInformation(request, status, read_count);
	WdfSpinLockRelease(port->iframe_spinlock);

    TraceEvents(TRACE_LEVEL_VERBOSE, TRACE_DEVICE, "%!FUNC! Exit");
}

void EvtRequestCancel (IN WDFREQUEST Request)
{
    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DEVICE, 
                "%!FUNC! Request 0x%p", Request);

	WdfRequestComplete(Request, STATUS_CANCELLED);
}

VOID port_write_handler(IN WDFQUEUE Queue, IN WDFREQUEST Request, IN size_t Length)
{
	NTSTATUS status;
	char *data_buffer = NULL;
	struct fscc_port *port = 0;

    TraceEvents(TRACE_LEVEL_VERBOSE, TRACE_DEVICE, 
                "%!FUNC! Queue 0x%p, Request 0x%p, Length %Iu", 
                Queue, Request, Length);
	
	port = WdfObjectGet_FSCC_PORT(WdfIoQueueGetDevice(Queue));

	if (Length == 0) {
		WdfRequestCompleteWithInformation(Request, STATUS_SUCCESS, Length);
		return;
	}

    if (fscc_port_using_async(port)) {
		TraceEvents(TRACE_LEVEL_WARNING, TRACE_DEVICE, 
					"use COMx nodes while in async mode");
		WdfRequestComplete(Request, STATUS_NOT_SUPPORTED);
		return;
    }

    if (Length > fscc_port_get_output_memory_cap(port)) {
        WdfRequestComplete(Request, STATUS_BUFFER_TOO_SMALL);
		return;
	}
	
	status = WdfRequestRetrieveInputBuffer(Request, Length, (PVOID *)&data_buffer, NULL);
	if (!NT_SUCCESS(status)) {
		TraceEvents(TRACE_LEVEL_ERROR, TRACE_DEVICE, 
			"WdfRequestRetrieveInputBuffer failed %!STATUS!", status);
		WdfRequestComplete(Request, status);
		return;
	}
	
	status = fscc_port_write(port, data_buffer, (unsigned)Length);
	if (!NT_SUCCESS(status)) {
		TraceEvents(TRACE_LEVEL_ERROR, TRACE_DEVICE, 
			"fscc_port_write failed %!STATUS!", status);
		WdfRequestComplete(Request, status);
		return;
	}
	
	WdfRequestCompleteWithInformation(Request, STATUS_SUCCESS, Length);

    TraceEvents(TRACE_LEVEL_VERBOSE, TRACE_DEVICE, "%!FUNC! Exit");
}

/* Create the data structures the work horse functions use to send data. */
int fscc_port_write(struct fscc_port *port, const char *data, unsigned length)
{
    struct fscc_frame *frame = 0;
    char *temp_storage = 0;

    TraceEvents(TRACE_LEVEL_VERBOSE, TRACE_DEVICE, 
                "%!FUNC! port 0x%p, data 0x%p, length %d", 
                port, data, length);

    return_val_if_untrue(port, 0);

    /* Checks to make sure there is a clock present. */
    if (port->ignore_timeout == 0 && fscc_port_timed_out(port)) {
		TraceEvents(TRACE_LEVEL_WARNING, TRACE_DEVICE, 
					"device stalled (wrong clock mode?)");
		return STATUS_IO_TIMEOUT;
    }
	
	temp_storage = (char *)ExAllocatePoolWithTag(NonPagedPool, length, 'pmeT');
    return_val_if_untrue(temp_storage != NULL, 0);

	memcpy(temp_storage, data, length);

    frame = fscc_frame_new(length, port->card->dma, port);

    if (!frame) {
		ExFreePoolWithTag(temp_storage, 'pmeT');
		return STATUS_INSUFFICIENT_RESOURCES;
	}

    fscc_frame_add_data(frame, temp_storage, length);

	ExFreePoolWithTag(temp_storage, 'pmeT');
	
	WdfSpinLockAcquire(port->oframe_spinlock);
	InsertTailList(&port->oframes, &frame->list);
	WdfSpinLockRelease(port->oframe_spinlock);

	WdfDpcEnqueue(port->oframe_dpc);

    TraceEvents(TRACE_LEVEL_VERBOSE, TRACE_DEVICE, "%!FUNC! Exit");

    return 0;
}

UINT32 fscc_port_get_register(struct fscc_port *port, unsigned bar,
							  unsigned register_offset)
{
	unsigned offset = 0;
	UINT32 value = 0;

    TraceEvents(TRACE_LEVEL_VERBOSE, TRACE_DEVICE, 
                "%!FUNC! port 0x%p, bar %d, register_offset %d", 
                port, bar, register_offset);

	offset = port_offset(port, bar, register_offset);
	value = fscc_card_get_register(port->card, bar, offset);

    TraceEvents(TRACE_LEVEL_VERBOSE, TRACE_DEVICE, "%!FUNC! Exit");

	return value;
}

/* Basic check to see if the CE bit is set. */
unsigned fscc_port_timed_out(struct fscc_port *port)
{
    UINT32 star_value = 0;
    unsigned i = 0;

    TraceEvents(TRACE_LEVEL_VERBOSE, TRACE_DEVICE, 
                "%!FUNC! port 0x%p", 
                port);

    return_val_if_untrue(port, 0);

    for (i = 0; i < 5; i++) {
        star_value = fscc_port_get_register(port, 0, STAR_OFFSET);

        if ((star_value & CE_BIT) == 0)
            return 0;
    }

    TraceEvents(TRACE_LEVEL_VERBOSE, TRACE_DEVICE, "%!FUNC! Exit");

    return 1;
}

NTSTATUS fscc_port_set_register(struct fscc_port *port, unsigned bar,
						   unsigned register_offset, UINT32 value)
{
	unsigned offset = 0;

    TraceEvents(TRACE_LEVEL_VERBOSE, TRACE_DEVICE, 
                "%!FUNC! port 0x%p, bar %d, register_offset %d, value %d", 
                port, bar, register_offset, value);

	offset = port_offset(port, bar, register_offset);

	/* Checks to make sure there is a clock present. */
	if (register_offset == CMDR_OFFSET && port->ignore_timeout == 0
		&& fscc_port_timed_out(port)) {
		return STATUS_IO_TIMEOUT;
	}

	fscc_card_set_register(port->card, bar, offset, value);

	if (bar == 0)
		((fscc_register *)&port->register_storage)[register_offset / 4] = value;
	else if (register_offset == FCR_OFFSET)
		port->register_storage.FCR = value;

    TraceEvents(TRACE_LEVEL_VERBOSE, TRACE_DEVICE, "%!FUNC! Exit");

	return STATUS_SUCCESS;
}

/* 
    At the port level the offset will automatically be converted to the port
    specific offset.
*/
void fscc_port_get_register_rep(struct fscc_port *port, unsigned bar,
                                unsigned register_offset, char *buf,
                                unsigned byte_count)
{
    unsigned offset = 0;

    TraceEvents(TRACE_LEVEL_VERBOSE, TRACE_DEVICE, 
                "%!FUNC! port 0x%p, bar %d, register_offset %d, buf 0x%p, byte_count %d", 
                port, bar, register_offset, buf, byte_count);

    return_if_untrue(port);
    return_if_untrue(bar <= 2);
    return_if_untrue(buf);
    return_if_untrue(byte_count > 0);

    offset = port_offset(port, bar, register_offset);

    fscc_card_get_register_rep(port->card, bar, offset, buf, byte_count);

    TraceEvents(TRACE_LEVEL_VERBOSE, TRACE_DEVICE, "%!FUNC! Exit");
}

/* 
    At the port level the offset will automatically be converted to the port
    specific offset.
*/
void fscc_port_set_register_rep(struct fscc_port *port, unsigned bar,
                                unsigned register_offset, const char *data,
                                unsigned byte_count)
{
    unsigned offset = 0;

    TraceEvents(TRACE_LEVEL_VERBOSE, TRACE_DEVICE, 
                "%!FUNC! port 0x%p, bar %d, register_offset %d, data 0x%p, byte_count %d", 
                port, bar, register_offset, data, byte_count);

    return_if_untrue(port);
    return_if_untrue(bar <= 2);
    return_if_untrue(data);
    return_if_untrue(byte_count > 0);

    offset = port_offset(port, bar, register_offset);

    TraceEvents(TRACE_LEVEL_VERBOSE, TRACE_DEVICE, "%!FUNC! Exit");

    fscc_card_set_register_rep(port->card, bar, offset, data, byte_count);
}

NTSTATUS fscc_port_set_registers(struct fscc_port *port,
							const struct fscc_registers *regs)
{
	unsigned stalled = 0;
	unsigned i = 0;

    TraceEvents(TRACE_LEVEL_VERBOSE, TRACE_DEVICE, 
                "%!FUNC! port 0x%p, regs 0x%p", 
                port, regs);

	for (i = 0; i < sizeof(*regs) / sizeof(fscc_register); i++) {
		unsigned register_offset = i * 4;

		if (is_read_only_register(register_offset)
			|| ((fscc_register *)regs)[i] < 0) {
			continue;
		}

		if (register_offset <= DPLLR_OFFSET) {
			if (fscc_port_set_register(port, 0, register_offset, (UINT32)(((fscc_register *)regs)[i])) == STATUS_IO_TIMEOUT)
				stalled = 1;
		}
		else {
			fscc_port_set_register(port, 2, FCR_OFFSET,
								   (UINT32)(((fscc_register *)regs)[i]));
		}
	}

    TraceEvents(TRACE_LEVEL_VERBOSE, TRACE_DEVICE, "%!FUNC! Exit");

	return (stalled) ? STATUS_IO_TIMEOUT : STATUS_SUCCESS;
}

void fscc_port_get_registers(struct fscc_port *port,
							 struct fscc_registers *regs)
{
	unsigned i = 0;

    TraceEvents(TRACE_LEVEL_VERBOSE, TRACE_DEVICE, 
                "%!FUNC! port 0x%p, regs 0x%p", 
                port, regs);
	
	for (i = 0; i < sizeof(*regs) / sizeof(fscc_register); i++) {		
		if (((fscc_register *)regs)[i] != FSCC_UPDATE_VALUE)
			continue;

		if (i * 4 <= MAX_OFFSET) {
			((fscc_register *)regs)[i] = fscc_port_get_register(port, 0, i * 4);
		}
		else {
			((fscc_register *)regs)[i] = fscc_port_get_register(port, 2,
																FCR_OFFSET);
		}
	}

    TraceEvents(TRACE_LEVEL_VERBOSE, TRACE_DEVICE, "%!FUNC! Exit");
}

UCHAR fscc_port_get_FREV(struct fscc_port *port)
{
	UINT32 vstr_value = 0;

    TraceEvents(TRACE_LEVEL_VERBOSE, TRACE_DEVICE, 
                "%!FUNC! port 0x%p", 
                port);

	vstr_value = fscc_port_get_register(port, 0, VSTR_OFFSET);

    TraceEvents(TRACE_LEVEL_VERBOSE, TRACE_DEVICE, "%!FUNC! Exit");

	return (UCHAR)((vstr_value & 0x000000FF));
}

UCHAR fscc_port_get_PREV(struct fscc_port *port)
{
	UINT32 vstr_value = 0;

    TraceEvents(TRACE_LEVEL_VERBOSE, TRACE_DEVICE, 
                "%!FUNC! port 0x%p", 
                port);

	vstr_value = fscc_port_get_register(port, 0, VSTR_OFFSET);

    TraceEvents(TRACE_LEVEL_VERBOSE, TRACE_DEVICE, "%!FUNC! Exit");

	return (UCHAR)((vstr_value & 0x0000FF00) >> 8);
}

UINT16 fscc_port_get_PDEV(struct fscc_port *port)
{
    UINT32 vstr_value = 0;

    TraceEvents(TRACE_LEVEL_VERBOSE, TRACE_DEVICE, 
                "%!FUNC! port 0x%p", 
                port);

    vstr_value = fscc_port_get_register(port, 0, VSTR_OFFSET);

    return (UINT16)((vstr_value & 0xFFFF0000) >> 16);
}

/* Locks oframe_spinlock. */
NTSTATUS fscc_port_execute_TRES(struct fscc_port *port)
{
    int status;

    TraceEvents(TRACE_LEVEL_VERBOSE, TRACE_DEVICE, 
                "%!FUNC! port 0x%p", 
                port);

    return_val_if_untrue(port, 0);

	WdfSpinLockAcquire(port->oframe_spinlock);

    status = fscc_port_set_register(port, 0, CMDR_OFFSET, 0x08000000);
	
	WdfSpinLockRelease(port->oframe_spinlock);

    TraceEvents(TRACE_LEVEL_VERBOSE, TRACE_DEVICE, "%!FUNC! Exit");

    return status;
}

/* Locks iframe_spinlock. */
NTSTATUS fscc_port_execute_RRES(struct fscc_port *port)
{
    NTSTATUS status;

    TraceEvents(TRACE_LEVEL_VERBOSE, TRACE_DEVICE, 
                "%!FUNC! port 0x%p", 
                port);

    return_val_if_untrue(port, 0);

	WdfSpinLockAcquire(port->iframe_spinlock);

    status = fscc_port_set_register(port, 0, CMDR_OFFSET, 0x00020000);
	
	WdfSpinLockRelease(port->iframe_spinlock);

    TraceEvents(TRACE_LEVEL_VERBOSE, TRACE_DEVICE, "%!FUNC! Exit");

    return status;
}

/* Locks iframe_spinlock. */
NTSTATUS fscc_port_purge_rx(struct fscc_port *port)

{
	NTSTATUS status;

    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DEVICE, 
                "%!FUNC! port 0x%p", 
                port);

    return_val_if_untrue(port, 0);

    /* Locks iframe_spinlock. */
	status = fscc_port_execute_RRES(port);
	if (!NT_SUCCESS(status)) {
		TraceEvents(TRACE_LEVEL_ERROR, TRACE_DEVICE, 
			"fscc_port_execute_RRES failed %!STATUS!", status);
		return status;
	}

    /* Locks iframe_spinlock. */
    fscc_port_clear_iframes(port, 1);
	
	WdfSpinLockAcquire(port->iframe_spinlock);
    fscc_stream_remove_data(port->istream,
                            fscc_stream_get_length(port->istream));
	WdfSpinLockRelease(port->iframe_spinlock);

    TraceEvents(TRACE_LEVEL_VERBOSE, TRACE_DEVICE, "%!FUNC! Exit");

    return STATUS_SUCCESS;
}

/* Locks oframe_spinlock. */
NTSTATUS fscc_port_purge_tx(struct fscc_port *port)
{
    int error_code = 0;

    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DEVICE, 
                "%!FUNC! port 0x%p", 
                port);

    return_val_if_untrue(port, 0);

    /* Locks oframe_spinlock. */
    if ((error_code = fscc_port_execute_TRES(port)) < 0)
        return error_code;

    /* Locks oframe_spinlock. */
    fscc_port_clear_oframes(port, 1);

	//TODO
    //wake_up_interruptible(&port->output_queue);

    TraceEvents(TRACE_LEVEL_VERBOSE, TRACE_DEVICE, "%!FUNC! Exit");

    return STATUS_SUCCESS;
}

void fscc_port_set_append_status(struct fscc_port *port, unsigned value)
{
    TraceEvents(TRACE_LEVEL_VERBOSE, TRACE_DEVICE, 
                "%!FUNC! port 0x%p, value %d", 
                port, value);

    return_if_untrue(port);

    TraceEvents(TRACE_LEVEL_VERBOSE, TRACE_DEVICE, "%!FUNC! Exit");

    port->append_status = (value) ? 1 : 0;
}

unsigned fscc_port_get_append_status(struct fscc_port *port)
{
    TraceEvents(TRACE_LEVEL_VERBOSE, TRACE_DEVICE, 
                "%!FUNC! port 0x%p", 
                port);

    return_val_if_untrue(port, 0);

    TraceEvents(TRACE_LEVEL_VERBOSE, TRACE_DEVICE, "%!FUNC! Exit");

    return port->append_status;
}

void fscc_port_set_ignore_timeout(struct fscc_port *port,
                                  unsigned ignore_timeout)
{
    TraceEvents(TRACE_LEVEL_VERBOSE, TRACE_DEVICE, 
                "%!FUNC! port 0x%p, ignore_timeout %d", 
                port, ignore_timeout);

    return_if_untrue(port);

    TraceEvents(TRACE_LEVEL_VERBOSE, TRACE_DEVICE, "%!FUNC! Exit");

    port->ignore_timeout = (ignore_timeout) ? 1 : 0;
}

unsigned fscc_port_get_ignore_timeout(struct fscc_port *port)
{
    TraceEvents(TRACE_LEVEL_VERBOSE, TRACE_DEVICE, 
                "%!FUNC! port 0x%p", 
                port);

    return_val_if_untrue(port, 0);

    TraceEvents(TRACE_LEVEL_VERBOSE, TRACE_DEVICE, "%!FUNC! Exit");

    return port->ignore_timeout;
}

/* Returns -EINVAL if you set an incorrect transmit modifier */
NTSTATUS fscc_port_set_tx_modifiers(struct fscc_port *port, int value)
{
    TraceEvents(TRACE_LEVEL_VERBOSE, TRACE_DEVICE, 
                "%!FUNC! port 0x%p, value %d", 
                port, value);

    return_val_if_untrue(port, 0);

    switch (value) {
        case XF:
        case XF|TXT:
        case XF|TXEXT:
        case XREP:
        case XREP|TXT:
        case XREP|TXEXT:
            if (port->tx_modifiers != value) {
				TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DEVICE, 
							"tx modifiers 0x%x => 0x%x", 
							port->tx_modifiers, value);
            }
            else {
				TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DEVICE, 
							"tx modifiers 0x%x", value);
            }
            
            port->tx_modifiers = value;
            
            break;
            
        default:
			TraceEvents(TRACE_LEVEL_WARNING, TRACE_DEVICE, 
						"tx modifiers (invalid value 0x%x)", value);
            
            return STATUS_INVALID_PARAMETER;
    }

    TraceEvents(TRACE_LEVEL_VERBOSE, TRACE_DEVICE, "%!FUNC! Exit");

    return STATUS_SUCCESS;
}

unsigned fscc_port_get_tx_modifiers(struct fscc_port *port)
{
    TraceEvents(TRACE_LEVEL_VERBOSE, TRACE_DEVICE, 
                "%!FUNC! port 0x%p", 
                port);

    return_val_if_untrue(port, 0);

    TraceEvents(TRACE_LEVEL_VERBOSE, TRACE_DEVICE, "%!FUNC! Exit");

    return port->tx_modifiers;
}

unsigned fscc_port_get_input_memory_cap(struct fscc_port *port)
{
    TraceEvents(TRACE_LEVEL_VERBOSE, TRACE_DEVICE, 
                "%!FUNC! port 0x%p", 
                port);

    return_val_if_untrue(port, 0);

    TraceEvents(TRACE_LEVEL_VERBOSE, TRACE_DEVICE, "%!FUNC! Exit");

    return port->memory_cap.input;
}

unsigned fscc_port_get_output_memory_cap(struct fscc_port *port)
{
    TraceEvents(TRACE_LEVEL_VERBOSE, TRACE_DEVICE, 
                "%!FUNC! port 0x%p", 
                port);

    return_val_if_untrue(port, 0);

    TraceEvents(TRACE_LEVEL_VERBOSE, TRACE_DEVICE, "%!FUNC! Exit");

    return port->memory_cap.output;
}

void fscc_port_set_memory_cap(struct fscc_port *port,
                              struct fscc_memory_cap *value)
{
    TraceEvents(TRACE_LEVEL_VERBOSE, TRACE_DEVICE, 
                "%!FUNC! port 0x%p, value 0x%p", 
                port, value);

    return_if_untrue(port);
    return_if_untrue(value);

    if (value->input >= 0) {
        if (port->memory_cap.input != value->input) {
			TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DEVICE, 
						"memory cap (input) %i => %i", 
						port->memory_cap.input, value->input);
        }
        else {
			TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DEVICE, 
						"memory cap (input) %i", 
						value->input);
        }
        
        port->memory_cap.input = value->input;
    }

    if (value->output >= 0) {
        if (port->memory_cap.output != value->output) {
			TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DEVICE, 
						"memory cap (output) %i => %i", 
						port->memory_cap.output, value->output);
        }
        else {
			TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DEVICE, 
						"memory cap (output) %i", 
						value->output);
        }
        
        port->memory_cap.output = value->output;
    }

    TraceEvents(TRACE_LEVEL_VERBOSE, TRACE_DEVICE, "%!FUNC! Exit");
}

#define STRB_BASE 0x00000008
#define DTA_BASE 0x00000001
#define CLK_BASE 0x00000002

void fscc_port_set_clock_bits(struct fscc_port *port,
                              unsigned char *clock_data)
{
    UINT32 orig_fcr_value = 0;
    UINT32 new_fcr_value = 0;
    int j = 0; // Must be signed because we are going backwards through the array
    int i = 0; // Must be signed because we are going backwards through the array
    unsigned strb_value = STRB_BASE;
    unsigned dta_value = DTA_BASE;
    unsigned clk_value = CLK_BASE;
    UINT32 *data = 0;
    unsigned data_index = 0;

    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DEVICE, 
                "%!FUNC! port 0x%p, clock_data 0x%p", 
                port, clock_data);

    return_if_untrue(port);


#ifdef DISABLE_XTAL
    clock_data[15] &= 0xfb;
#else
    /* This enables XTAL on all cards except green FSCC cards with a revision
       greater than 6. Some old protoype SuperFSCC cards will need to manually
       disable XTAL as they are not supported in this driver by default. */
    if (fscc_port_get_PDEV(port) == 0x0f && fscc_port_get_PREV(port) <= 6)
        clock_data[15] &= 0xfb;
    else
        clock_data[15] |= 0x04;
#endif


	data = (UINT32 *)ExAllocatePoolWithTag(NonPagedPool, sizeof(UINT32) * 323, 'stiB');

	if (data == NULL) {
		TraceEvents(TRACE_LEVEL_ERROR, TRACE_DEVICE, 
			"ExAllocatePoolWithTag failed");
		return;
	}

    if (port->channel == 1) {
        strb_value <<= 0x08;
        dta_value <<= 0x08;
        clk_value <<= 0x08;
    }

    orig_fcr_value = fscc_card_get_register(port->card, 2, FCR_OFFSET);

    data[data_index++] = new_fcr_value = orig_fcr_value & 0xfffff0f0;

    for (i = 19; i >= 0; i--) {
        for (j = 7; j >= 0; j--) {
            int bit = ((clock_data[i] >> j) & 1);

            if (bit)
                new_fcr_value |= dta_value; /* Set data bit */
            else
                new_fcr_value &= ~dta_value; /* Clear clock bit */

            data[data_index++] = new_fcr_value |= clk_value; /* Set clock bit */
            data[data_index++] = new_fcr_value &= ~clk_value; /* Clear clock bit */
        }
    }

    new_fcr_value = orig_fcr_value & 0xfffff0f0;

    new_fcr_value |= strb_value; /* Set strobe bit */
    new_fcr_value &= ~clk_value; /* Clear clock bit */

    data[data_index++] = new_fcr_value;
    data[data_index++] = orig_fcr_value;

    fscc_port_set_register_rep(port, 2, FCR_OFFSET, (char *)data, data_index * 4);
	
    ExFreePoolWithTag (data, 'stiB');

    TraceEvents(TRACE_LEVEL_VERBOSE, TRACE_DEVICE, "%!FUNC! Exit");
}

void clear_frames(struct fscc_frame **pending_frame,
                  LIST_ENTRY *frame_list, WDFSPINLOCK *spinlock)
{
    TraceEvents(TRACE_LEVEL_VERBOSE, TRACE_DEVICE, 
                "%!FUNC! pending_frame 0x%p, frame_list 0x%p, spinlock 0x%p", 
                pending_frame, frame_list, spinlock);

    if (spinlock)
		WdfSpinLockAcquire(*spinlock);
	
    if (*pending_frame) {
		fscc_frame_delete(*pending_frame);
        *pending_frame = 0;
    }
	
	empty_frame_list(frame_list);

    if (spinlock)
		WdfSpinLockRelease(*spinlock);

    TraceEvents(TRACE_LEVEL_VERBOSE, TRACE_DEVICE, "%!FUNC! Exit");
}

void fscc_port_clear_iframes(struct fscc_port *port, unsigned lock)
{
    WDFSPINLOCK *spinlock = 0;

    TraceEvents(TRACE_LEVEL_VERBOSE, TRACE_DEVICE, 
                "%!FUNC! port 0x%p, lock %d", 
                port, lock);

    return_if_untrue(port);

    spinlock = (lock) ? &port->iframe_spinlock : 0;

    clear_frames(&port->pending_iframe, &port->iframes,
                 spinlock);

    TraceEvents(TRACE_LEVEL_VERBOSE, TRACE_DEVICE, "%!FUNC! Exit");
}

void fscc_port_clear_oframes(struct fscc_port *port, unsigned lock)
{
    WDFSPINLOCK *spinlock = 0;

    TraceEvents(TRACE_LEVEL_VERBOSE, TRACE_DEVICE, 
                "%!FUNC! port 0x%p, lock %d", 
                port, lock);

    return_if_untrue(port);

    spinlock = (lock) ? &port->oframe_spinlock : 0;

    clear_frames(&port->pending_oframe, &port->oframes,
                 spinlock);

    TraceEvents(TRACE_LEVEL_VERBOSE, TRACE_DEVICE, "%!FUNC! Exit");
}

void empty_frame_list(LIST_ENTRY *frames)
{
    TraceEvents(TRACE_LEVEL_VERBOSE, TRACE_DEVICE, 
                "%!FUNC! frames 0x%p", 
                frames);

    return_if_untrue(frames);
	
	while (!IsListEmpty(frames)) {	
		LIST_ENTRY *frame_iter = 0;
		struct fscc_frame *frame = 0;	

		frame_iter = RemoveHeadList(frames);
		frame = CONTAINING_RECORD(frame_iter, FSCC_FRAME, list);	

		fscc_frame_delete(frame);
	}

    TraceEvents(TRACE_LEVEL_VERBOSE, TRACE_DEVICE, "%!FUNC! Exit");
}

unsigned fscc_port_using_async(struct fscc_port *port)
{
    TraceEvents(TRACE_LEVEL_VERBOSE, TRACE_DEVICE, 
                "%!FUNC! port 0x%p", 
                port);

    return_val_if_untrue(port, 0);

    switch (port->channel) {
    case 0:
        return port->register_storage.FCR & 0x01000000;

    case 1:
        return port->register_storage.FCR & 0x02000000;
    }

    TraceEvents(TRACE_LEVEL_VERBOSE, TRACE_DEVICE, "%!FUNC! Exit");

    return 0;
}

unsigned calculate_memory_usage(struct fscc_frame *pending_frame,
                                LIST_ENTRY *frame_list,
                                WDFSPINLOCK *spinlock)
{
	LIST_ENTRY *frame_iter = 0;
    unsigned memory = 0;

    TraceEvents(TRACE_LEVEL_VERBOSE, TRACE_DEVICE, 
                "%!FUNC! pending_frame 0x%p, frame_list 0x%p, spinlock 0x%p", 
                pending_frame, frame_list, spinlock);

    return_val_if_untrue(frame_list, 0);

    if (spinlock)
		WdfSpinLockAcquire(*spinlock);
	
	frame_iter = frame_list->Flink;
	while (frame_iter != frame_list->Blink) {
		struct fscc_frame *current_frame = 0;

		current_frame = CONTAINING_RECORD(frame_iter, FSCC_FRAME, list);	
		memory += fscc_frame_get_current_length(current_frame);

		frame_iter = frame_iter->Flink;
	}

    if (pending_frame)
        memory += fscc_frame_get_current_length(pending_frame);

    if (spinlock)
		WdfSpinLockRelease(*spinlock);

    TraceEvents(TRACE_LEVEL_VERBOSE, TRACE_DEVICE, "%!FUNC! Exit");

    return memory;
}

unsigned fscc_port_get_output_memory_usage(struct fscc_port *port,
                                           unsigned lock)
{
    WDFSPINLOCK *spinlock = 0;
	unsigned value = 0;

    TraceEvents(TRACE_LEVEL_VERBOSE, TRACE_DEVICE, 
                "%!FUNC! port 0x%p, lock %d", 
                port, lock);

    return_val_if_untrue(port, 0);

    spinlock = (lock) ? &port->oframe_spinlock : 0;
	value = calculate_memory_usage(port->pending_oframe, &port->oframes,
                                   spinlock);

    TraceEvents(TRACE_LEVEL_VERBOSE, TRACE_DEVICE, "%!FUNC! exit");

    return value;
}

unsigned fscc_port_get_input_memory_usage(struct fscc_port *port,
                                          unsigned lock)
{
    WDFSPINLOCK *spinlock = 0;
    unsigned memory = 0;

    TraceEvents(TRACE_LEVEL_VERBOSE, TRACE_DEVICE, 
                "%!FUNC! port 0x%p, lock %d", 
                port, lock);

    return_val_if_untrue(port, 0);

    spinlock = (lock) ? &port->iframe_spinlock : 0;

    memory = calculate_memory_usage(port->pending_iframe, &port->iframes,
                                    spinlock);

    memory += fscc_stream_get_length(port->istream);

    TraceEvents(TRACE_LEVEL_VERBOSE, TRACE_DEVICE, "%!FUNC! Exit");

    return memory;
}

//TODO: Should spinlock
struct fscc_frame *fscc_port_peek_front_frame(struct fscc_port *port,
                                              LIST_ENTRY *frames)
{
    TraceEvents(TRACE_LEVEL_VERBOSE, TRACE_DEVICE, 
                "%!FUNC! port 0x%p, frames 0x%p", 
                port, frames);

    return_val_if_untrue(port, 0);
    return_val_if_untrue(frames, 0);


	if (IsListEmpty(frames))
		return 0;

    TraceEvents(TRACE_LEVEL_VERBOSE, TRACE_DEVICE, "%!FUNC! Exit");
	
	return CONTAINING_RECORD(frames->Flink, FSCC_FRAME, list);
}

/* TODO: Test whether termination bytes will frame data. If so add the
   scenario here. */
unsigned fscc_port_is_streaming(struct fscc_port *port)
{
    unsigned transparent_mode = 0;
    unsigned rlc_mode = 0;
    unsigned fsc_mode = 0;

    TraceEvents(TRACE_LEVEL_VERBOSE, TRACE_DEVICE, 
                "%!FUNC! port 0x%p", 
                port);
    
    return_val_if_untrue(port, 0);

    transparent_mode = ((port->register_storage.CCR0 & 0x3) == 0x2) ? 1 : 0;
    rlc_mode = (port->register_storage.CCR2 & 0xffff0000) ? 1 : 0;
    fsc_mode = (port->register_storage.CCR0 & 0x700) ? 1 : 0;

    TraceEvents(TRACE_LEVEL_VERBOSE, TRACE_DEVICE, "%!FUNC! Exit");
    
    return (transparent_mode && !(rlc_mode || fsc_mode)) ? 1 : 0;
}

BOOLEAN has_frames(LIST_ENTRY *frames, WDFSPINLOCK *spinlock)
{
    BOOLEAN empty = 0;

    TraceEvents(TRACE_LEVEL_VERBOSE, TRACE_DEVICE, 
                "%!FUNC! frames 0x%p, spinlock 0x%p", 
                frames, spinlock);

    return_val_if_untrue(frames, 0);

    if (spinlock)
		WdfSpinLockAcquire(*spinlock);

	empty = IsListEmpty(frames);
	
	if (empty) {} // Silences PREfast warning 28193

    if (spinlock)
		WdfSpinLockRelease(*spinlock);

    TraceEvents(TRACE_LEVEL_VERBOSE, TRACE_DEVICE, "%!FUNC! Exit");

    return !empty;
}

BOOLEAN fscc_port_has_iframes(struct fscc_port *port, unsigned lock)
{
    WDFSPINLOCK *spinlock = 0;

    TraceEvents(TRACE_LEVEL_VERBOSE, TRACE_DEVICE, 
                "%!FUNC! port 0x%pk %d lock", 
                port, lock);

    return_val_if_untrue(port, 0);


    spinlock = (lock) ? &port->iframe_spinlock : 0;

    TraceEvents(TRACE_LEVEL_VERBOSE, TRACE_DEVICE, "%!FUNC! Exit");

    return has_frames(&port->iframes, spinlock);
}

BOOLEAN fscc_port_has_oframes(struct fscc_port *port, unsigned lock)
{
    WDFSPINLOCK *spinlock = 0;

    TraceEvents(TRACE_LEVEL_VERBOSE, TRACE_DEVICE, 
                "%!FUNC! port 0x%p, lock %d", 
                port, lock);

    return_val_if_untrue(port, 0);

    spinlock = (lock) ? &port->oframe_spinlock : 0;

    TraceEvents(TRACE_LEVEL_VERBOSE, TRACE_DEVICE, "%!FUNC! Exit");

    return has_frames(&port->oframes, spinlock);
}

unsigned fscc_port_has_dma(struct fscc_port *port)
{
    return_val_if_untrue(port, 0);

    TraceEvents(TRACE_LEVEL_VERBOSE, TRACE_DEVICE, 
                "%!FUNC! port 0x%p", 
                port);
    
    //if (force_fifo)
       //return 0;

    TraceEvents(TRACE_LEVEL_VERBOSE, TRACE_DEVICE, "%!FUNC! Exit");
        
    return port->card->dma;
}

UINT32 fscc_port_get_TXCNT(struct fscc_port *port)
{
    UINT32 fifo_bc_value = 0;

    TraceEvents(TRACE_LEVEL_VERBOSE, TRACE_DEVICE, 
                "%!FUNC! port 0x%p", 
                port);

    return_val_if_untrue(port, 0);

    fifo_bc_value = fscc_port_get_register(port, 0, FIFO_BC_OFFSET);

    TraceEvents(TRACE_LEVEL_VERBOSE, TRACE_DEVICE, "%!FUNC! Exit");

    return (fifo_bc_value & 0x1FFF0000) >> 16;
}

void fscc_port_execute_transmit(struct fscc_port *port)
{
    unsigned command_register = 0;
    unsigned command_value = 0;
    unsigned command_bar = 0;

    TraceEvents(TRACE_LEVEL_VERBOSE, TRACE_DEVICE, 
                "%!FUNC! port 0x%p", 
                port);

    return_if_untrue(port);

    if (fscc_port_has_dma(port)) {
        command_bar = 2;
        command_register = DMACCR_OFFSET;
        command_value = 0x00000002;

        if (port->tx_modifiers & XREP)
            command_value |= 0x40000000;

        if (port->tx_modifiers & TXT)
            command_value |= 0x10000000;

        if (port->tx_modifiers & TXEXT)
            command_value |= 0x20000000;
    }
    else {
        command_bar = 0;
        command_register = CMDR_OFFSET;
        command_value = 0x01000000;

        if (port->tx_modifiers & XREP)
            command_value |= 0x02000000;    

        if (port->tx_modifiers & TXT)
            command_value |= 0x10000000;
        
        if (port->tx_modifiers & TXEXT) 
            command_value |= 0x20000000;
    }

    fscc_port_set_register(port, command_bar, command_register, command_value);

    TraceEvents(TRACE_LEVEL_VERBOSE, TRACE_DEVICE, "%!FUNC! Exit");
}

unsigned fscc_port_get_RFCNT(struct fscc_port *port)
{
    UINT32 fifo_fc_value = 0;

    TraceEvents(TRACE_LEVEL_VERBOSE, TRACE_DEVICE, 
                "%!FUNC! port 0x%p", 
                port);

    return_val_if_untrue(port, 0);

    fifo_fc_value = fscc_port_get_register(port, 0, FIFO_FC_OFFSET);

    TraceEvents(TRACE_LEVEL_VERBOSE, TRACE_DEVICE, "%!FUNC! Exit");

    return (unsigned)(fifo_fc_value & 0x00000eff);
}

UINT32 fscc_port_get_RXCNT(struct fscc_port *port)
{
    UINT32 fifo_bc_value = 0;

    TraceEvents(TRACE_LEVEL_VERBOSE, TRACE_DEVICE, 
                "%!FUNC! port 0x%p", 
                port);

    return_val_if_untrue(port, 0);

    fifo_bc_value = fscc_port_get_register(port, 0, FIFO_BC_OFFSET);

    TraceEvents(TRACE_LEVEL_VERBOSE, TRACE_DEVICE, "%!FUNC! Exit");

    return fifo_bc_value & 0x00003FFF;
}

void fscc_port_reset_timer(struct fscc_port *port)
{   
	WdfTimerStart(port->timer, WDF_ABS_TIMEOUT_IN_MS(TIMER_DELAY_MS));
}

/* Count is for streaming mode where we need to check there is enough
   streaming data.

   Locks iframe_spinlock.
*/
unsigned fscc_port_has_incoming_data(struct fscc_port *port)
{
	unsigned status = 0;

    return_val_if_untrue(port, 0);
	
	WdfSpinLockAcquire(port->iframe_spinlock);

    if (fscc_port_is_streaming(port))
        status = (fscc_stream_is_empty(port->istream)) ? 0 : 1;
    else if (fscc_port_has_iframes(port, 0))
        status = 1;

	WdfSpinLockRelease(port->iframe_spinlock);

    return status;
}

//key may be null
NTSTATUS fscc_port_registry_registers_create(struct fscc_port *port, WDFKEY *key)
{	
	NTSTATUS status;
	WDFKEY parent_key;
	UNICODE_STRING key_str;

	RtlInitUnicodeString(&key_str, L"Registers");

	status = fscc_port_registry_open(port, &parent_key);
	if (!NT_SUCCESS(status)) {
		TraceEvents(TRACE_LEVEL_ERROR, TRACE_DEVICE, 
			"WdfDeviceOpenRegistryKey failed %!STATUS!", status);
		return status;
	}

	status = registry_create_key(parent_key, &key_str, key);

	WdfRegistryClose(parent_key);

	return status;
}

//key may be null
NTSTATUS fscc_port_registry_create_pre(struct fscc_card *card, unsigned channel, WDFKEY *key)
{	
	NTSTATUS status;
	WDFKEY parent_key;
	WCHAR key_str_buffer[20];
	UNICODE_STRING key_str;

	RtlInitEmptyUnicodeString(&key_str, key_str_buffer, sizeof(key_str_buffer));
	status = RtlUnicodeStringPrintf(&key_str, L"Channel %i", channel);
	if (!NT_SUCCESS(status)) {
        TraceEvents(TRACE_LEVEL_ERROR, TRACE_DEVICE, 
			"RtlUnicodeStringPrintf failed %!STATUS!", status);
		return status;
	}
	
	status = fscc_card_registry_open(card, &parent_key);
	if (!NT_SUCCESS(status)) {
		TraceEvents(TRACE_LEVEL_ERROR, TRACE_DEVICE, 
			"WdfDeviceOpenRegistryKey failed %!STATUS!", status);
		return status;
	}

	status = registry_create_key(parent_key, &key_str, key);

	WdfRegistryClose(parent_key);

	return status;
}

NTSTATUS fscc_port_registry_open_pre(struct fscc_card *card, unsigned channel, WDFKEY *key)
{
	return fscc_port_registry_create_pre(card, channel, key);
}

NTSTATUS fscc_port_registry_open(struct fscc_port *port, WDFKEY *key)
{
	if (!port)
		return STATUS_INVALID_PARAMETER;

	return fscc_port_registry_open_pre(port->card, port->channel, key);
}

NTSTATUS fscc_port_registry_get_ulong_pre(struct fscc_card *card, unsigned channel, PUNICODE_STRING key_str, ULONG *value)
{	
	NTSTATUS status;
	WDFKEY port_key;

	status = fscc_port_registry_open_pre(card, channel, &port_key);
	if (!NT_SUCCESS(status) && status != STATUS_OBJECT_NAME_NOT_FOUND) {
		TraceEvents(TRACE_LEVEL_ERROR, TRACE_DEVICE, 
			"WdfDeviceOpenRegistryKey failed %!STATUS!", status);
		return status;
	}

	if (status == STATUS_OBJECT_NAME_NOT_FOUND)
		return status;
	
	status = registry_get_ulong(port_key, key_str, value);
	
	WdfRegistryClose(port_key);

	return status;
}

NTSTATUS fscc_port_registry_set_ulong_pre(struct fscc_card *card, unsigned channel, PUNICODE_STRING key_str, ULONG value)
{
	NTSTATUS status;
	WDFKEY port_key;

	status = fscc_port_registry_open_pre(card, channel, &port_key);
	if (!NT_SUCCESS(status) && status != STATUS_OBJECT_NAME_NOT_FOUND) {
		TraceEvents(TRACE_LEVEL_ERROR, TRACE_DEVICE, 
			"WdfDeviceOpenRegistryKey failed %!STATUS!", status);
		return status;
	}

	status = registry_set_ulong(port_key, key_str, value);
	
	WdfRegistryClose(port_key);

	return status;
}

NTSTATUS fscc_port_registry_get_portnum_pre(struct fscc_card *card, unsigned channel, unsigned *portnum)
{	
	NTSTATUS status;
	ULONG value;
	UNICODE_STRING key_str;

	RtlInitUnicodeString(&key_str, L"PortNum");

	status = fscc_port_registry_get_ulong_pre(card, channel, &key_str, &value);

	*portnum = (unsigned)value;

	return status;
}

NTSTATUS fscc_port_registry_set_portnum_pre(struct fscc_card *card, unsigned channel, unsigned portnum)
{	
	UNICODE_STRING key_str;

	RtlInitUnicodeString(&key_str, L"PortNum");

	return fscc_port_registry_set_ulong_pre(card, channel, &key_str, (ULONG)portnum);
}