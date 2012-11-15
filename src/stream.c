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
#include "stream.tmh"

#include "utils.h" /* return_{val_}if_true */

int fscc_stream_update_buffer_size(struct fscc_stream *stream,
                                    unsigned length);

struct fscc_stream *fscc_stream_new(void)
{
    struct fscc_stream *stream = 0;
	
	stream = (struct fscc_stream *)ExAllocatePoolWithTag(NonPagedPool, sizeof(*stream), 'ertS');

    if (stream == NULL) {
		TraceEvents(TRACE_LEVEL_ERROR, TRACE_DEVICE, "Not enough memory to allocate stream");
        return 0;
    }

    stream->length = 0;
    stream->data = 0;

    return stream;
}

void fscc_stream_delete(struct fscc_stream *stream)
{
    return_if_untrue(stream);

    if (stream->data)
		ExFreePoolWithTag(stream->data, 'ataD');
	
	ExFreePoolWithTag(stream, 'ertS');
}

unsigned fscc_stream_get_length(struct fscc_stream *stream)
{
    return_val_if_untrue(stream, 0);

    return stream->length;
}

unsigned fscc_stream_is_empty(struct fscc_stream *stream)
{
    return_val_if_untrue(stream, 0);

    return !fscc_stream_get_length(stream);
}

int fscc_stream_add_data(struct fscc_stream *stream, const char *data,
                          unsigned length)
{
    unsigned old_length = 0;
	int status = TRUE;

    return_val_if_untrue(stream, FALSE);

    old_length = stream->length;

    if (fscc_stream_update_buffer_size(stream, stream->length + length) == FALSE)
		return FALSE;

    memmove(stream->data + old_length, data, length);

	return TRUE;
}

int fscc_stream_remove_data(struct fscc_stream *stream, unsigned length)
{
    unsigned new_length = 0;

    return_val_if_untrue(stream, FALSE);

	if (length == 0)
		return TRUE;

	if (stream->length == 0) {
		TraceEvents(TRACE_LEVEL_WARNING, TRACE_DEVICE, "Attempting data removal from empty stream");
		return TRUE;
	}

    new_length = stream->length - length;

    memmove(stream->data, stream->data + length, new_length);

    return fscc_stream_update_buffer_size(stream, new_length);
}

char *fscc_stream_get_data(struct fscc_stream *stream)
{
    return_val_if_untrue(stream, 0);

    return stream->data;
}

int fscc_stream_update_buffer_size(struct fscc_stream *stream,
                                    unsigned length)
{
    char *new_data = 0;
    //int malloc_flags = 0;

    return_val_if_untrue(stream, FALSE);

    if (length == 0) {
        if (stream->data) {
			ExFreePoolWithTag(stream->data, 'ataD');
            stream->data = 0;
        }

        stream->length = 0;
        return TRUE;
    }

    //malloc_flags |= GFP_ATOMIC;
	
	new_data = (char *)ExAllocatePoolWithTag(NonPagedPool, length, 'ataD');

    if (new_data == NULL) {
		TraceEvents(TRACE_LEVEL_ERROR, TRACE_DEVICE, "Not enough memory to update stream buffer size");
        return FALSE;
    }

    memset(new_data, 0, length);

    if (stream->data) {
        if (stream->length)
            memmove(new_data, stream->data, min(stream->length, length));
		
		ExFreePoolWithTag(stream->data, 'ataD');
    }

    stream->data = new_data;
    stream->length = length;

	return TRUE;
}