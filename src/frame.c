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

/*
#include "frame.h"
#include "utils.h" // return_{val_}if_true 
#include "port.h" // struct fscc_port 

#if defined(EVENT_TRACING)
#include "frame.tmh"
#endif

static unsigned frame_counter = 1;

int fscc_frame_update_buffer_size(struct fscc_frame *frame, unsigned size);

struct fscc_frame *fscc_frame_new(struct fscc_port *port)
{
    struct fscc_frame *frame = 0;

    frame = (struct fscc_frame *)ExAllocatePoolWithTag(NonPagedPool,
                                     sizeof(*frame), 'marF');

    if (frame == NULL)
        return 0;

    frame->data_length = 0;
    frame->buffer_size = 0;
    frame->buffer = 0;
    frame->fifo_initialized = 0;
    frame->port = port;

    frame->number = frame_counter;
    frame_counter += 1;

    return frame;
}

void fscc_frame_delete(struct fscc_frame *frame)
{
    return_if_untrue(frame);

    fscc_frame_update_buffer_size(frame, 0);

    ExFreePoolWithTag(frame, 'marF');
}

unsigned fscc_frame_get_length(struct fscc_frame *frame)
{
    return_val_if_untrue(frame, 0);

    return frame->data_length;
}

unsigned fscc_frame_get_buffer_size(struct fscc_frame *frame)
{
    return_val_if_untrue(frame, 0);

    return frame->buffer_size;
}

unsigned fscc_frame_is_empty(struct fscc_frame *frame)
{
    return_val_if_untrue(frame, 0);

    return frame->data_length == 0;
}

int fscc_frame_add_data(struct fscc_frame *frame, const char *data,
                         unsigned length)
{
    return_val_if_untrue(frame, FALSE);
    return_val_if_untrue(length > 0, FALSE);

    // Only update buffer size if there isn't enough space already 
    if (frame->data_length + length > frame->buffer_size) {
        if (fscc_frame_update_buffer_size(frame, frame->data_length + length)
            == FALSE) {
            return FALSE;
        }
    }

    // Copy the new data to the end of the frame 
    memmove(frame->buffer + frame->data_length, data, length);

    frame->data_length += length;

    return TRUE;
}

int fscc_frame_add_data_from_port(struct fscc_frame *frame, struct fscc_port *port,
                                  unsigned length)
{
    return_val_if_untrue(frame, FALSE);
    return_val_if_untrue(length > 0, FALSE);

    // Only update buffer size if there isn't enough space already 
    if (frame->data_length + length > frame->buffer_size) {
        if (fscc_frame_update_buffer_size(frame, frame->data_length + length)
            == FALSE) {
            return FALSE;
        }
    }

    // Copy the new data to the end of the frame 
    fscc_port_get_register_rep(port, 0, FIFO_OFFSET, frame->buffer + frame->data_length, length);

    frame->data_length += length;

    return TRUE;
}

int fscc_frame_remove_data(struct fscc_frame *frame, char *destination,
                           unsigned length)
{
    return_val_if_untrue(frame, FALSE);

    if (length == 0)
        return TRUE;

    if (frame->data_length == 0) {
        TraceEvents(TRACE_LEVEL_WARNING, TRACE_DEVICE,
                    "Attempting data removal from empty frame");
        return TRUE;
    }

    // Make sure we don't remove more data than we have 
    if (length > frame->data_length) {
        TraceEvents(TRACE_LEVEL_WARNING, TRACE_DEVICE,
                    "Attempting removal of more data than available");
        return FALSE;
    }

    // Copy the data into the outside buffer 
    if (destination)
        memmove(destination, frame->buffer, length);

    frame->data_length -= length;

    // Move the data up in the buffer (essentially removing the old data) 
    memmove(frame->buffer, frame->buffer + length, frame->data_length);

    return TRUE;
}

void fscc_frame_clear(struct fscc_frame *frame)
{
    fscc_frame_update_buffer_size(frame, 0);
}

int fscc_frame_update_buffer_size(struct fscc_frame *frame, unsigned size)
{
    char *new_buffer = 0;

    return_val_if_untrue(frame, FALSE);

    if (size == 0) {
        if (frame->buffer) {
            ExFreePoolWithTag(frame->buffer, 'ataD');
            frame->buffer = 0;
        }

        frame->buffer_size = 0;
        frame->data_length = 0;

        return TRUE;
    }

    new_buffer = (char *)ExAllocatePoolWithTag(NonPagedPool, size, 'ataD');

    if (new_buffer == NULL) {
        TraceEvents(TRACE_LEVEL_ERROR, TRACE_DEVICE,
                    "Not enough memory to update frame buffer size");
        return FALSE;
    }

    memset(new_buffer, 0, size);

    if (frame->buffer) {
        if (frame->data_length) {
            // Truncate data length if the new buffer size is less than the data length 
			frame->data_length = min(frame->data_length, size);

            // Copy over the old buffer data to the new buffer 
            memmove(new_buffer, frame->buffer, frame->data_length);
        }

        ExFreePoolWithTag(frame->buffer, 'ataD');
    }

    frame->buffer = new_buffer;
    frame->buffer_size = size;

    return TRUE;
}

int fscc_frame_setup_descriptors(struct fscc_frame *frame)
{
    UNUSED(frame);

    return 1;
}

unsigned fscc_frame_is_fifo(struct fscc_frame *frame)
{
    return (frame->fifo_initialized);
}
*/