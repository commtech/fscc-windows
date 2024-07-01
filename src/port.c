/*
Copyright 2023 Commtech, Inc.

Permission is hereby granted, free of charge, to any person obtaining a copy 
of this software and associated documentation files (the "Software"), to deal 
in the Software without restriction, including without limitation the rights 
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell 
copies of the Software, and to permit persons to whom the Software is 
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in 
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR 
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, 
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE 
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER 
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, 
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN 
THE SOFTWARE.
*/


#include "port.h"
#include "utils.h"
#include "isr.h"
#include "public.h"
#include "driver.h"
#include "debug.h"
#include "io.h"

#include <ntddser.h>
#include <ntstrsafe.h>
#include <devpkey.h>

#if defined(EVENT_TRACING)
#include "port.tmh"
#endif

#define NUM_CLOCK_BYTES 20
#define TIMER_DELAY_MS 250

EVT_WDF_IO_QUEUE_IO_DEVICE_CONTROL FsccEvtIoDeviceControl;
EVT_WDF_DEVICE_FILE_CREATE FsccDeviceFileCreate;
EVT_WDF_FILE_CLOSE FsccFileClose;
EVT_WDF_DEVICE_PREPARE_HARDWARE FsccEvtDevicePrepareHardware;
EVT_WDF_DEVICE_RELEASE_HARDWARE FsccEvtDeviceReleaseHardware;

NTSTATUS fscc_port_get_port_num(struct fscc_port *port, unsigned *port_num);
NTSTATUS fscc_port_set_port_num(struct fscc_port *port, unsigned value);
NTSTATUS fscc_port_get_default_memory(struct fscc_port *port, struct fscc_memory *memory);
NTSTATUS fscc_port_get_default_registers(struct fscc_port *port, struct fscc_registers *regs);
NTSTATUS fscc_port_set_friendly_name(_In_ WDFDEVICE Device, unsigned portnum);

#pragma warning( disable: 4267 )

