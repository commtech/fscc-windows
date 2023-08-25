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


#ifndef FSCC_CARD_H
#define FSCC_CARD_H

#include <ntddk.h>
#include <wdf.h>

#include "defines.h"
#include "trace.h"

#define FCR_OFFSET 0x00
#define DSTAR_OFFSET 0x30

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
