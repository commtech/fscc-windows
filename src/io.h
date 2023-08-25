/*
Copyright 2023 Commtech, Inc.

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


#ifndef FSCC_IO
#define FSCC_IO
#include "fscc.h"
#include "port.h"
#include "defines.h"

#define DESC_FE_BIT 0x80000000
#define DESC_CSTOP_BIT 0x40000000
#define DESC_HI_BIT 0x20000000
#define DMA_MAX_LENGTH 0x1fffffff


EVT_WDF_DPC FsccProcessRead;
EVT_WDF_IO_QUEUE_IO_WRITE FsccEvtIoWrite;
EVT_WDF_IO_QUEUE_IO_READ FsccEvtIoRead;

BOOLEAN fscc_port_uses_dma(struct fscc_port *port);
unsigned fscc_io_is_streaming(struct fscc_port *port);
unsigned fscc_io_has_incoming_data(struct fscc_port *port);
unsigned fscc_io_transmit_frame(struct fscc_port *port);
void fscc_io_execute_transmit(struct fscc_port *port, unsigned dma);

NTSTATUS fscc_io_initialize(struct fscc_port *port);
NTSTATUS fscc_io_create_rx(struct fscc_port *port, UINT32 number_of_buffers, UINT32 size_of_buffers);
NTSTATUS fscc_io_create_tx(struct fscc_port *port, UINT32 number_of_buffers, UINT32 size_of_buffers);
void fscc_io_destroy_rx(struct fscc_port *port);
void fscc_io_destroy_tx(struct fscc_port *port);
NTSTATUS fscc_io_purge_tx(struct fscc_port *port);
NTSTATUS fscc_io_purge_rx(struct fscc_port *port);

NTSTATUS fscc_io_execute_RRES(struct fscc_port *port);
NTSTATUS fscc_io_execute_TRES(struct fscc_port *port);
unsigned fscc_io_get_RFCNT(struct fscc_port *port);
unsigned fscc_io_get_TFCNT(struct fscc_port *port);
unsigned fscc_io_get_RXCNT(struct fscc_port *port);
unsigned fscc_io_get_TXCNT(struct fscc_port *port);
NTSTATUS fscc_dma_execute_RST_T(struct fscc_port *port);
NTSTATUS fscc_dma_execute_RST_R(struct fscc_port *port);
NTSTATUS fscc_dma_execute_STOP_T(struct fscc_port *port);
NTSTATUS fscc_dma_execute_STOP_R(struct fscc_port *port);
NTSTATUS fscc_dma_execute_GO_T(struct fscc_port *port);
NTSTATUS fscc_dma_execute_GO_R(struct fscc_port *port);
NTSTATUS fscc_dma_port_enable(struct fscc_port *port);
NTSTATUS fscc_dma_port_disable(struct fscc_port *port);
BOOLEAN fscc_dma_is_tx_running(struct fscc_port *port);
BOOLEAN fscc_dma_is_rx_running(struct fscc_port *port);

void fscc_dma_apply_timestamps(struct fscc_port *port);
size_t fscc_user_get_tx_space(struct fscc_port *port);
int fscc_fifo_read_data(struct fscc_port *port);
int fscc_fifo_write_data(struct fscc_port *port);
int fscc_user_read_stream(struct fscc_port *port, char *buf, UINT32 buf_length, UINT32*out_length);
int fscc_user_read_frame(struct fscc_port *port, char *buf, UINT32 buf_length, UINT32*out_length);
int fscc_user_write_frame(struct fscc_port *port, char *buf, UINT32 data_length, UINT32*out_length);
unsigned fscc_user_next_read_size(struct fscc_port *port, UINT32*bytes);
#endif