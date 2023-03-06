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
#include "io.h"
#include "public.h"
#include "driver.h"
#include "debug.h"
#include "utils.h"

#include <ntddser.h>
#include <ntstrsafe.h>

#if defined(EVENT_TRACING)
#include "io.tmh"
#endif

NTSTATUS fscc_io_reset_tx(struct fscc_port *port);
NTSTATUS fscc_io_reset_rx(struct fscc_port *port);

void FsccProcessRead(WDFDPC Dpc)
{
	struct fscc_port *port = 0;
	NTSTATUS status = STATUS_SUCCESS;
	PCHAR data_buffer = NULL;
	size_t read_count = 0;
	WDFREQUEST request;
	unsigned length = 0;
	WDF_REQUEST_PARAMETERS params;
	unsigned frame_ready, streaming;
	size_t bytes_ready;
	
	port = WdfObjectGet_FSCC_PORT(WdfDpcGetParentObject(Dpc));
	streaming = fscc_io_is_streaming(port);
	WdfSpinLockAcquire(port->board_rx_spinlock);
	frame_ready = fscc_user_next_read_size(port, &bytes_ready);
	WdfSpinLockRelease(port->board_rx_spinlock);
	if (bytes_ready == 0) return;
	if (!streaming && !frame_ready) return;
	
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
	
	if (streaming) status = fscc_user_read_stream(port, data_buffer, length, &read_count);
	else status = fscc_user_read_frame(port, data_buffer, length, &read_count);

	if (!NT_SUCCESS(status)) {
		TraceEvents(TRACE_LEVEL_WARNING, TRACE_DEVICE,
		"fscc_port_{frame,stream}_read failed %!STATUS!", status);
		WdfRequestComplete(request, status);
		return;
	}

	WdfRequestCompleteWithInformation(request, status, read_count);
}

struct dma_frame *fscc_io_create_frame(struct fscc_port *port, size_t size_of_buffer) 
{
	NTSTATUS status = STATUS_SUCCESS;
	PHYSICAL_ADDRESS temp_address;
	struct dma_frame *frame = 0;
	
	frame = (struct dma_frame *)ExAllocatePoolWithTag(NonPagedPool, sizeof(struct dma_frame), 'CSED');
	if(frame == NULL) {
		DbgPrint("Failed to make frame..\n");
		TraceEvents(TRACE_LEVEL_ERROR, TRACE_DEVICE, "ExAllocatePoolWithTag failed!");
		return 0;
	}
	status = WdfCommonBufferCreate(port->dma_enabler, sizeof(struct fscc_descriptor), WDF_NO_OBJECT_ATTRIBUTES, &frame->desc_buffer);
	if(!NT_SUCCESS(status)) {
		DbgPrint("Failed to make desc_buffer\n");
		ExFreePoolWithTag(frame, 'CSED');
		TraceEvents(TRACE_LEVEL_ERROR, TRACE_DEVICE, "WdfCommonBufferCreate failed!");
		return 0;
	}
	status = WdfCommonBufferCreate(port->dma_enabler, size_of_buffer, WDF_NO_OBJECT_ATTRIBUTES, &frame->data_buffer);
	if(!NT_SUCCESS(status)) {
		DbgPrint("Failed to make data_buffer\n");
		WdfObjectDelete(frame->desc_buffer);
		ExFreePoolWithTag(frame, 'CSED');
		TraceEvents(TRACE_LEVEL_ERROR, TRACE_DEVICE, "WdfCommonBufferCreate failed! %!STATUS!", status);
		return 0;
	}
	
	frame->desc = WdfCommonBufferGetAlignedVirtualAddress(frame->desc_buffer);
	temp_address = WdfCommonBufferGetAlignedLogicalAddress(frame->desc_buffer);
	frame->desc_physical_address = temp_address.LowPart;
	frame->desc_size = WdfCommonBufferGetLength(frame->desc_buffer);
	RtlZeroMemory(frame->desc, frame->desc_size);
	
	frame->buffer = WdfCommonBufferGetAlignedVirtualAddress(frame->data_buffer);
	temp_address = WdfCommonBufferGetAlignedLogicalAddress(frame->data_buffer);
	frame->desc->data_address = temp_address.LowPart;
	frame->data_size = WdfCommonBufferGetLength(frame->data_buffer);
	RtlZeroMemory(frame->buffer, frame->data_size);
	
	clear_timestamp(&frame->timestamp);
	
	return frame;
}

void fscc_io_destroy_frame(struct fscc_port *port, struct dma_frame *frame)
{
	RtlZeroMemory(frame->buffer, frame->data_size);
	frame->desc->data_address = 0;
	frame->buffer = 0;
	if(frame->data_buffer)
		WdfObjectDelete(frame->data_buffer);
	frame->data_buffer = 0;
	
	RtlZeroMemory(frame->desc, frame->desc_size);
	frame->desc_physical_address = 0;
	frame->desc = 0;
	if(frame->desc_buffer) 
		WdfObjectDelete(frame->desc_buffer);
	frame->desc_buffer = 0;
	
	ExFreePoolWithTag(frame, 'CSED');
	frame = 0;
}

