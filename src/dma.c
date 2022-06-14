/*
Copyright 2020 Commtech, Inc.

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
#include "descriptor.h"
#include "public.h"
#include "driver.h"
#include "debug.h"
#include "utils.h"
#include "frame.h"

#include <ntddser.h>
#include <ntstrsafe.h>

#if defined(EVENT_TRACING)
#include "dma.tmh"
#endif

void fscc_io_destroy_data(struct fscc_port *port, struct dma_frame *frame);
void fscc_io_destroy_desc(struct fscc_port *port, struct dma_frame *frame);
NTSTATUS fscc_io_create_rx_desc(struct fscc_port *port, size_t number_of_desc);
NTSTATUS fscc_io_create_tx_desc(struct fscc_port *port, size_t number_of_desc);
NTSTATUS fscc_io_create_rx_buffers(struct fscc_port *port, size_t number_of_desc, size_t size_of_desc);
NTSTATUS fscc_io_create_tx_buffers(struct fscc_port *port, size_t number_of_desc, size_t size_of_desc);
BOOLEAN fscc_dma_is_rx_running(struct fscc_port *port);
BOOLEAN fscc_dma_is_tx_running(struct fscc_port *port);

NTSTATUS fscc_io_initialize(struct fscc_port *port)
{
    NTSTATUS status;
    WDF_OBJECT_ATTRIBUTES attributes;
    WDF_DMA_ENABLER_CONFIG dma_config;

    PAGED_CODE();
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

NTSTATUS fscc_io_build_rx(struct fscc_port *port, size_t number_of_desc, size_t size_of_desc)
{
    NTSTATUS status;
    
    // Rebuild the RX DMA stuff.
    status = fscc_io_create_rx_desc(port, number_of_desc);
    if(status != STATUS_SUCCESS)
		return status;
	
    status = fscc_io_create_rx_buffers(port, number_of_desc, size_of_desc);
    if(status != STATUS_SUCCESS)
        return status;
	
    port->desc_rx_num = number_of_desc;
	port->desc_rx_size = size_of_desc;
    
    fscc_io_reset_rx(port);
    return STATUS_SUCCESS;
}

NTSTATUS fscc_io_build_tx(struct fscc_port *port, size_t number_of_desc, size_t size_of_desc)
{
    NTSTATUS status;
    
    // Rebuild the TX DMA stuff.
    status = fscc_io_create_tx_desc(port, number_of_desc);
    if(status != STATUS_SUCCESS)
        return status;

    status = fscc_io_create_tx_buffers(port, number_of_desc, size_of_desc);
    if(status != STATUS_SUCCESS)
        return status;
	
    port->desc_tx_num = number_of_desc;
	port->desc_tx_size = size_of_desc;
	
	fscc_io_reset_tx(port);
    return STATUS_SUCCESS;
}

NTSTATUS fscc_io_create_rx_desc(struct fscc_port *port, size_t number_of_desc)
{
    NTSTATUS status;
    size_t i;
    PHYSICAL_ADDRESS temp_address;
    
    // Allocate space for the list of dma_frames
    port->rx_descriptors = (struct dma_frame **)ExAllocatePoolWithTag(NonPagedPool, (sizeof(struct dma_frame) * number_of_desc), 'CSED');
    if(port->rx_descriptors == NULL)
		return STATUS_UNSUCCESSFUL;
    
    // Allocate a common buffer for each descriptor and set it up
    for(i=0;i<number_of_desc;i++)
    {
        port->rx_descriptors[i] = (struct dma_frame *)ExAllocatePoolWithTag(NonPagedPool, sizeof(struct dma_frame), 'CSED');
        status = WdfCommonBufferCreate(port->dma_enabler, sizeof(struct fscc_descriptor), WDF_NO_OBJECT_ATTRIBUTES, &port->rx_descriptors[i]->desc_buffer);
        if(!NT_SUCCESS(status)) {
            TraceEvents(TRACE_LEVEL_ERROR, TRACE_DEVICE, "WdfCommonBufferCreate(%d) failed: %!STATUS!", i, status);
            return status;
        } 
        port->rx_descriptors[i]->desc = WdfCommonBufferGetAlignedVirtualAddress(port->rx_descriptors[i]->desc_buffer);
        temp_address = WdfCommonBufferGetAlignedLogicalAddress(port->rx_descriptors[i]->desc_buffer);
        port->rx_descriptors[i]->desc_physical_address = temp_address.LowPart;
        RtlZeroMemory(port->rx_descriptors[i]->desc, sizeof(struct fscc_descriptor));
        port->rx_descriptors[i]->desc->control = DESC_HI_BIT;
    }
    
    // Create the singly linked list
    for(i=0;i<number_of_desc;i++)
    {
        port->rx_descriptors[i]->desc->next_descriptor = port->rx_descriptors[i < number_of_desc-1 ? i + 1 : 0]->desc_physical_address;
    }
    
    return STATUS_SUCCESS;
}

NTSTATUS fscc_io_create_tx_desc(struct fscc_port *port, size_t number_of_desc)
{
    NTSTATUS status;
    size_t i;
    PHYSICAL_ADDRESS temp_address;
    
    // Allocate space for the list of dma_frames
    port->tx_descriptors = (struct dma_frame **)ExAllocatePoolWithTag(NonPagedPool, (sizeof(struct dma_frame) * number_of_desc), 'CSED');
    if(port->tx_descriptors == NULL)
		return STATUS_UNSUCCESSFUL;
    
    // Allocate a common buffer for each descriptor and set it up
    for(i=0;i<number_of_desc;i++)
    {
        port->tx_descriptors[i] = (struct dma_frame *)ExAllocatePoolWithTag(NonPagedPool, sizeof(struct dma_frame), 'CSED');
        status = WdfCommonBufferCreate(port->dma_enabler, sizeof(struct fscc_descriptor), WDF_NO_OBJECT_ATTRIBUTES, &port->tx_descriptors[i]->desc_buffer);
        if(!NT_SUCCESS(status)) {
            TraceEvents(TRACE_LEVEL_ERROR, TRACE_DEVICE, "WdfCommonBufferCreate(%d) failed: %!STATUS!", i, status);
            return status;
        }
        port->tx_descriptors[i]->desc = WdfCommonBufferGetAlignedVirtualAddress(port->tx_descriptors[i]->desc_buffer);
        temp_address = WdfCommonBufferGetAlignedLogicalAddress(port->tx_descriptors[i]->desc_buffer);
        port->tx_descriptors[i]->desc_physical_address = temp_address.LowPart;
        RtlZeroMemory(port->tx_descriptors[i]->desc, sizeof(struct fscc_descriptor));
        port->tx_descriptors[i]->desc->control = DESC_CSTOP_BIT;
    }
    
    // Create the singly linked list
    for(i=0;i<number_of_desc;i++)
    {
        port->tx_descriptors[i]->desc->next_descriptor = port->tx_descriptors[i < number_of_desc-1 ? i + 1 : 0]->desc_physical_address;
    }
    
    return STATUS_SUCCESS;
}

NTSTATUS fscc_io_create_rx_buffers(struct fscc_port *port, size_t number_of_desc, size_t size_of_desc)
{
    NTSTATUS status;
    size_t i;
    PHYSICAL_ADDRESS temp_address;
    
    for(i=0;i<number_of_desc;i++)
    {
        status = WdfCommonBufferCreate(port->dma_enabler, size_of_desc, WDF_NO_OBJECT_ATTRIBUTES, &port->rx_descriptors[i]->data_buffer);
        if(!NT_SUCCESS(status)) {
            TraceEvents(TRACE_LEVEL_ERROR, TRACE_DEVICE, "WdfCommonBufferCreate(%d) failed: %!STATUS!", i, status);
            return status;
        }
        port->rx_descriptors[i]->buffer = WdfCommonBufferGetAlignedVirtualAddress(port->rx_descriptors[i]->data_buffer);
        port->rx_descriptors[i]->desc->data_count = size_of_desc;
        temp_address = WdfCommonBufferGetAlignedLogicalAddress(port->rx_descriptors[i]->data_buffer);
        port->rx_descriptors[i]->desc->data_address = temp_address.LowPart;
    }
    return STATUS_SUCCESS;
}

NTSTATUS fscc_io_create_tx_buffers(struct fscc_port *port, size_t number_of_desc, size_t size_of_desc)
{
    NTSTATUS status;
    size_t i;
    PHYSICAL_ADDRESS temp_address;
    
    for(i=0;i<number_of_desc;i++)
    {
        status = WdfCommonBufferCreate(port->dma_enabler, size_of_desc, WDF_NO_OBJECT_ATTRIBUTES, &port->tx_descriptors[i]->data_buffer);
        if(!NT_SUCCESS(status)) {
            TraceEvents(TRACE_LEVEL_ERROR, TRACE_DEVICE, "WdfCommonBufferCreate(%d) failed: %!STATUS!", i, status);
            return status;
        }
        port->tx_descriptors[i]->buffer = WdfCommonBufferGetAlignedVirtualAddress(port->tx_descriptors[i]->data_buffer);
        port->tx_descriptors[i]->desc->data_count = 0;
        temp_address = WdfCommonBufferGetAlignedLogicalAddress(port->tx_descriptors[i]->data_buffer);
        port->tx_descriptors[i]->desc->data_address = temp_address.LowPart;
    }
    return STATUS_SUCCESS;
}

void fscc_io_reset_rx(struct fscc_port *port)
{
    size_t i;
    
    fscc_dma_execute_RSTR(port);
    for(i=0;i<port->desc_rx_num;i++)
    {
        port->rx_descriptors[i]->desc->control = DESC_HI_BIT;
        port->rx_descriptors[i]->desc->data_count = port->desc_rx_size;
    }
    port->user_rx_desc = 0;
	port->fifo_rx_desc = 0;
	port->rx_bytes_in_frame = 0;
	port->rx_frame_size = 0;
    fscc_port_set_register(port, 2, DMA_RX_BASE_OFFSET, port->rx_descriptors[0]->desc_physical_address);
}

void fscc_io_reset_tx(struct fscc_port *port)
{
    size_t i;
    
    fscc_dma_execute_RSTT(port);
    for(i=0;i<port->desc_tx_num;i++)
    {
        port->tx_descriptors[i]->desc->control = DESC_CSTOP_BIT;
        port->tx_descriptors[i]->desc->data_count = 0;
    }
    port->user_tx_desc = 0;
	port->fifo_tx_desc = 0;
	port->tx_bytes_in_frame = 0;
	port->tx_frame_size = 0;
    fscc_port_set_register(port, 2, DMA_TX_BASE_OFFSET, port->tx_descriptors[0]->desc_physical_address);
}

void fscc_io_destroy_rx(struct fscc_port *port)
{
    size_t i;
    
    fscc_dma_execute_RSTR(port);
    fscc_port_set_register(port, 2, DMA_RX_BASE_OFFSET, 0);
    if(!port->rx_descriptors) return;
    
    for(i=0;i<port->desc_rx_num;i++) 
    {
        fscc_io_destroy_desc(port, port->rx_descriptors[i]);
        if(port->rx_descriptors[i]) ExFreePoolWithTag(port->rx_descriptors[i], 'CSED');
    }
    
    ExFreePoolWithTag(port->rx_descriptors, 'CSED');
    port->rx_descriptors = 0;
}

void fscc_io_destroy_tx(struct fscc_port *port)
{
    size_t i;
    
    fscc_dma_execute_RSTT(port);
    fscc_port_set_register(port, 2, DMA_TX_BASE_OFFSET, 0);
    if(!port->tx_descriptors) return;
    
    for(i=0;i<port->desc_tx_num;i++)
    {
        fscc_io_destroy_desc(port, port->tx_descriptors[i]);
        if(port->tx_descriptors[i]) ExFreePoolWithTag(port->tx_descriptors[i], 'CSED');
    }
    
    ExFreePoolWithTag(port->tx_descriptors, 'CSED');
    port->tx_descriptors = 0;
}

void fscc_io_destroy_data(struct fscc_port *port, struct dma_frame *frame)
{
    frame->desc->data_address = 0;
    frame->buffer = 0;
    if(frame->data_buffer) WdfObjectDelete(frame->data_buffer);
    frame->data_buffer = 0;
}

void fscc_io_destroy_desc(struct fscc_port *port, struct dma_frame *frame)
{
    fscc_io_destroy_data(port, frame);
    RtlZeroMemory(frame->desc, sizeof(struct fscc_descriptor));
    frame->desc_physical_address = 0;
    if(frame->desc_buffer) WdfObjectDelete(frame->desc_buffer);
    frame->desc_buffer = 0;
}

NTSTATUS fscc_io_rebuild_rx(struct fscc_port *port)
{
    if(!port) return STATUS_UNSUCCESSFUL;
    
    fscc_io_destroy_rx(port);
    
    return fscc_io_build_rx(port, port->desc_rx_num, port->desc_rx_size);
}

NTSTATUS fscc_io_rebuild_tx(struct fscc_port *port)
{
    if(!port) return STATUS_UNSUCCESSFUL;
    
    fscc_io_destroy_tx(port);
    
    return fscc_io_build_tx(port, port->desc_tx_num, port->desc_tx_size);
}

BOOLEAN fscc_dma_is_rx_running(struct fscc_port *port)
{
    UINT32 dstar_value = 0;

    return_val_if_untrue(port, 0);
    dstar_value = fscc_port_get_register(port, 2, DSTAR_OFFSET);
    
    if(port->channel==0) return (dstar_value&0x1) ? 0 : 1;
    else return (dstar_value&0x4) ? 0 : 1;
}

BOOLEAN fscc_dma_is_tx_running(struct fscc_port *port)
{
    UINT32 dstar_value = 0;

    return_val_if_untrue(port, 0);
    dstar_value = fscc_port_get_register(port, 2, DSTAR_OFFSET);
    
    if(port->channel==0) return (dstar_value&0x2) ? 0 : 1;
    else return (dstar_value&0x8) ? 0 : 1;
}

NTSTATUS fscc_dma_execute_RSTR(struct fscc_port *port)
{
    NTSTATUS status;
    status  = fscc_port_set_register(port, 2, DMACCR_OFFSET, 0x100);
    status |= fscc_port_set_register(port, 2, DMACCR_OFFSET, 0x10);
    
    return status;
}

NTSTATUS fscc_dma_execute_GO_R(struct fscc_port *port)
{
    return fscc_port_set_register(port, 2, DMACCR_OFFSET, 0x1);
}

NTSTATUS fscc_dma_execute_RSTT(struct fscc_port *port)
{
    NTSTATUS status;
    status  = fscc_port_set_register(port, 2, DMACCR_OFFSET, 0x200);
    status |= fscc_port_set_register(port, 2, DMACCR_OFFSET, 0x20);
    
    return status;
}

NTSTATUS fscc_dma_execute_GO_T(struct fscc_port *port)
{
    return fscc_port_set_register(port, 2, DMACCR_OFFSET, 0x2);
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
    unsigned i, cur_desc;
    UINT32 control = 0;
	
	*bytes = 0;
	cur_desc = port->user_rx_desc;
    for(i = 0; i < port->desc_rx_num; i++) {
		control = port->rx_descriptors[cur_desc]->desc->control;
		
		// If neither FE or CSTOP, desc is unfinished.
		if(!(control&DESC_FE_BIT) && !(control&DESC_CSTOP_BIT))
			break;
		
		if((control&DESC_FE_BIT) && (control&DESC_CSTOP_BIT)) {
			// CSTOP and FE means this is the final desc in a finished frame. Return
			// CNT to get the total size of the frame.
			*bytes = (port->rx_descriptors[cur_desc]->desc->control & DMA_MAX_LENGTH);
			return 1;
		}

		*bytes += port->rx_descriptors[cur_desc]->desc->data_count;

		cur_desc++;
		if(cur_desc == port->desc_rx_num) cur_desc = 0;
    }
	
    return 0;
}

// Check if there's space for data from the user to transmit.
size_t fscc_user_get_tx_space(struct fscc_port *port)
{
    unsigned i, cur_desc;
    size_t space = 0;
	
    cur_desc = port->user_tx_desc;
    for(i = 0; i < port->desc_tx_num; i++) {
        if((port->tx_descriptors[cur_desc]->desc->control&DESC_CSTOP_BIT)!=DESC_CSTOP_BIT) 
			break;
        
        space += port->desc_tx_size;

        cur_desc++;
        if(cur_desc == port->desc_tx_num) 
			cur_desc = 0;
    }
    
    return space;
}

// TODO Verify, probably won't use the frames but still.
// Currently used in fscc_fifo_write_data()
unsigned fscc_fifo_write_has_data(struct fscc_port *port, size_t *bytes)
{
	unsigned i, cur_desc, frames = 0;
	
	*bytes=0;
	cur_desc = port->fifo_tx_desc;
	for(i = 0; i < port->desc_tx_num; i++) {
        if((port->tx_descriptors[cur_desc]->desc->control&DESC_CSTOP_BIT)==DESC_CSTOP_BIT) 
			break;
		
		if((port->tx_descriptors[cur_desc]->desc->control&DESC_FE_BIT)==DESC_FE_BIT)
			frames++;
		
		*bytes += port->tx_descriptors[cur_desc]->desc->data_count;
		
		cur_desc++;
		if(cur_desc == port->desc_tx_num) cur_desc = 0;
	}
	
	return frames;
}

// Call this on TDU, TDO, TFT, ALLS
// Seems good?
// TX desc consumer, FIFO only
#define TX_FIFO_SIZE 4096
int fscc_fifo_write_data(struct fscc_port *port)
{
	unsigned used_bytes, fifo_space, write_length, frames_ready, frame_size;
	size_t bytes_ready = 0, i;

    WdfSpinLockAcquire(port->board_tx_spinlock);
	frames_ready = fscc_fifo_write_has_data(port, &bytes_ready);
	fifo_space = TX_FIFO_SIZE - fscc_port_get_TXCNT(port) - 1;
    fifo_space -= fifo_space % 4;
	// fifo_space > TX_FIFO_SIZE is just in case the subtraction above breaks everything.
	if(bytes_ready == 0 || fifo_space < 4 || fifo_space > TX_FIFO_SIZE) {
		WdfSpinLockRelease(port->board_tx_spinlock);
		return 0;
	}
	
	DbgPrint("%s: fifo_space: %d, bytes_ready: %d\n", __FUNCTION__, fifo_space, bytes_ready);
	for(i = 0; i < port->desc_tx_num; i++) {
		if((port->tx_descriptors[port->fifo_tx_desc]->desc->control&DESC_CSTOP_BIT)==DESC_CSTOP_BIT) 
			break;
	
		// If this is the first descriptor in a frame, write the size into the TXCNT
		if((port->tx_descriptors[port->fifo_tx_desc]->desc->control&DESC_FE_BIT)==DESC_FE_BIT) {
			if(fscc_port_get_TFCNT(port) > 255) 
				break;
			frame_size = port->tx_descriptors[port->fifo_tx_desc]->desc->control&DMA_MAX_LENGTH;
			fscc_port_set_register(port, 0, BC_FIFO_L_OFFSET, frame_size);
		}
		write_length = min(port->tx_descriptors[port->fifo_tx_desc]->desc->data_count, fifo_space);
		fscc_port_set_register_rep(port, 0, FIFO_OFFSET, port->tx_descriptors[port->fifo_tx_desc]->buffer, write_length);
		fifo_space -= write_length;
		
		// The descriptor isn't empty, so that means the FIFO is full.
		if(port->tx_descriptors[port->fifo_tx_desc]->desc->data_count > write_length) {
			int remaining = port->tx_descriptors[port->fifo_tx_desc]->desc->data_count - write_length;
			memmove(port->tx_descriptors[port->fifo_tx_desc]->buffer, 
				port->tx_descriptors[port->fifo_tx_desc]->buffer+write_length, 
				remaining);
			port->tx_descriptors[port->fifo_tx_desc]->desc->data_count = remaining;
			break;
		}
		
		// Descriptor is empty, time to reset it.
		port->tx_descriptors[port->fifo_tx_desc]->desc->control = DESC_CSTOP_BIT;
		
		port->fifo_tx_desc++;
		if(port->fifo_tx_desc == port->desc_tx_num)
			port->fifo_tx_desc = 0;
		if(fifo_space == 0)
			break;
	}
	WdfSpinLockRelease(port->board_tx_spinlock);
	return 1;
}

int fscc_fifo_read_data(struct fscc_port *port)
{
	size_t i;
	unsigned rfcnt, rxcnt, receive_length = 0, pending_frame_size = 0;
	UINT32 new_control = 0;
		
	WdfSpinLockAcquire(port->board_rx_spinlock);
	rxcnt = fscc_port_get_RXCNT(port);
	rfcnt = fscc_port_get_RFCNT(port);
	rxcnt -= (rxcnt % 4); // Only safe to move 4 bytes at a time.
	DbgPrint("%s: rxcnt: %d, rfcnt: %d\n", __FUNCTION__, rxcnt, rfcnt);
	for(i = 0; i < port->desc_rx_num; i++) {
		// Out of space, done.
		new_control = port->rx_descriptors[port->fifo_rx_desc]->desc->control;
		if((new_control&DESC_CSTOP_BIT)==DESC_CSTOP_BIT)
			break;

		// There's a frame ready, better get the size.
		if (rfcnt > 0 && port->rx_frame_size == 0) {
			port->rx_frame_size = fscc_port_get_register(port, 0, BC_FIFO_L_OFFSET);
			rfcnt--;
		}
		
		// If there's a frame, I want either the remaining frame size or the descriptor size, whichever is smaller
		// If not, I want either the remaining bytes or the descriptor size, whichever is smaller
		if(port->rx_frame_size) {
			pending_frame_size = port->rx_frame_size - port->rx_bytes_in_frame;
			receive_length = min(port->desc_rx_size - (new_control&DMA_MAX_LENGTH), pending_frame_size);
		}
		else {
			receive_length = min(port->desc_rx_size - (new_control&DMA_MAX_LENGTH), rxcnt);
		}
		
		// Move data into the descriptor and update counters.
		fscc_port_get_register_rep(port, 0, FIFO_OFFSET, port->rx_descriptors[port->fifo_rx_desc]->buffer, receive_length);
		// At the end of a frame, receive_length can be larger than rxcnt.
		if(rxcnt < receive_length) rxcnt = 0;
		else rxcnt -= receive_length;
		new_control += receive_length;
		port->rx_bytes_in_frame += receive_length;

		// We've finished this descriptor, so we finalize it.
		if((new_control&DMA_MAX_LENGTH) >= port->desc_rx_size) {
			KeQuerySystemTime(&port->rx_descriptors[port->fifo_rx_desc]->timestamp);
			port->rx_descriptors[port->fifo_rx_desc]->desc->data_count = port->desc_rx_size;
			new_control = port->desc_rx_size;
			new_control |= DESC_CSTOP_BIT;
		}
		
		if(port->rx_frame_size > 0) {
			// We've finished a frame, so we finalize it and clean up a bit.
			if(port->rx_bytes_in_frame >= port->rx_frame_size) {
				KeQuerySystemTime(&port->rx_descriptors[port->fifo_rx_desc]->timestamp);
				port->rx_descriptors[port->fifo_rx_desc]->desc->data_count = port->desc_rx_size;
				// This is intentional. The last desc CNT = full frame size.
				new_control = port->rx_bytes_in_frame;
				new_control |= DESC_CSTOP_BIT;
				new_control |= DESC_FE_BIT;
				port->rx_frame_size = 0;
				port->rx_bytes_in_frame = 0;
			}
		}
		
		port->rx_descriptors[port->fifo_rx_desc]->desc->control = new_control;
		// Desc isn't finished, which means we're out of data.
		if((port->rx_descriptors[port->fifo_rx_desc]->desc->control&DESC_CSTOP_BIT)!=DESC_CSTOP_BIT)
			break;
		
		port->fifo_rx_desc++;
		if(port->fifo_rx_desc == port->desc_rx_num) 
			port->fifo_rx_desc = 0;
		
		// Desc full and out of data.
		if(rxcnt == 0) 
			break;
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
	for(i = 0; i < port->desc_tx_num; i++) {
		if((port->tx_descriptors[port->user_tx_desc]->desc->control&DESC_CSTOP_BIT)!=DESC_CSTOP_BIT) {
			status = STATUS_BUFFER_TOO_SMALL;
			break;
		}
		if(start_desc==0)
			start_desc = port->tx_descriptors[port->user_tx_desc]->desc_physical_address;
		transmit_length = min(data_length - *out_length, port->desc_tx_size);
		
		memmove(port->tx_descriptors[port->user_tx_desc]->buffer, buf + *out_length, transmit_length);
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
        if(port->user_tx_desc == port->desc_tx_num) 
			port->user_tx_desc = 0;
		if(*out_length == data_length)
			break;
	}
	WdfSpinLockRelease(port->board_tx_spinlock);
	// There is no additional prep for DMA.. so lets just start it.
	if(fscc_port_uses_dma(port) && !fscc_dma_is_tx_running(port)) {
		fscc_port_set_register(port, 2, DMA_TX_BASE_OFFSET, start_desc);
		fscc_port_execute_transmit(port, 1);
	}
	DbgPrint("%s: %d total bytes written\n", __FUNCTION__, *out_length);
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
	
    for(i = 0; i < port->desc_rx_num; i++) {		
		control = port->rx_descriptors[port->user_rx_desc]->desc->control;
		
		planned_move_size = port->rx_descriptors[port->user_rx_desc]->desc->data_count;
        if((control&DESC_FE_BIT) && (control&DESC_CSTOP_BIT)) {
			// CSTOP and FE means this is the final desc in a finished frame.
			// CNT to get the total size of the frame, subtract already gathered bytes.
			planned_move_size = (control&DMA_MAX_LENGTH) - filled_frame_size;
        }
		if(planned_move_size > total_valid_data) real_move_size = total_valid_data;
		else real_move_size = planned_move_size;
        
		if(real_move_size)
			memmove(buf + *out_length, port->rx_descriptors[port->user_rx_desc]->buffer, real_move_size);
		bytes_in_descs -= planned_move_size;
		total_valid_data -= real_move_size;
		filled_frame_size += real_move_size;
        *out_length += real_move_size;
		
		if(bytes_in_descs == 0 && port->append_timestamp) {
			memcpy(buf + *out_length, &port->rx_descriptors[port->user_rx_desc]->timestamp, sizeof(fscc_timestamp));
			*out_length += sizeof(fscc_timestamp);
		}

		port->rx_descriptors[port->user_rx_desc]->desc->control = DESC_HI_BIT;
		
        port->user_rx_desc++;
        if(port->user_rx_desc == port->desc_rx_num) 
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
	DbgPrint("%s: %d total bytes read\n", __FUNCTION__, *out_length);
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
    for(i = 0; i < port->desc_rx_num; i++) {
		DbgPrint("%s: Attempting to read %d\n", __FUNCTION__, port->rx_descriptors[port->user_rx_desc]->desc->data_address);	
		control = port->rx_descriptors[port->user_rx_desc]->desc->control;
		
		// If not CSTOP && not FE, then break
		if(!(control&DESC_FE_BIT) && !(control&DESC_CSTOP_BIT))
			break;
			
        receive_length = min(port->rx_descriptors[port->user_rx_desc]->desc->data_count, buf_length - *out_length);
		
        memmove(buf + *out_length, port->rx_descriptors[port->user_rx_desc]->buffer, receive_length);
        *out_length += receive_length;
		
		if(receive_length == port->rx_descriptors[port->user_rx_desc]->desc->data_count) {
			port->rx_descriptors[port->user_rx_desc]->desc->data_count = port->desc_rx_size;
			port->rx_descriptors[port->user_rx_desc]->desc->control = DESC_HI_BIT;
		}
        else {
			int remaining = port->rx_descriptors[port->user_rx_desc]->desc->data_count - receive_length;
			// Moving data to the front of the descriptor.
			memmove(port->rx_descriptors[port->user_rx_desc]->buffer, 
				port->rx_descriptors[port->user_rx_desc]->buffer+receive_length, 
				remaining);
			port->rx_descriptors[port->user_rx_desc]->desc->data_count = remaining;
			break;
		}
		
        port->user_rx_desc++;
        if(port->user_rx_desc == port->desc_rx_num) 
			port->user_rx_desc = 0;
    }
    WdfSpinLockRelease(port->board_rx_spinlock);
	
    DbgPrint("%s: %d total bytes read\n", __FUNCTION__, *out_length);
    return STATUS_SUCCESS;
}