struct fscc_port *fscc_port_new(WDFDRIVER Driver,
IN PWDFDEVICE_INIT DeviceInit)
{
	NTSTATUS status = STATUS_SUCCESS;
	struct fscc_port *port = 0;
	WDF_OBJECT_ATTRIBUTES attributes;
	WDFDEVICE device;
	WDF_IO_QUEUE_CONFIG queue_config;

	WCHAR device_name_buffer[20];
	UNICODE_STRING device_name;

	WCHAR dos_name_buffer[30];
	UNICODE_STRING dos_name;

	WDF_DEVICE_STATE    device_state;

	WDF_PNPPOWER_EVENT_CALLBACKS  pnpPowerCallbacks;
	WDF_DEVICE_PNP_CAPABILITIES pnpCaps;

	WDF_INTERRUPT_CONFIG  interruptConfig;
	WDF_OBJECT_ATTRIBUTES  interruptAttributes;

	WDF_DPC_CONFIG dpcConfig;
	WDF_OBJECT_ATTRIBUTES dpcAttributes;

	WDF_FILEOBJECT_CONFIG deviceConfig;

	static int instance = 0;
	int last_port_num = -1;
	unsigned port_num = 0;

	status = fscc_driver_get_last_port_num(Driver, &last_port_num);
	if (status == STATUS_OBJECT_NAME_NOT_FOUND) {
		last_port_num = -1;

		status = fscc_driver_set_last_port_num(Driver, last_port_num);
		if (!NT_SUCCESS(status)) {
			TraceEvents(TRACE_LEVEL_ERROR, TRACE_DEVICE,
			"fscc_driver_set_last_port_num failed %!STATUS!", status);
			return 0;
		}
	}
	else if (!NT_SUCCESS(status)) {
		TraceEvents(TRACE_LEVEL_ERROR, TRACE_DEVICE,
		"fscc_driver_get_last_port_num failed %!STATUS!", status);
		return 0;
	}

	WDF_FILEOBJECT_CONFIG_INIT(&deviceConfig,
	FsccDeviceFileCreate,
	FsccFileClose,
	WDF_NO_EVENT_CALLBACK // No cleanup callback
	);

	WdfDeviceInitSetFileObjectConfig(DeviceInit,
	&deviceConfig,
	WDF_NO_OBJECT_ATTRIBUTES
	);    

	RtlInitEmptyUnicodeString(&device_name, device_name_buffer,
	sizeof(device_name_buffer));
	status = RtlUnicodeStringPrintf(&device_name, L"\\Device\\FSCC%i",
	instance++);
	if (!NT_SUCCESS(status)) {
		TraceEvents(TRACE_LEVEL_ERROR, TRACE_DEVICE,
		"RtlUnicodeStringPrintf failed %!STATUS!", status);
		return 0;
	}


	status = WdfDeviceInitAssignName(DeviceInit, &device_name);
	if (!NT_SUCCESS(status)) {
		WdfDeviceInitFree(DeviceInit);
		TraceEvents(TRACE_LEVEL_ERROR, TRACE_DEVICE,
		"WdfDeviceInitAssignName failed %!STATUS!", status);
		return 0;
	}
	
	status = WdfDeviceInitAssignSDDLString(DeviceInit,
	&SDDL_DEVOBJ_SYS_ALL_ADM_RWX_WORLD_RWX_RES_RWX);
	if (!NT_SUCCESS(status)) {
		WdfDeviceInitFree(DeviceInit);
		TraceEvents(TRACE_LEVEL_ERROR, TRACE_DEVICE,
		"WdfDeviceInitAssignSDDLString failed %!STATUS!", status);
		return 0;
	}
	
	WdfDeviceInitSetDeviceType(DeviceInit, FILE_DEVICE_SERIAL_PORT);
	WdfDeviceInitSetDeviceClass(DeviceInit, (LPGUID)&GUID_DEVCLASS_FSCC);

	WDF_PNPPOWER_EVENT_CALLBACKS_INIT(&pnpPowerCallbacks);
	pnpPowerCallbacks.EvtDevicePrepareHardware = FsccEvtDevicePrepareHardware;
	pnpPowerCallbacks.EvtDeviceReleaseHardware = FsccEvtDeviceReleaseHardware;
	WdfDeviceInitSetPnpPowerEventCallbacks(DeviceInit, &pnpPowerCallbacks);

	WDF_OBJECT_ATTRIBUTES_INIT_CONTEXT_TYPE(&attributes, FSCC_PORT);

	status = WdfDeviceCreate(&DeviceInit, &attributes, &device);
	if(!NT_SUCCESS(status)) {
		TraceEvents(TRACE_LEVEL_ERROR, TRACE_DEVICE,
		"WdfDeviceCreate failed %!STATUS!", status);
		return 0;
	}

	port = WdfObjectGet_FSCC_PORT(device);

	port->device = device;
	port->open_counter = 0;


	WDF_INTERRUPT_CONFIG_INIT(&interruptConfig, fscc_isr, NULL);
	interruptConfig.ShareVector = WdfTrue;
	WDF_OBJECT_ATTRIBUTES_INIT_CONTEXT_TYPE(&interruptAttributes, FSCC_PORT);

	status = WdfInterruptCreate(port->device, &interruptConfig,
	&interruptAttributes, &port->interrupt);
	if (!NT_SUCCESS(status)) {
		TraceEvents(TRACE_LEVEL_ERROR, TRACE_DEVICE,
		"WdfInterruptCreate failed %!STATUS!", status);
		return 0;
	}


	WDF_IO_QUEUE_CONFIG_INIT(&queue_config, WdfIoQueueDispatchSequential);
	queue_config.EvtIoDeviceControl = FsccEvtIoDeviceControl;

	status = WdfIoQueueCreate(port->device, &queue_config,
	WDF_NO_OBJECT_ATTRIBUTES, &port->ioctl_queue);
	if(!NT_SUCCESS(status)) {
		TraceEvents(TRACE_LEVEL_ERROR, TRACE_DEVICE,
		"WdfIoQueueCreate failed %!STATUS!", status);
		return 0;
	}
	status = WdfDeviceConfigureRequestDispatching(port->device,
	port->ioctl_queue, WdfRequestTypeDeviceControl);
	if(!NT_SUCCESS(status)) {
		TraceEvents(TRACE_LEVEL_ERROR, TRACE_DEVICE,
		"WdfDeviceConfigureRequestDispatching failed %!STATUS!", status);
		return 0;
	}

	WDF_IO_QUEUE_CONFIG_INIT(&queue_config, WdfIoQueueDispatchSequential);
	queue_config.EvtIoWrite = FsccEvtIoWrite;

	status = WdfIoQueueCreate(port->device, &queue_config,
	WDF_NO_OBJECT_ATTRIBUTES, &port->write_queue);
	if(!NT_SUCCESS(status)) {
		TraceEvents(TRACE_LEVEL_ERROR, TRACE_DEVICE,
		"WdfIoQueueCreate failed %!STATUS!", status);
		return 0;
	}

	status = WdfDeviceConfigureRequestDispatching(port->device,
	port->write_queue, WdfRequestTypeWrite);
	if(!NT_SUCCESS(status)) {
		TraceEvents(TRACE_LEVEL_ERROR, TRACE_DEVICE,
		"WdfDeviceConfigureRequestDispatching failed %!STATUS!", status);
		return 0;
	}


	WDF_IO_QUEUE_CONFIG_INIT(&queue_config, WdfIoQueueDispatchManual);

	status = WdfIoQueueCreate(port->device, &queue_config,
	WDF_NO_OBJECT_ATTRIBUTES, &port->write_queue2);
	if(!NT_SUCCESS(status)) {
		TraceEvents(TRACE_LEVEL_ERROR, TRACE_DEVICE,
		"WdfIoQueueCreate failed %!STATUS!", status);
		return 0;
	}

	WDF_IO_QUEUE_CONFIG_INIT(&queue_config, WdfIoQueueDispatchManual);

	status = WdfIoQueueCreate(port->device, &queue_config,
	WDF_NO_OBJECT_ATTRIBUTES, &port->blocking_request_queue);
	if (!NT_SUCCESS(status)) {
		TraceEvents(TRACE_LEVEL_ERROR, TRACE_DEVICE,
		"WdfIoQueueCreate failed %!STATUS!", status);
		return 0;
	}


	WDF_IO_QUEUE_CONFIG_INIT(&queue_config, WdfIoQueueDispatchSequential);
	queue_config.EvtIoRead = FsccEvtIoRead;

	status = WdfIoQueueCreate(port->device, &queue_config,
	WDF_NO_OBJECT_ATTRIBUTES, &port->read_queue);
	if(!NT_SUCCESS(status)) {
		TraceEvents(TRACE_LEVEL_ERROR, TRACE_DEVICE,
		"WdfIoQueueCreate failed %!STATUS!", status);
		return 0;
	}

	status = WdfDeviceConfigureRequestDispatching(port->device,
	port->read_queue, WdfRequestTypeRead);
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


	WDF_IO_QUEUE_CONFIG_INIT(&queue_config, WdfIoQueueDispatchManual);

	status = WdfIoQueueCreate(port->device, &queue_config,
	WDF_NO_OBJECT_ATTRIBUTES, &port->isr_queue);
	if(!NT_SUCCESS(status)) {
		TraceEvents(TRACE_LEVEL_ERROR, TRACE_DEVICE,
		"WdfIoQueueCreate failed %!STATUS!", status);
		return 0;
	}

	//
	// In addition to setting NoDisplayInUI in DeviceCaps, we
	// have to do the following to hide the device. Following call
	// tells the framework to report the device state in
	// IRP_MN_QUERY_DEVICE_STATE request.
	//
	WDF_DEVICE_STATE_INIT(&device_state);
	device_state.DontDisplayInUI = WdfFalse;
	WdfDeviceSetDeviceState(port->device, &device_state);

	status = WdfDeviceCreateDeviceInterface(port->device,
	(LPGUID)&GUID_DEVINTERFACE_FSCC, NULL);
	if (!NT_SUCCESS(status)) {
		WdfObjectDelete(port->device);
		TraceEvents(TRACE_LEVEL_ERROR, TRACE_DEVICE,
		"WdfDeviceCreateDeviceInterface failed %!STATUS!", status);
		return 0;
	}

	status = fscc_port_get_port_num(port, &port_num);
	if (status == STATUS_OBJECT_NAME_NOT_FOUND) {
		port_num = last_port_num + 1;

		status = fscc_port_set_port_num(port, port_num);
		if (!NT_SUCCESS(status)) {
			WdfObjectDelete(port->device);
			TraceEvents(TRACE_LEVEL_ERROR, TRACE_DEVICE,
			"fscc_port_set_port_num failed %!STATUS!", status);
			return 0;
		}
	}
	else if (!NT_SUCCESS(status)) {
		WdfObjectDelete(port->device);
		TraceEvents(TRACE_LEVEL_ERROR, TRACE_DEVICE,
		"fscc_port_get_port_num failed %!STATUS!", status);
		return 0;
	}

	status = fscc_port_set_friendly_name(port->device, port_num);
	if (!NT_SUCCESS(status)) {
		WdfObjectDelete(port->device);
		TraceEvents(TRACE_LEVEL_ERROR, TRACE_DEVICE,
			"fscc_port_set_friendly_name failed %!STATUS!", status);
		return 0;
	}

	last_port_num = port_num;

	WDF_DEVICE_PNP_CAPABILITIES_INIT(&pnpCaps);
	pnpCaps.Removable = WdfFalse;
	pnpCaps.UniqueID = WdfTrue;
	pnpCaps.UINumber = port_num;
	WdfDeviceSetPnpCapabilities(port->device, &pnpCaps);

	status = fscc_driver_set_last_port_num(Driver, last_port_num);
	if (!NT_SUCCESS(status)) {
		WdfObjectDelete(port->device);
		TraceEvents(TRACE_LEVEL_ERROR, TRACE_DEVICE,
		"fscc_driver_set_last_port_num failed %!STATUS!", status);
		return 0;
	}

	RtlInitEmptyUnicodeString(&dos_name, dos_name_buffer,
	sizeof(dos_name_buffer));
	status = RtlUnicodeStringPrintf(&dos_name, L"\\DosDevices\\FSCC%i",
	port_num);
	if (!NT_SUCCESS(status)) {
		WdfObjectDelete(port->device);
		TraceEvents(TRACE_LEVEL_ERROR, TRACE_DEVICE,
		"RtlUnicodeStringPrintf failed %!STATUS!", status);
		return 0;
	}

	status = WdfDeviceCreateSymbolicLink(port->device, &dos_name);
	if (!NT_SUCCESS(status)) {
		WdfObjectDelete(port->device);
		TraceEvents(TRACE_LEVEL_WARNING, TRACE_DEVICE,
		"WdfDeviceCreateSymbolicLink failed %!STATUS!", status);
		return 0;
	}

	WDF_OBJECT_ATTRIBUTES_INIT(&attributes);
	attributes.ParentObject = port->device;

	status = WdfSpinLockCreate(&attributes, &port->board_settings_spinlock);
	if (!NT_SUCCESS(status)) {
		WdfObjectDelete(port->device);
		TraceEvents(TRACE_LEVEL_ERROR, TRACE_DEVICE,
		"WdfSpinLockCreate failed %!STATUS!", status);
		return 0;
	}

	status = WdfSpinLockCreate(&attributes, &port->board_rx_spinlock);
	if (!NT_SUCCESS(status)) {
		WdfObjectDelete(port->device);
		TraceEvents(TRACE_LEVEL_ERROR, TRACE_DEVICE,
		"WdfSpinLockCreate failed %!STATUS!", status);
		return 0;
	}

	status = WdfSpinLockCreate(&attributes, &port->board_tx_spinlock);
	if (!NT_SUCCESS(status)) {
		WdfObjectDelete(port->device);
		TraceEvents(TRACE_LEVEL_ERROR, TRACE_DEVICE,
		"WdfSpinLockCreate failed %!STATUS!", status);
		return 0;
	}

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

	WDF_DPC_CONFIG_INIT(&dpcConfig, &FsccProcessRead);
	dpcConfig.AutomaticSerialization = TRUE;

	WDF_OBJECT_ATTRIBUTES_INIT(&dpcAttributes);
	dpcAttributes.ParentObject = port->device;

	status = WdfDpcCreate(&dpcConfig, &dpcAttributes, &port->process_read_dpc);
	if (!NT_SUCCESS(status)) {
		WdfObjectDelete(port->device);
		TraceEvents(TRACE_LEVEL_ERROR, TRACE_DEVICE,
		"WdfDpcCreate failed %!STATUS!", status);
		return 0;
	}    

	WDF_DPC_CONFIG_INIT(&dpcConfig, &isr_alert_worker);
	dpcConfig.AutomaticSerialization = TRUE;

	WDF_OBJECT_ATTRIBUTES_INIT(&dpcAttributes);
	dpcAttributes.ParentObject = port->device;

	status = WdfDpcCreate(&dpcConfig, &dpcAttributes, &port->isr_alert_dpc);
	if (!NT_SUCCESS(status)) {
		WdfObjectDelete(port->device);
		TraceEvents(TRACE_LEVEL_ERROR, TRACE_DEVICE,
		"WdfDpcCreate failed %!STATUS!", status);
		return 0;
	}    

	WDF_DPC_CONFIG_INIT(&dpcConfig, &request_worker);
	dpcConfig.AutomaticSerialization = TRUE;

	WDF_OBJECT_ATTRIBUTES_INIT(&dpcAttributes);
	dpcAttributes.ParentObject = port->device;

	status = WdfDpcCreate(&dpcConfig, &dpcAttributes, &port->request_dpc);
	if (!NT_SUCCESS(status)) {
		WdfObjectDelete(port->device);
		TraceEvents(TRACE_LEVEL_ERROR, TRACE_DEVICE,
		"WdfDpcCreate failed %!STATUS!", status);
		return 0;
	}

	WDF_DPC_CONFIG_INIT(&dpcConfig, &alls_worker);
	dpcConfig.AutomaticSerialization = TRUE;

	WDF_OBJECT_ATTRIBUTES_INIT(&dpcAttributes);
	dpcAttributes.ParentObject = port->device;

	status = WdfDpcCreate(&dpcConfig, &dpcAttributes, &port->alls_dpc);
	if (!NT_SUCCESS(status)) {
		WdfObjectDelete(port->device);
		TraceEvents(TRACE_LEVEL_ERROR, TRACE_DEVICE,
		"WdfDpcCreate failed %!STATUS!", status);
		return 0;
	}

	WDF_DPC_CONFIG_INIT(&dpcConfig, &timestamp_worker);
	dpcConfig.AutomaticSerialization = TRUE;

	WDF_OBJECT_ATTRIBUTES_INIT(&dpcAttributes);
	dpcAttributes.ParentObject = port->device;

	status = WdfDpcCreate(&dpcConfig, &dpcAttributes, &port->timestamp_dpc);
	if (!NT_SUCCESS(status)) {
		WdfObjectDelete(port->device);
		TraceEvents(TRACE_LEVEL_ERROR, TRACE_DEVICE,
		"WdfDpcCreate failed %!STATUS!", status);
		return 0;
	}

	return port;
}