void fscc_io_link_desc(struct fscc_port *port, struct dma_frame **descs, size_t number_of_buffers)
{
	size_t i;
	
	for(i=0;i<number_of_buffers;i++)
	{
		descs[i]->desc->next_descriptor = descs[i < number_of_buffers-1 ? i + 1 : 0]->desc_physical_address;
	}
}

NTSTATUS fscc_io_create_rx(struct fscc_port *port, size_t number_of_buffers, size_t size_of_buffers)
{
	NTSTATUS status;
	PHYSICAL_ADDRESS temp_address;
	size_t i;
	
	if(number_of_buffers < 2) 
		number_of_buffers = 2;
	if(size_of_buffers % 4)
		size_of_buffers += (4 - (size_of_buffers % 4));
	
	port->rx_descriptors = (struct dma_frame **)ExAllocatePoolWithTag(NonPagedPool, (sizeof(struct dma_frame *) * number_of_buffers), 'CSED');
	if(port->rx_descriptors == NULL) {
		TraceEvents(TRACE_LEVEL_ERROR, TRACE_DEVICE, "ExAllocatePoolWithTag for all rx desc failed!");
		DbgPrint("Failed all rx desc\n");
		port->memory.rx_num = 0;
		port->memory.rx_size = 0;
		return STATUS_UNSUCCESSFUL;
	}
	
	for(i=0;i<number_of_buffers;i++) {
		port->rx_descriptors[i] = fscc_io_create_frame(port, size_of_buffers);
		if(!port->rx_descriptors[i]) {
			DbgPrint("Failed to create rx frame at %d\n", i);
			TraceEvents(TRACE_LEVEL_ERROR, TRACE_DEVICE, "Failed to create rx frame at %d!", i);
			break;
		}

		if(i%2) 
			port->rx_descriptors[i]->desc->control = DESC_HI_BIT;
		else 
			port->rx_descriptors[i]->desc->control = 0;
					
		port->rx_descriptors[i]->desc->data_count = port->rx_descriptors[i]->data_size;
	}
	
	port->memory.rx_size = size_of_buffers;
	port->memory.rx_num = i;
	
	fscc_io_link_desc(port, port->rx_descriptors, port->memory.rx_num);

	return STATUS_SUCCESS;
}

NTSTATUS fscc_io_create_tx(struct fscc_port *port, size_t number_of_buffers, size_t size_of_buffers)
{
	NTSTATUS status;
	PHYSICAL_ADDRESS temp_address;
	size_t i;

	if(number_of_buffers < 2) 
		number_of_buffers = 2;
	if(size_of_buffers % 4)
		size_of_buffers += (4 - (size_of_buffers % 4));
	
	port->tx_descriptors = (struct dma_frame **)ExAllocatePoolWithTag(NonPagedPool, (sizeof(struct dma_frame *) * number_of_buffers), 'CSED');
	if(port->tx_descriptors == NULL) {
		TraceEvents(TRACE_LEVEL_ERROR, TRACE_DEVICE, "ExAllocatePoolWithTag for all tx desc failed!");
		DbgPrint("Failed all tx desc\n");
		port->memory.tx_num = 0;
		port->memory.tx_size = 0;
		return STATUS_UNSUCCESSFUL;
	}
	
	for(i=0;i<number_of_buffers;i++) {
		port->tx_descriptors[i] = fscc_io_create_frame(port, size_of_buffers);
		if(!port->tx_descriptors[i]) {
			DbgPrint("Failed to create tx frame at %d\n", i);
			TraceEvents(TRACE_LEVEL_ERROR, TRACE_DEVICE, "Failed to create tx frame at %d!", i);
			break;
		}
		
		if(i%2) 
			port->tx_descriptors[i]->desc->control = DESC_HI_BIT;
		else 
			port->tx_descriptors[i]->desc->control = 0;
		
		port->tx_descriptors[i]->desc->data_count = 0;
		
	}
	
	port->memory.tx_size = size_of_buffers;
	port->memory.tx_num = i;
	
	fscc_io_link_desc(port, port->tx_descriptors, port->memory.tx_num);
	
	return STATUS_SUCCESS;
}

void fscc_io_destroy_rx(struct fscc_port *port)
{
	size_t i;
	
	fscc_dma_execute_STOP_R(port);
	fscc_dma_execute_RST_R(port);
	if(fscc_port_uses_dma(port))
		fscc_port_set_register(port, 2, DMA_RX_BASE_OFFSET, 0);
	
	if(!port->rx_descriptors) 
		return;
	
	for(i=0;i<port->memory.rx_num;i++) 
	{
		fscc_io_destroy_frame(port, port->rx_descriptors[i]);
	}
	
	ExFreePoolWithTag(port->rx_descriptors, 'CSED');
	port->rx_descriptors = 0;
}

