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


#include "port.h"
#include "frame.h"
#include "utils.h"
#include "isr.h"
#include "public.h"
#include "driver.h"
#include "debug.h"

#include <ntddser.h>
#include <ntstrsafe.h>

#if defined(EVENT_TRACING)
#include "port.tmh"
#endif

#define NUM_CLOCK_BYTES 20
#define TIMER_DELAY_MS 250

EVT_WDF_IO_QUEUE_IO_DEVICE_CONTROL FsccEvtIoDeviceControl;
EVT_WDF_IO_QUEUE_IO_WRITE FsccEvtIoWrite;
EVT_WDF_IO_QUEUE_IO_READ FsccEvtIoRead;
EVT_WDF_DEVICE_FILE_CREATE FsccDeviceFileCreate;
EVT_WDF_FILE_CLOSE FsccFileClose;
EVT_WDF_DEVICE_PREPARE_HARDWARE FsccEvtDevicePrepareHardware;
EVT_WDF_DEVICE_RELEASE_HARDWARE FsccEvtDeviceReleaseHardware;

EVT_WDF_DPC FsccProcessRead;

unsigned fscc_port_timed_out(struct fscc_port *port);
unsigned fscc_port_is_streaming(struct fscc_port *port);
NTSTATUS fscc_port_get_port_num(struct fscc_port *port, unsigned *port_num);
NTSTATUS fscc_port_set_port_num(struct fscc_port *port, unsigned value);

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
    port->istream = fscc_frame_new(port);
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

    status = WdfSpinLockCreate(&attributes, &port->istream_spinlock);
    if (!NT_SUCCESS(status)) {
        WdfObjectDelete(port->device);
        TraceEvents(TRACE_LEVEL_ERROR, TRACE_DEVICE,
            "WdfSpinLockCreate failed %!STATUS!", status);
        return 0;
    }

    status = WdfSpinLockCreate(&attributes, &port->pending_iframe_spinlock);
    if (!NT_SUCCESS(status)) {
        WdfObjectDelete(port->device);
        TraceEvents(TRACE_LEVEL_ERROR, TRACE_DEVICE,
            "WdfSpinLockCreate failed %!STATUS!", status);
        return 0;
    }

    status = WdfSpinLockCreate(&attributes, &port->pending_oframe_spinlock);
    if (!NT_SUCCESS(status)) {
        WdfObjectDelete(port->device);
        TraceEvents(TRACE_LEVEL_ERROR, TRACE_DEVICE,
            "WdfSpinLockCreate failed %!STATUS!", status);
        return 0;
    }

    status = WdfSpinLockCreate(&attributes, &port->sent_oframes_spinlock);
    if (!NT_SUCCESS(status)) {
        WdfObjectDelete(port->device);
        TraceEvents(TRACE_LEVEL_ERROR, TRACE_DEVICE,
            "WdfSpinLockCreate failed %!STATUS!", status);
        return 0;
    }

    status = WdfSpinLockCreate(&attributes, &port->queued_oframes_spinlock);
    if (!NT_SUCCESS(status)) {
        WdfObjectDelete(port->device);
        TraceEvents(TRACE_LEVEL_ERROR, TRACE_DEVICE,
            "WdfSpinLockCreate failed %!STATUS!", status);
        return 0;
    }

    status = WdfSpinLockCreate(&attributes, &port->queued_iframes_spinlock);
    if (!NT_SUCCESS(status)) {
        WdfObjectDelete(port->device);
        TraceEvents(TRACE_LEVEL_ERROR, TRACE_DEVICE,
            "WdfSpinLockCreate failed %!STATUS!", status);
        return 0;
    }

    fscc_flist_init(&port->queued_oframes);
    fscc_flist_init(&port->sent_oframes);
    fscc_flist_init(&port->queued_iframes);

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

    WDF_DPC_CONFIG_INIT(&dpcConfig, &clear_oframe_worker);
    dpcConfig.AutomaticSerialization = TRUE;

    WDF_OBJECT_ATTRIBUTES_INIT(&dpcAttributes);
    dpcAttributes.ParentObject = port->device;

    status = WdfDpcCreate(&dpcConfig, &dpcAttributes, &port->clear_oframe_dpc);
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

    return port;
}

