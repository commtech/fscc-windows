/*
Copyright 2019 Commtech, Inc.

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


#include "isr.h"
#include "port.h" /* struct fscc_port */
#include "utils.h" /* port_exists */
#include "frame.h" /* struct fscc_frame */
#include "debug.h"

#if defined(EVENT_TRACING)
#include "isr.tmh"
#endif

#pragma warning( disable: 4127 )

#define TX_FIFO_SIZE 4096
#define MAX_LEFTOVER_BYTES 3

BOOLEAN fscc_isr(WDFINTERRUPT Interrupt, ULONG MessageID)
{
    struct fscc_port *port = 0;
    BOOLEAN handled = FALSE;
    unsigned isr_value = 0;
    unsigned using_dma = 0;

    UNREFERENCED_PARAMETER(MessageID);

    port = WdfObjectGet_FSCC_PORT(WdfInterruptGetDevice(Interrupt));

    //WdfTimerStop(port->timer, FALSE);

    isr_value = fscc_port_get_register(port, 0, ISR_OFFSET);

    if (!isr_value)
        return handled;

    handled = TRUE;

    port->last_isr_value |= isr_value;
	
    using_dma = fscc_port_uses_dma(port);
    if (using_dma) {
        if (isr_value & (DR_HI | DR_FE)) {
            WdfDpcEnqueue(port->process_read_dpc);
        }
    }
    else {
		if (isr_value & (RFE | RFT | RFS | RFO | RDO ))
			WdfDpcEnqueue(port->iframe_dpc);
		
		if (isr_value & (TFT | TDU | ALLS))
			WdfDpcEnqueue(port->oframe_dpc);
    }
	
	// TODO error handling for RDO, RFO, TDU, etc?
    //if (isr_value & ALLS)
    //    WdfDpcEnqueue(port->clear_oframe_dpc);
	//if (isr_value & ALLS)
	//	wait on write?
    WdfDpcEnqueue(port->isr_alert_dpc);
    //DbgPrint("--------ISR occurred: 0x%8.8x---------\n", isr_value);

    //fscc_port_reset_timer(port);

    return handled;
}

void isr_alert_worker(WDFDPC Dpc)
{
    struct fscc_port *port = 0;
    NTSTATUS status = STATUS_SUCCESS;
    WDFREQUEST Request;
    unsigned *mask = 0;
    unsigned *matches = 0;
    unsigned isr_value = 0;

    WDFREQUEST tagRequest = NULL;
    WDFREQUEST prevTagRequest = NULL;

    port = WdfObjectGet_FSCC_PORT(WdfDpcGetParentObject(Dpc));
	
    isr_value = port->last_isr_value;
	
    port->last_isr_value = 0;
	//DbgPrint("RXCNT: %d, RFCNT: %d, TXCNT: %d\n", fscc_port_get_RXCNT(port),fscc_port_get_RFCNT(port),fscc_port_get_TXCNT(port));

    do {
        status = WdfIoQueueFindRequest(
                                       port->isr_queue,
                                       prevTagRequest,
                                       NULL,
                                       NULL,
                                       &tagRequest
                                       );

        if (prevTagRequest) {
            WdfObjectDereference(prevTagRequest);
        }
        if (status == STATUS_NO_MORE_ENTRIES) {
            break;
        }
        if (status == STATUS_NOT_FOUND) {
            //
            // The prevTagRequest request has disappeared from the
            // queue. There might be other requests that match
            // the criteria, so restart the search. 
            //
            prevTagRequest = tagRequest = NULL;
            continue;
        }
        if (!NT_SUCCESS(status)) { 
            TraceEvents(TRACE_LEVEL_ERROR, TRACE_DEVICE,
                        "WdfIoQueueFindRequest failed %!STATUS!",
                        status);
            break;
        }

        status = WdfRequestRetrieveInputBuffer(tagRequest,
                    sizeof(*mask), (PVOID *)&mask, NULL);
        if (!NT_SUCCESS(status)) {
            TraceEvents(TRACE_LEVEL_WARNING, TRACE_DEVICE,
                "WdfRequestRetrieveInputBuffer failed %!STATUS!", status);
                WdfObjectDereference(tagRequest);
                break;
        }

        status = WdfRequestRetrieveOutputBuffer(tagRequest,
                    sizeof(*matches), (PVOID *)&matches, NULL);
        if (!NT_SUCCESS(status)) {
            TraceEvents(TRACE_LEVEL_WARNING, TRACE_DEVICE,
                "WdfRequestRetrieveOutputBuffer failed %!STATUS!", status);
                WdfObjectDereference(tagRequest);
            break;
        }

        if (isr_value & *mask) {
            //
            // Found a match. Retrieve the request from the queue.
            //

            status = WdfIoQueueRetrieveFoundRequest(
                                                    port->isr_queue,
                                                    tagRequest,
                                                    &Request
                                                    );

            WdfObjectDereference(tagRequest);
            if (status == STATUS_NOT_FOUND) {
                //
                // The tagRequest request has disappeared from the
                // queue. There might be other requests that match 
                // the criteria, so restart the search. 
                //
                prevTagRequest = tagRequest = NULL;
                continue;
            }
            if (!NT_SUCCESS(status)) {
                TraceEvents(TRACE_LEVEL_ERROR, TRACE_DEVICE,
                            "WdfIoQueueRetrieveFoundRequest failed %!STATUS!",
                            status);
                break;
            }
            //
            //  Found a request.
            //
            prevTagRequest = tagRequest = NULL;
            *matches = isr_value & *mask;
            WdfRequestCompleteWithInformation(Request, status, sizeof(*matches));
        } else {
            //
            // This request is not the correct one. Drop the reference 
            // on the tagRequest after the driver obtains the next request.
            //
            prevTagRequest = tagRequest;
        }
    } while (TRUE);

#ifdef DEBUG
    print_interrupts(isr_value);
#endif
}