void fscc_io_destroy_tx(struct fscc_port *port)
{
	size_t i;
	
	fscc_dma_execute_STOP_T(port);
	fscc_dma_execute_RST_T(port);
	if(fscc_port_uses_dma(port))
		fscc_port_set_register(port, 2, DMA_TX_BASE_OFFSET, 0);
	
	if(!port->tx_descriptors) 
		return;
	
	for(i=0;i<port->memory.tx_num;i++)
	{
		fscc_io_destroy_frame(port, port->tx_descriptors[i]);
	}
	
	ExFreePoolWithTag(port->tx_descriptors, 'CSED');
	port->tx_descriptors = 0;
}

NTSTATUS fscc_io_initialize(struct fscc_port *port)
{
	NTSTATUS status;
	WDF_OBJECT_ATTRIBUTES attributes;
	WDF_DMA_ENABLER_CONFIG dma_config;

	// The registers are 4 bytes.
	WdfDeviceSetAlignmentRequirement(port->device, FILE_LONG_ALIGNMENT);
	// Technically the descriptors allowed for 536870911 (0x1FFFFFFF) but rounding might be best.
	WDF_DMA_ENABLER_CONFIG_INIT(&dma_config, WdfDmaProfilePacket, 536870000);
	status = WdfDmaEnablerCreate(port->device, &dma_config, WDF_NO_OBJECT_ATTRIBUTES, &port->dma_enabler);
	if(!NT_SUCCESS(status)) {
		port->has_dma = 0;
		TraceEvents(TRACE_LEVEL_ERROR, TRACE_DEVICE, "WdfDmaEnablerCreate failed: %!STATUS!", status);
		return status;
	}
	return status;
}

NTSTATUS fscc_io_reset_rx(struct fscc_port *port) {
	NTSTATUS status;
	size_t i;
	
	fscc_dma_execute_STOP_R(port);
	status = fscc_io_execute_RRES(port);
	fscc_dma_execute_RST_R(port);
	for(i=0;i<port->memory.rx_num;i++)
	{
		port->rx_descriptors[i]->desc->control = DESC_HI_BIT;
		port->rx_descriptors[i]->desc->data_count = port->rx_descriptors[i]->data_size;
		clear_timestamp(&port->rx_descriptors[i]->timestamp);
	}
	port->user_rx_desc = 0;
	port->fifo_rx_desc = 0;
	port->rx_bytes_in_frame = 0;
	port->rx_frame_size = 0;
	
	return status;
}

NTSTATUS fscc_io_reset_tx(struct fscc_port *port) {
	NTSTATUS status;
	size_t i;
	
	fscc_dma_execute_STOP_T(port);
	status = fscc_io_execute_TRES(port);
	fscc_dma_execute_RST_T(port);
	for(i=0;i<port->memory.tx_num;i++)
	{
		port->tx_descriptors[i]->desc->control = DESC_CSTOP_BIT;
		port->tx_descriptors[i]->desc->data_count = 0;
		clear_timestamp(&port->tx_descriptors[i]->timestamp);
	}
	port->user_tx_desc = 0;
	port->fifo_tx_desc = 0;
	port->tx_bytes_in_frame = 0;
	port->tx_frame_size = 0;
	
	return status;
}

NTSTATUS fscc_io_purge_rx(struct fscc_port *port)
{
	UINT32 orig_CCR0;
	
	return_val_if_untrue(port, 0);

	TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DEVICE, "Purging receive data");
	
	orig_CCR0 = (UINT32)port->register_storage.CCR0;
	// enable RECD
	fscc_port_set_register(port, 0, CCR0_OFFSET, orig_CCR0 | 0x02000000);
	
	WdfSpinLockAcquire(port->board_rx_spinlock);
	fscc_io_reset_rx(port);
	WdfSpinLockRelease(port->board_rx_spinlock);

	if(fscc_port_uses_dma(port)) {
		fscc_port_set_register(port, 2, DMA_RX_BASE_OFFSET, port->rx_descriptors[0]->desc_physical_address);
		fscc_dma_execute_GO_R(port);
	}
	
	WdfIoQueuePurgeSynchronously(port->read_queue);
	WdfIoQueuePurgeSynchronously(port->read_queue2);
	
	WdfIoQueueStart(port->read_queue);
	WdfIoQueueStart(port->read_queue2);
	
	// disable RECD
	fscc_port_set_register(port, 0, CCR0_OFFSET, orig_CCR0);
	
	return STATUS_SUCCESS;
}

NTSTATUS fscc_io_purge_tx(struct fscc_port *port)
{
	NTSTATUS status;
	size_t i;

	return_val_if_untrue(port, 0);

	TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DEVICE,
	"Purging transmit data");
	
	WdfSpinLockAcquire(port->board_tx_spinlock);
	fscc_io_reset_tx(port);
	WdfSpinLockRelease(port->board_tx_spinlock);
	
	if(fscc_port_uses_dma(port))
		fscc_port_set_register(port, 2, DMA_TX_BASE_OFFSET, port->tx_descriptors[0]->desc_physical_address);
	
	WdfIoQueuePurgeSynchronously(port->write_queue);
	WdfIoQueuePurgeSynchronously(port->write_queue2);

	WdfIoQueueStart(port->write_queue);
	WdfIoQueueStart(port->write_queue2);

	return STATUS_SUCCESS;
}

