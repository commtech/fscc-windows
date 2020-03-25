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

#include <ntddser.h>
#include <ntstrsafe.h>

#if defined(EVENT_TRACING)
#include "dma.tmh"
#endif

void fscc_dma_destroy_data(struct fscc_port *port, struct dma_frame *frame);
void fscc_dma_destroy_desc(struct fscc_port *port, struct dma_frame *frame);
void fscc_dma_destroy_rx(struct fscc_port *port);
void fscc_dma_destroy_tx(struct fscc_port *port);
NTSTATUS fscc_dma_create_rx_desc(struct fscc_port *port);
NTSTATUS fscc_dma_create_tx_desc(struct fscc_port *port);
NTSTATUS fscc_dma_create_rx_buffers(struct fscc_port *port);
NTSTATUS fscc_dma_create_tx_buffers(struct fscc_port *port);
NTSTATUS fscc_dma_rx_find_next_frame(struct fscc_port *port, unsigned *bytes, unsigned *start_desc, unsigned *end_desc);
unsigned fscc_dma_tx_required_desc(struct fscc_port *port, unsigned size);
BOOLEAN fscc_dma_is_rx_running(struct fscc_port *port);
BOOLEAN fscc_dma_is_tx_running(struct fscc_port *port);

NTSTATUS fscc_dma_initialize(struct fscc_port *port)
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

NTSTATUS fscc_dma_rebuild_rx(struct fscc_port *port)
{
    if(!port) return STATUS_UNSUCCESSFUL;
    
    fscc_dma_destroy_rx(port);
    if(!fscc_port_uses_dma(port)) return STATUS_UNSUCCESSFUL;
    
    return fscc_dma_build_rx(port);
}

NTSTATUS fscc_dma_rebuild_tx(struct fscc_port *port)
{
    if(!port) return STATUS_UNSUCCESSFUL;
    
    fscc_dma_destroy_tx(port);
    if(!fscc_port_uses_dma(port)) return STATUS_UNSUCCESSFUL;
    
    return fscc_dma_build_tx(port);
}

NTSTATUS fscc_dma_build_rx(struct fscc_port *port)
{
    NTSTATUS status;
    // Recalculate the RX DMA stuff.
    port->num_rx_desc = port->memory_cap.input / port->common_frame_size;
    
    // Rebuild the RX DMA stuff.
    status = fscc_dma_create_rx_desc(port);
    if(status != STATUS_SUCCESS)
    {
        port->has_dma = 0;
        return status;
    }
    status = fscc_dma_create_rx_buffers(port);
    if(status != STATUS_SUCCESS)
    {
        fscc_dma_destroy_rx(port);
        port->has_dma = 0;
        return status;
    }
    
    // Reset the RX DMA stuff.
    port->current_rx_desc = 0;
    status = fscc_port_set_register(port, 2, DMA_RX_BASE_OFFSET, port->rx_descriptors[0]->desc_physical_address);
    if(status != STATUS_SUCCESS)
    {
        port->has_dma = 0;
        return status;
    }
    
    return STATUS_SUCCESS;
}

NTSTATUS fscc_dma_build_tx(struct fscc_port *port)
{
    NTSTATUS status;
    
    port->num_tx_desc = port->memory_cap.output / port->common_frame_size;
    
    // Rebuild the TX DMA stuff.
    status = fscc_dma_create_tx_desc(port);
    if(status != STATUS_SUCCESS)
    {
        port->has_dma = 0;
        return status;
    }
    status = fscc_dma_create_tx_buffers(port);
    if(status != STATUS_SUCCESS)
    {
        fscc_dma_destroy_tx(port);
        port->has_dma = 0;
        return status;
    }
    
    // Reset the TX DMA stuff.
    port->current_tx_desc = 0;
    status = fscc_port_set_register(port, 2, DMA_TX_BASE_OFFSET, port->tx_descriptors[0]->desc_physical_address);
    if(status != STATUS_SUCCESS)
    {
        port->has_dma = 0;
        return status;
    }
    
    return STATUS_SUCCESS;
}

