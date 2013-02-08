#include "frame.h"
#include "utils.h" /* return_{val_}if_true */
#include "port.h" /* struct fscc_port */

#if defined(EVENT_TRACING)
#include "frame.tmh"
#endif

static unsigned frame_counter = 1;

void fscc_frame_update_buffer_size(struct fscc_frame *frame, unsigned length);
//int fscc_frame_setup_descriptors(struct fscc_frame *frame, struct pci_dev *pci_dev);

struct fscc_frame *fscc_frame_new(unsigned target_length, unsigned dma,
                                  struct fscc_port *port)
{
    struct fscc_frame *frame = 0;
		
	frame = (struct fscc_frame *)ExAllocatePoolWithTag(NonPagedPool, sizeof(*frame), 'marF');

	if (frame == NULL)
		return 0;

    memset(frame, 0, sizeof(*frame));

    frame->dma = dma;
    frame->port = port;

    frame->number = frame_counter;
    frame_counter += 1;

    fscc_frame_update_buffer_size(frame, target_length);

    frame->handled = 0;

    return frame;
}

void fscc_frame_delete(struct fscc_frame *frame)
{
    return_if_untrue(frame);

    if (frame->data)
		ExFreePoolWithTag(frame->data, 'ataD');
	
	ExFreePoolWithTag(frame, 'marF');
}

unsigned fscc_frame_get_target_length(struct fscc_frame *frame)
{
    return_val_if_untrue(frame, 0);

    return frame->target_length;
}

unsigned fscc_frame_get_current_length(struct fscc_frame *frame)
{
    return_val_if_untrue(frame, 0);

    return frame->current_length;
}

unsigned fscc_frame_get_missing_length(struct fscc_frame *frame)
{
    return_val_if_untrue(frame, 0);

    return frame->target_length - frame->current_length;
}

unsigned fscc_frame_is_empty(struct fscc_frame *frame)
{
    return_val_if_untrue(frame, 0);

    return !fscc_frame_get_current_length(frame);
}

unsigned fscc_frame_is_full(struct fscc_frame *frame)
{
    return_val_if_untrue(frame, 0);

    return fscc_frame_get_current_length(frame) == fscc_frame_get_target_length(frame);
}

void fscc_frame_add_data(struct fscc_frame *frame, const char *data,
                         unsigned length)
{
    return_if_untrue(frame);
    return_if_untrue(length > 0);

    if (frame->current_length + length > frame->target_length)
        fscc_frame_update_buffer_size(frame, frame->current_length + length);

    memmove(frame->data + frame->current_length, data, length);
    frame->current_length += length;
}

void fscc_frame_remove_data(struct fscc_frame *frame, unsigned length)
{
    return_if_untrue(frame);

    frame->current_length -= length;
}

char *fscc_frame_get_remaining_data(struct fscc_frame *frame)
{
    return_val_if_untrue(frame, 0);

    return frame->data + (frame->target_length - frame->current_length);
}

void fscc_frame_trim(struct fscc_frame *frame)
{
    return_if_untrue(frame);

    fscc_frame_update_buffer_size(frame, frame->current_length);
}

void fscc_frame_update_buffer_size(struct fscc_frame *frame, unsigned length)
{
    char *new_data = 0;

    return_if_untrue(frame);

    warn_if_untrue(length >= frame->current_length);

    if (length == 0) {
        if (frame->data) {
			ExFreePoolWithTag(frame->data, 'ataD');
            frame->data = 0;
        }

        frame->current_length = 0;
        frame->target_length = 0;

        return;
    }

    if (frame->target_length == length)
        return;
	
	new_data = (char *)ExAllocatePoolWithTag(NonPagedPool, length, 'ataD');

    return_if_untrue(new_data);

    memset(new_data, 0, length);

    if (frame->data) {
        if (frame->current_length)
            memmove(new_data, frame->data, length);
		
		ExFreePoolWithTag(frame->data, 'ataD');
    }

    frame->data = new_data;
    frame->current_length = min(frame->current_length, length);
    frame->target_length = length;
}