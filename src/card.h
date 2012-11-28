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

#ifndef FSCC_CARD_H
#define FSCC_CARD_H

#include <ntddk.h>
#include <wdf.h>

#include "trace.h"

#define FCR_OFFSET 0x00
#define DSTAR_OFFSET 0x30

struct BAR {
	void *address;
	BOOLEAN memory_mapped;
	PCM_PARTIAL_RESOURCE_DESCRIPTOR raw_descriptor;
	PCM_PARTIAL_RESOURCE_DESCRIPTOR tr_descriptor;
};

typedef struct fscc_card {
	WDFDRIVER driver;
	WDFDEVICE device;
	
	struct BAR bar[3];
	
	WCHAR name_buffer[200];
	UNICODE_STRING name;

    unsigned dma;

	WDFINTERRUPT interrupt;
	PCM_PARTIAL_RESOURCE_DESCRIPTOR interrupt_tr_descriptor;
	PCM_PARTIAL_RESOURCE_DESCRIPTOR interrupt_raw_descriptor;

	struct fscc_port *ports[2];
} FSCC_CARD;

WDF_DECLARE_CONTEXT_TYPE(FSCC_CARD);

struct fscc_card *fscc_card_new(WDFDRIVER Driver, IN PWDFDEVICE_INIT DeviceInit);

void *fscc_card_get_BAR(struct fscc_card *card, unsigned number);

UINT32 fscc_card_get_register(struct fscc_card *card, unsigned bar,
							  unsigned offset);

void fscc_card_set_register(struct fscc_card *card, unsigned bar,
							unsigned offset, UINT32 value);

void fscc_card_get_register_rep(struct fscc_card *card, unsigned bar,
                                unsigned offset, char *buf,
                                unsigned byte_count);

void fscc_card_set_register_rep(struct fscc_card *card, unsigned bar,
                                unsigned offset, const char *data,
                                unsigned byte_count);

char *fscc_card_get_name(struct fscc_card *card);

NTSTATUS fscc_card_registry_open(struct fscc_card *card, WDFKEY *key);

#endif