NTSTATUS FsccEvtDevicePrepareHardware(WDFDEVICE Device,
WDFCMRESLIST ResourcesRaw, WDFCMRESLIST ResourcesTranslated)
{
	unsigned char clock_bits[20] = DEFAULT_CLOCK_BITS;

	WDF_TIMER_CONFIG  timerConfig;
	WDF_OBJECT_ATTRIBUTES  timerAttributes;
	NTSTATUS  status;
	UINT32 vstr;
	struct fscc_memory memory;

	struct fscc_port *port = 0;

	UNREFERENCED_PARAMETER(ResourcesRaw);

	port = WdfObjectGet_FSCC_PORT(Device);

	status = fscc_card_init(&port->card, ResourcesTranslated, Device);
	if (!NT_SUCCESS(status)) {
		TraceEvents(TRACE_LEVEL_ERROR, TRACE_DEVICE,
		"fscc_card_init failed %!STATUS!", status);
		return status;
	}

	TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DEVICE, "%s (%x.%02x)",
	fscc_card_get_name(&port->card), fscc_port_get_PREV(port),
	fscc_port_get_FREV(port));

	switch (PtrToUlong(port->card.bar[0].address) & 0x000000FF) {
	case 0x00:
		port->channel = 0;
		break;

	case 0x80:
		port->channel = 1;
		break;

	default:
		TraceEvents(TRACE_LEVEL_ERROR, TRACE_DEVICE, "Problem detecting channel.");
		return STATUS_UNSUCCESSFUL;
	}

	TraceEvents(TRACE_LEVEL_VERBOSE, TRACE_DEVICE, "Channel = %i",
	port->channel);
	
	vstr = fscc_port_get_PREV(port);
	if(vstr & 0x04) 
		port->has_dma = 1;
	else 
		port->has_dma = 0;
	TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DEVICE, "Board has DMA: %d!", port->has_dma);
	
	fscc_io_initialize(port);

	fscc_port_set_append_status(port, DEFAULT_APPEND_STATUS_VALUE);
	fscc_port_set_append_timestamp(port, DEFAULT_APPEND_TIMESTAMP_VALUE);
	fscc_port_set_ignore_timeout(port, DEFAULT_IGNORE_TIMEOUT_VALUE);
	fscc_port_set_tx_modifiers(port, DEFAULT_TX_MODIFIERS_VALUE);
	fscc_port_set_rx_multiple(port, DEFAULT_RX_MULTIPLE_VALUE);
	fscc_port_set_wait_on_write(port, DEFAULT_WAIT_ON_WRITE_VALUE);
	fscc_port_set_blocking_write(port, DEFAULT_BLOCKING_WRITE_VALUE);
	fscc_port_set_force_fifo(port, DEFAULT_FORCE_FIFO_VALUE);
	
	memory.tx_num = DEFAULT_BUFFER_TX_NUM;
	memory.rx_num = DEFAULT_BUFFER_RX_NUM;
	memory.tx_size = DEFAULT_BUFFER_TX_SIZE;
	memory.rx_size = DEFAULT_BUFFER_RX_SIZE;
	fscc_port_get_default_memory(port, &memory);
	status = fscc_io_create_rx(port, memory.rx_num, memory.rx_size);
	if (!NT_SUCCESS(status)) {
		fscc_io_destroy_rx(port);
		fscc_io_destroy_tx(port);
		TraceEvents(TRACE_LEVEL_ERROR, TRACE_DEVICE,
		"fscc_card_init build_rx failed %!STATUS!", status);
		return status;
	}
	status = fscc_io_create_tx(port, memory.tx_num, memory.tx_size);
	if (!NT_SUCCESS(status)) {
		fscc_io_destroy_tx(port);
		fscc_io_destroy_rx(port);
		TraceEvents(TRACE_LEVEL_ERROR, TRACE_DEVICE,
		"fscc_card_init build_tx failed %!STATUS!", status);
		return status;
	}
	

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
	fscc_port_get_default_registers(port, &port->register_storage);
	fscc_port_set_registers(port, &port->register_storage);
	
	port->rx_frame_size = 0;
	port->last_isr_value = 0;

	fscc_port_set_clock_bits(port, clock_bits);
	
	if(fscc_port_uses_dma(port)) 
		fscc_dma_port_enable(port);
	
	fscc_io_purge_rx(port);
	fscc_io_purge_tx(port);
	
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

	return STATUS_SUCCESS;
}