BOOLEAN fscc_dma_is_rx_running(struct fscc_port *port)
{
	UINT32 dstar_value = 0;

	return_val_if_untrue(port, 0);
	dstar_value = fscc_port_get_register(port, 2, DSTAR_OFFSET);
	
	if(port->channel==0) 
		return (dstar_value&0x1) ? 0 : 1;
	else 
		return (dstar_value&0x4) ? 0 : 1;
}

BOOLEAN fscc_dma_is_tx_running(struct fscc_port *port)
{
	UINT32 dstar_value = 0;

	return_val_if_untrue(port, 0);
	dstar_value = fscc_port_get_register(port, 2, DSTAR_OFFSET);
	
	if(port->channel==0) 
		return (dstar_value&0x2) ? 0 : 1;
	else 
		return (dstar_value&0x8) ? 0 : 1;
}

BOOLEAN fscc_dma_is_master_dead(struct fscc_port *port)
{
	UINT32 dstar_value = 0;
	
	return_val_if_untrue(port, 0);
	dstar_value = fscc_port_get_register(port, 2, DSTAR_OFFSET);
	
	return (dstar_value&0x10) ? 1 : 0;
}

// To reset the master, we need to write to the first port, so we skip port_set_register
void fscc_dma_reset_master(struct fscc_port *port)
{
	fscc_card_set_register(&port->card, 2, DMACCR_OFFSET, 0x10000);
}

NTSTATUS fscc_dma_execute_STOP_R(struct fscc_port *port)
{
	if(!fscc_port_uses_dma(port)) return STATUS_SUCCESS;
	return fscc_port_set_register(port, 2, DMACCR_OFFSET, 0x100);
}

NTSTATUS fscc_dma_execute_STOP_T(struct fscc_port *port)
{
	if(!fscc_port_uses_dma(port)) return STATUS_SUCCESS;
	return fscc_port_set_register(port, 2, DMACCR_OFFSET, 0x200);
}

NTSTATUS fscc_dma_execute_GO_R(struct fscc_port *port)
{
	if(!fscc_port_uses_dma(port)) return STATUS_SUCCESS;
	return fscc_port_set_register(port, 2, DMACCR_OFFSET, 0x1);
}

NTSTATUS fscc_dma_execute_GO_T(struct fscc_port *port)
{
	if(!fscc_port_uses_dma(port)) return STATUS_SUCCESS;
	return fscc_port_set_register(port, 2, DMACCR_OFFSET, 0x2);
}

NTSTATUS fscc_dma_execute_RST_R(struct fscc_port *port)
{
	if(!fscc_port_uses_dma(port)) return STATUS_SUCCESS;
	return fscc_port_set_register(port, 2, DMACCR_OFFSET, 0x10);
}

NTSTATUS fscc_dma_execute_RST_T(struct fscc_port *port)
{
	if(!fscc_port_uses_dma(port)) return STATUS_SUCCESS;
	return fscc_port_set_register(port, 2, DMACCR_OFFSET, 0x20);
}

unsigned fscc_io_get_RFCNT(struct fscc_port *port)
{
	UINT32 fifo_fc_value = 0;

	return_val_if_untrue(port, 0);

	fifo_fc_value = fscc_port_get_register(port, 0, FIFO_FC_OFFSET);

	return (unsigned)(fifo_fc_value & 0x000003ff);
}

unsigned fscc_io_get_TFCNT(struct fscc_port *port)
{
	UINT32 fifo_fc_value = 0;

	return_val_if_untrue(port, 0);

	fifo_fc_value = fscc_port_get_register(port, 0, FIFO_FC_OFFSET);

	return (unsigned)((fifo_fc_value & 0x01ff0000) >> 16);
}

unsigned fscc_io_get_RXCNT(struct fscc_port *port)
{
	UINT32 fifo_bc_value = 0;

	return_val_if_untrue(port, 0);

	fifo_bc_value = fscc_port_get_register(port, 0, FIFO_BC_OFFSET);

	/* Not sure why, but this can be larger than 8192. We add
	the 8192 check here so other code can count on the value
	not being larger than 8192. */
	fifo_bc_value = fifo_bc_value & 0x00003FFF;
	return (fifo_bc_value > 8192) ? 0 : fifo_bc_value;
}

unsigned fscc_io_get_TXCNT(struct fscc_port *port)
{
	UINT32 fifo_bc_value = 0;

	return_val_if_untrue(port, 0);

	fifo_bc_value = fscc_port_get_register(port, 0, FIFO_BC_OFFSET);

	return (fifo_bc_value & 0x1FFF0000) >> 16;
}

NTSTATUS fscc_io_execute_TRES(struct fscc_port *port)
{
	return fscc_port_set_register(port, 0, CMDR_OFFSET, 0x08000000);
}

