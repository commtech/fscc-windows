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


#include "flist.h"
#include "utils.h" /* return_{val_}if_true */
#include "frame.h"
#include "debug.h"

#if defined(EVENT_TRACING)
#include "flist.tmh"
#endif

//TODO: Error checking
void fscc_flist_init(struct fscc_flist *flist)
{
    NTSTATUS status = STATUS_SUCCESS;

    status = WdfSpinLockCreate(WDF_NO_OBJECT_ATTRIBUTES, &flist->spinlock);
    if (!NT_SUCCESS(status)) {
        TraceEvents(TRACE_LEVEL_ERROR, TRACE_DEVICE, 
            "WdfSpinLockCreate failed %!STATUS!", status);
        return;
    }

    InitializeListHead(&flist->frames);
}

void fscc_flist_delete(struct fscc_flist *flist)
{
    return_if_untrue(flist);

    fscc_flist_clear(flist);
}

void fscc_flist_add_frame(struct fscc_flist *flist, struct fscc_frame *frame)
{
    WdfSpinLockAcquire(flist->spinlock);

    InsertTailList(&flist->frames, &frame->list);

    WdfSpinLockRelease(flist->spinlock);
}

struct fscc_frame *fscc_flist_remove_frame(struct fscc_flist *flist)
{
    struct fscc_frame *frame = 0;

    WdfSpinLockAcquire(flist->spinlock);

    if (IsListEmpty(&flist->frames)) {
        WdfSpinLockRelease(flist->spinlock);
        return 0;
    }
    
    frame = CONTAINING_RECORD(flist->frames.Flink, FSCC_FRAME, list);

    RemoveHeadList(&flist->frames);

    WdfSpinLockRelease(flist->spinlock);

    return frame;
}

struct fscc_frame *fscc_flist_remove_frame_if_lte(struct fscc_flist *flist, unsigned size)
{
    struct fscc_frame *frame = 0;

    WdfSpinLockAcquire(flist->spinlock);

    if (IsListEmpty(&flist->frames)) {
        WdfSpinLockRelease(flist->spinlock);
        return 0;
    }

    frame = CONTAINING_RECORD(flist->frames.Flink, FSCC_FRAME, list);

    if (fscc_frame_get_current_length(frame) > size) {
        WdfSpinLockRelease(flist->spinlock);
        return 0;
    }

    RemoveHeadList(&flist->frames);

    WdfSpinLockRelease(flist->spinlock);

    return frame;
}

void fscc_flist_clear(struct fscc_flist *flist)
{
    WdfSpinLockAcquire(flist->spinlock);

    while (!IsListEmpty(&flist->frames)) {  
        LIST_ENTRY *frame_iter = 0;
        struct fscc_frame *frame = 0;   

        frame_iter = RemoveHeadList(&flist->frames);
        frame = CONTAINING_RECORD(frame_iter, FSCC_FRAME, list);    

        fscc_frame_delete(frame);
    }   

    WdfSpinLockRelease(flist->spinlock);
}

BOOLEAN fscc_flist_is_empty(struct fscc_flist *flist)
{
    return IsListEmpty(&flist->frames);
}

unsigned fscc_flist_calculate_memory_usage(struct fscc_flist *flist)
{
    LIST_ENTRY *frame_iter = 0;
    unsigned memory = 0;

    WdfSpinLockAcquire(flist->spinlock);
    
    frame_iter = flist->frames.Flink;
    while (frame_iter != flist->frames.Flink) {
        struct fscc_frame *current_frame = 0;

        current_frame = CONTAINING_RECORD(frame_iter, FSCC_FRAME, list);    
        memory += fscc_frame_get_current_length(current_frame);

        frame_iter = frame_iter->Flink;
    }

    WdfSpinLockRelease(flist->spinlock);

    return memory;
}