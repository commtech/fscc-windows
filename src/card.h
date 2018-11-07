/*
    Copyright (C) 2018  Commtech, Inc.

    This file is part of fscc-windows.

    fscc-windows is free software: you can redistribute it and/or modify it
    under the terms of the GNU General Public License as published bythe Free
    Software Foundation, either version 3 of the License, or (at your option)
    any later version.

    fscc-windows is distributed in the hope that it will be useful, but WITHOUT
    ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
    FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
    more details.

    You should have received a copy of the GNU General Public License along
    with fscc-windows.  If not, see <http://www.gnu.org/licenses/>.

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
};


typedef struct fscc_card {	
    UINT32 device_id;
    struct BAR bar[3];
} FSCC_CARD;

NTSTATUS fscc_card_init(struct fscc_card *card,
            WDFCMRESLIST ResourcesTranslated, WDFDEVICE port_device);
NTSTATUS fscc_card_delete(struct fscc_card *card,
            WDFCMRESLIST ResourcesTranslated);

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

#endif
