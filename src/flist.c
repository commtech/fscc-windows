/*
    Copyright (C) 2016  Commtech, Inc.

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


#include "flist.h"
#include "flist.tmh"
#include "utils.h" /* return_{val_}if_true */
#include "frame.h"
#include "debug.h"

void fscc_flist_init(struct fscc_flist *flist)
{
    flist->estimated_memory_usage = 0;

    InitializeListHead(&flist->frames);
}

void fscc_flist_delete(struct fscc_flist *flist)
{
    return_if_untrue(flist);

    fscc_flist_clear(flist);
}

void fscc_flist_add_frame(struct fscc_flist *flist, struct fscc_frame *frame)
{
    InsertTailList(&flist->frames, &frame->list);

    flist->estimated_memory_usage += fscc_frame_get_length(frame);
}

struct fscc_frame *fscc_flist_peak_front(struct fscc_flist *flist)
{
    if (IsListEmpty(&flist->frames))
        return 0;

    return CONTAINING_RECORD(flist->frames.Flink, FSCC_FRAME, list);
}

struct fscc_frame *fscc_flist_remove_frame(struct fscc_flist *flist)
{
    struct fscc_frame *frame = 0;

    if (IsListEmpty(&flist->frames))
        return 0;

    frame = CONTAINING_RECORD(flist->frames.Flink, FSCC_FRAME, list);

    RemoveHeadList(&flist->frames);

    flist->estimated_memory_usage -= fscc_frame_get_length(frame);

    return frame;
}

struct fscc_frame *fscc_flist_remove_frame_if_lte(struct fscc_flist *flist,
                                                  unsigned size)
{
    struct fscc_frame *frame = 0;
    unsigned frame_length = 0;

    if (IsListEmpty(&flist->frames))
        return 0;

    frame = CONTAINING_RECORD(flist->frames.Flink, FSCC_FRAME, list);

    frame_length = fscc_frame_get_length(frame);

    if (frame_length > size)
        return 0;

    RemoveHeadList(&flist->frames);

    flist->estimated_memory_usage -= frame_length;

    return frame;
}

void fscc_flist_clear(struct fscc_flist *flist)
{
    while (!IsListEmpty(&flist->frames)) {
        LIST_ENTRY *frame_iter = 0;
        struct fscc_frame *frame = 0;

        frame_iter = RemoveHeadList(&flist->frames);
        frame = CONTAINING_RECORD(frame_iter, FSCC_FRAME, list);

        fscc_frame_delete(frame);
    }

    flist->estimated_memory_usage = 0;
}

BOOLEAN fscc_flist_is_empty(struct fscc_flist *flist)
{
    return IsListEmpty(&flist->frames);
}

unsigned fscc_flist_calculate_memory_usage(struct fscc_flist *flist)
{
    LIST_ENTRY *frame_iter = 0;
    unsigned memory = 0;

    frame_iter = flist->frames.Flink;
    while (frame_iter != flist->frames.Blink) {
        struct fscc_frame *current_frame = 0;

        current_frame = CONTAINING_RECORD(frame_iter, FSCC_FRAME, list);
        memory += fscc_frame_get_length(current_frame);

        frame_iter = frame_iter->Flink;
    }

    return memory;
}