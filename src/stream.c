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


#include "stream.h"
#include "utils.h" /* return_{val_}if_true */
#include "stream.h"
#include "debug.h"

#if defined(EVENT_TRACING)
#include "stream.tmh"
#endif

int fscc_stream_update_buffer_size(struct fscc_stream *stream,
                                   unsigned length);

void fscc_stream_init(struct fscc_stream *stream)
{
    NTSTATUS status = STATUS_SUCCESS;

    status = WdfSpinLockCreate(WDF_NO_OBJECT_ATTRIBUTES, &stream->spinlock);
    if (!NT_SUCCESS(status)) {
        TraceEvents(TRACE_LEVEL_ERROR, TRACE_DEVICE, 
            "WdfSpinLockCreate failed %!STATUS!", status);
        return;
    }

    WdfSpinLockAcquire(stream->spinlock);

    stream->data_length = 0;
    stream->buffer_size = 0;
    stream->buffer = 0;

    WdfSpinLockRelease(stream->spinlock);
}

void fscc_stream_delete(struct fscc_stream *stream)
{
    return_if_untrue(stream);

    WdfSpinLockAcquire(stream->spinlock);

    fscc_stream_update_buffer_size(stream, 0);

    WdfSpinLockRelease(stream->spinlock);
}

unsigned fscc_stream_get_length(struct fscc_stream *stream)
{
    return_val_if_untrue(stream, 0);

    return stream->data_length;
}

//TODO: This could cause an issue w here data_length is less before it makes it into remove_Data
void fscc_stream_clear(struct fscc_stream *stream)
{
    fscc_stream_remove_data(stream, NULL, stream->data_length);
}

unsigned fscc_stream_is_empty(struct fscc_stream *stream)
{
    return_val_if_untrue(stream, 0);

    return stream->data_length == 0;
}

int fscc_stream_add_data(struct fscc_stream *stream, const char *data,
                         unsigned length)
{
    return_val_if_untrue(stream, FALSE);

    WdfSpinLockAcquire(stream->spinlock);

    /* Only update buffer size if there isn't enough space already */
    if (stream->data_length + length > stream->buffer_size) {
        if (fscc_stream_update_buffer_size(stream, stream->data_length + length) == FALSE) {
            WdfSpinLockRelease(stream->spinlock);
    		return FALSE;
        }
    }

    /* Copy the new data to the end of the stream */
    memmove(stream->buffer + stream->data_length, data, length);

    stream->data_length += length;

    WdfSpinLockRelease(stream->spinlock);

	return TRUE;
}

int fscc_stream_remove_data(struct fscc_stream *stream, char *destination, unsigned length)
{
    return_val_if_untrue(stream, FALSE);

    if (length == 0)
        return TRUE;

    WdfSpinLockAcquire(stream->spinlock);

    if (stream->data_length == 0) {
        TraceEvents(TRACE_LEVEL_WARNING, TRACE_DEVICE, "Attempting data removal from empty stream");
        WdfSpinLockRelease(stream->spinlock);
        return TRUE;
    }

    /* Make sure we don't remove remove more than we have */
    if (length > stream->data_length) {
        TraceEvents(TRACE_LEVEL_WARNING, TRACE_DEVICE, "Attempting removal of more data than available"); 
        WdfSpinLockRelease(stream->spinlock);
        return FALSE;
    }

    /* Copy the data into the outside buffer */
    if (destination)
        memmove(destination, stream->buffer, length);

    stream->data_length -= length;

    /* Move the data up in the buffer (essentially removing the old data) */
    memmove(stream->buffer, stream->buffer + length, stream->data_length);

    WdfSpinLockRelease(stream->spinlock);

    return TRUE;
}

int fscc_stream_update_buffer_size(struct fscc_stream *stream,
                                   unsigned size)
{
    char *new_buffer = 0;

    return_val_if_untrue(stream, FALSE);

    if (size == 0) {
        if (stream->buffer) {
			ExFreePoolWithTag(stream->buffer, 'ataD');
            stream->buffer = 0;
        }

        stream->buffer_size = 0;
        stream->data_length = 0;

        return TRUE;
    }
	
	new_buffer = (char *)ExAllocatePoolWithTag(NonPagedPool, size, 'ataD');

    if (new_buffer == NULL) {
		TraceEvents(TRACE_LEVEL_ERROR, TRACE_DEVICE, "Not enough memory to update stream buffer size");
        return FALSE;
    }

    memset(new_buffer, 0, size);

    if (stream->buffer) {
        if (stream->data_length) {
            /* Truncate data length if the new buffer size is less than the data length */
            stream->data_length = min(stream->data_length, size);

            /* Copy over the old buffer data to the new buffer */
            memmove(new_buffer, stream->buffer, stream->data_length);
        }

		ExFreePoolWithTag(stream->buffer, 'ataD');
    }

    stream->buffer = new_buffer;
    stream->buffer_size = size;

	return TRUE;
}