NTSTATUS FsccEvtDevicePrepareHardware(WDFDEVICE Device,
            WDFCMRESLIST ResourcesRaw, WDFCMRESLIST ResourcesTranslated)
{
    unsigned char clock_bits[20] = DEFAULT_CLOCK_BITS;
    struct fscc_memory_cap memory_cap;

    WDF_TIMER_CONFIG  timerConfig;
    WDF_OBJECT_ATTRIBUTES  timerAttributes;
    NTSTATUS  status;

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

    fscc_port_set_append_status(port, DEFAULT_APPEND_STATUS_VALUE);
    fscc_port_set_append_timestamp(port, DEFAULT_APPEND_TIMESTAMP_VALUE);
    fscc_port_set_ignore_timeout(port, DEFAULT_IGNORE_TIMEOUT_VALUE);
    fscc_port_set_tx_modifiers(port, DEFAULT_TX_MODIFIERS_VALUE);
    fscc_port_set_rx_multiple(port, DEFAULT_RX_MULTIPLE_VALUE);
    fscc_port_set_wait_on_write(port, DEFAULT_WAIT_ON_WRITE_VALUE);

    memory_cap.input = DEFAULT_INPUT_MEMORY_CAP_VALUE;
    memory_cap.output = DEFAULT_OUTPUT_MEMORY_CAP_VALUE;

    fscc_port_set_memory_cap(port, &memory_cap);

    port->force_fifo = DEFAULT_FORCE_FIFO_VALUE;

    port->pending_oframe = 0;
    port->pending_iframe = 0;

    port->last_isr_value = 0;

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

    fscc_port_purge_rx(port);
    fscc_port_purge_tx(port);

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

    WdfSpinLockAcquire(port->istream_spinlock);
    fscc_frame_delete(port->istream);
    WdfSpinLockRelease(port->istream_spinlock);

    WdfSpinLockAcquire(port->queued_iframes_spinlock);
    fscc_flist_delete(&port->queued_iframes);
    WdfSpinLockRelease(port->queued_iframes_spinlock);

    WdfSpinLockAcquire(port->queued_oframes_spinlock);
    fscc_flist_delete(&port->queued_oframes);
    WdfSpinLockRelease(port->queued_oframes_spinlock);

    WdfSpinLockAcquire(port->sent_oframes_spinlock);
    fscc_flist_delete(&port->sent_oframes);
    WdfSpinLockRelease(port->sent_oframes_spinlock);

    //WdfTimerStop(port->timer, FALSE);

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

VOID FsccEvtIoDeviceControl(IN WDFQUEUE Queue, IN WDFREQUEST Request,
    IN size_t OutputBufferLength, IN size_t InputBufferLength,
    IN ULONG IoControlCode)
{
    NTSTATUS status = STATUS_SUCCESS;
    struct fscc_port *port = 0;
    size_t bytes_returned = 0;

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

            memcpy(output_regs, input_regs, sizeof(struct fscc_registers));

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
        status = fscc_port_purge_tx(port);
        if (!NT_SUCCESS(status)) {
            TraceEvents(TRACE_LEVEL_WARNING, TRACE_DEVICE,
                "fscc_port_purge_tx failed %!STATUS!", status);
            break;
        }

        break;

    case FSCC_PURGE_RX:
        status = fscc_port_purge_rx(port);
        if (!NT_SUCCESS(status)) {
            TraceEvents(TRACE_LEVEL_WARNING, TRACE_DEVICE,
                "fscc_port_purge_rx failed %!STATUS!", status);
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

    case FSCC_SET_MEMORY_CAP: {
            struct fscc_memory_cap *memcap = 0;

            status = WdfRequestRetrieveInputBuffer(Request, sizeof(*memcap),
                        (PVOID *)&memcap, NULL);
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

            status = WdfRequestRetrieveOutputBuffer(Request, sizeof(*memcap),
                        (PVOID *)&memcap, NULL);
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

    default:
        TraceEvents(TRACE_LEVEL_WARNING, TRACE_DEVICE,
            "Unknown DeviceIoControl 0x%x", IoControlCode);
        break;
    }

    WdfRequestCompleteWithInformation(Request, status, bytes_returned);
}

/*
    Handles taking the frames already retrieved from the card and giving them
    to the user. This is purely a helper for the fscc_port_read function.
*/
int fscc_port_frame_read(struct fscc_port *port, char *buf, size_t buf_length,
                         size_t *out_length)
{
    struct fscc_frame *frame = 0;
    unsigned remaining_buf_length = 0;
    int max_frame_length = 0;
    unsigned current_frame_length = 0;

    return_val_if_untrue(port, 0);
    
    *out_length = 0;

    do {
        remaining_buf_length = buf_length - *out_length;
        
        if (port->append_status && port->append_timestamp)
            max_frame_length = remaining_buf_length - sizeof(LARGE_INTEGER);
        else if (port->append_status)
            max_frame_length = remaining_buf_length;
        else if (port->append_timestamp)
            max_frame_length = remaining_buf_length + 2 - sizeof(LARGE_INTEGER); // Status length
        else
            max_frame_length = remaining_buf_length + 2; // Status length

        if (max_frame_length < 0)
            break;

        WdfSpinLockAcquire(port->queued_iframes_spinlock);
        frame = fscc_flist_remove_frame_if_lte(&port->queued_iframes, max_frame_length);
        WdfSpinLockRelease(port->queued_iframes_spinlock);

        if (!frame)
            break;

        current_frame_length = fscc_frame_get_length(frame);
        current_frame_length -= (port->append_status) ? 0 : 2;

        fscc_frame_remove_data(frame, buf + *out_length, current_frame_length);
        *out_length += current_frame_length;

        if (port->append_timestamp) {
            memcpy(buf + *out_length, &frame->timestamp, sizeof(frame->timestamp));
            current_frame_length += sizeof(frame->timestamp);
            *out_length += sizeof(frame->timestamp);
        }

        fscc_frame_delete(frame);
    }
    while (port->rx_multiple);
    
    if (*out_length == 0)
        return STATUS_BUFFER_TOO_SMALL;
        
    return STATUS_SUCCESS;
}

/*
    Handles taking the streams already retrieved from the card and giving them
    to the user. This is purely a helper for the fscc_port_read function.
*/
int fscc_port_stream_read(struct fscc_port *port, char *buf, size_t buf_length,
                          size_t *out_length)
{
    return_val_if_untrue(port, 0);

    WdfSpinLockAcquire(port->istream_spinlock);

    *out_length = min(buf_length, (size_t)fscc_frame_get_length(port->istream));
    
    fscc_frame_remove_data(port->istream, buf, (unsigned)(*out_length));

    WdfSpinLockRelease(port->istream_spinlock);

    return STATUS_SUCCESS;
}

VOID FsccEvtIoRead(IN WDFQUEUE Queue, IN WDFREQUEST Request, IN size_t Length)
{
    NTSTATUS status = STATUS_SUCCESS;
    struct fscc_port *port = 0;

    port = WdfObjectGet_FSCC_PORT(WdfIoQueueGetDevice(Queue));

    /* The user is requsting 0 bytes so return immediately */
    if (Length == 0) {
        WdfRequestCompleteWithInformation(Request, STATUS_SUCCESS, Length);
        return;
    }

    status = WdfRequestForwardToIoQueue(Request, port->read_queue2);
    if (!NT_SUCCESS(status)) {
        TraceEvents(TRACE_LEVEL_ERROR, TRACE_DEVICE,
                    "WdfRequestForwardToIoQueue failed %!STATUS!", status);
        WdfRequestComplete(Request, status);
        return;
    }

    WdfDpcEnqueue(port->process_read_dpc);
}

void FsccProcessRead(WDFDPC Dpc)
{
    struct fscc_port *port = 0;
    NTSTATUS status = STATUS_SUCCESS;
    PCHAR data_buffer = NULL;
    size_t read_count = 0;
    WDFREQUEST request;
    unsigned length = 0;
    WDF_REQUEST_PARAMETERS params;

    port = WdfObjectGet_FSCC_PORT(WdfDpcGetParentObject(Dpc));

    if (!fscc_port_has_incoming_data(port))
        return;

    status = WdfIoQueueRetrieveNextRequest(port->read_queue2, &request);
    if (!NT_SUCCESS(status)) {
        if (status != STATUS_NO_MORE_ENTRIES) {
            TraceEvents(TRACE_LEVEL_ERROR, TRACE_DEVICE,
                        "WdfIoQueueRetrieveNextRequest failed %!STATUS!",
                        status);
        }

        return;
    }

    WDF_REQUEST_PARAMETERS_INIT(&params);
    WdfRequestGetParameters(request, &params);
    length = (unsigned)params.Parameters.Read.Length;

    status = WdfRequestRetrieveOutputBuffer(request, length,
                (PVOID*)&data_buffer, NULL);
    if (!NT_SUCCESS(status)) {
        TraceEvents(TRACE_LEVEL_ERROR, TRACE_DEVICE,
                    "WdfRequestRetrieveOutputBuffer failed %!STATUS!", status);
        WdfRequestComplete(request, status);
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
        return;
    }

    WdfRequestCompleteWithInformation(request, status, read_count);
}

VOID FsccEvtIoWrite(IN WDFQUEUE Queue, IN WDFREQUEST Request, IN size_t Length)
{
    NTSTATUS status;
    char *data_buffer = NULL;
    struct fscc_port *port = 0;
    struct fscc_frame *frame = 0;

    port = WdfObjectGet_FSCC_PORT(WdfIoQueueGetDevice(Queue));

    if (Length == 0) {
        WdfRequestCompleteWithInformation(Request, STATUS_SUCCESS, Length);
        return;
    }

    if (Length > fscc_port_get_output_memory_cap(port)) {
        WdfRequestComplete(Request, STATUS_BUFFER_TOO_SMALL);
        return;
    }

    /* Checks to make sure there is a clock present. */
    if (port->ignore_timeout == FALSE && fscc_port_timed_out(port)) {
        TraceEvents(TRACE_LEVEL_WARNING, TRACE_DEVICE,
                    "device stalled (wrong clock mode?)");
        WdfRequestComplete(Request, STATUS_IO_TIMEOUT);
        return;
    }

    status = WdfRequestRetrieveInputBuffer(Request, Length,
                (PVOID *)&data_buffer, NULL);
    if (!NT_SUCCESS(status)) {
        TraceEvents(TRACE_LEVEL_ERROR, TRACE_DEVICE,
            "WdfRequestRetrieveInputBuffer failed %!STATUS!", status);
        WdfRequestComplete(Request, status);
        return;
    }

    frame = fscc_frame_new(port);

    if (!frame) {
        WdfRequestComplete(Request, STATUS_INSUFFICIENT_RESOURCES);
        return;
    }

    fscc_frame_add_data(frame, data_buffer, Length);

    WdfSpinLockAcquire(port->queued_oframes_spinlock);
    fscc_flist_add_frame(&port->queued_oframes, frame);
    WdfSpinLockRelease(port->queued_oframes_spinlock);

    if (port->wait_on_write) {
        status = WdfRequestForwardToIoQueue(Request, port->write_queue2);
        if (!NT_SUCCESS(status)) {
            TraceEvents(TRACE_LEVEL_ERROR, TRACE_DEVICE,
                        "WdfRequestForwardToIoQueue failed %!STATUS!", status);
            WdfRequestComplete(Request, status);
            return;
        }
    }
    else {
        WdfRequestCompleteWithInformation(Request, STATUS_SUCCESS, Length);
    }

    WdfDpcEnqueue(port->oframe_dpc);
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

    offset = port_offset(port, bar, register_offset);

    /* Checks to make sure there is a clock present. */
    if (register_offset == CMDR_OFFSET && port->ignore_timeout == FALSE
        && fscc_port_timed_out(port)) {
        return STATUS_IO_TIMEOUT;
    }

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
NTSTATUS fscc_port_set_registers(struct fscc_port *port,
                            const struct fscc_registers *regs)
{
    unsigned stalled = 0;
    unsigned i = 0;

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
                                   (UINT32)(((fscc_register *)regs)[i]) & 0x3fffffff);
        }
    }

    return (stalled) ? STATUS_IO_TIMEOUT : STATUS_SUCCESS;
}

