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


#include "flist.h"
#include "utils.h" /* return_{val_}if_true */
#include "frame.h"
#include "debug.h"

#if defined(EVENT_TRACING)
#include "flist.tmh"
#endif

// TODO Initialize flist->next_to_process when a head is added, and clear it when the list is empty?
void fscc_flist_init(struct fscc_flist *flist)
{
    flist->estimated_memory_usage = 0;
    flist->next_to_process = 0;
    InitializeListHead(&flist->frames);
}

void fscc_flist_delete(struct fscc_flist *flist)
{
    return_if_untrue(flist);

    fscc_flist_clear(flist);
}

void fscc_flist_add_frame(struct fscc_flist *flist, struct fscc_frame *frame)
{
    InsertTailList(&flist->frames, &frame->list);
    if(flist->next_to_process == 0) flist->next_to_process = frame->list.Flink;

    flist->estimated_memory_usage += fscc_frame_get_buffer_size(frame);
}

struct fscc_frame *fscc_flist_peak_front(struct fscc_flist *flist)
{
    if (IsListEmpty(&flist->frames))
        return 0;

    return CONTAINING_RECORD(flist->frames.Flink, FSCC_FRAME, list);
}

struct fscc_frame *fscc_flist_remove_frame(struct fscc_flist *flist)
{
    struct fscc_frame *frame = 0;

    if (IsListEmpty(&flist->frames))
        return 0;
    
    frame = CONTAINING_RECORD(flist->frames.Flink, FSCC_FRAME, list);
    if(flist->next_to_process == &frame->list) flist->next_to_process = frame->list.Flink;

    RemoveHeadList(&flist->frames);
    if(IsListEmpty(&flist->frames)) flist->next_to_process = 0;

    flist->estimated_memory_usage -= fscc_frame_get_buffer_size(frame);

    return frame;
}

struct fscc_frame *fscc_flist_remove_frame_if_lte(struct fscc_flist *flist,
                                                  unsigned size)
{
    struct fscc_frame *frame = 0;
    unsigned frame_length = 0;

    if (IsListEmpty(&flist->frames))
        return 0;

    frame = CONTAINING_RECORD(flist->frames.Flink, FSCC_FRAME, list);

    frame_length = fscc_frame_get_buffer_size(frame);

    if (frame_length > size)
        return 0;

    if(flist->next_to_process == &frame->list) flist->next_to_process = frame->list.Flink;
    RemoveHeadList(&flist->frames);
    if(IsListEmpty(&flist->frames)) flist->next_to_process = 0;

    flist->estimated_memory_usage -= frame_length;

    return frame;
}

void fscc_flist_clear(struct fscc_flist *flist)
{
    while (!IsListEmpty(&flist->frames)) {
        LIST_ENTRY *frame_iter = 0;
        struct fscc_frame *frame = 0;

        frame_iter = RemoveHeadList(&flist->frames);
        frame = CONTAINING_RECORD(frame_iter, FSCC_FRAME, list);

        fscc_frame_delete(frame);
    }
    flist->next_to_process = 0;
    flist->estimated_memory_usage = 0;
}

BOOLEAN fscc_flist_is_empty(struct fscc_flist *flist)
{
    return IsListEmpty(&flist->frames);
}

unsigned fscc_flist_calculate_memory_usage(struct fscc_flist *flist)
{
    LIST_ENTRY *frame_iter = 0;
    unsigned memory = 0;

    frame_iter = flist->frames.Flink;
    while (frame_iter != flist->frames.Blink) {
        struct fscc_frame *current_frame = 0;

        current_frame = CONTAINING_RECORD(frame_iter, FSCC_FRAME, list);
        memory += fscc_frame_get_buffer_size(current_frame);

        frame_iter = frame_iter->Flink;
    }

    return memory;
}


// fscc_flist_next_frame_size, fscc_flist_remove_frame_data, and fscc_flist_remove_stream_data 
// are written exclusively for the RX DMA operation.
// I've also implemented a 'next_to_process' member which tracks the last frame
// in the flist that was processed. I don't know the best practice for handling
// this.
// Unfortunately, we have to pre-allocate frames for RX DMA, which goes against our
// current design of allocating as we need when using just the FIFOs. I've tried not 
// to break compatibility but still offer good throughput.