NTSTATUS FsccEvtDeviceReleaseHardware(WDFDEVICE Device,
WDFCMRESLIST ResourcesTranslated)
{
	NTSTATUS status = STATUS_SUCCESS;
	struct fscc_port *port = 0;

	port = WdfObjectGet_FSCC_PORT(Device);

	fscc_io_destroy_tx(port);
	fscc_io_destroy_rx(port);

	status = fscc_card_delete(&port->card, ResourcesTranslated);

	return status;
}

VOID FsccDeviceFileCreate(
IN  WDFDEVICE Device,
IN  WDFREQUEST Request,
IN  WDFFILEOBJECT FileObject
)
{
	struct fscc_port *port = 0;

	UNREFERENCED_PARAMETER(FileObject);

	port = WdfObjectGet_FSCC_PORT(Device);

	WdfSpinLockAcquire(port->board_settings_spinlock);

	if (fscc_port_using_async(port)) {
		TraceEvents(TRACE_LEVEL_WARNING, TRACE_DEVICE,
		"use COMx nodes while in async mode");
		WdfRequestComplete(Request, STATUS_NOT_SUPPORTED);
		WdfSpinLockRelease(port->board_settings_spinlock);
		return;
	}

	port->open_counter++;

	/* Mark the port as open in synchronous mode (so async won't open) */
	if (port->open_counter == 1) {
		UINT32 orig_fcr, new_fcr;

		orig_fcr = fscc_port_get_register(port, 2, FCR_OFFSET);
		new_fcr = orig_fcr | (0x40000000 << port->channel);

		fscc_port_set_register(port, 2, FCR_OFFSET, new_fcr);
	}

	WdfSpinLockRelease(port->board_settings_spinlock);

	WdfRequestComplete(Request, STATUS_SUCCESS);
}

VOID FsccFileClose(
IN  WDFFILEOBJECT FileObject
)
{
	struct fscc_port *port = 0;

	port = WdfObjectGet_FSCC_PORT(WdfFileObjectGetDevice(FileObject));

	port->open_counter--;

	if (port->open_counter == 0) {
		UINT32 orig_fcr, new_fcr;

		WdfSpinLockAcquire(port->board_settings_spinlock);

		orig_fcr = fscc_port_get_register(port, 2, FCR_OFFSET);
		new_fcr = orig_fcr & ~(0x40000000 << port->channel);

		fscc_port_set_register(port, 2, FCR_OFFSET, new_fcr);

		WdfSpinLockRelease(port->board_settings_spinlock);
	}
}

