#include "frame.h"
#include "utils.h" /* return_{val_}if_true */
#include "port.h" /* struct fscc_port */

#if defined(EVENT_TRACING)
#include "frame.tmh"
#endif

static unsigned frame_counter = 1;

int fscc_frame_update_buffer_size(struct fscc_frame *frame, unsigned size);

struct fscc_frame *fscc_frame_new(unsigned dma)
{
    NTSTATUS status = STATUS_SUCCESS;
    struct fscc_frame *frame = 0;
		
	frame = (struct fscc_frame *)ExAllocatePoolWithTag(NonPagedPool, sizeof(*frame), 'marF');

	if (frame == NULL)
		return 0;

    status = WdfSpinLockCreate(WDF_NO_OBJECT_ATTRIBUTES, &frame->spinlock);
    if (!NT_SUCCESS(status)) {
        TraceEvents(TRACE_LEVEL_ERROR, TRACE_DEVICE, 
            "WdfSpinLockCreate failed %!STATUS!", status);
        ExFreePoolWithTag(frame, 'marF');
        return 0;
    }

    WdfSpinLockAcquire(frame->spinlock);

    frame->data_length = 0;
    frame->buffer_size = 0;
    frame->buffer = 0;

    frame->number = frame_counter;
    frame_counter += 1;

    WdfSpinLockRelease(frame->spinlock);

    return frame;
}

void fscc_frame_delete(struct fscc_frame *frame)
{
    return_if_untrue(frame);

    WdfSpinLockAcquire(frame->spinlock);

    fscc_frame_update_buffer_size(frame, 0);

    WdfSpinLockRelease(frame->spinlock);
	
	ExFreePoolWithTag(frame, 'marF');
}

unsigned fscc_frame_get_length(struct fscc_frame *frame)
{
    return_val_if_untrue(frame, 0);

    return frame->data_length;
}

//TODO: Eventually remove
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

    WdfSpinLockAcquire(frame->spinlock);

    /* Only update buffer size if there isn't enough space already */
    if (frame->data_length + length > frame->buffer_size) {
        if (fscc_frame_update_buffer_size(frame, frame->data_length + length) == FALSE) {
            WdfSpinLockRelease(frame->spinlock);
            return FALSE;
        }
    }

    /* Copy the new data to the end of the frame */
    memmove(frame->buffer + frame->data_length, data, length);

    frame->data_length += length;

    WdfSpinLockRelease(frame->spinlock);

    return TRUE;
}

int fscc_frame_remove_data(struct fscc_frame *frame, unsigned length)
{
    return_val_if_untrue(frame, FALSE);

    if (length == 0)
        return TRUE;

    WdfSpinLockAcquire(frame->spinlock);

    if (frame->data_length == 0) {
        TraceEvents(TRACE_LEVEL_WARNING, TRACE_DEVICE, "Attempting data removal from empty frame");
        WdfSpinLockRelease(frame->spinlock);
        return TRUE;
    }

    /* Make sure we don't remove more data than we have */
    if (length > frame->data_length) {
        TraceEvents(TRACE_LEVEL_WARNING, TRACE_DEVICE, "Attempting removal of more data than available"); 
        WdfSpinLockRelease(frame->spinlock);
        return FALSE;
    }

    frame->data_length -= length;

    WdfSpinLockRelease(frame->spinlock);

    return TRUE;
}

//TODO: Remove?
char *fscc_frame_get_remaining_data(struct fscc_frame *frame)
{
    return_val_if_untrue(frame, 0);

    return frame->buffer + (frame->buffer_size - frame->data_length);
}

void fscc_frame_trim(struct fscc_frame *frame)
{
    return_if_untrue(frame);

    fscc_frame_update_buffer_size(frame, frame->data_length);
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
        TraceEvents(TRACE_LEVEL_ERROR, TRACE_DEVICE, "Not enough memory to update frame buffer size");
        return FALSE;
    }

    memset(new_buffer, 0, size);

    if (frame->buffer) {
        if (frame->data_length) {
            /* Truncate data length if the new buffer size is less than the data length */
            frame->data_length = min(frame->data_length, size);

            /* Copy over the old buffer data to the new buffer */
            memmove(new_buffer, frame->buffer, frame->data_length);
        }

        ExFreePoolWithTag(frame->buffer, 'ataD');
    }

    frame->buffer = new_buffer;
    frame->buffer_size = size;

    return TRUE;
}