void fscc_dma_reset_rx(struct fscc_port *port)
{
    unsigned i;
    
    fscc_dma_execute_RSTR(port);
    for(i=0;i<port->num_rx_desc;i++)
    {
        port->rx_descriptors[i]->desc->control = 0x20000000;
        port->rx_descriptors[i]->desc->data_count = port->common_frame_size;
    }
    port->current_rx_desc = 0;
    fscc_port_set_register(port, 2, DMA_RX_BASE_OFFSET, port->rx_descriptors[0]->desc_physical_address);
    fscc_dma_execute_GO_R(port);
}

void fscc_dma_reset_tx(struct fscc_port *port)
{
    unsigned i;
    
    fscc_port_execute_TRES(port);
    for(i=0;i<port->num_tx_desc;i++)
    {
        port->tx_descriptors[i]->desc->control = 0x40000000;
        port->tx_descriptors[i]->desc->data_count = 0;
    }
    port->current_tx_desc = 0;
    fscc_port_set_register(port, 2, DMA_TX_BASE_OFFSET, port->tx_descriptors[0]->desc_physical_address);
}

void fscc_dma_destroy_rx(struct fscc_port *port)
{
    unsigned i;
    
    fscc_dma_execute_RSTR(port);
    fscc_port_set_register(port, 2, DMA_RX_BASE_OFFSET, 0);
    if(!port->rx_descriptors) return;
    
    for(i=0;i<port->num_rx_desc;i++) 
    {
        fscc_dma_destroy_desc(port, port->rx_descriptors[i]);
        if(port->rx_descriptors[i]) ExFreePoolWithTag(port->rx_descriptors[i], 'CSED');
    }
    
    ExFreePoolWithTag(port->rx_descriptors, 'CSED');
    port->rx_descriptors = 0;
}

void fscc_dma_destroy_tx(struct fscc_port *port)
{
    unsigned i;
    
    fscc_dma_execute_RSTT(port);
    fscc_port_set_register(port, 2, DMA_TX_BASE_OFFSET, 0);
    if(!port->tx_descriptors) return;
    
    for(i=0;i<port->num_tx_desc;i++)
    {
        fscc_dma_destroy_desc(port, port->tx_descriptors[i]);
        if(port->tx_descriptors[i]) ExFreePoolWithTag(port->tx_descriptors[i], 'CSED');
    }
    
    ExFreePoolWithTag(port->tx_descriptors, 'CSED');
    port->tx_descriptors = 0;
}

NTSTATUS fscc_dma_add_write_data(struct fscc_port *port, char *data_buffer, unsigned length, size_t *out_length)
{
    unsigned required_descriptors, data_to_move,i, current_desc;

    *out_length = 0;
    required_descriptors = fscc_dma_tx_required_desc(port, length);
    if(required_descriptors < 1) return STATUS_BUFFER_TOO_SMALL;
    
    
    current_desc = port->current_tx_desc;
    for(i=0;i<required_descriptors;i++)
    {
        data_to_move = length > port->common_frame_size ? port->common_frame_size : length;
        RtlCopyMemory(port->tx_descriptors[current_desc]->buffer, data_buffer, data_to_move);
        port->tx_descriptors[current_desc]->desc->data_count = data_to_move;
        if(i==0) port->tx_descriptors[current_desc]->desc->control = DESC_FE_BIT | length;
        else     port->tx_descriptors[current_desc]->desc->control = length;
        length -= data_to_move;
        *out_length += data_to_move;
        current_desc++;
        if(current_desc == port->num_tx_desc) current_desc = 0; 
    }
    if(!fscc_dma_is_tx_running(port)) 
    {
        fscc_port_set_register(port, 2, DMA_TX_BASE_OFFSET, port->tx_descriptors[port->current_tx_desc]->desc_physical_address);
        fscc_port_execute_transmit(port, 1);
    }
    port->current_tx_desc = current_desc;
    
    return STATUS_SUCCESS;
}