// The ability to modify the number and size of buffers while the driver
// is active has been intentionally removed. This feature clashed with
// the ability to DMA because of CommonBuffers
VOID FsccEvtIoDeviceControl(IN WDFQUEUE Queue, IN WDFREQUEST Request,
IN size_t OutputBufferLength, IN size_t InputBufferLength,
IN ULONG IoControlCode)
{
	NTSTATUS status = STATUS_SUCCESS;
	struct fscc_port *port = 0;
	size_t bytes_returned = 0;

	UNUSED(InputBufferLength);
	UNUSED(OutputBufferLength);

	port = WdfObjectGet_FSCC_PORT(WdfIoQueueGetDevice(Queue));

	switch(IoControlCode) {
	case FSCC_GET_REGISTERS: {
			struct fscc_registers *input_regs = 0;
			struct fscc_registers *output_regs = 0;

			status = WdfRequestRetrieveInputBuffer(Request, sizeof(*input_regs),
			(PVOID *)&input_regs, NULL);
			if (!NT_SUCCESS(status)) {
				TraceEvents(TRACE_LEVEL_WARNING, TRACE_DEVICE,
				"WdfRequestRetrieveInputBuffer failed %!STATUS!", status);
				break;
			}

			status = WdfRequestRetrieveOutputBuffer(Request,
			sizeof(*output_regs), (PVOID *)&output_regs, NULL);
			if (!NT_SUCCESS(status)) {
				TraceEvents(TRACE_LEVEL_WARNING, TRACE_DEVICE,
				"WdfRequestRetrieveOutputBuffer failed %!STATUS!", status);
				break;
			}

			RtlCopyMemory(output_regs, input_regs, sizeof(struct fscc_registers));

			WdfSpinLockAcquire(port->board_settings_spinlock);
			fscc_port_get_registers(port, output_regs);
			WdfSpinLockRelease(port->board_settings_spinlock);

			bytes_returned = sizeof(*output_regs);
		}

		break;

	case FSCC_SET_REGISTERS: {
			struct fscc_registers *input_regs = 0;

			status = WdfRequestRetrieveInputBuffer(Request,
			sizeof(*input_regs), (PVOID *)&input_regs, NULL);
			if (!NT_SUCCESS(status)) {
				TraceEvents(TRACE_LEVEL_WARNING, TRACE_DEVICE,
				"WdfRequestRetrieveInputBuffer failed %!STATUS!", status);
				break;
			}


			WdfSpinLockAcquire(port->board_settings_spinlock);
			status = fscc_port_set_registers(port, input_regs);
			WdfSpinLockRelease(port->board_settings_spinlock);

			if (!NT_SUCCESS(status)) {
				TraceEvents(TRACE_LEVEL_WARNING, TRACE_DEVICE,
				"fscc_port_set_registers failed %!STATUS!", status);
				break;
			}
		}

		break;

	case FSCC_PURGE_TX:
		status = fscc_io_purge_tx(port);
		if (!NT_SUCCESS(status)) {
			TraceEvents(TRACE_LEVEL_WARNING, TRACE_DEVICE,
			"fscc_io_purge_tx failed %!STATUS!", status);
			break;
		}

		break;

	case FSCC_PURGE_RX:
		status = fscc_io_purge_rx(port);
		if (!NT_SUCCESS(status)) {
			TraceEvents(TRACE_LEVEL_WARNING, TRACE_DEVICE,
			"fscc_io_purge_rx failed %!STATUS!", status);
			break;
		}

		break;

	case FSCC_ENABLE_APPEND_STATUS:
		fscc_port_set_append_status(port, TRUE);
		break;

	case FSCC_DISABLE_APPEND_STATUS:
		fscc_port_set_append_status(port, FALSE);
		break;

	case FSCC_GET_APPEND_STATUS: {
			BOOLEAN *append_status = 0;

			status = WdfRequestRetrieveOutputBuffer(Request,
			sizeof(*append_status), (PVOID *)&append_status, NULL);
			if (!NT_SUCCESS(status)) {
				TraceEvents(TRACE_LEVEL_WARNING, TRACE_DEVICE,
				"WdfRequestRetrieveOutputBuffer failed %!STATUS!", status);
				break;
			}

			*append_status = fscc_port_get_append_status(port);

			bytes_returned = sizeof(*append_status);
		}

		break;

	case FSCC_ENABLE_APPEND_TIMESTAMP:
		fscc_port_set_append_timestamp(port, TRUE);
		break;

	case FSCC_DISABLE_APPEND_TIMESTAMP:
		fscc_port_set_append_timestamp(port, FALSE);
		break;

	case FSCC_GET_APPEND_TIMESTAMP: {
			BOOLEAN *append_timestamp = 0;

			status = WdfRequestRetrieveOutputBuffer(Request,
			sizeof(*append_timestamp), (PVOID *)&append_timestamp, NULL);
			if (!NT_SUCCESS(status)) {
				TraceEvents(TRACE_LEVEL_WARNING, TRACE_DEVICE,
				"WdfRequestRetrieveOutputBuffer failed %!STATUS!", status);
				break;
			}

			*append_timestamp = fscc_port_get_append_timestamp(port);

			bytes_returned = sizeof(*append_timestamp);
		}

		break;

	case FSCC_ENABLE_RX_MULTIPLE:
		fscc_port_set_rx_multiple(port, TRUE);
		break;

	case FSCC_DISABLE_RX_MULTIPLE:
		fscc_port_set_rx_multiple(port, FALSE);
		break;

	case FSCC_GET_RX_MULTIPLE: {
			BOOLEAN *rx_multiple = 0;

			status = WdfRequestRetrieveOutputBuffer(Request,
			sizeof(*rx_multiple), (PVOID *)&rx_multiple, NULL);
			if (!NT_SUCCESS(status)) {
				TraceEvents(TRACE_LEVEL_WARNING, TRACE_DEVICE,
				"WdfRequestRetrieveOutputBuffer failed %!STATUS!", status);
				break;
			}

			*rx_multiple = fscc_port_get_rx_multiple(port);

			bytes_returned = sizeof(*rx_multiple);
		}

		break;

	case FSCC_SET_CLOCK_BITS: {
			unsigned char *clock_bits = 0;

			status = WdfRequestRetrieveInputBuffer(Request, NUM_CLOCK_BYTES,
			(PVOID *)&clock_bits, NULL);
			if (!NT_SUCCESS(status)) {
				TraceEvents(TRACE_LEVEL_WARNING, TRACE_DEVICE,
				"WdfRequestRetrieveInputBuffer failed %!STATUS!", status);
				break;
			}

			fscc_port_set_clock_bits(port, clock_bits);
		}

		break;

	case FSCC_ENABLE_IGNORE_TIMEOUT:
		fscc_port_set_ignore_timeout(port, TRUE);
		break;

	case FSCC_DISABLE_IGNORE_TIMEOUT:
		fscc_port_set_ignore_timeout(port, FALSE);
		break;

	case FSCC_GET_IGNORE_TIMEOUT: {
			BOOLEAN *ignore_timeout = 0;

			status = WdfRequestRetrieveOutputBuffer(Request,
			sizeof(*ignore_timeout), (PVOID *)&ignore_timeout,
			NULL);
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

			status = WdfRequestRetrieveInputBuffer(Request,
			sizeof(*tx_modifiers), (PVOID *)&tx_modifiers, NULL);
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

			status = WdfRequestRetrieveOutputBuffer(Request,
			sizeof(*tx_modifiers), (PVOID *)&tx_modifiers, NULL);
			if (!NT_SUCCESS(status)) {
				TraceEvents(TRACE_LEVEL_WARNING, TRACE_DEVICE,
				"WdfRequestRetrieveOutputBuffer failed %!STATUS!", status);
				break;
			}

			*tx_modifiers = fscc_port_get_tx_modifiers(port);

			bytes_returned = sizeof(*tx_modifiers);
		}

		break;

	case FSCC_GET_WAIT_ON_WRITE: {
			BOOLEAN *wait_on_write = 0;

			status = WdfRequestRetrieveOutputBuffer(Request,
			sizeof(*wait_on_write), (PVOID *)&wait_on_write, NULL);
			if (!NT_SUCCESS(status)) {
				TraceEvents(TRACE_LEVEL_WARNING, TRACE_DEVICE,
				"WdfRequestRetrieveOutputBuffer failed %!STATUS!", status);
				break;
			}

			*wait_on_write = fscc_port_get_wait_on_write(port);

			bytes_returned = sizeof(*wait_on_write);
		}

		break;

	case FSCC_ENABLE_WAIT_ON_WRITE:
		fscc_port_set_wait_on_write(port, TRUE);
		break;

	case FSCC_DISABLE_WAIT_ON_WRITE:
		fscc_port_set_wait_on_write(port, FALSE);
		break;

	case FSCC_TRACK_INTERRUPTS:
		status = WdfRequestForwardToIoQueue(Request, port->isr_queue);
		if (!NT_SUCCESS(status)) {
			TraceEvents(TRACE_LEVEL_ERROR, TRACE_DEVICE,
			"WdfRequestForwardToIoQueue failed %!STATUS!", status);
			WdfRequestComplete(Request, status);
		}
		return;

	case FSCC_GET_BLOCKING_WRITE: {
			BOOLEAN *blocking_write = 0;

			status = WdfRequestRetrieveOutputBuffer(Request,
			sizeof(*blocking_write), (PVOID *)&blocking_write, NULL);
			if (!NT_SUCCESS(status)) {
				TraceEvents(TRACE_LEVEL_WARNING, TRACE_DEVICE,
				"WdfRequestRetrieveOutputBuffer failed %!STATUS!", status);
				break;
			}

			*blocking_write = fscc_port_get_blocking_write(port);

			bytes_returned = sizeof(*blocking_write);
		}
		break;

	case FSCC_ENABLE_BLOCKING_WRITE:
		fscc_port_set_blocking_write(port, TRUE);
		break;

	case FSCC_DISABLE_BLOCKING_WRITE:
		fscc_port_set_blocking_write(port, FALSE);
		break;
	case FSCC_ENABLE_FORCE_FIFO:
		status = fscc_port_set_force_fifo(port, TRUE);
		break;

	case FSCC_DISABLE_FORCE_FIFO:
		status = fscc_port_set_force_fifo(port, FALSE);
		break;

	case FSCC_GET_FORCE_FIFO: {
			BOOLEAN *force_fifo = 0;

			status = WdfRequestRetrieveOutputBuffer(Request,
			sizeof(*force_fifo), (PVOID *)&force_fifo,
			NULL);
			if (!NT_SUCCESS(status)) {
				TraceEvents(TRACE_LEVEL_WARNING, TRACE_DEVICE,
				"WdfRequestRetrieveOutputBuffer failed %!STATUS!", status);
				break;
			}

			*force_fifo = fscc_port_get_force_fifo(port);

			bytes_returned = sizeof(*force_fifo);
		}
		break;
	default:
		status = STATUS_NOT_SUPPORTED;
		TraceEvents(TRACE_LEVEL_WARNING, TRACE_DEVICE,
		"Unknown DeviceIoControl 0x%x", IoControlCode);
		break;
	}

	WdfRequestCompleteWithInformation(Request, status, bytes_returned);
}

UINT32 fscc_port_get_register(struct fscc_port *port, unsigned bar,
unsigned register_offset)
{
	unsigned offset = 0;
	UINT32 value = 0;

	offset = port_offset(port, bar, register_offset);
	value = fscc_card_get_register(&port->card, bar, offset);

	return value;
}

/* Basic check to see if the CE bit is set. */
unsigned fscc_port_timed_out(struct fscc_port *port)
{
	UINT32 star_value = 0;
	unsigned i = 0;

	return_val_if_untrue(port, 0);

	for (i = 0; i < DEFAULT_TIMEOUT_VALUE; i++) {
		star_value = fscc_port_get_register(port, 0, STAR_OFFSET);

		if ((star_value & CE_BIT) == 0)
		return 0;
	}

	return 1;
}

NTSTATUS fscc_port_set_register(struct fscc_port *port, unsigned bar,
unsigned register_offset, UINT32 value)
{
	unsigned offset = 0;
	return_val_if_untrue(port, 0);

	offset = port_offset(port, bar, register_offset);

	/* Checks to make sure there is a clock present. */
	if (register_offset == CMDR_OFFSET && port->ignore_timeout == FALSE
			&& fscc_port_timed_out(port)) {
		return STATUS_IO_TIMEOUT;
	}
	// TODO Maybe remove this?
	if((register_offset == DMACCR_OFFSET) && fscc_port_uses_dma(port) && bar == 2) value |= 0x03000000;
	else if((register_offset == DMACCR_OFFSET) && !fscc_port_uses_dma(port) && bar == 2) value &= ~0x03000000;
	fscc_card_set_register(&port->card, bar, offset, value);

	if (bar == 0) {
		display_register(bar, register_offset,
		(UINT32)((fscc_register *)&port->register_storage)[register_offset / 4],
		value);

		((fscc_register *)&port->register_storage)[register_offset / 4] = value;
	}
	else if (register_offset == FCR_OFFSET) {
		display_register(bar, register_offset,
		(UINT32)port->register_storage.FCR,
		value);

		port->register_storage.FCR = value;
	}

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

	return_if_untrue(port);
	return_if_untrue(bar <= 2);
	return_if_untrue(buf);
	return_if_untrue(byte_count > 0);

	offset = port_offset(port, bar, register_offset);

	fscc_card_get_register_rep(&port->card, bar, offset, buf, byte_count);
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

	return_if_untrue(port);
	return_if_untrue(bar <= 2);
	return_if_untrue(data);
	return_if_untrue(byte_count > 0);

	offset = port_offset(port, bar, register_offset);

	fscc_card_set_register_rep(&port->card, bar, offset, data, byte_count);
}

