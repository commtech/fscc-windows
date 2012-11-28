/*
	Copyright (C) 2010  Commtech, Inc.
	
	This file is part of fscc-windows.

	fscc-windows is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.

	fscc-windows is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with fscc-windows.  If not, see <http://www.gnu.org/licenses/>.
	
*/

#ifndef FSCC_PORT_H
#define FSCC_PORT_H

#include <ntddk.h>
#include <wdf.h>

#include "trace.h"

#include "card.h"
#include "fscc.h"

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
#define MAX_OFFSET 0x58 //must equal the highest FCore register address

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

typedef struct fscc_port {
	WDFDEVICE device;
	
	struct fscc_card *card;	

	WDFQUEUE write_queue;
	WDFQUEUE read_queue;
	WDFQUEUE ioctl_queue;
	
	WDFQUEUE read_queue2; /* TODO: Change name to be more descriptive. */

	unsigned channel;
	struct fscc_registers register_storage; /* Only valid on suspend/resume */

    unsigned append_status;
    unsigned ignore_timeout;
    int tx_modifiers;

    struct fscc_memory_cap memory_cap;

    WDFSPINLOCK oframe_spinlock;
    WDFSPINLOCK iframe_spinlock;

    LIST_ENTRY iframes; /* Frames already retrieved from the FIFO */
    LIST_ENTRY oframes; /* Frames not yet in the FIFO yet */

    struct fscc_stream *istream; /* Transparent stream */
	
    struct fscc_frame *pending_iframe; /* Frame retrieving from the FIFO */
    struct fscc_frame *pending_oframe; /* Frame being put in the FIFO */

    WDFDPC oframe_dpc;
    WDFDPC iframe_dpc;
    WDFDPC istream_dpc;

    WDFDPC user_read_dpc;

    unsigned last_isr_value;

	WDFTIMER timer;
	
} FSCC_PORT;

WDF_DECLARE_CONTEXT_TYPE(FSCC_PORT);

struct fscc_port *fscc_port_new(struct fscc_card *card, unsigned channel);
NTSTATUS fscc_port_prepare_hardware(struct fscc_port *port);
NTSTATUS fscc_port_release_hardware(struct fscc_port *port);

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
						   
NTSTATUS fscc_port_set_registers(struct fscc_port *port,
							const struct fscc_registers *regs);
							
void fscc_port_get_registers(struct fscc_port *port,
							 struct fscc_registers *regs);
						   
UCHAR fscc_port_get_FREV(struct fscc_port *port);
UCHAR fscc_port_get_PREV(struct fscc_port *port);
UINT16 fscc_port_get_PDEV(struct fscc_port *port);

NTSTATUS fscc_port_purge_tx(struct fscc_port *port);
NTSTATUS fscc_port_purge_rx(struct fscc_port *port);

void fscc_port_set_append_status(struct fscc_port *port, unsigned value);
unsigned fscc_port_get_append_status(struct fscc_port *port);

void fscc_port_set_ignore_timeout(struct fscc_port *port,
                                  unsigned ignore_timeout);
unsigned fscc_port_get_ignore_timeout(struct fscc_port *port);
NTSTATUS fscc_port_set_tx_modifiers(struct fscc_port *port, int value);
unsigned fscc_port_get_tx_modifiers(struct fscc_port *port);

unsigned fscc_port_get_input_memory_cap(struct fscc_port *port);
unsigned fscc_port_get_output_memory_cap(struct fscc_port *port);

void fscc_port_set_memory_cap(struct fscc_port *port,
                              struct fscc_memory_cap *memory_cap);

void fscc_port_set_clock_bits(struct fscc_port *port,
                              unsigned char *clock_data);

NTSTATUS fscc_port_execute_RRES(struct fscc_port *port);
NTSTATUS fscc_port_execute_TRES(struct fscc_port *port);

unsigned fscc_port_using_async(struct fscc_port *port);

unsigned fscc_port_get_output_memory_usage(struct fscc_port *port,
                                           unsigned lock);
struct fscc_frame *fscc_port_peek_front_frame(struct fscc_port *port,
                                              LIST_ENTRY *frames);

BOOLEAN fscc_port_has_iframes(struct fscc_port *port, unsigned lock);
BOOLEAN fscc_port_has_oframes(struct fscc_port *port, unsigned lock);

unsigned fscc_port_has_dma(struct fscc_port *port);

UINT32 fscc_port_get_TXCNT(struct fscc_port *port);

void fscc_port_execute_transmit(struct fscc_port *port);
unsigned fscc_port_is_streaming(struct fscc_port *port);

unsigned fscc_port_get_RFCNT(struct fscc_port *port);
UINT32 fscc_port_get_RXCNT(struct fscc_port *port);

unsigned fscc_port_get_input_memory_usage(struct fscc_port *port,
                                          unsigned lock);
void fscc_port_reset_timer(struct fscc_port *port);
unsigned fscc_port_has_incoming_data(struct fscc_port *port);

#endif