void fscc_port_get_registers(struct fscc_port *port,
                             struct fscc_registers *regs)
{
    unsigned i = 0;

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

NTSTATUS fscc_port_execute_TRES(struct fscc_port *port)
{
    return_val_if_untrue(port, 0);

    return fscc_port_set_register(port, 0, CMDR_OFFSET, 0x08000000);
}

NTSTATUS fscc_port_execute_RRES(struct fscc_port *port)
{
    return_val_if_untrue(port, 0);

    return fscc_port_set_register(port, 0, CMDR_OFFSET, 0x00020000);
}

NTSTATUS fscc_port_purge_rx(struct fscc_port *port)

{
    NTSTATUS status;

    return_val_if_untrue(port, 0);

    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DEVICE,
                "Purging receive data");

    WdfSpinLockAcquire(port->board_rx_spinlock);
    status = fscc_port_execute_RRES(port);
    WdfSpinLockRelease(port->board_rx_spinlock);

    if (!NT_SUCCESS(status)) {
        TraceEvents(TRACE_LEVEL_ERROR, TRACE_DEVICE,
            "fscc_port_execute_RRES failed %!STATUS!", status);
        return status;
    }

    WdfSpinLockAcquire(port->queued_iframes_spinlock);
    fscc_flist_clear(&port->queued_iframes);
    WdfSpinLockRelease(port->queued_iframes_spinlock);

