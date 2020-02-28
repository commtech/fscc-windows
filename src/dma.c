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

#include <ntddser.h>
#include <ntstrsafe.h>

#if defined(EVENT_TRACING)
#include "dma.tmh"
#endif

// These should probably not be accessed outside of this file.
void fscc_dma_destroy_data(struct fscc_port *port, struct dma_frame *frame);
void fscc_dma_destroy_desc(struct fscc_port *port, struct dma_frame *frame);
void fscc_dma_destroy_rx(struct fscc_port *port);
void fscc_dma_destroy_tx(struct fscc_port *port);
NTSTATUS fscc_dma_create_rx_desc(struct fscc_port *port);
NTSTATUS fscc_dma_create_tx_desc(struct fscc_port *port);
NTSTATUS fscc_dma_create_rx_buffers(struct fscc_port *port);
NTSTATUS fscc_dma_create_tx_buffers(struct fscc_port *port);

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
        TraceEvents(TRACE_LEVEL_ERROR, TRACE_DEVICE, "WdfDmaEnablerCreate failed: %!STATUS!", status);
        return status;
    }
    return status;
}

NTSTATUS fscc_dma_rebuild_rx(struct fscc_port *port)
{
    NTSTATUS status;
    
    return_val_if_untrue(port, STATUS_UNSUCCESSFUL);
    
    // Destroy the current RX DMA stuff.
    fscc_dma_destroy_rx(port);
    if(!fscc_port_uses_dma(port)) return STATUS_UNSUCCESSFUL;
    
    // Recalculate the RX DMA stuff.
    port->num_rx_desc = (int)(port->memcap.input / port->common_frame_size);
    
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
    
    // This is unique to RX. TX issues its GO' when it writes.
    fscc_port_execute_GO_R(port);
    return STATUS_SUCCESS;
}

