/*
    Copyright (C) 2012 Commtech, Inc.

    This file is part of fscc-linux.

    fscc-linux is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    fscc-linux is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with fscc-linux.  If not, see <http://www.gnu.org/licenses/>.

*/

#include "isr.h"
#include "port.h" /* struct fscc_port */
#include "utils.h" /* port_exists */
#include "frame.h" /* struct fscc_frame */
#include "debug.h"

#if defined(EVENT_TRACING)
#include "isr.tmh"
#endif

#define TX_FIFO_SIZE 4096
#define MAX_LEFTOVER_BYTES 3

//TODO: Not sure if I should delay some of this to the ISR DPC
BOOLEAN fscc_isr(WDFINTERRUPT Interrupt, ULONG MessageID)
{
    struct fscc_port *port = 0;
	BOOLEAN handled = FALSE;
    unsigned isr_value = 0;
    unsigned streaming = 0;

	UNREFERENCED_PARAMETER(MessageID);
	
	port = WdfObjectGet_FSCC_PORT(WdfInterruptGetDevice(Interrupt));

	//WdfTimerStop(port->timer, FALSE);
	
	isr_value = fscc_port_get_register(port, 0, ISR_OFFSET);

	if (!isr_value)
		return handled;

	handled = TRUE;

	port->last_isr_value |= isr_value;
	streaming = fscc_port_is_streaming(port);

	if (streaming) {
		if (isr_value & (RFT | RFS))
			WdfDpcEnqueue(port->istream_dpc);
	}
	else {
		if (isr_value & (RFE | RFT | RFS))
			WdfDpcEnqueue(port->iframe_dpc);
	}

	if (isr_value & TFT && !fscc_port_has_dma(port))
		WdfDpcEnqueue(port->oframe_dpc);

	/* We have to wait until an ALLS to delete a DMA frame because if we
		delete the frame right away the DMA engine will lose the data to
		transfer. */
	if (fscc_port_has_dma(port) && isr_value & ALLS) {
		if (port->pending_oframe) {
			fscc_frame_delete(port->pending_oframe);
			port->pending_oframe = 0;
		}
		
		WdfDpcEnqueue(port->oframe_dpc);
	}

#ifdef DEBUG
	WdfDpcEnqueue(port->print_dpc);
#endif
	//fscc_port_reset_timer(port);

	return handled;
}