int fscc_dma_get_stream_data(struct fscc_port *port, char *data_buffer, size_t buffer_size, size_t *out_length)
{
    unsigned bytes_in_desc, bytes_to_move, current_desc;
    
    bytes_to_move = 0;

    current_desc = port->current_rx_desc;
    for(*out_length = 0; *out_length < buffer_size; )
    {
        if((port->rx_descriptors[current_desc]->desc->control&DESC_FE_BIT)!=DESC_FE_BIT) break;
        bytes_in_desc = port->rx_descriptors[current_desc]->desc->data_count;
        // TODO accomodate for append_status
        bytes_to_move = (bytes_in_desc > buffer_size - *out_length) ? buffer_size - *out_length : bytes_in_desc;
        RtlCopyMemory(data_buffer + *out_length, port->rx_descriptors[current_desc]->buffer, bytes_to_move);
        if(bytes_to_move == bytes_in_desc) 
        {
            port->rx_descriptors[current_desc]->desc->control = 0;
            current_desc++;
            if(current_desc == port->num_rx_desc) current_desc = 0; 
        }
        else
        {
            RtlMoveMemory(port->rx_descriptors[current_desc]->buffer, port->rx_descriptors[current_desc]->buffer + bytes_to_move, bytes_in_desc - bytes_to_move);
            port->rx_descriptors[current_desc]->desc->data_count = bytes_in_desc - bytes_to_move;
        }
        *out_length += bytes_to_move;
    }
    port->current_rx_desc = current_desc;
    
    return STATUS_SUCCESS;
}

int fscc_dma_get_frame_data(struct fscc_port *port, char *data_buffer, size_t buffer_size, size_t *out_length)
{
    unsigned bytes_in_frame, bytes_to_move, start_desc, end_desc, cur_desc;
    
    if(fscc_dma_rx_find_next_frame(port, &bytes_in_frame, &start_desc, &end_desc)!=STATUS_SUCCESS) 
        return STATUS_UNSUCCESSFUL;
    if(bytes_in_frame > buffer_size) 
        return STATUS_BUFFER_TOO_SMALL;
    
    cur_desc = start_desc;
    for(*out_length = 0; *out_length < bytes_in_frame; )
    {
        if(port->rx_descriptors[cur_desc]->desc->data_count > (bytes_in_frame - *out_length))
            bytes_to_move = bytes_in_frame - *out_length;
        else
            bytes_to_move = port->rx_descriptors[cur_desc]->desc->data_count;
        
        RtlCopyMemory(data_buffer + *out_length, port->rx_descriptors[cur_desc]->buffer, bytes_to_move);
        port->rx_descriptors[cur_desc]->desc->control = 0x20000000;
        *out_length += bytes_to_move;
        
        cur_desc++;
        if(cur_desc == port->num_rx_desc) cur_desc = 0;
        // This is just to make sure there's no infinite loop.
        if(cur_desc == start_desc) break;
    }
    port->current_rx_desc = cur_desc;
    
    return STATUS_SUCCESS;
}

BOOLEAN fscc_dma_is_rx_running(struct fscc_port *port)
{
    UINT32 dstar_value = 0;
    BOOLEAN running = 1;

    return_val_if_untrue(port, 0);
    dstar_value = fscc_port_get_register(port, 2, DSTAR_OFFSET);
    
    if(port->channel==0) return (dstar_value&0x1) ? 0 : 1;
    else return (dstar_value&0x4) ? 0 : 1;
}

BOOLEAN fscc_dma_is_tx_running(struct fscc_port *port)
{
    UINT32 dstar_value = 0;
    BOOLEAN running = 1;

    return_val_if_untrue(port, 0);
    dstar_value = fscc_port_get_register(port, 2, DSTAR_OFFSET);
    
    if(port->channel==0) return (dstar_value&0x2) ? 0 : 1;
    else return (dstar_value&0x8) ? 0 : 1;
}

void fscc_dma_destroy_data(struct fscc_port *port, struct dma_frame *frame)
{
    frame->desc->data_address = 0;
    frame->buffer = 0;
    if(frame->data_buffer) WdfObjectDelete(frame->data_buffer);
    frame->data_buffer = 0;
}

