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


#ifndef FSCC_DESCRIPTOR
#define FSCC_DESCRIPTOR

#define DESC_FE_BIT 0x80000000
#define DESC_CSTOP_BIT 0x40000000
#define DESC_HI_BIT 0x20000000
#define DMA_MAX_LENGTH 0x1fffffff

struct fscc_descriptor {
    volatile UINT32 control;
    volatile UINT32 data_address;
    volatile UINT32 data_count;
    volatile UINT32 next_descriptor;
};

typedef struct dma_frame {
    WDFCOMMONBUFFER desc_buffer;
    struct fscc_descriptor *desc;
    UINT32 desc_physical_address;
    WDFCOMMONBUFFER data_buffer;
    unsigned char *buffer;
} DMA_FRAME;

NTSTATUS fscc_dma_initialize(struct fscc_port *port);
NTSTATUS fscc_dma_rebuild_rx(struct fscc_port *port);
NTSTATUS fscc_dma_rebuild_tx(struct fscc_port *port);
NTSTATUS fscc_dma_build_rx(struct fscc_port *port);
NTSTATUS fscc_dma_build_tx(struct fscc_port *port);
NTSTATUS fscc_dma_add_write_data(struct fscc_port *port, char *data_buffer, unsigned length, size_t *out_length);
int fscc_dma_get_stream_data(struct fscc_port *port, char *data_buffer, size_t buffer_size, size_t *out_length);
int fscc_dma_get_frame_data(struct fscc_port *port, char *data_buffer, size_t buffer_size, size_t *out_length);
void fscc_dma_destroy_rx(struct fscc_port *port);
void fscc_dma_destroy_tx(struct fscc_port *port);
NTSTATUS fscc_dma_execute_GO_T(struct fscc_port *port);
NTSTATUS fscc_dma_execute_GO_R(struct fscc_port *port);
NTSTATUS fscc_dma_execute_RSTT(struct fscc_port *port);
NTSTATUS fscc_dma_execute_RSTR(struct fscc_port *port);
int fscc_dma_rx_data_waiting(port);
NTSTATUS fscc_dma_port_enable(struct fscc_port *port);
NTSTATUS fscc_dma_port_disable(struct fscc_port *port);
void fscc_peek_tx_desc(struct fscc_port *port, unsigned num);
void fscc_peek_rx_desc(struct fscc_port *port, unsigned num);
void fscc_peek_desc(struct dma_frame *frame);
void fscc_dma_current_regs(struct fscc_port *port);

#endif