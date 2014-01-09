/*
    Copyright (C) 2014  Commtech, Inc.

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


#ifndef FSCC_FRAME_H
#define FSCC_FRAME_H

//#include "descriptor.h" /* struct fscc_descriptor */
#include <ntddk.h>
#include <wdf.h>

typedef struct fscc_frame {
    LIST_ENTRY list;
    char *buffer;
    unsigned buffer_size;
    unsigned data_length;
    unsigned number;
    LARGE_INTEGER timestamp;
    unsigned dma_initialized;
    unsigned fifo_initialized;
    struct fscc_port *port;
} FSCC_FRAME;

struct fscc_frame *fscc_frame_new(struct fscc_port *port);
void fscc_frame_delete(struct fscc_frame *frame);

unsigned fscc_frame_get_length(struct fscc_frame *frame);
unsigned fscc_frame_get_buffer_size(struct fscc_frame *frame);

int fscc_frame_add_data(struct fscc_frame *frame, const char *data,
                         unsigned length);

int fscc_frame_add_data_from_port(struct fscc_frame *frame, struct fscc_port *port,
                                  unsigned length);

int fscc_frame_remove_data(struct fscc_frame *frame, char *destination,
                           unsigned length);
unsigned fscc_frame_is_empty(struct fscc_frame *frame);
void fscc_frame_clear(struct fscc_frame *frame);
int fscc_frame_setup_descriptors(struct fscc_frame *frame);
unsigned fscc_frame_is_dma(struct fscc_frame *frame);
unsigned fscc_frame_is_fifo(struct fscc_frame *frame);

#endif