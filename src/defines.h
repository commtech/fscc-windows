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

#pragma once
#include <ntddk.h>
#include <wdf.h>

struct clock_data_fscc {
	unsigned long frequency;
	unsigned char clock_bits[20];
};

struct BAR {
	void* address;
	BOOLEAN memory_mapped;
};

typedef struct fscc_card {
	UINT32 device_id;
	struct BAR bar[3];
} FSCC_CARD;


typedef INT64 fscc_register;
struct fscc_registers {
	/* BAR 0 */
	fscc_register __reserved1[2];

	fscc_register FIFOT;

	fscc_register __reserved2[2];

	fscc_register CMDR;
	fscc_register STAR; /* Read-only */
	fscc_register CCR0;
	fscc_register CCR1;
	fscc_register CCR2;
	fscc_register BGR;
	fscc_register SSR;
	fscc_register SMR;
	fscc_register TSR;
	fscc_register TMR;
	fscc_register RAR;
	fscc_register RAMR;
	fscc_register PPR;
	fscc_register TCR;
	fscc_register VSTR; /* Read-only */

	fscc_register __reserved3[1];

	fscc_register IMR;
	fscc_register DPLLR;

	/* BAR 2 */
	fscc_register FCR;
	fscc_register DMACCR;
	fscc_register __reserved4[4];
	fscc_register DSTAR;
};

struct fscc_memory {
	UINT32 tx_size;
	UINT32 tx_num;
	UINT32 rx_size;
	UINT32 rx_num;
};

typedef struct fscc_port {
	WDFDEVICE device;

	struct fscc_card card;

	unsigned channel;
	struct fscc_registers register_storage; /* Only valid on suspend/resume */
	BOOLEAN append_status;
	BOOLEAN append_timestamp;
	BOOLEAN ignore_timeout;
	BOOLEAN rx_multiple;
	BOOLEAN wait_on_write;
	BOOLEAN blocking_write;
	BOOLEAN force_fifo;
	int tx_modifiers;
	unsigned last_isr_value;
	unsigned open_counter;
	struct fscc_memory memory;

	WDFQUEUE write_queue;
	WDFQUEUE write_queue2; /* TODO: Change name to be more descriptive. */
	WDFQUEUE read_queue;
	WDFQUEUE read_queue2; /* TODO: Change name to be more descriptive. */
	WDFQUEUE ioctl_queue;
	WDFQUEUE isr_queue; /* List of user tracked interrupts */
	WDFQUEUE blocking_request_queue; /* For blocking write requests */

	WDFSPINLOCK board_settings_spinlock; /* Anything that will alter the settings at a board level */
	WDFSPINLOCK board_rx_spinlock; /* Anything that will alter the state of rx at a board level */
	WDFSPINLOCK board_tx_spinlock; /* Anything that will alter the state of rx at a board level */

	WDFDPC oframe_dpc;
	WDFDPC iframe_dpc;
	WDFDPC isr_alert_dpc;
	WDFDPC request_dpc;
	WDFDPC process_read_dpc;
	WDFDPC alls_dpc;
	WDFDPC timestamp_dpc;

	WDFTIMER timer;

	WDFINTERRUPT interrupt;

	BOOLEAN has_dma;
	WDFDMAENABLER dma_enabler;
	struct dma_frame** rx_descriptors;
	unsigned user_rx_desc; // DMA & FIFO, this is where the drivers are working.
	unsigned fifo_rx_desc; // For non-DMA use, this is where the FIFO is currently working.
	int rx_bytes_in_frame; // FIFO, How many bytes are in the current RX frame
	int rx_frame_size; // FIFO, The current RX frame size

	struct dma_frame** tx_descriptors;
	unsigned user_tx_desc; // DMA & FIFO, this is where the drivers are working.
	unsigned fifo_tx_desc; // For non-DMA use, this is where the FIFO is currently working.
	int tx_bytes_in_frame; // FIFO, How many bytes are in the current TX frame
	int tx_frame_size; // FIFO, The current TX frame size
} FSCC_PORT;
WDF_DECLARE_CONTEXT_TYPE(FSCC_PORT);

typedef LARGE_INTEGER fscc_timestamp;

struct fscc_descriptor {
	volatile UINT32 control;
	volatile UINT32 data_address;
	volatile UINT32 data_count;
	volatile UINT32 next_descriptor;
};

typedef struct dma_frame {
	WDFCOMMONBUFFER desc_buffer;
	struct fscc_descriptor* desc;
	UINT32 desc_physical_address;
	WDFCOMMONBUFFER data_buffer;
	unsigned char* buffer;
	fscc_timestamp timestamp;
	UINT32 data_size;
	UINT32 desc_size;
} DMA_FRAME;