/* Prevents the internal async bits from being set */
NTSTATUS fscc_port_set_registers(struct fscc_port *port, const struct fscc_registers *regs)
{
	unsigned stalled = 0;
	unsigned i = 0;

	for (i = 0; i < sizeof(*regs) / sizeof(fscc_register); i++) {
		unsigned register_offset = i * 4;

		if (is_read_only_register(register_offset) || ((fscc_register *)regs)[i] < 0)
			continue;
		
		if(i * 4 > MAX_OFFSET)
			break;
		
		if (fscc_port_set_register(port, 0, register_offset, (UINT32)(((fscc_register *)regs)[i])) == STATUS_IO_TIMEOUT)
			stalled = 1;
	}
	// Because BAR 2 offsets aren't uniform, the reg structure requires
	// custom code
	if(regs->FCR > 0)
		fscc_port_set_register(port, 2, FCR_OFFSET, regs->FCR & 0x3fffffff);
	// Special condition for 'master reset' bit, which only exists on the first port.
	if(regs->DMACCR & 0x10000)
		fscc_card_set_register(&port->card, 2, DMACCR_OFFSET, 0x10000);
	if(regs->DMACCR > 0)
		fscc_port_set_register(port, 2, DMACCR_OFFSET, (UINT32)regs->DMACCR & 0xfffeffff);
	
	return (stalled) ? STATUS_IO_TIMEOUT : STATUS_SUCCESS;
}

void fscc_port_get_registers(struct fscc_port *port, struct fscc_registers *regs)
{
	unsigned i = 0;

	for (i = 0; i < sizeof(*regs) / sizeof(fscc_register); i++) {        
		if (((fscc_register *)regs)[i] != FSCC_UPDATE_VALUE)
			continue;

		if(i * 4 > MAX_OFFSET)
			break;

		((fscc_register *)regs)[i] = fscc_port_get_register(port, 0, i * 4);
	}
	// Because BAR 2 offsets aren't uniform, the reg structure requires
	// custom code
	if(regs->FCR == FSCC_UPDATE_VALUE) 
		regs->FCR = fscc_port_get_register(port, 2, FCR_OFFSET);
	if(regs->DMACCR == FSCC_UPDATE_VALUE) 
		regs->DMACCR = fscc_port_get_register(port, 2, DMACCR_OFFSET);
	if(regs->DSTAR == FSCC_UPDATE_VALUE) 
		regs->DSTAR = fscc_port_get_register(port, 2, DSTAR_OFFSET);
}

UCHAR fscc_port_get_FREV(struct fscc_port *port)
{
	UINT32 vstr_value = 0;

	vstr_value = fscc_port_get_register(port, 0, VSTR_OFFSET);

	return (UCHAR)((vstr_value & 0x000000FF));
}

UCHAR fscc_port_get_PREV(struct fscc_port *port)
{
	UINT32 vstr_value = 0;

	vstr_value = fscc_port_get_register(port, 0, VSTR_OFFSET);

	return (UCHAR)((vstr_value & 0x0000FF00) >> 8);
}

UINT16 fscc_port_get_PDEV(struct fscc_port *port)
{
	UINT32 vstr_value = 0;

	vstr_value = fscc_port_get_register(port, 0, VSTR_OFFSET);

	return (UINT16)((vstr_value & 0xFFFF0000) >> 16);
}

void fscc_port_set_append_status(struct fscc_port *port, BOOLEAN value)
{
	return_if_untrue(port);

	if (port->append_status != value) {
		TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DEVICE,
		"Append status %i => %i",
		port->append_status, value);
	}
	else {
		TraceEvents(TRACE_LEVEL_VERBOSE, TRACE_DEVICE,
		"Append status = %i", value);
	}

	port->append_status = (value) ? 1 : 0;
}

BOOLEAN fscc_port_get_append_status(struct fscc_port *port)
{
	return_val_if_untrue(port, 0);

	return !fscc_io_is_streaming(port) && port->append_status;
}

void fscc_port_set_append_timestamp(struct fscc_port *port, BOOLEAN value)
{
	return_if_untrue(port);

	if (port->append_timestamp != value) {
		TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DEVICE,
		"Append timestamp %i => %i",
		port->append_timestamp, value);
	}
	else {
		TraceEvents(TRACE_LEVEL_VERBOSE, TRACE_DEVICE,
		"Append timestamp = %i", value);
	}

	port->append_timestamp = (value) ? 1 : 0;
}

BOOLEAN fscc_port_get_append_timestamp(struct fscc_port *port)
{
	return_val_if_untrue(port, 0);

	return !fscc_io_is_streaming(port) && port->append_timestamp;
}

void fscc_port_set_ignore_timeout(struct fscc_port *port, BOOLEAN value)
{
	return_if_untrue(port);

	if (port->ignore_timeout != value) {
		TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DEVICE,
		"Ignore timeout %i => %i",
		port->ignore_timeout, value);
	}
	else {
		TraceEvents(TRACE_LEVEL_VERBOSE, TRACE_DEVICE,
		"Ignore timeout = %i", value);
	}

	port->ignore_timeout = (value) ? TRUE : FALSE;
}

BOOLEAN fscc_port_get_ignore_timeout(struct fscc_port *port)
{
	return_val_if_untrue(port, 0);

	return port->ignore_timeout;
}

void fscc_port_set_rx_multiple(struct fscc_port *port, BOOLEAN value)
{
	return_if_untrue(port);

	if (port->rx_multiple != value) {
		TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DEVICE,
		"Receive multiple %i => %i",
		port->rx_multiple, value);
	}
	else {
		TraceEvents(TRACE_LEVEL_VERBOSE, TRACE_DEVICE,
		"Receive multiple = %i", value);
	}

	port->rx_multiple = (value) ? 1 : 0;
}

BOOLEAN fscc_port_get_rx_multiple(struct fscc_port *port)
{
	return_val_if_untrue(port, 0);

	return port->rx_multiple;
}

void fscc_port_set_wait_on_write(struct fscc_port *port, BOOLEAN value)
{
	return_if_untrue(port);

	if (port->wait_on_write != value) {
		TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DEVICE,
		"Wait on Write %i => %i",
		port->wait_on_write, value);
	}
	else {
		TraceEvents(TRACE_LEVEL_VERBOSE, TRACE_DEVICE,
		"Wait on Write = %i", value);
	}

	port->wait_on_write = (value) ? 1 : 0;
}

BOOLEAN fscc_port_get_wait_on_write(struct fscc_port *port)
{
	return_val_if_untrue(port, 0);

	return port->wait_on_write;
}

void fscc_port_set_blocking_write(struct fscc_port *port, BOOLEAN value)
{
	return_if_untrue(port);

	if (port->blocking_write != value) {
		TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DEVICE,
		"Blocking Write %i => %i",
		port->blocking_write, value);
	}
	else {
		TraceEvents(TRACE_LEVEL_VERBOSE, TRACE_DEVICE,
		"Blocking Write = %i", value);
	}

	port->blocking_write = (value) ? 1 : 0;
}

BOOLEAN fscc_port_get_blocking_write(struct fscc_port *port)
{
	return_val_if_untrue(port, 0);

	return port->blocking_write;
}