void iframe_worker(WDFDPC Dpc)
{
    struct fscc_port *port = 0;
    int receive_length = 0; /* Needs to be signed */
    unsigned finished_frame = 0;
	static int rejected_last_frame = 0;
	
	port = WdfObjectGet_FSCC_PORT(WdfDpcGetParentObject(Dpc));

    return_if_untrue(port);
	
	WdfSpinLockAcquire(port->board_rx_spinlock);

    finished_frame = (fscc_port_get_RFCNT(port) > 0) ? 1 : 0;

    if (finished_frame) {
        unsigned bc_fifo_l = 0;
        unsigned current_length = 0;

        bc_fifo_l = fscc_port_get_register(port, 0, BC_FIFO_L_OFFSET);

        if (port->pending_iframe)
            current_length = fscc_frame_get_length(port->pending_iframe);
        else
            current_length = 0;

        receive_length = bc_fifo_l - current_length;
    } else {
        unsigned rxcnt = 0;

        rxcnt = fscc_port_get_RXCNT(port);

        /* We choose a safe amount to read due to more data coming in after we
           get our values. The rest will be read on the next interrupt. */
        receive_length = rxcnt - STATUS_LENGTH - MAX_LEFTOVER_BYTES;
        receive_length -= receive_length % 4;
    }

    if (receive_length > 0) {
        /* Make sure we don't go over the user's memory constraint. */
        if (fscc_port_get_input_memory_usage(port, 0) + receive_length > fscc_port_get_input_memory_cap(port)) {
            if (rejected_last_frame == 0) {
                TraceEvents(TRACE_LEVEL_WARNING, TRACE_DEVICE, "Rejecting frames (memory constraint)");
                rejected_last_frame = 1; /* Track that we dropped a frame so we
                                        don't have to warn the user again. */
            }

            if (port->pending_iframe) {
                fscc_frame_delete(port->pending_iframe);
                port->pending_iframe = 0;
            }

            WdfSpinLockRelease(port->board_rx_spinlock);
            return;
        }

        if (!port->pending_iframe) {
            port->pending_iframe = fscc_frame_new(fscc_port_has_dma(port));

            if (!port->pending_iframe) {
				WdfSpinLockRelease(port->board_rx_spinlock);
                return;
            }
        }

        {
            char buffer[8192];

            fscc_port_get_register_rep(port, 0, FIFO_OFFSET, buffer, receive_length);
            fscc_frame_add_data(port->pending_iframe, buffer, receive_length);
        }

#ifdef __BIG_ENDIAN
        {
            char status[STATUS_LENGTH];

            /* Moves the status bytes to the end. */
            memmove(&status, port->pending_iframe->data, STATUS_LENGTH);
            memmove(port->pending_iframe->data, port->pending_iframe->data + STATUS_LENGTH, port->pending_iframe->current_length - STATUS_LENGTH);
            memmove(port->pending_iframe->data + port->pending_iframe->current_length - STATUS_LENGTH, &status, STATUS_LENGTH);
        }
#endif

		TraceEvents(TRACE_LEVEL_VERBOSE, TRACE_DEVICE, 
			"F#%i <= %i byte%s (%sfinished)",
            port->pending_iframe->number, receive_length,
            (receive_length == 1) ? "" : "s",
            (finished_frame) ? "" : "un");
    }

    if (!finished_frame) {
		WdfSpinLockRelease(port->board_rx_spinlock);
        return;
    }

    fscc_frame_trim(port->pending_iframe);

	if (port->pending_iframe)
        fscc_flist_add_frame(&port->iframes, port->pending_iframe);
	
    rejected_last_frame = 0; /* Track that we received a frame to reset the
                            memory constraint warning print message. */

    port->pending_iframe = 0;

    WdfDpcEnqueue(port->process_read_dpc);

	WdfSpinLockRelease(port->board_rx_spinlock);
}

/* This function is syncronized so we don't have to worry about it being ran in parallel */
void istream_worker(WDFDPC Dpc)
{
    struct fscc_port *port = 0;
    int receive_length = 0; /* Needs to be signed */
    unsigned rxcnt = 0;
    unsigned current_memory = 0;
    unsigned memory_cap = 0;
	static int rejected_last_stream = 0;
	int status;
	char buffer[8192];
	
	port = WdfObjectGet_FSCC_PORT(WdfDpcGetParentObject(Dpc));

    return_if_untrue(port);

    //TODO: This needs a spinlock for the iframe stuff I think. Skip it for now.
    current_memory = fscc_port_get_input_memory_usage(port, 0);
    memory_cap = fscc_port_get_input_memory_cap(port); //TODO: Doesn't need spinlock

    /* Leave the interrupt handler if we are at our memory cap. */
    if (current_memory == memory_cap) {
		TraceEvents(TRACE_LEVEL_WARNING, TRACE_DEVICE, "Stream rejected (memory constraint)");

		if (rejected_last_stream == 0)
			TraceEvents(TRACE_LEVEL_WARNING, TRACE_DEVICE, "Rejecting stream (memory constraint)");
		else
			TraceEvents(TRACE_LEVEL_WARNING, TRACE_DEVICE, "Stream rejected (memory constraint)");

        rejected_last_stream = 1; /* Track that we dropped stream data so we
                                don't have to warn the user again. */
        return;
    }
    
    WdfSpinLockAcquire(port->board_rx_spinlock);

    rxcnt = fscc_port_get_RXCNT(port);

    /* We choose a safe amount to read due to more data coming in after we
        get our values. The rest will be read on the next interrupt. */
    receive_length = rxcnt - MAX_LEFTOVER_BYTES;
    receive_length -= receive_length % 4;

    /* Leave the interrupt handler if there is no data to read. */
    if (receive_length == 0) {
		WdfSpinLockRelease(port->board_rx_spinlock);
        return;
    }

    /* Trim the amount to read if there isn't enough memory space to read all
       of it. */
    if (receive_length + current_memory > memory_cap)
        receive_length = memory_cap - current_memory;

    fscc_port_get_register_rep(port, 0, FIFO_OFFSET, buffer,
                               receive_length);
    
    WdfSpinLockRelease(port->board_rx_spinlock);

    status = fscc_frame_add_data(port->istream, buffer, receive_length);

	if (status == FALSE) {
		TraceEvents(TRACE_LEVEL_ERROR, TRACE_DEVICE, 
			"Error adding stream data");
		return;
	}

    rejected_last_stream = 0; /* Track that we received stream data to reset
                                the memory constraint warning print message.
                            */

	TraceEvents(TRACE_LEVEL_VERBOSE, TRACE_DEVICE, 
		"Stream <= %i byte%s", receive_length,
            (receive_length == 1) ? "" : "s");
	
	WdfDpcEnqueue(port->process_read_dpc);
}