    WdfSpinLockAcquire(port->istream_spinlock);
    fscc_frame_clear(port->istream);
    WdfSpinLockRelease(port->istream_spinlock);

    WdfSpinLockAcquire(port->pending_iframe_spinlock);
    if (port->pending_iframe) {
        fscc_frame_delete(port->pending_iframe);
        port->pending_iframe = 0;
    }
    WdfSpinLockRelease(port->pending_iframe_spinlock);

    WdfIoQueuePurgeSynchronously(port->read_queue);
    WdfIoQueuePurgeSynchronously(port->read_queue2);
    WdfIoQueueStart(port->read_queue);
    WdfIoQueueStart(port->read_queue2);

    return STATUS_SUCCESS;
}

NTSTATUS fscc_port_purge_tx(struct fscc_port *port)
{
    int error_code = 0;

    return_val_if_untrue(port, 0);

    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DEVICE,
                "Purging transmit data");

    WdfSpinLockAcquire(port->board_tx_spinlock);
    error_code = fscc_port_execute_TRES(port);
    WdfSpinLockRelease(port->board_tx_spinlock);

    if (error_code < 0)
        return error_code;

    WdfSpinLockAcquire(port->queued_oframes_spinlock);
    fscc_flist_clear(&port->queued_oframes);
    WdfSpinLockRelease(port->queued_oframes_spinlock);

    WdfSpinLockAcquire(port->sent_oframes_spinlock);
    fscc_flist_clear(&port->sent_oframes);
    WdfSpinLockRelease(port->sent_oframes_spinlock);

    WdfSpinLockAcquire(port->pending_oframe_spinlock);
    if (port->pending_oframe) {
        fscc_frame_delete(port->pending_oframe);
        port->pending_oframe = 0;
    }
    WdfSpinLockRelease(port->pending_oframe_spinlock);

    WdfIoQueuePurgeSynchronously(port->write_queue);
    WdfIoQueuePurgeSynchronously(port->write_queue2);
    WdfIoQueueStart(port->write_queue);
    WdfIoQueueStart(port->write_queue2);

    return STATUS_SUCCESS;
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

    return !fscc_port_is_streaming(port) && port->append_status;
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

    return !fscc_port_is_streaming(port) && port->append_timestamp;
}