/* Returns -EINVAL if you set an incorrect transmit modifier */
NTSTATUS fscc_port_set_tx_modifiers(struct fscc_port *port, int value)
{
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
			"Transmit modifiers 0x%x => 0x%x",
			port->tx_modifiers, value);
		}
		else {
			TraceEvents(TRACE_LEVEL_VERBOSE, TRACE_DEVICE,
			"Transmit modifiers = 0x%x", value);
		}

		port->tx_modifiers = value;

		break;

	default:
		TraceEvents(TRACE_LEVEL_WARNING, TRACE_DEVICE,
		"Transmit modifiers (invalid value 0x%x)", value);

		return STATUS_INVALID_PARAMETER;
	}

	return STATUS_SUCCESS;
}

unsigned fscc_port_get_tx_modifiers(struct fscc_port *port)
{
	return_val_if_untrue(port, 0);

	return port->tx_modifiers;
}

#define STRB_BASE 0x00000008
#define DTA_BASE 0x00000001
#define CLK_BASE 0x00000002
void fscc_port_set_clock_bits(struct fscc_port *port, unsigned char *clock_data)
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

	return_if_untrue(port);


#ifdef DISABLE_XTAL
	clock_data[15] &= 0xfb;
#else
	/* This enables XTAL on all cards except green FSCC cards with a revision
	greater than 6 and 232 cards. Some old protoype SuperFSCC cards will 
	need to manually disable XTAL as they are not supported in this driver 
	by default. */
	if ((fscc_port_get_PDEV(port) == 0x0f && fscc_port_get_PREV(port) <= 6) ||
			fscc_port_get_PDEV(port) == 0x16) {
		clock_data[15] &= 0xfb;
	}
	else {
		clock_data[15] |= 0x04;
	}
#endif


	data = (UINT32 *)ExAllocatePool2(POOL_FLAG_NON_PAGED, sizeof(UINT32) * 323, 'stiB');

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

	WdfSpinLockAcquire(port->board_settings_spinlock);

	orig_fcr_value = fscc_card_get_register(&port->card, 2, FCR_OFFSET);

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

	WdfSpinLockRelease(port->board_settings_spinlock);

	ExFreePoolWithTag(data, 'stiB');
}

unsigned fscc_port_using_async(struct fscc_port *port)
{
	return_val_if_untrue(port, 0);

	/* We must refresh FCR because it is shared with serialfc */
	port->register_storage.FCR = fscc_port_get_register(port, 2, FCR_OFFSET);

	switch (port->channel) {
	case 0:
		return port->register_storage.FCR & 0x01000000;

	case 1:
		return port->register_storage.FCR & 0x02000000;
	}

	return 0;
}

void fscc_port_reset_timer(struct fscc_port *port)
{
	WdfTimerStart(port->timer, WDF_ABS_TIMEOUT_IN_MS(TIMER_DELAY_MS));
}

NTSTATUS fscc_port_get_port_num(struct fscc_port *port, unsigned *port_num)
{
	NTSTATUS status;
	WDFKEY devkey;
	UNICODE_STRING key_str;
	ULONG port_num_long;

	RtlInitUnicodeString(&key_str, L"PortNumber");

	status = WdfDeviceOpenRegistryKey(port->device, PLUGPLAY_REGKEY_DEVICE,
	STANDARD_RIGHTS_ALL,
	WDF_NO_OBJECT_ATTRIBUTES, &devkey);
	if (!NT_SUCCESS(status)) {
		TraceEvents(TRACE_LEVEL_ERROR, TRACE_DEVICE,
		"WdfDeviceOpenRegistryKey failed %!STATUS!", status);
		return status;
	}

	status = WdfRegistryQueryULong(devkey, &key_str, &port_num_long);
	if (!NT_SUCCESS(status)) {
		TraceEvents(TRACE_LEVEL_ERROR, TRACE_DEVICE,
		"WdfRegistryQueryULong failed %!STATUS!", status);
		return status;
	}

	*port_num = (unsigned)port_num_long;

	WdfRegistryClose(devkey);

	return status;
}

NTSTATUS fscc_port_set_port_num(struct fscc_port *port, unsigned value)
{
	NTSTATUS status;
	WDFKEY devkey;
	UNICODE_STRING key_str;

	RtlInitUnicodeString(&key_str, L"PortNumber");

	status = WdfDeviceOpenRegistryKey(port->device, PLUGPLAY_REGKEY_DEVICE,
	STANDARD_RIGHTS_ALL,
	WDF_NO_OBJECT_ATTRIBUTES, &devkey);
	if (!NT_SUCCESS(status)) {
		TraceEvents(TRACE_LEVEL_ERROR, TRACE_DEVICE,
		"WdfDeviceOpenRegistryKey failed %!STATUS!", status);
		return status;
	}

	status = WdfRegistryAssignULong(devkey, &key_str, value);
	if (!NT_SUCCESS(status)) {
		TraceEvents(TRACE_LEVEL_ERROR, TRACE_DEVICE,
		"WdfRegistryAssignULong failed %!STATUS!", status);
		return status;
	}

	WdfRegistryClose(devkey);

	return status;
}

NTSTATUS fscc_port_set_force_fifo(struct fscc_port *port, BOOLEAN value)
{
	return_val_if_untrue(port, STATUS_UNSUCCESSFUL);
	if(!value && !port->has_dma) return STATUS_NOT_SUPPORTED;
	
	// We only care if it's changing, and if it can even do DMA.
	if (port->force_fifo != value && port->has_dma) 
	{
		TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DEVICE, "Force FIFO %i => %i", port->force_fifo, value);
		port->force_fifo = (value) ? TRUE : FALSE;
		if(port->force_fifo) fscc_dma_port_disable(port);
		else fscc_dma_port_enable(port);
	}
	return STATUS_SUCCESS;
}

BOOLEAN fscc_port_get_force_fifo(struct fscc_port *port)
{
	return_val_if_untrue(port, 0);

	return port->force_fifo;
}

NTSTATUS fscc_port_get_default_memory(struct fscc_port *port, struct fscc_memory *memory)
{
	NTSTATUS status;
	WDFKEY devkey;
	UNICODE_STRING key_str;
	ULONG value;
	
	status = WdfDeviceOpenRegistryKey(port->device, PLUGPLAY_REGKEY_DEVICE,
	STANDARD_RIGHTS_ALL,
	WDF_NO_OBJECT_ATTRIBUTES, &devkey);
	if (!NT_SUCCESS(status)) {
		TraceEvents(TRACE_LEVEL_ERROR, TRACE_DEVICE,
		"WdfDeviceOpenRegistryKey failed %!STATUS!", status);
		return status;
	}
	
	RtlInitUnicodeString(&key_str, L"TxNum");
	status = WdfRegistryQueryULong(devkey, &key_str, &value);
	if (!NT_SUCCESS(status)) {
		value = DEFAULT_BUFFER_TX_NUM;
		status = WdfRegistryAssignULong(devkey, &key_str, value);
	}
	memory->tx_num = (unsigned)value;

	RtlInitUnicodeString(&key_str, L"RxNum");
	status = WdfRegistryQueryULong(devkey, &key_str, &value);
	if (!NT_SUCCESS(status)) {
		value = DEFAULT_BUFFER_RX_NUM;
		status = WdfRegistryAssignULong(devkey, &key_str, value);
	}
	memory->rx_num = (unsigned)value;
	
	RtlInitUnicodeString(&key_str, L"TxSize");
	status = WdfRegistryQueryULong(devkey, &key_str, &value);
	if (!NT_SUCCESS(status)) {
		value = DEFAULT_BUFFER_TX_SIZE;
		status = WdfRegistryAssignULong(devkey, &key_str, value);
	}
	memory->tx_size = (unsigned)value;
	
	RtlInitUnicodeString(&key_str, L"RxSize");
	status = WdfRegistryQueryULong(devkey, &key_str, &value);
	if (!NT_SUCCESS(status)) {
		value = DEFAULT_BUFFER_RX_SIZE;
		status = WdfRegistryAssignULong(devkey, &key_str, value);
	}
	memory->rx_size = (unsigned)value;

	WdfRegistryClose(devkey);

	return STATUS_SUCCESS;
}

