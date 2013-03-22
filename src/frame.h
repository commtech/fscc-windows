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
    WDFSPINLOCK spinlock;
} FSCC_FRAME;

struct fscc_frame *fscc_frame_new(unsigned dma);
void fscc_frame_delete(struct fscc_frame *frame);

unsigned fscc_frame_get_length(struct fscc_frame *frame);
unsigned fscc_frame_get_buffer_size(struct fscc_frame *frame);

int fscc_frame_add_data(struct fscc_frame *frame, const char *data,
                         unsigned length);

int fscc_frame_remove_data(struct fscc_frame *frame, char *destination, unsigned length);
unsigned fscc_frame_is_empty(struct fscc_frame *frame);
void fscc_frame_clear(struct fscc_frame *frame);
void fscc_frame_trim(struct fscc_frame *frame);

#endif