NTSTATUS fscc_io_execute_RRES(struct fscc_port *port)
{
	return fscc_port_set_register(port, 0, CMDR_OFFSET, 0x00020000);
}

NTSTATUS fscc_dma_port_enable(struct fscc_port *port)
{
	return fscc_port_set_register(port, 2, DMACCR_OFFSET, 0x03000000);
}

NTSTATUS fscc_dma_port_disable(struct fscc_port *port)
{
	return fscc_port_set_register(port, 2, DMACCR_OFFSET, 0x00000000);
}

unsigned fscc_user_next_read_size(struct fscc_port *port, size_t *bytes)
{
	size_t i, cur_desc;
	UINT32 control = 0;
	
	*bytes = 0;
	cur_desc = port->user_rx_desc;
	for(i = 0; i < port->memory.rx_num; i++) {
		control = port->rx_descriptors[cur_desc]->desc->control;
		
		// If neither FE or CSTOP, desc is unfinished.
		if(!(control&DESC_FE_BIT) && !(control&DESC_CSTOP_BIT))
			break;
		
		if((control&DESC_FE_BIT) && (control&DESC_CSTOP_BIT)) {
			// CSTOP and FE means this is the final desc in a finished frame. Return
			// CNT to get the total size of the frame.
			*bytes = (control & DMA_MAX_LENGTH);
			return 1;
		}
		
		*bytes += port->rx_descriptors[cur_desc]->desc->data_count;

		cur_desc++;
		if(cur_desc == port->memory.rx_num) 
			cur_desc = 0;
	}
	
	return 0;
}

void fscc_dma_apply_timestamps(struct fscc_port *port)
{
	size_t i, cur_desc;
	UINT32 control = 0;

	WdfSpinLockAcquire(port->board_rx_spinlock);	
	cur_desc = port->user_rx_desc;
	for(i=0;i<port->memory.rx_num;i++) {
		control = port->rx_descriptors[cur_desc]->desc->control;

		if(!(control&DESC_FE_BIT) && !(control&DESC_CSTOP_BIT))
			break;
		
		if(timestamp_is_empty(&port->rx_descriptors[cur_desc]->timestamp))
			set_timestamp(&port->rx_descriptors[cur_desc]->timestamp);
		
		cur_desc++;
		if(cur_desc == port->memory.rx_num) 
			cur_desc = 0;
	}
	WdfSpinLockRelease(port->board_rx_spinlock);
}

size_t fscc_user_get_tx_space(struct fscc_port *port)
{
	size_t i, cur_desc;
	size_t space = 0;
	
	cur_desc = port->user_tx_desc;
	for(i = 0; i < port->memory.tx_num; i++) {
		if((port->tx_descriptors[cur_desc]->desc->control&DESC_CSTOP_BIT)!=DESC_CSTOP_BIT) 
			break;
		
		space += port->tx_descriptors[cur_desc]->data_size;

		cur_desc++;
		if(cur_desc == port->memory.tx_num) 
			cur_desc = 0;
	}
	
	return space;
}

#define TX_FIFO_SIZE 4096
int fscc_fifo_write_data(struct fscc_port *port)
{
	unsigned fifo_space, write_length, frame_size = 0, size_in_fifo;
	size_t data_written = 0;
	size_t i;
	UINT32 txcnt = 0, tfcnt = 0;

	tfcnt = fscc_io_get_TFCNT(port);
	if(tfcnt > 254)
		return 0;
	
	txcnt = fscc_io_get_TXCNT(port);
	fifo_space = TX_FIFO_SIZE - txcnt - 1;
	fifo_space -= fifo_space % 4;
	if(fifo_space > TX_FIFO_SIZE)
		return 0;
	
	WdfSpinLockAcquire(port->board_tx_spinlock);
	for(i = 0; i < port->memory.tx_num; i++) {
		if((port->tx_descriptors[port->fifo_tx_desc]->desc->control&DESC_CSTOP_BIT)==DESC_CSTOP_BIT)
			break;
		
		write_length = port->tx_descriptors[port->fifo_tx_desc]->desc->data_count;
		size_in_fifo = write_length + (4 - write_length % 4);
		if(fifo_space < size_in_fifo)
			break;
		
		if((port->tx_descriptors[port->fifo_tx_desc]->desc->control&DESC_FE_BIT)==DESC_FE_BIT) {
			if(tfcnt > 255)
				break;
			if(frame_size)
				break;
			frame_size = port->tx_descriptors[port->fifo_tx_desc]->desc->control&DMA_MAX_LENGTH;
		}
		
		fscc_port_set_register_rep(port, 0, FIFO_OFFSET, port->tx_descriptors[port->fifo_tx_desc]->buffer, write_length);
		data_written = 1;
		fifo_space -= size_in_fifo;
		
		// Descriptor is empty, time to reset it.
		port->tx_descriptors[port->fifo_tx_desc]->desc->data_count = 0;
		port->tx_descriptors[port->fifo_tx_desc]->desc->control = DESC_CSTOP_BIT;

		port->fifo_tx_desc++;
		if(port->fifo_tx_desc == port->memory.tx_num)
			port->fifo_tx_desc = 0;
	}
	if(frame_size) 
		fscc_port_set_register(port, 0, BC_FIFO_L_OFFSET, frame_size);

	WdfSpinLockRelease(port->board_tx_spinlock);
	return data_written;
}