NTSTATUS fscc_port_get_default_registers(struct fscc_port *port, struct fscc_registers *regs)
{
	NTSTATUS status;
	WDFKEY devkey;
	UNICODE_STRING key_str;
	ULONG value;

	status = WdfDeviceOpenRegistryKey(port->device, PLUGPLAY_REGKEY_DEVICE, STANDARD_RIGHTS_ALL, WDF_NO_OBJECT_ATTRIBUTES, &devkey);
	if (!NT_SUCCESS(status)) {
		TraceEvents(TRACE_LEVEL_ERROR, TRACE_DEVICE, "WdfDeviceOpenRegistryKey failed %!STATUS!", status);
		return status;
	}

	RtlInitUnicodeString(&key_str, L"FIFOT");
	status = WdfRegistryQueryULong(devkey, &key_str, &value);
	if (!NT_SUCCESS(status)) {
		value = DEFAULT_FIFOT_VALUE;
		status = WdfRegistryAssignULong(devkey, &key_str, value);
	}
	regs->FIFOT = (unsigned)value;

	RtlInitUnicodeString(&key_str, L"CCR0");
	status = WdfRegistryQueryULong(devkey, &key_str, &value);
	if (!NT_SUCCESS(status)) {
		value = DEFAULT_CCR0_VALUE;
		status = WdfRegistryAssignULong(devkey, &key_str, value);
	}
	regs->CCR0 = (unsigned)value;

	RtlInitUnicodeString(&key_str, L"CCR1");
	status = WdfRegistryQueryULong(devkey, &key_str, &value);
	if (!NT_SUCCESS(status)) {
		value = DEFAULT_CCR1_VALUE;
		status = WdfRegistryAssignULong(devkey, &key_str, value);
	}
	regs->CCR1 = (unsigned)value;

	RtlInitUnicodeString(&key_str, L"CCR2");
	status = WdfRegistryQueryULong(devkey, &key_str, &value);
	if (!NT_SUCCESS(status)) {
		value = DEFAULT_CCR2_VALUE;
		status = WdfRegistryAssignULong(devkey, &key_str, value);
	}
	regs->CCR2 = (unsigned)value;
	
	RtlInitUnicodeString(&key_str, L"BGR");
	status = WdfRegistryQueryULong(devkey, &key_str, &value);
	if (!NT_SUCCESS(status)) {
		value = DEFAULT_BGR_VALUE;
		status = WdfRegistryAssignULong(devkey, &key_str, value);
	}
	regs->BGR = (unsigned)value;
	
	RtlInitUnicodeString(&key_str, L"SSR");
	status = WdfRegistryQueryULong(devkey, &key_str, &value);
	if (!NT_SUCCESS(status)) {
		value = DEFAULT_SSR_VALUE;
		status = WdfRegistryAssignULong(devkey, &key_str, value);
	}
	regs->SSR = (unsigned)value;
	
	RtlInitUnicodeString(&key_str, L"SMR");
	status = WdfRegistryQueryULong(devkey, &key_str, &value);
	if (!NT_SUCCESS(status)) {
		value = DEFAULT_SMR_VALUE;
		status = WdfRegistryAssignULong(devkey, &key_str, value);
	}
	regs->SMR = (unsigned)value;
	
	RtlInitUnicodeString(&key_str, L"TSR");
	status = WdfRegistryQueryULong(devkey, &key_str, &value);
	if (!NT_SUCCESS(status)) {
		value = DEFAULT_TSR_VALUE;
		status = WdfRegistryAssignULong(devkey, &key_str, value);
	}
	regs->TSR = (unsigned)value;
	
	RtlInitUnicodeString(&key_str, L"TMR");
	status = WdfRegistryQueryULong(devkey, &key_str, &value);
	if (!NT_SUCCESS(status)) {
		value = DEFAULT_TMR_VALUE;
		status = WdfRegistryAssignULong(devkey, &key_str, value);
	}
	regs->TMR = (unsigned)value;
	
	RtlInitUnicodeString(&key_str, L"RAR");
	status = WdfRegistryQueryULong(devkey, &key_str, &value);
	if (!NT_SUCCESS(status)) {
		value = DEFAULT_RAR_VALUE;
		status = WdfRegistryAssignULong(devkey, &key_str, value);
	}
	regs->RAR = (unsigned)value;
	
	RtlInitUnicodeString(&key_str, L"RAMR");
	status = WdfRegistryQueryULong(devkey, &key_str, &value);
	if (!NT_SUCCESS(status)) {
		value = DEFAULT_RAMR_VALUE;
		status = WdfRegistryAssignULong(devkey, &key_str, value);
	}
	regs->RAMR = (unsigned)value;
	
	RtlInitUnicodeString(&key_str, L"PPR");
	status = WdfRegistryQueryULong(devkey, &key_str, &value);
	if (!NT_SUCCESS(status)) {
		value = DEFAULT_PPR_VALUE;
		status = WdfRegistryAssignULong(devkey, &key_str, value);
	}
	regs->PPR = (unsigned)value;
	
	RtlInitUnicodeString(&key_str, L"TCR");
	status = WdfRegistryQueryULong(devkey, &key_str, &value);
	if (!NT_SUCCESS(status)) {
		value = DEFAULT_TCR_VALUE;
		status = WdfRegistryAssignULong(devkey, &key_str, value);
	}
	regs->TCR = (unsigned)value;
	
	RtlInitUnicodeString(&key_str, L"IMR");
	status = WdfRegistryQueryULong(devkey, &key_str, &value);
	if (!NT_SUCCESS(status)) {
		value = DEFAULT_IMR_VALUE;
		status = WdfRegistryAssignULong(devkey, &key_str, value);
	}
	regs->IMR = (unsigned)value;
	
	RtlInitUnicodeString(&key_str, L"DPLLR");
	status = WdfRegistryQueryULong(devkey, &key_str, &value);
	if (!NT_SUCCESS(status)) {
		value = DEFAULT_DPLLR_VALUE;
		status = WdfRegistryAssignULong(devkey, &key_str, value);
	}
	regs->DPLLR = (unsigned)value;
	
	RtlInitUnicodeString(&key_str, L"FCR");
	status = WdfRegistryQueryULong(devkey, &key_str, &value);
	if (!NT_SUCCESS(status)) {
		value = DEFAULT_FCR_VALUE;
		status = WdfRegistryAssignULong(devkey, &key_str, value);
	}
	regs->FCR = (unsigned)value;
	
	WdfRegistryClose(devkey);

	return STATUS_SUCCESS;
}

#define FRIENDLYNAME_SIZE 256
NTSTATUS fscc_port_set_friendly_name(_In_ WDFDEVICE Device, unsigned portnum)
{
	NTSTATUS status;
	WDF_DEVICE_PROPERTY_DATA dpd;
	WCHAR friendlyName[FRIENDLYNAME_SIZE] = { 0 };
	int characters_written = 0;

	WDF_DEVICE_PROPERTY_DATA_INIT(&dpd, &DEVPKEY_Device_FriendlyName);
	dpd.Lcid = LOCALE_NEUTRAL;
	dpd.Flags = PLUGPLAY_PROPERTY_PERSISTENT;
	characters_written = swprintf_s(friendlyName, FRIENDLYNAME_SIZE, L"FSCC Port (FSCC%i)", portnum);
	if(characters_written < 0) {
		TraceEvents(TRACE_LEVEL_ERROR, TRACE_DEVICE, "swprintf_s failed with %d", characters_written);
		return STATUS_INVALID_PARAMETER;
	}

	status = WdfDeviceAssignProperty(Device, &dpd, DEVPROP_TYPE_STRING, (characters_written + 1)*sizeof(WCHAR), (PVOID)&friendlyName);
	if (!NT_SUCCESS(status)) {
		TraceEvents(TRACE_LEVEL_ERROR, TRACE_DEVICE, "WdfDeviceAssignProperty failed %!STATUS!", status);

		DbgPrint("WdfDeviceAssignProperty failed with 0x%X\n", status);
		return status;
	}
	
	return status;
}