void fscc_port_set_ignore_timeout(struct fscc_port *port,
                                  BOOLEAN value)
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

unsigned fscc_port_get_input_memory_cap(struct fscc_port *port)
{
    return_val_if_untrue(port, 0);

    return port->memory_cap.input;
}

unsigned fscc_port_get_output_memory_cap(struct fscc_port *port)
{
    return_val_if_untrue(port, 0);

    return port->memory_cap.output;
}

void fscc_port_set_memory_cap(struct fscc_port *port,
                              struct fscc_memory_cap *value)
{
    return_if_untrue(port);
    return_if_untrue(value);

    if (value->input >= 0) {
        if (port->memory_cap.input != value->input) {
            TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DEVICE,
                        "Memory cap (input) %i => %i",
                        port->memory_cap.input, value->input);
        }
        else {
            TraceEvents(TRACE_LEVEL_VERBOSE, TRACE_DEVICE,
                        "Memory cap (input) = %i",
                        value->input);
        }

        port->memory_cap.input = value->input;
    }

    if (value->output >= 0) {
        if (port->memory_cap.output != value->output) {
            TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DEVICE,
                        "Memory cap (output) %i => %i",
                        port->memory_cap.output, value->output);
        }
        else {
            TraceEvents(TRACE_LEVEL_VERBOSE, TRACE_DEVICE,
                        "Memory cap (output) = %i",
                        value->output);
        }

        port->memory_cap.output = value->output;
    }
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


    data = (UINT32 *)ExAllocatePoolWithTag(PagedPool, sizeof(UINT32) * 323, 'stiB');

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
    UINT32 fcr;

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