int fscc_fifo_read_data(struct fscc_port *port)
{
	size_t i;
	unsigned rxcnt, receive_length = 0, pending_frame_size = 0;
	UINT32 new_control = 0;
	
	WdfSpinLockAcquire(port->board_rx_spinlock);
	for(i = 0; i < port->memory.rx_num; i++) {
		
		new_control = port->rx_descriptors[port->fifo_rx_desc]->desc->control;
		if((new_control&DESC_CSTOP_BIT)==DESC_CSTOP_BIT)
			break;
		
		if (port->rx_frame_size == 0) {
			if (fscc_io_get_RFCNT(port)) 
				port->rx_frame_size = fscc_port_get_register(port, 0, BC_FIFO_L_OFFSET);
		}
		
		if(port->rx_frame_size) {
			if (port->rx_frame_size > port->rx_bytes_in_frame)
				receive_length = port->rx_frame_size - port->rx_bytes_in_frame;
			else
				receive_length = 0;
		}
		else {
			rxcnt = fscc_io_get_RXCNT(port);
			receive_length = rxcnt - (rxcnt % 4);
		}
		
		receive_length = min(receive_length, port->rx_descriptors[port->fifo_rx_desc]->data_size - (new_control&DMA_MAX_LENGTH));
		// Instead of breaking out if this is 0, we move on to allow the FE/CSTOP processing.
		
		if(receive_length)
			fscc_port_get_register_rep(port, 0, FIFO_OFFSET, port->rx_descriptors[port->fifo_rx_desc]->buffer+(new_control&DMA_MAX_LENGTH), receive_length);
		new_control += receive_length;
		port->rx_bytes_in_frame += receive_length;

		// We've finished this descriptor, so we finalize it.
		if((new_control&DMA_MAX_LENGTH) >= (unsigned)port->rx_descriptors[port->fifo_rx_desc]->data_size) {
			new_control &= ~DMA_MAX_LENGTH;
			new_control |= DESC_CSTOP_BIT;
		}
		
		if(port->rx_frame_size > 0) {
			// We've finished a frame, so we finalize it and clean up a bit.
			if(port->rx_bytes_in_frame >= port->rx_frame_size) {
				new_control = port->rx_frame_size;
				new_control |= DESC_CSTOP_BIT;
				new_control |= DESC_FE_BIT;
				port->rx_frame_size = 0;
				port->rx_bytes_in_frame = 0;
			}
		}
		
		// Finalize the descriptor if it's finished.
		if(new_control&DESC_CSTOP_BIT) {
			if(timestamp_is_empty(&port->rx_descriptors[port->fifo_rx_desc]->timestamp))
				set_timestamp(&port->rx_descriptors[port->fifo_rx_desc]->timestamp);
			port->rx_descriptors[port->fifo_rx_desc]->desc->data_count = port->rx_descriptors[port->fifo_rx_desc]->data_size;
		}
		
		port->rx_descriptors[port->fifo_rx_desc]->desc->control = new_control;
		
		// Desc isn't finished, which means we're out of data.
		if((new_control&DESC_CSTOP_BIT)!=DESC_CSTOP_BIT)
			break;
		
		port->fifo_rx_desc++;
		if(port->fifo_rx_desc == port->memory.rx_num) 
			port->fifo_rx_desc = 0;
	}
	WdfSpinLockRelease(port->board_rx_spinlock);
	return STATUS_SUCCESS;
}

int fscc_user_write_frame(struct fscc_port *port, char *buf, size_t data_length, size_t *out_length)
{
	size_t i;
	int status = STATUS_SUCCESS;
	UINT32 new_control = 0;
	size_t transmit_length;
	UINT32 start_desc = 0;
	
	*out_length = 0;
	WdfSpinLockAcquire(port->board_tx_spinlock);
	for(i = 0; i < port->memory.tx_num; i++) {
		if((port->tx_descriptors[port->user_tx_desc]->desc->control&DESC_CSTOP_BIT)!=DESC_CSTOP_BIT) {
			status = STATUS_BUFFER_TOO_SMALL;
			break;
		}
		if(start_desc==0)
			start_desc = port->tx_descriptors[port->user_tx_desc]->desc_physical_address;
		transmit_length = min(data_length - *out_length,  port->tx_descriptors[port->user_tx_desc]->data_size);
		
		RtlCopyMemory(port->tx_descriptors[port->user_tx_desc]->buffer, buf + *out_length, transmit_length);
		*out_length += transmit_length;
		port->tx_descriptors[port->user_tx_desc]->desc->data_count = transmit_length;
		new_control = DESC_HI_BIT;
		if(i == 0) {
			new_control |= DESC_FE_BIT;
			new_control |= data_length;
		}
		else {
			new_control |= transmit_length;
		}
		port->tx_descriptors[port->user_tx_desc]->desc->control = new_control;
		
		port->user_tx_desc++;
		if(port->user_tx_desc == port->memory.tx_num) 
			port->user_tx_desc = 0;
		if(*out_length == data_length)
			break;
	}
	WdfSpinLockRelease(port->board_tx_spinlock);
	
	// There is no additional prep for DMA.. so lets just start it.
	if(fscc_port_uses_dma(port) && !fscc_dma_is_tx_running(port)) {
		fscc_port_set_register(port, 2, DMA_TX_BASE_OFFSET, start_desc);
		fscc_io_execute_transmit(port, 1);
	}
	return status;
}