void fscc_dma_destroy_desc(struct fscc_port *port, struct dma_frame *frame)
{
    fscc_dma_destroy_data(port, frame);
    RtlZeroMemory(frame->desc, sizeof(struct fscc_descriptor));
    frame->desc_physical_address = 0;
    if(frame->desc_buffer) WdfObjectDelete(frame->desc_buffer);
    frame->desc_buffer = 0;
}

NTSTATUS fscc_dma_execute_RSTR(struct fscc_port *port)
{
    NTSTATUS status;
    
    return_val_if_untrue(port, 0);
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
    
    return_val_if_untrue(port, 0);
    status  = fscc_port_set_register(port, 2, DMACCR_OFFSET, 0x200);
    status |= fscc_port_set_register(port, 2, DMACCR_OFFSET, 0x20);
    
    return status;
}

NTSTATUS fscc_dma_execute_GO_T(struct fscc_port *port)
{
    return fscc_port_set_register(port, 2, DMACCR_OFFSET, 0x2);
}

NTSTATUS fscc_dma_create_rx_desc(struct fscc_port *port)
{
    NTSTATUS status;
    unsigned i;
    PHYSICAL_ADDRESS temp_address;
    
    // Allocate space for the list of dma_frames
    port->rx_descriptors = (struct dma_frame **)ExAllocatePoolWithTag(NonPagedPool, (sizeof(struct dma_frame) * port->num_rx_desc), 'CSED');
    if(port->rx_descriptors == NULL)
    {
        port->has_dma = 0;
        return STATUS_UNSUCCESSFUL;
    }
    
    // Allocate a common buffer for each descriptor and set it up
    for(i=0;i<port->num_rx_desc;i++)
    {
        port->rx_descriptors[i] = (struct dma_frame *)ExAllocatePoolWithTag(NonPagedPool, sizeof(struct dma_frame), 'CSED');
        status = WdfCommonBufferCreate(port->dma_enabler, sizeof(struct fscc_descriptor), WDF_NO_OBJECT_ATTRIBUTES, &port->rx_descriptors[i]->desc_buffer);
        if(!NT_SUCCESS(status)) {
            port->has_dma = 0;
            TraceEvents(TRACE_LEVEL_ERROR, TRACE_DEVICE, "WdfCommonBufferCreate(%d) failed: %!STATUS!", i, status);
            return status;
        } 
        port->rx_descriptors[i]->desc = WdfCommonBufferGetAlignedVirtualAddress(port->rx_descriptors[i]->desc_buffer);
        temp_address = WdfCommonBufferGetAlignedLogicalAddress(port->rx_descriptors[i]->desc_buffer);
        port->rx_descriptors[i]->desc_physical_address = temp_address.LowPart;
        RtlZeroMemory(port->rx_descriptors[i]->desc, sizeof(struct fscc_descriptor));
        port->rx_descriptors[i]->desc->control = 0x20000000;
    }
    
    // Create the singly linked list
    for(i=0;i<port->num_rx_desc;i++)
    {
        port->rx_descriptors[i]->desc->next_descriptor = port->rx_descriptors[i < port->num_rx_desc-1 ? i + 1 : 0]->desc_physical_address;
    }
    
    return STATUS_SUCCESS;
}

