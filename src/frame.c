#include "frame.h"
#include "utils.h" /* return_{val_}if_true */
#include "port.h" /* struct fscc_port */

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

#if 0
    if (frame->dma) {
        /* Switch to FIFO based transmission as a fall back. */
        if (!fscc_frame_setup_descriptors(frame, port->card->pci_dev))
            frame->dma = 0;
    }
#endif

    fscc_frame_update_buffer_size(frame, target_length);

    frame->handled = 0;

    return frame;
}

//Returns 0 on failure. 1 on success
#if 0
int fscc_frame_setup_descriptors(struct fscc_frame *frame,
                                 struct pci_dev *pci_dev)
{
	//TODO, DMA modifier?
	frame->d1 = (struct fscc_frame *)ExAllocatePoolWithTag(NonPagedPool, sizeof(*frame->d1), 'marF');

	if (frame->d1 == NULL)
		return 0;

    frame->d2 = kmalloc(sizeof(*frame->d2), GFP_ATOMIC | GFP_DMA);

    if (!frame->d2) {
		ExFreePoolWithTag(frame->d1, 'marF'); 
        return 0;
    }

    memset(frame->d1, 0, sizeof(*frame->d1));
    memset(frame->d2, 0, sizeof(*frame->d2));

    frame->d1_handle = pci_map_single(pci_dev, frame->d1, sizeof(*frame->d1),
                                      DMA_TO_DEVICE);

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 27)
    if (dma_mapping_error(&pci_dev->dev, frame->d1_handle)) {
#else
    if (dma_mapping_error(frame->d1_handle)) {
#endif
        dev_err(frame->port->device, "dma_mapping_error failed\n");

        kfree(frame->d1);
        kfree(frame->d2);

        frame->d1 = 0;
        frame->d2 = 0;

        return 0;
    }

    frame->d2_handle = pci_map_single(pci_dev, frame->d2, sizeof(*frame->d2),
                                      DMA_TO_DEVICE);

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 27)
    if (dma_mapping_error(&pci_dev->dev, frame->d2_handle)) {
#else
    if (dma_mapping_error(frame->d2_handle)) {
#endif
        dev_err(frame->port->device, "dma_mapping_error failed\n");

        pci_unmap_single(frame->port->card->pci_dev, frame->d1_handle,
                         sizeof(*frame->d1), DMA_TO_DEVICE);

        kfree(frame->d1);
        kfree(frame->d2);

        frame->d1 = 0;
        frame->d2 = 0;

        return 0;
    }

    frame->d2->control = 0x40000000;
    frame->d1->next_descriptor = cpu_to_le32(frame->d2_handle);

    return 1;
}
#endif

void fscc_frame_delete(struct fscc_frame *frame)
{
    return_if_untrue(frame);

#if 0
    if (frame->dma) {
        pci_unmap_single(frame->port->card->pci_dev, frame->d1_handle,
                         sizeof(*frame->d1), DMA_TO_DEVICE);

        pci_unmap_single(frame->port->card->pci_dev, frame->d2_handle,
                         sizeof(*frame->d2), DMA_TO_DEVICE);

        if (frame->data_handle && frame->current_length) {
            pci_unmap_single(frame->port->card->pci_dev, frame->data_handle,
                             frame->current_length, DMA_TO_DEVICE);
        }

        if (frame->d1)
            kfree(frame->d1);

        if (frame->d2)
            kfree(frame->d2);
    }
#endif

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

#if 0
    if (frame->dma && frame->data) {
        pci_unmap_single(frame->port->card->pci_dev, frame->data_handle,
                         frame->current_length, DMA_TO_DEVICE);
    }
#endif

    if (frame->current_length + length > frame->target_length)
        fscc_frame_update_buffer_size(frame, frame->current_length + length);

    memmove(frame->data + frame->current_length, data, length);
    frame->current_length += length;

#if 0
    if (frame->dma && frame->data) {
        frame->data_handle = pci_map_single(frame->port->card->pci_dev,
                                            frame->data,
                                            frame->current_length,
                                            DMA_TO_DEVICE);

        frame->d1->control = 0xA0000000 | frame->current_length;
        frame->d1->data_address = cpu_to_le32(frame->data_handle);
        frame->d1->data_count = frame->current_length;
    }
#endif
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