int fscc_user_read_frame(struct fscc_port *port, char *buf, size_t buf_length, size_t *out_length)
{
	size_t i;
	int frame_ready = 0;
	size_t planned_move_size = 0;
	size_t real_move_size = 0;
	size_t bytes_in_descs = 0;
	size_t total_valid_data = 0;
	size_t filled_frame_size = 0;
	size_t buffer_requirement = 0;
	UINT32 control = 0;
	unsigned char *tempbuf;
	
	return_val_if_untrue(port, STATUS_UNSUCCESSFUL);
	
	*out_length = 0;
	WdfSpinLockAcquire(port->board_rx_spinlock);
	
	frame_ready = fscc_user_next_read_size(port, &bytes_in_descs);
	if(!frame_ready) {
		WdfSpinLockRelease(port->board_rx_spinlock);
		return STATUS_BUFFER_TOO_SMALL;
	}
	
	total_valid_data = bytes_in_descs;
	total_valid_data -= (port->append_status) ? 0 : 2;
	
	buffer_requirement = bytes_in_descs;
	buffer_requirement -= (port->append_status) ? 0 : 2;
	buffer_requirement += (port->append_timestamp) ? sizeof(fscc_timestamp) : 0;
	if(buffer_requirement > buf_length) {
		WdfSpinLockRelease(port->board_rx_spinlock);
		return STATUS_BUFFER_TOO_SMALL;
	}
	for(i = 0; i < port->memory.rx_num; i++) {		
		control = port->rx_descriptors[port->user_rx_desc]->desc->control;
		
		planned_move_size = port->rx_descriptors[port->user_rx_desc]->desc->data_count;
		if((control&DESC_FE_BIT) && (control&DESC_CSTOP_BIT)) {
			// CSTOP and FE means this is the final desc in a finished frame.
			// CNT to get the total size of the frame, subtract already gathered bytes.
			planned_move_size = (control&DMA_MAX_LENGTH) - filled_frame_size;
		}
		
		if(planned_move_size > total_valid_data) 
			real_move_size = total_valid_data;
		else 
			real_move_size = planned_move_size;
		
		if(real_move_size)
			RtlCopyMemory(buf + *out_length, port->rx_descriptors[port->user_rx_desc]->buffer, real_move_size);
		
		if(planned_move_size > bytes_in_descs) 
			bytes_in_descs = 0;
		else 
			bytes_in_descs -= planned_move_size;
		total_valid_data -= real_move_size;
		filled_frame_size += real_move_size;
		*out_length += real_move_size;
		
		if(bytes_in_descs == 0 && port->append_timestamp) {
			if(timestamp_is_empty(&port->rx_descriptors[port->user_rx_desc]->timestamp))
				set_timestamp(&port->rx_descriptors[port->user_rx_desc]->timestamp);
			RtlCopyMemory(buf + *out_length, &port->rx_descriptors[port->user_rx_desc]->timestamp, sizeof(fscc_timestamp));
			*out_length += sizeof(fscc_timestamp);
		}
		clear_timestamp(&port->rx_descriptors[port->user_rx_desc]->timestamp);
		port->rx_descriptors[port->user_rx_desc]->desc->control = DESC_HI_BIT;
		
		port->user_rx_desc++;
		if(port->user_rx_desc == port->memory.rx_num) 
			port->user_rx_desc = 0;
		
		if(bytes_in_descs == 0) {
			if(!port->rx_multiple)
				break;
			frame_ready = fscc_user_next_read_size(port, &bytes_in_descs);
			if(!frame_ready) 
				break;
			
			total_valid_data = bytes_in_descs;
			total_valid_data -= (port->append_status) ? 0 : 2;

			buffer_requirement = bytes_in_descs;
			buffer_requirement -= (port->append_status) ? 0 : 2;
			buffer_requirement += (port->append_timestamp) ? sizeof(fscc_timestamp) : 0;

			if(buffer_requirement > buf_length - *out_length) 
				break;
			filled_frame_size = 0;
			
		}
	}
	WdfSpinLockRelease(port->board_rx_spinlock);
	return STATUS_SUCCESS;
}