void oframe_worker(WDFDPC Dpc)
{
    struct fscc_port *port = 0;

    unsigned fifo_space = 0;
    unsigned current_length = 0;
    unsigned buffer_size = 0;
    unsigned transmit_length = 0;
    unsigned size_in_fifo = 0;
	
	port = WdfObjectGet_FSCC_PORT(WdfDpcGetParentObject(Dpc));

    return_if_untrue(port);
	
	WdfSpinLockAcquire(port->oframe_spinlock);
	 
    /* Check if exists and if so, grabs the frame to transmit. */
    if (!port->pending_oframe) {
        port->pending_oframe = fscc_flist_remove_frame(&port->oframes);

        /* No frames in queue to transmit */
        if (!port->pending_oframe) {
            WdfSpinLockRelease(port->oframe_spinlock);
            return;
        }
    }

    current_length = fscc_frame_get_length(port->pending_oframe);
    buffer_size = fscc_frame_get_buffer_size(port->pending_oframe);
    size_in_fifo = current_length + (4 - current_length % 4);

    /* Subtracts 1 so a TDO overflow doesn't happen on the 4096th byte. */
    fifo_space = TX_FIFO_SIZE - fscc_port_get_TXCNT(port) - 1;
    fifo_space -= fifo_space % 4;

    /* Determine the maximum amount of data we can send this time around. */
    transmit_length = (size_in_fifo > fifo_space) ? fifo_space : current_length;

    if (transmit_length == 0) {
		WdfSpinLockRelease(port->oframe_spinlock);
        return;
    }
	
    //TODO: Manually accessing the buffer here is not good
    fscc_port_set_register_rep(port, 0, FIFO_OFFSET,
                               port->pending_oframe->buffer,
                               transmit_length);
	
    fscc_frame_remove_data(port->pending_oframe, NULL, transmit_length);

	TraceEvents(TRACE_LEVEL_VERBOSE, TRACE_DEVICE, 
			"F#%i => %i byte%s%s",
            port->pending_oframe->number, transmit_length,
            (transmit_length == 1) ? "" : "s",
            (fscc_frame_is_empty(port->pending_oframe)) ? " (starting)" : "");

    //TODO: There needs to be a better way of telling if this is the first time
    /* If this is the first time we add data to the FIFO for this frame we
       tell the port how much data is in this frame. */
    if (current_length == buffer_size)
        fscc_port_set_register(port, 0, BC_FIFO_L_OFFSET, buffer_size);

    /* If we have sent all of the data we clean up. */
    if (fscc_frame_is_empty(port->pending_oframe)) {
        fscc_frame_delete(port->pending_oframe);
        port->pending_oframe = 0;
        //wake_up_interruptible(&port->output_queue);
    }

    fscc_port_execute_transmit(port);

	WdfSpinLockRelease(port->oframe_spinlock);
}

VOID timer_handler(WDFTIMER Timer)
{
    struct fscc_port *port = 0;
    unsigned streaming = 0;

	port = WdfObjectGet_FSCC_PORT(WdfTimerGetParentObject(Timer));
    
    streaming = fscc_port_is_streaming(port);

    if (streaming)
		WdfDpcEnqueue(port->istream_dpc);
    //else
	//	WdfDpcEnqueue(port->iframe_dpc);
}