NTSTATUS fscc_dma_create_tx_desc(struct fscc_port *port)
{
    NTSTATUS status;
    unsigned i;
    PHYSICAL_ADDRESS temp_address;
    
    // Allocate space for the list of dma_frames
    port->tx_descriptors = (struct dma_frame **)ExAllocatePoolWithTag(NonPagedPool, (sizeof(struct dma_frame) * port->num_tx_desc), 'CSED');
    if(port->tx_descriptors == NULL)
    {
        port->has_dma = 0;
        return STATUS_UNSUCCESSFUL;
    }
    
    // Allocate a common buffer for each descriptor and set it up
    for(i=0;i<port->num_tx_desc;i++)
    {
        port->tx_descriptors[i] = (struct dma_frame *)ExAllocatePoolWithTag(NonPagedPool, sizeof(struct dma_frame), 'CSED');
        status = WdfCommonBufferCreate(port->dma_enabler, sizeof(struct fscc_descriptor), WDF_NO_OBJECT_ATTRIBUTES, &port->tx_descriptors[i]->desc_buffer);
        if(!NT_SUCCESS(status)) {
            port->has_dma = 0;
            TraceEvents(TRACE_LEVEL_ERROR, TRACE_DEVICE, "WdfCommonBufferCreate(%d) failed: %!STATUS!", i, status);
            return status;
        }
        port->tx_descriptors[i]->desc = WdfCommonBufferGetAlignedVirtualAddress(port->tx_descriptors[i]->desc_buffer);
        temp_address = WdfCommonBufferGetAlignedLogicalAddress(port->tx_descriptors[i]->desc_buffer);
        port->tx_descriptors[i]->desc_physical_address = temp_address.LowPart;
        RtlZeroMemory(port->tx_descriptors[i]->desc, sizeof(struct fscc_descriptor));
        port->tx_descriptors[i]->desc->control = 0x40000000;
    }
    
    // Create the singly linked list
    for(i=0;i<port->num_tx_desc;i++)
    {
        port->tx_descriptors[i]->desc->next_descriptor = port->tx_descriptors[i < port->num_rx_desc-1 ? i + 1 : 0]->desc_physical_address;
    }
    
    return STATUS_SUCCESS;
}

NTSTATUS fscc_dma_create_rx_buffers(struct fscc_port *port)
{
    NTSTATUS status;
    unsigned i;
    PHYSICAL_ADDRESS temp_address;
    
    for(i=0;i<port->num_rx_desc;i++)
    {
        status = WdfCommonBufferCreate(port->dma_enabler, port->common_frame_size, WDF_NO_OBJECT_ATTRIBUTES, &port->rx_descriptors[i]->data_buffer);
        if(!NT_SUCCESS(status)) {
            port->has_dma = 0;
            TraceEvents(TRACE_LEVEL_ERROR, TRACE_DEVICE, "WdfCommonBufferCreate(%d) failed: %!STATUS!", i, status);
            return status;
        }
        port->rx_descriptors[i]->buffer = WdfCommonBufferGetAlignedVirtualAddress(port->rx_descriptors[i]->data_buffer);
        port->rx_descriptors[i]->desc->data_count = port->common_frame_size;
        temp_address = WdfCommonBufferGetAlignedLogicalAddress(port->rx_descriptors[i]->data_buffer);
        port->rx_descriptors[i]->desc->data_address = temp_address.LowPart;
    }
    return STATUS_SUCCESS;
}

