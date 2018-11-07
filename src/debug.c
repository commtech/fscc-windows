/*
    Copyright (C) 2018  Commtech, Inc.

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


#include "debug.h"
#include "utils.h" /* return_{val_}if_true */

#if defined(EVENT_TRACING)
#include "debug.tmh"
#endif

void display_register(unsigned bar, unsigned offset, UINT32 old_val,
                      UINT32 new_val)
{
    TRACEHANDLE SessionHandle = TRACE_LEVEL_INFORMATION;

    switch (offset) {
    case FIFO_OFFSET:
    case BC_FIFO_L_OFFSET:
    case FIFO_BC_OFFSET:
    case FIFO_FC_OFFSET:
    case CMDR_OFFSET:
        SessionHandle = TRACE_LEVEL_VERBOSE;
        break;
    }

    if (bar == 0 && (offset == FIFO_OFFSET || offset == BC_FIFO_L_OFFSET || offset == CMDR_OFFSET))
        old_val = new_val;

    if (old_val != new_val) {
        TraceEvents(SessionHandle, TRACE_DEVICE,
                    "Register (%i:0x%02x) 0x%08x => 0x%08x", bar, offset,
                    old_val, new_val);
    }
    else {
        TraceEvents(TRACE_LEVEL_VERBOSE, TRACE_DEVICE,
                    "Register (%i:0x%02x) = 0x%08x", bar, offset, new_val);
    }
}

void print_interrupts(unsigned isr_value)
{
    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DEVICE,
                "Interrupt: 0x%08x\n", isr_value);

    if (isr_value & RFE) {
        TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DEVICE,
                    "RFE (Receive Frame End Interrupt)");
    }

    if (isr_value & RFT) {
        TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DEVICE,
                    "RFT (Receive FIFO Trigger Interrupt)");
    }

    if (isr_value & RFS) {
        TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DEVICE,
                    "RFS (Receive Frame Start Interrupt)");
    }

    if (isr_value & RFO) {
        TraceEvents(TRACE_LEVEL_WARNING, TRACE_DEVICE,
                    "RFO (Receive Frame Overflow Interrupt)");
    }

    if (isr_value & RDO) {
        TraceEvents(TRACE_LEVEL_WARNING, TRACE_DEVICE,
                    "RDO (Receive Data Overflow Interrupt)");
    }

    if (isr_value & RFL) {
        TraceEvents(TRACE_LEVEL_WARNING, TRACE_DEVICE,
                    "RFL (Receive Frame Lost Interrupt)");
    }

    if (isr_value & TIN) {
        TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DEVICE,
                    "TIN (Timer Expiration Interrupt)");
    }

    if (isr_value & TFT) {
        TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DEVICE,
                    "TFT (Transmit FIFO Trigger Interrupt)");
    }

    if (isr_value & TDU) {
        TraceEvents(TRACE_LEVEL_WARNING, TRACE_DEVICE,
                    "TDU (Transmit Data Underrun Interrupt)");
    }

    if (isr_value & ALLS) {
        TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DEVICE,
                    "ALLS (All Sent Interrupt)");
    }

    if (isr_value & CTSS) {
        TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DEVICE,
                    "CTSS (CTS State Change Interrupt)");
    }

    if (isr_value & DSRC) {
        TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DEVICE,
                    "DSRC (DSR Change Interrupt)");
    }

    if (isr_value & CDC) {
        TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DEVICE,
                    "CDC (CD Change Interrupt)");
    }

    if (isr_value & DT_STOP) {
        TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DEVICE,
                    "DT_STOP (DMA Transmitter Full Stop indication)");
    }

    if (isr_value & DR_STOP) {
        TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DEVICE,
                    "DR_STOP (DMA Receiver Full Stop indication)");
    }

    if (isr_value & DT_FE) {
        TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DEVICE,
                    "DT_FE (DMA Transmit Frame End indication)");
    }

    if (isr_value & DR_FE) {
        TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DEVICE,
                    "DR_FE (DMA Receive Frame End indication)");
    }

    if (isr_value & DT_HI) {
        TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DEVICE,
                    "DT_HI (DMA Transmit Host Interrupt indication)");
    }

    if (isr_value & DR_HI) {
        TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DEVICE,
                    "DR_HI (DMA Receive Host Interrupt indication)");
    }
}

#if !defined(EVENT_TRACING)

VOID
TraceEvents    (
    IN TRACEHANDLE TraceEventsLevel,
    IN ULONG   TraceEventsFlag,
    IN PCCHAR  DebugMessage,
    ...
    )

/*++

Routine Description:

    Debug print for the sample driver.

Arguments:

    TraceEventsLevel - print level between 0 and 3, with 3 the most verbose

Return Value:

    None.

 --*/
 {
#if DEBUG

#define     TEMP_BUFFER_SIZE        1024

    va_list    list;
    CHAR      debugMessageBuffer [TEMP_BUFFER_SIZE];
    NTSTATUS   status;

    va_start(list, DebugMessage);

    if (DebugMessage) {

        //
        // Using new safe string functions instead of _vsnprintf.
        // This function takes care of NULL terminating if the message
        // is longer than the buffer.
        //
        status = RtlStringCbVPrintfA( debugMessageBuffer,
                                      sizeof(debugMessageBuffer),
                                      DebugMessage,
                                      list );
        if(!NT_SUCCESS(status)) {

            KdPrint((_DRIVER_NAME_": RtlStringCbVPrintfA failed %x\n", status));
            return;
        }
        if (TraceEventsLevel < TRACE_LEVEL_INFORMATION ||
            (TraceEventsLevel <= DebugLevel &&
             ((TraceEventsFlag & DebugFlag) == TraceEventsFlag))) {

            KdPrint((debugMessageBuffer));
        }
    }
    va_end(list);

    return;

#else

    UNREFERENCED_PARAMETER(TraceEventsLevel);
    UNREFERENCED_PARAMETER(TraceEventsFlag);
    UNREFERENCED_PARAMETER(DebugMessage);
#endif
}

#endif