NTSTATUS fscc_dma_rebuild_tx(struct fscc_port *port)
{
    NTSTATUS status;
    
    return_val_if_untrue(port, STATUS_UNSUCCESSFUL);
    
    // Destroy the current TX DMA stuff.
    fscc_dma_destroy_tx(port);
    if(!fscc_port_uses_dma(port)) return STATUS_UNSUCCESSFUL;
    
    // Recalculate the TX DMA stuff.
    port->num_tx_desc = (int)(port->memcap.output / port->common_frame_size);
    
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

void fscc_dma_destroy_data(struct fscc_port *port, struct dma_frame *frame)
{
    frame->desc->data_address = 0;
    frame->buffer = 0;
    if(frame->data_buffer) WdfObjectDelete(frame->data_buffer);
}

void fscc_dma_destroy_desc(struct fscc_port *port, struct dma_frame *frame)
{
    fscc_dma_destroy_data(port, frame);
    RtlZeroMemory(frame->desc, sizeof(struct fscc_descriptor));
    frame->desc_physical_address = 0;
    if(frame->desc_buffer) WdfObjectDelete(frame->desc_buffer);
}

NTSTATUS fscc_dma_create_rx_desc(struct fscc_port *port)
{
    NTSTATUS status;
    int i;
    PHYSICAL_ADDRESS temp_address;
    
    // Allocate space for the list of dma_frames
    port->rx_descriptors = (struct dma_frame **)ExAllocatePoolWithTag(NonPagedPool, (sizeof(struct dma_frame) * port->num_rx_desc), 'CSED');
    if(port->rx_descriptors == NULL)
    {
        port->has_dma = 0;
        return STATUS_UNSUCCESSFUL;
    }
    
    // Allocate a common buffer for each descriptor and set it up
    for(i=0;i<num_rx_desc;i++)
    {
        status = WdfCommonBufferCreate(port->dma_enabler, sizeof(struct fscc_descriptor), WDF_NO_OBJECT_ATTRIBUTES, &port->rx_descriptors[i]->desc_buffer);
        port->rx_descriptors[i]->desc = WdfCommonBufferGetAlignedVirtualAddress(port->rx_descriptors[i]->desc_buffer);
        temp_address = WdfCommonBufferGetAlignedLogicalAddress(port->rx_descriptors[i]->desc_buffer);
        port->rx_descriptors[i]->desc_physical_address = temp_address.LowPart;
        RtlZeroMemory(port->rx_descriptors[i]->desc, sizeof(struct fscc_descriptor));
        port->rx_descriptors[i]->desc->control = 0x20000000;
    }
    
    // Create the singly linked list
    for(i=0;i<num_rx_desc;i++)
    {
        port->rx_descriptors[i]->desc->next_descriptor = port->rx_descriptors[i < num_rx_desc-1 ? i + 1; 0]->desc_physical_address;
    }
    
    return STATUS_SUCCESS;
}

NTSTATUS fscc_dma_create_tx_desc(struct fscc_port *port)
{
    NTSTATUS status;
    int i;
    PHYSICAL_ADDRESS temp_address;
    
    // Allocate space for the list of dma_frames
    port->tx_descriptors = (struct dma_frame **)ExAllocatePoolWithTag(NonPagedPool, (sizeof(struct dma_frame) * port->num_tx_desc), 'CSED');
    if(port->tx_descriptors == NULL)
    {
        port->has_dma = 0;
        return STATUS_UNSUCCESSFUL;
    }
    
    // Allocate a common buffer for each descriptor and set it up
    for(i=0;i<num_tx_desc;i++)
    {
        status = WdfCommonBufferCreate(port->dma_enabler, sizeof(struct fscc_descriptor), WDF_NO_OBJECT_ATTRIBUTES, &port->tx_descriptors[i]->desc_buffer);
        port->tx_descriptors[i]->desc = WdfCommonBufferGetAlignedVirtualAddress(port->tx_descriptors[i]->desc_buffer);
        temp_address = WdfCommonBufferGetAlignedLogicalAddress(port->tx_descriptors[i]->desc_buffer);
        port->tx_descriptors[i]->desc_physical_address = temp_address.LowPart;
        RtlZeroMemory(port->tx_descriptors[i]->desc, sizeof(struct fscc_descriptor));
        port->tx_descriptors[i]->desc->control = 0x20000000;
    }
    
    // Create the singly linked list
    for(i=0;i<num_tx_desc;i++)
    {
        port->tx_descriptors[i]->desc->next_descriptor = port->tx_descriptors[i < num_rx_desc-1 ? i + 1; 0]->desc_physical_address;
    }
    
    return STATUS_SUCCESS;
}

NTSTATUS fscc_dma_create_rx_buffers(struct fscc_port *port)
{
    int i;
    for(i=0;i<port->num_rx_desc;i++)
    {
        status = WdfCommonBufferCreate(port->dma_enabler, sizeof(port->common_frame_size), WDF_NO_OBJECT_ATTRIBUTES, &port->rx_descriptors[i]->data_buffer);
        port->rx_descriptors[i]->buffer = WdfCommonBuffergetAlignedVirtualAddress(port->rx_descriptors[i]->data_buffer);
        temp_address = WdfCommonBufferGetAlignedLogicalAddress(port->rx_descriptors[i]->data_buffer);
        port->rx_descriptors[i]->desc->data_address = temp_address.LowPart;
    }
    return STATUS_SUCCESS;
}

NTSTATUS fscc_dma_create_tx_buffers(struct fscc_port *port)
{
    int i;
    for(i=0;i<port->num_tx_desc;i++)
    {
        status = WdfCommonBufferCreate(port->dma_enabler, sizeof(port->common_frame_size), WDF_NO_OBJECT_ATTRIBUTES, &port->tx_descriptors[i]->data_buffer);
        port->tx_descriptors[i]->buffer = WdfCommonBuffergetAlignedVirtualAddress(port->tx_descriptors[i]->data_buffer);
        temp_address = WdfCommonBufferGetAlignedLogicalAddress(port->tx_descriptors[i]->data_buffer);
        port->tx_descriptors[i]->desc->data_address = temp_address.LowPart;
    }
    return STATUS_SUCCESS;
}

void fscc_dma_destroy_rx(struct fscc_port *port)
{
    int i;
    
    if(port->has_dma) fscc_port_execute_RSTR(port);
    if(!port->rx_descriptors) return;
    
    for(i=0;i<port->num_rx_desc;i++) fscc_dma_destroy_desc(port, port->rx_descriptors[i]);
    
    ExFreePoolWithTag(port->rx_descriptors, 'CSED');
}

void fscc_dma_destroy_tx(struct fscc_port *port)
{
    int i;
    
    if(port->has_dma) fscc_port_execute_RSTT(port);
    if(!port->tx_descriptors) return;
    
    for(i=0;i<port->num_tx_desc;i++) fscc_dma_destroy_desc(port, port->tx_descriptors[i]); 
    
    ExFreePoolWithTag(port->tx_descriptors, 'CSED');
}


