/*
    Copyright (C) 2015  Commtech, Inc.

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


#ifndef FSCC_DEBUG_H
#define FSCC_DEBUG_H

#include <ntddk.h>
#include <wdf.h>

#include "Trace.h"

void display_register(unsigned bar, unsigned offset, UINT32 old_val,
                      UINT32 new_val);

void print_interrupts(unsigned isr_value);

#if !defined(EVENT_TRACING)
VOID
TraceEvents    (
    IN TRACEHANDLE   TraceEventsLevel,
    IN ULONG   TraceEventsFlag,
    IN PCCHAR  DebugMessage,
    ...
    );
#endif

#endif