unsigned fscc_port_get_output_memory_usage(struct fscc_port *port)
{
    unsigned value = 0;

    return_val_if_untrue(port, 0);

    WdfSpinLockAcquire(port->queued_oframes_spinlock);
    value = port->queued_oframes.estimated_memory_usage;
    WdfSpinLockRelease(port->queued_oframes_spinlock);

    WdfSpinLockAcquire(port->pending_oframe_spinlock);
    if (port->pending_oframe)
        value += fscc_frame_get_length(port->pending_oframe);
    WdfSpinLockRelease(port->pending_oframe_spinlock);

    return value;
}

unsigned fscc_port_get_input_memory_usage(struct fscc_port *port)
{
    unsigned value = 0;

    return_val_if_untrue(port, 0);

    WdfSpinLockAcquire(port->queued_iframes_spinlock);
    value = port->queued_iframes.estimated_memory_usage;
    WdfSpinLockRelease(port->queued_iframes_spinlock);

    WdfSpinLockAcquire(port->istream_spinlock);
    value += fscc_frame_get_length(port->istream);
    WdfSpinLockRelease(port->istream_spinlock);

    WdfSpinLockAcquire(port->pending_iframe_spinlock);
    if (port->pending_oframe)
        value += fscc_frame_get_length(port->pending_iframe);
    WdfSpinLockRelease(port->pending_iframe_spinlock);

    return value;
}

unsigned fscc_port_is_streaming(struct fscc_port *port)
{
    unsigned transparent_mode = 0;
    unsigned xsync_mode = 0;
    unsigned rlc_mode = 0;
    unsigned fsc_mode = 0;
    unsigned ntb = 0;

    return_val_if_untrue(port, 0);

    transparent_mode = ((port->register_storage.CCR0 & 0x3) == 0x2) ? 1 : 0;
    xsync_mode = ((port->register_storage.CCR0 & 0x3) == 0x1) ? 1 : 0;
    rlc_mode = (port->register_storage.CCR2 & 0xffff0000) ? 1 : 0;
    fsc_mode = (port->register_storage.CCR0 & 0x700) ? 1 : 0;
    ntb = (port->register_storage.CCR0 & 0x70000) >> 16;

    return ((transparent_mode || xsync_mode) && !(rlc_mode || fsc_mode || ntb)) ? 1 : 0;
}

BOOLEAN fscc_port_has_dma(struct fscc_port *port)
{
    return_val_if_untrue(port, 0);

    if (port->force_fifo)
       return FALSE;

    return port->dma;
}

UINT32 fscc_port_get_TXCNT(struct fscc_port *port)
{
    UINT32 fifo_bc_value = 0;

    return_val_if_untrue(port, 0);

    fifo_bc_value = fscc_port_get_register(port, 0, FIFO_BC_OFFSET);

    return (fifo_bc_value & 0x1FFF0000) >> 16;
}

