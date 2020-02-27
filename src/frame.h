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


#ifndef FSCC_FRAME_H
#define FSCC_FRAME_H

//#include "descriptor.h" /* struct fscc_descriptor */
#include <ntddk.h>
#include <wdf.h>

#include "trace.h"

typedef struct fscc_frame {
    LIST_ENTRY list;
    WDFCOMMONBUFFER dma_buffer;
    unsigned char *buffer;
    WDFCOMMONBUFFER desc_buffer;
    struct fscc_descriptor *desc;
    UINT32 logical_desc;
    unsigned buffer_size;
    unsigned number;
    LARGE_INTEGER timestamp;
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

#endif