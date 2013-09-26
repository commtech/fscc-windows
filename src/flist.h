/*
    Copyright (C) 2013  Commtech, Inc.

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


#ifndef FSCC_FLIST_H
#define FSCC_FLIST_H

#include <ntddk.h>
#include <wdf.h>

#include "Trace.h"

struct fscc_flist {
    LIST_ENTRY frames;
};

void fscc_flist_init(struct fscc_flist *flist);
void fscc_flist_delete(struct fscc_flist *flist);
void fscc_flist_add_frame(struct fscc_flist *flist, struct fscc_frame *frame);
struct fscc_frame *fscc_flist_remove_frame(struct fscc_flist *flist);
struct fscc_frame *fscc_flist_remove_frame_if_lte(struct fscc_flist *flist,
                                                  unsigned size);
void fscc_flist_clear(struct fscc_flist *flist);
BOOLEAN fscc_flist_is_empty(struct fscc_flist *flist);
unsigned fscc_flist_calculate_memory_usage(struct fscc_flist *flist);

#endif