void iframe_worker(WDFDPC Dpc)
{
    struct fscc_port *port = 0;

    port = WdfObjectGet_FSCC_PORT(WdfDpcGetParentObject(Dpc));

    return_if_untrue(port);
	
	if(fscc_port_uses_dma(port)) return;
	
	fscc_fifo_read_data(port);

	WdfDpcEnqueue(port->process_read_dpc);

}

void oframe_worker(WDFDPC Dpc)
{
    struct fscc_port *port = 0;
	size_t bytes_ready = 0, frames = 0;

    port = WdfObjectGet_FSCC_PORT(WdfDpcGetParentObject(Dpc));

    return_if_untrue(port);
	if(fscc_port_uses_dma(port)) return;
	
    frames = fscc_fifo_write_has_data(port, &bytes_ready);
	if(bytes_ready == 0) return;
	
	DbgPrint("%s: frames: %d, bytes: %d\n", __FUNCTION__, frames, bytes_ready);
    fscc_port_transmit_frame(port);
}

void request_worker(WDFDPC Dpc)
{
    struct fscc_port *port = 0;
    struct fscc_frame *frame = 0;
    char *data_buffer = NULL;
	size_t write_count = 0;
    NTSTATUS status = STATUS_SUCCESS;
    WDFREQUEST Request = NULL, tagRequest = NULL, prevTagRequest = NULL;
    WDF_REQUEST_PARAMETERS params;
    size_t Length;

    port = WdfObjectGet_FSCC_PORT(WdfDpcGetParentObject(Dpc));

    WDF_REQUEST_PARAMETERS_INIT(&params);
    status = WdfIoQueueFindRequest(port->blocking_request_queue, prevTagRequest, NULL, &params, &tagRequest);
    if (prevTagRequest) WdfObjectDereference(prevTagRequest);
    if (!NT_SUCCESS(status)) {
        TraceEvents(TRACE_LEVEL_VERBOSE, TRACE_DEVICE, "WdfIoQueueFindRequest failed %!STATUS!", status);
        return;
    }
    Length = params.Parameters.Write.Length;
    
    if(fscc_user_get_tx_space(port) < Length) 
        return;
    
    status = WdfIoQueueRetrieveFoundRequest(port->blocking_request_queue, tagRequest, &Request);
    WdfObjectDereference(tagRequest);
    if (!NT_SUCCESS(status)) return;
 
    status = WdfRequestRetrieveInputBuffer(Request, Length, (PVOID *)&data_buffer, NULL);
    if (!NT_SUCCESS(status)) {
        TraceEvents(TRACE_LEVEL_ERROR, TRACE_DEVICE, "WdfRequestRetrieveInputBuffer failed %!STATUS!", status);
        WdfRequestComplete(Request, status);
        return;
    }
	// TODO can I pass the request to wait_on_write queue here?
	status = fscc_user_write_frame(port, data_buffer, Length, &write_count);

    WdfRequestCompleteWithInformation(Request, status, write_count);
    if(!fscc_port_uses_dma(port))
		WdfDpcEnqueue(port->oframe_dpc);
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
    //    WdfDpcEnqueue(port->iframe_dpc);

    // Had to remove the condition check, otherwise there
    // was a chance that a request could be stuck in the 
    // queue forever if the condition changed.
    WdfDpcEnqueue(port->orequest_worker);
}