void fscc_port_execute_transmit(struct fscc_port *port, unsigned dma)
{
    unsigned command_register = 0;
    unsigned command_value = 0;
    unsigned command_bar = 0;

    return_if_untrue(port);

    if (dma) {
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
}

unsigned fscc_port_get_RFCNT(struct fscc_port *port)
{
    UINT32 fifo_fc_value = 0;

    return_val_if_untrue(port, 0);

    fifo_fc_value = fscc_port_get_register(port, 0, FIFO_FC_OFFSET);

    return (unsigned)(fifo_fc_value & 0x000003ff);
}

unsigned fscc_port_get_RXCNT(struct fscc_port *port)
{
    UINT32 fifo_bc_value = 0;

    return_val_if_untrue(port, 0);

    fifo_bc_value = fscc_port_get_register(port, 0, FIFO_BC_OFFSET);

    /* Not sure why, but this can be larger than 8192. We add
       the 8192 check here so other code can count on the value
       not being larger than 8192. */
    return min(fifo_bc_value & 0x00003FFF, 8192);
}

void fscc_port_reset_timer(struct fscc_port *port)
{
    WdfTimerStart(port->timer, WDF_ABS_TIMEOUT_IN_MS(TIMER_DELAY_MS));
}

/* Count is for streaming mode where we need to check there is enough
   streaming data.
*/
unsigned fscc_port_has_incoming_data(struct fscc_port *port)
{
    unsigned status = 0;

    return_val_if_untrue(port, 0);

    if (fscc_port_is_streaming(port)) {
        status = (fscc_frame_is_empty(port->istream)) ? 0 : 1;
    }
    else {
        WdfSpinLockAcquire(port->queued_iframes_spinlock);
        if (fscc_flist_is_empty(&port->queued_iframes) == FALSE)
            status = 1;
        WdfSpinLockRelease(port->queued_iframes_spinlock);
    }
    return status;
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

#define TX_FIFO_SIZE 4096

int prepare_frame_for_dma(struct fscc_port *port, struct fscc_frame *frame,
                          unsigned *length)
{
	return 2;
}

int prepare_frame_for_fifo(struct fscc_port *port, struct fscc_frame *frame,
                           unsigned *length)
{
	unsigned current_length = 0;
	unsigned buffer_size = 0;
	unsigned fifo_space = 0;
	unsigned size_in_fifo = 0;
	unsigned transmit_length = 0;

	current_length = fscc_frame_get_length(frame);
	buffer_size = fscc_frame_get_buffer_size(frame);    
    /* How much space this will take up in the FIFO */
	size_in_fifo = current_length + (4 - current_length % 4);

	/* Subtracts 1 so a TDO overflow doesn't happen on the 4096th byte. */
	fifo_space = TX_FIFO_SIZE - fscc_port_get_TXCNT(port) - 1;
	fifo_space -= fifo_space % 4;

	/* Determine the maximum amount of data we can send this time around. */
	transmit_length = (size_in_fifo > fifo_space) ? fifo_space : current_length;

	frame->fifo_initialized = 1;

	if (transmit_length == 0)
		return 0;

	fscc_port_set_register_rep(port, 0, FIFO_OFFSET,
							   frame->buffer,
							   transmit_length);

	fscc_frame_remove_data(frame, NULL, transmit_length);

	*length = transmit_length;

	/* If this is the first time we add data to the FIFO for this frame we
	   tell the port how much data is in this frame. */
	if (current_length == buffer_size)
		fscc_port_set_register(port, 0, BC_FIFO_L_OFFSET, buffer_size);

	/* We still have more data to send. */
	if (!fscc_frame_is_empty(frame))
		return 1;

	return 2;
}

unsigned fscc_port_transmit_frame(struct fscc_port *port, struct fscc_frame *frame)
{
    unsigned transmit_dma = 0;
    unsigned transmit_length = 0;
    int result;

    if (fscc_port_has_dma(port) &&
        (fscc_frame_get_length(frame) <= TX_FIFO_SIZE) &&
        !fscc_frame_is_fifo(frame)) {
        transmit_dma = 1;
    }

    //if (transmit_dma)
    //	result = prepare_frame_for_dma(port, frame, &transmit_length);
    //else
        result = prepare_frame_for_fifo(port, frame, &transmit_length);

    if (result)
        fscc_port_execute_transmit(port, transmit_dma);

    TraceEvents(TRACE_LEVEL_VERBOSE, TRACE_DEVICE,
            "F#%i => %i byte%s%s",
            frame->number, transmit_length,
            (transmit_length == 1) ? "" : "s",
            (result != 2) ? " (starting)" : "");

    return result;
}