int fscc_flist_next_frame_size(struct fscc_flist *flist)
{
    LIST_ENTRY *frame_iter = 0;
    LIST_ENTRY *frame_start = 0;
    
    if (IsListEmpty(&flist->frames)) return 0;
    
    frame_iter = frame_start = flist->next_to_process;
    
    do {
        struct fscc_frame *current_frame = 0;
        
        current_frame = CONTAINING_RECORD(frame_iter, FSCC_FRAME, list);
        // If 0x40000000 isn't set, this descriptor isn't finished, and therefore we return 0
        if((current_frame->desc->control&0x40000000)==0x40000000) return 0;
        // If 0x80000000 is set, this descriptor is the end of a frame, and we return the size.
        if((current_frame->desc->control&0x80000000)==0x80000000) return current_frame->desc->control&DMA_MAX_LENGTH;
        // Move on.
        frame_iter = frame_iter->Flink;
    } 
    while(frame_iter != frame_start);
    
    // We're back to the starting frame and no FE was found.
    return 0;
}

unsigned fscc_flist_remove_frame_data(struct fscc_flist *flist, char *destination, unsigned length)
{
    LIST_ENTRY *frame_start = 0;
    unsigned frame_size = 0, data_moved = 0;
    
    return_val_if_untrue(flist, 0);
    if (length==0) return 0;
    
    // If there's no finished frame, or our buffer is smaller than the frame, return 0.
    frame_size = fscc_flist_next_frame_size(flist);
    if (frame_size==0) return 0;
    if (length < frame_size) return 0;
    
    // Memorize where we started so we don't infinitely loop.
    frame_start = flist->next_to_process;
    do {
        struct fscc_frame *current_frame = 0;
        
        // Get the frame associated with the list entry we're processing.
        current_frame = CONTAINING_RECORD(flist->next_to_process, FSCC_FRAME, list);

        // We can move the whole descriptor because we've verified we have
        // room for the whole frame. remove_data already verifies the destination
        fscc_frame_remove_data(current_frame, (destination + data_moved), current_frame->desc->data_count);
        data_moved += current_frame->desc->data_count;
            
        // We check for a frame end, and either way we reset the descriptor so the 
        // DMA controller can use it again. Then we mark the next descriptor as to
        // be processed, and either return the amount of data moved or keep processing.
        if(current_frame->desc->control&0x80000000)
        {
            current_frame->desc->control = 0;
            current_frame->desc->data_count = 0;
            flist->next_to_process = flist->next_to_process->Flink;
            return data_moved;
        }
        else 
        {
            current_frame->desc->control = 0;
            current_frame->desc->data_count = 0;
            flist->next_to_process = flist->next_to_process->Flink;
        }
    }
    while(flist->next_to_process != frame_start);
    // This might be an error condition - we've circled back around and the frame isn't finished.
    return data_moved;
}

int fscc_flist_remove_stream_data(struct fscc_flist *flist, char *destination, unsigned length)
{
    LIST_ENTRY *frame_start = 0;
    int data_moved = 0, data_to_move = 0;
    
    return_val_if_untrue(flist, 0);
    if(length==0) return 0;
    
    // Memorize where we started so we don't infinitely loop.
    frame_start = flist->next_to_process;
    do {
        struct fscc_frame *current_frame = 0;
        
        // Get the frame associated with the list entry we're processing.
        current_frame = CONTAINING_RECORD(flist->next_to_process, FSCC_FRAME, list);
        
        // If 0x40000000 isn't in control, this desc isn't finished being used
        if((current_frame->desc->control&0x40000000)==0) return data_moved;
        // If we've already moved all we plannyed to move, we're done.
        if(data_moved == length) return data_moved;
        
        // We move either the full descriptor, or the remaining 'length'.
        data_to_move = current_frame->desc->data_count > (length-data_moved) ? (length-data_moved) : current_frame->desc->data_count;
        fscc_frame_remove_data(current_frame, (destination + data_moved), data_to_move);
        data_moved += data_to_move;
        
        // We reset the descriptor so the DMA controller can use it again, then we
        // mark the next descriptor as to be processed.
        if(data_to_move == current_frame->desc->data_count) current_frame->desc->control = 0;
        current_frame->desc->data_count = 0;
        
        flist->next_to_process = flist->next_to_process->Flink;
    }
    while(flist->next_to_process != frame_start);
        
    // This might be an error condition - we've circled back around somehow.
    // This might also be valid. idk. Needs more thinking.
    return data_moved;
}