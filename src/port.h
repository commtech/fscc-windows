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


#ifndef FSCC_PORT_H
#define FSCC_PORT_H

#include <ntddk.h>
#include <wdf.h>

#include "defines.h"
#include "Trace.h"

#include "card.h"
#include "fscc.h"
#include "io.h"

#define FIFO_OFFSET 0x00
#define BC_FIFO_L_OFFSET 0x04
#define FIFOT_OFFSET 0x08
#define FIFO_BC_OFFSET 0x0C
#define FIFO_FC_OFFSET 0x10
#define CMDR_OFFSET 0x14
#define STAR_OFFSET 0x18
#define CCR0_OFFSET 0x1C
#define CCR1_OFFSET 0x20
#define CCR2_OFFSET 0x24
#define BGR_OFFSET 0x28
#define SSR_OFFSET 0x2C
#define SMR_OFFSET 0x30
#define TSR_OFFSET 0x34
#define TMR_OFFSET 0x38
#define RAR_OFFSET 0x3C
#define RAMR_OFFSET 0x40
#define PPR_OFFSET 0x44
#define TCR_OFFSET 0x48
#define VSTR_OFFSET 0x4C
#define ISR_OFFSET 0x50
#define IMR_OFFSET 0x54
#define DPLLR_OFFSET 0x58
#define MAX_OFFSET 0x58 //must equal the highest FCore register address in the first BAR

#define DMACCR_OFFSET 0x04
#define DMA_RX_BASE_OFFSET 0x0c
#define DMA_TX_BASE_OFFSET 0x10
#define DMA_CURRENT_RX_BASE_OFFSET 0x20
#define DMA_CURRENT_TX_BASE_OFFSET 0x24

#define RFE 0x00000004
#define RFT 0x00000002
#define RFS 0x00000001
#define RFO 0x00000008
#define RDO 0x00000010
#define RFL 0x00000020
#define TIN 0x00000100
#define TDU 0x00040000
#define TFT 0x00010000
#define ALLS 0x00020000
#define CTSS 0x01000000
#define DSRC 0x02000000
#define CDC 0x04000000
#define CTSA 0x08000000
#define DR_STOP 0x00004000
#define DT_STOP 0x00008000
#define DT_FE 0x00002000
#define DR_FE 0x00001000
#define DT_HI 0x00000800
#define DR_HI 0x00000400

#define CE_BIT 0x00040000

struct fscc_port *fscc_port_new(WDFDRIVER Driver, IN PWDFDEVICE_INIT DeviceInit);

UINT32 fscc_port_get_register(struct fscc_port *port, unsigned bar,
unsigned register_offset);

NTSTATUS fscc_port_set_register(struct fscc_port *port, unsigned bar,
unsigned register_offset, UINT32 value);

void fscc_port_get_register_rep(struct fscc_port *port, unsigned bar,
unsigned register_offset, char *buf,
unsigned byte_count);

void fscc_port_set_register_rep(struct fscc_port *port, unsigned bar,
unsigned register_offset, const char *data,
unsigned byte_count);

NTSTATUS fscc_port_set_registers(struct fscc_port *port, const struct fscc_registers *regs);

void fscc_port_get_registers(struct fscc_port *port, struct fscc_registers *regs);

UCHAR fscc_port_get_FREV(struct fscc_port *port);
UCHAR fscc_port_get_PREV(struct fscc_port *port);
UINT16 fscc_port_get_PDEV(struct fscc_port *port);

void fscc_port_set_append_status(struct fscc_port *port, BOOLEAN value);
BOOLEAN fscc_port_get_append_status(struct fscc_port *port);

void fscc_port_set_append_timestamp(struct fscc_port *port, BOOLEAN value);
BOOLEAN fscc_port_get_append_timestamp(struct fscc_port *port);

void fscc_port_set_ignore_timeout(struct fscc_port *port, BOOLEAN ignore_timeout);
BOOLEAN fscc_port_get_ignore_timeout(struct fscc_port *port);

void fscc_port_set_rx_multiple(struct fscc_port *port, BOOLEAN rx_multiple);
BOOLEAN fscc_port_get_rx_multiple(struct fscc_port *port);

void fscc_port_set_wait_on_write(struct fscc_port *port, BOOLEAN wait_on_write);
BOOLEAN fscc_port_get_wait_on_write(struct fscc_port *port);

NTSTATUS fscc_port_set_force_fifo(struct fscc_port *port, BOOLEAN force_fifo);
BOOLEAN fscc_port_get_force_fifo(struct fscc_port *port);

void fscc_port_set_blocking_write(struct fscc_port *port, BOOLEAN blocking);
BOOLEAN fscc_port_get_blocking_write(struct fscc_port *port);

NTSTATUS fscc_port_set_tx_modifiers(struct fscc_port *port, int value);
unsigned fscc_port_get_tx_modifiers(struct fscc_port *port);

void fscc_port_set_clock_bits(struct fscc_port *port, unsigned char *clock_data);

unsigned fscc_port_using_async(struct fscc_port *port);
unsigned fscc_port_timed_out(struct fscc_port *port);
void fscc_port_reset_timer(struct fscc_port *port);

#endif