NTSTATUS fscc_dma_create_tx_buffers(struct fscc_port *port)
{
    NTSTATUS status;
    unsigned i;
    PHYSICAL_ADDRESS temp_address;
    
    for(i=0;i<port->num_tx_desc;i++)
    {
        status = WdfCommonBufferCreate(port->dma_enabler, port->common_frame_size, WDF_NO_OBJECT_ATTRIBUTES, &port->tx_descriptors[i]->data_buffer);
        if(!NT_SUCCESS(status)) {
            port->has_dma = 0;
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

int fscc_dma_rx_data_waiting(struct fscc_port *port)
{
    unsigned i, streaming, cur_desc;
    streaming = fscc_port_is_streaming(port);
    
    for(i = 0; i < port->num_rx_desc; i++)
    {
        if((port->rx_descriptors[i]->desc->control&DESC_FE_BIT)==DESC_FE_BIT&&streaming) return 1;
        if((port->rx_descriptors[i]->desc->control&DESC_CSTOP_BIT)==DESC_CSTOP_BIT) return 1;
    }
    return 0;
}

NTSTATUS fscc_dma_rx_find_next_frame(struct fscc_port *port, unsigned *bytes, unsigned *start_desc, unsigned *end_desc)
{
    unsigned i, cur_desc;
    int start_found = 0;
    
    cur_desc = port->current_rx_desc;
    for(i = 0; i < port->num_rx_desc; i++)
    {
        if((port->rx_descriptors[cur_desc]->desc->control&DESC_FE_BIT)==DESC_FE_BIT) 
        {
            if(start_found==0) 
            {
                *start_desc = cur_desc;
                start_found = 1;
            }
            if((port->rx_descriptors[cur_desc]->desc->control&DESC_CSTOP_BIT)==DESC_CSTOP_BIT)
            {
                *bytes = port->rx_descriptors[cur_desc]->desc->control&DMA_MAX_LENGTH;
                if(!port->append_status) *bytes -= 2;
                *end_desc = cur_desc;
                return STATUS_SUCCESS;
            }
        }
        cur_desc++;
        if(cur_desc == port->num_rx_desc) cur_desc = 0;
    }
    return STATUS_UNSUCCESSFUL;
}

unsigned fscc_dma_tx_required_desc(struct fscc_port *port, unsigned size)
{
    unsigned leftover_bytes, required_descriptors, current_desc, i;
    
    leftover_bytes = size % port->common_frame_size;
    required_descriptors = size/port->common_frame_size;
    if(leftover_bytes) required_descriptors++;
    if(required_descriptors > port->num_tx_desc) return 0;
    
    // First we verify we have space for the data.
    current_desc = port->current_tx_desc;
    for(i=0;i<required_descriptors;i++)
    {
        // If 0x40000000 is set, we own the descriptor for TX.
        if((port->tx_descriptors[current_desc]->desc->control&DESC_CSTOP_BIT)==0) return 0;
        current_desc++;
        if(current_desc == port->num_tx_desc) current_desc = 0;
    }
    return required_descriptors;
}

NTSTATUS fscc_dma_port_enable(struct fscc_port *port)
{
    return fscc_port_set_register(port, 2, DMACCR_OFFSET, 0x03000000);
}

NTSTATUS fscc_dma_port_disable(struct fscc_port *port)
{
    return fscc_port_set_register(port, 2, DMACCR_OFFSET, 0x00000000);
}

// Everything from here down is for debugging.
void fscc_peek_rx_desc(struct fscc_port *port, unsigned num)
{
    unsigned i;
    if(num < 1 || num > port->num_rx_desc) num = port->num_rx_desc;
    DbgPrint("\nPeeking at RX descriptors: ");
    for(i=0;i<num;i++)
    {
        DbgPrint("\nDesc %8.8d, ", i);
        fscc_peek_desc(port->rx_descriptors[i]);
    }
}

void fscc_peek_tx_desc(struct fscc_port *port, unsigned num)
{
    unsigned i;
    if(num < 1 || num > port->num_tx_desc) num = port->num_tx_desc;
    DbgPrint("\nPeeking at TX descriptors: ");
    for(i=0;i<num;i++)
    {
        DbgPrint("\nDesc %8.8d,  ", i);
        fscc_peek_desc(port->tx_descriptors[i]);
    }
}

void fscc_peek_desc(struct dma_frame *frame)
{
    DbgPrint(" Data Count: %8.8d, Control: 0x%8.8X,", frame->desc->data_count, frame->desc->control);
    DbgPrint(" Cur Add: 0x%8.8X, Next Add: 0x%8.8X ", frame->desc_physical_address, frame->desc->next_descriptor);
    return;
}

void fscc_dma_current_regs(struct fscc_port *port)
{
    UINT32 cur_reg;
    cur_reg = fscc_port_get_register(port, 2, DMA_CURRENT_TX_BASE_OFFSET);
    DbgPrint("\n%s: Current TX desc: 0x%8.8x ", __FUNCTION__, cur_reg);
    cur_reg = fscc_port_get_register(port, 2, DMA_CURRENT_RX_BASE_OFFSET);
    DbgPrint("\n%s: Current RX desc: 0x%8.8x ", __FUNCTION__, cur_reg);
}