int fscc_user_read_stream(struct fscc_port *port, char *buf, size_t buf_length, size_t *out_length)
{
	size_t i;
	int receive_length = 0;
	int status;
	UINT32 control;
	
	return_val_if_untrue(port, STATUS_UNSUCCESSFUL);
	
	*out_length = 0;
	WdfSpinLockAcquire(port->board_rx_spinlock);
	for(i = 0; i < port->memory.rx_num; i++) {	
		control = port->rx_descriptors[port->user_rx_desc]->desc->control;
		
		// If not CSTOP && not FE, then break
		if(!(control&DESC_FE_BIT) && !(control&DESC_CSTOP_BIT))
			break;
		
		receive_length = min(port->rx_descriptors[port->user_rx_desc]->desc->data_count, buf_length - *out_length);
		
		RtlCopyMemory(buf + *out_length, port->rx_descriptors[port->user_rx_desc]->buffer, receive_length);
		*out_length += receive_length;
		
		if(receive_length == port->rx_descriptors[port->user_rx_desc]->desc->data_count) {
			port->rx_descriptors[port->user_rx_desc]->desc->data_count = port->rx_descriptors[port->user_rx_desc]->data_size;
			clear_timestamp(&port->rx_descriptors[port->user_rx_desc]->timestamp);
			if(i%2) port->rx_descriptors[port->user_rx_desc]->desc->control = DESC_HI_BIT;
			else port->rx_descriptors[port->user_rx_desc]->desc->control = 0;
		}
		else {
			int remaining = port->rx_descriptors[port->user_rx_desc]->desc->data_count - receive_length;
			// Moving data to the front of the descriptor.
			RtlMoveMemory(port->rx_descriptors[port->user_rx_desc]->buffer, 
			port->rx_descriptors[port->user_rx_desc]->buffer+receive_length, 
			remaining);
			port->rx_descriptors[port->user_rx_desc]->desc->data_count = remaining;
			break;
		}
		
		port->user_rx_desc++;
		if(port->user_rx_desc == port->memory.rx_num) 
			port->user_rx_desc = 0;
	}
	WdfSpinLockRelease(port->board_rx_spinlock);
	
	return STATUS_SUCCESS;
}

unsigned fscc_io_transmit_frame(struct fscc_port *port)
{
	int result;

	result = fscc_fifo_write_data(port);
	if(result) fscc_io_execute_transmit(port, 0);

	return result;
}

unsigned fscc_io_has_incoming_data(struct fscc_port *port)
{
	int frame_waiting = 0;
	size_t bytes_waiting = 0;
	
	return_val_if_untrue(port, 0);

	frame_waiting = fscc_user_next_read_size(port, &bytes_waiting);
	if(fscc_io_is_streaming(port)) return bytes_waiting;
	else return frame_waiting;
}

void fscc_io_execute_transmit(struct fscc_port *port, unsigned dma)
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

		fscc_port_set_register(port, command_bar, command_register, command_value);
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
		
		fscc_port_set_register(port, command_bar, command_register, command_value);
	}
}

BOOLEAN fscc_port_uses_dma(struct fscc_port *port)
{
	return_val_if_untrue(port, 0);

	if (port->force_fifo) return FALSE;

	return port->has_dma;
}

unsigned fscc_io_is_streaming(struct fscc_port *port)
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

VOID FsccEvtIoWrite(IN WDFQUEUE Queue, IN WDFREQUEST Request, IN size_t Length)
{
	NTSTATUS status;
	char *data_buffer = NULL;
	struct fscc_port *port = 0;
	size_t write_count = 0;
	int uses_dma;

	port = WdfObjectGet_FSCC_PORT(WdfIoQueueGetDevice(Queue));
	
	if (Length == 0) {
		WdfRequestCompleteWithInformation(Request, STATUS_SUCCESS, Length);
		return;
	}
	
	if(Length > (port->memory.tx_size * port->memory.tx_num)) {
		WdfRequestComplete(Request, STATUS_BUFFER_TOO_SMALL);
		return;
	}
	
	if (port->blocking_write) {
		status = WdfRequestForwardToIoQueue(Request, port->blocking_request_queue);
		if (!NT_SUCCESS(status)) {
			TraceEvents(TRACE_LEVEL_ERROR, TRACE_DEVICE, 
			"WdfRequestForwardToIoQueue failed %!STATUS!", status);
			WdfRequestComplete(Request, status);
			return;
		}
		WdfDpcEnqueue(port->request_dpc);
		return;
	}
	
	if(fscc_user_get_tx_space(port) < Length) {
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
	
	status = WdfRequestRetrieveInputBuffer(Request, Length, (PVOID *)&data_buffer, NULL);
	if (!NT_SUCCESS(status)) {
		TraceEvents(TRACE_LEVEL_ERROR, TRACE_DEVICE,
		"WdfRequestRetrieveInputBuffer failed %!STATUS!", status);
		WdfRequestComplete(Request, status);
		return;
	}
	
	status = fscc_user_write_frame(port, data_buffer, Length, &write_count);
	
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
		WdfRequestCompleteWithInformation(Request, status, write_count);
	}

	if(!fscc_port_uses_dma(port))
		WdfDpcEnqueue(port->oframe_dpc);
}


