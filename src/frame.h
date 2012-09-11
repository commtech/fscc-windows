#ifndef FSCC_FRAME_H
#define FSCC_FRAME_H

//#include "descriptor.h" /* struct fscc_descriptor */
#include <ntddk.h>
#include <wdf.h>

typedef struct fscc_frame {
	LIST_ENTRY list;
    char *data;
    unsigned target_length;
    unsigned current_length;
    unsigned number;
    unsigned dma;

    struct fscc_descriptor *d1;
    struct fscc_descriptor *d2;

    //dma_addr_t data_handle;
    //dma_addr_t d1_handle;
    //dma_addr_t d2_handle;

    struct fscc_port *port;

    /* Used for DMA to signify the frame has been sent but not cleared. */
    unsigned handled;
} FSCC_FRAME;

struct fscc_frame *fscc_frame_new(unsigned target_length, unsigned dma,
                                  struct fscc_port *port);
void fscc_frame_delete(struct fscc_frame *frame);

unsigned fscc_frame_get_target_length(struct fscc_frame *frame);
unsigned fscc_frame_get_current_length(struct fscc_frame *frame);
unsigned fscc_frame_get_missing_length(struct fscc_frame *frame);

void fscc_frame_add_data(struct fscc_frame *frame, const char *data,
                         unsigned length);

void fscc_frame_remove_data(struct fscc_frame *frame, unsigned length);
unsigned fscc_frame_is_empty(struct fscc_frame *frame);
unsigned fscc_frame_is_full(struct fscc_frame *frame);

char *fscc_frame_get_remaining_data(struct fscc_frame *frame);
void fscc_frame_trim(struct fscc_frame *frame);

#endif