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

#include "card.h"
#include "card.tmh"

#include "port.h"
#include "utils.h"
#include "isr.h"
#include "driver.h"

#include <ntddser.h>
#include <ntstrsafe.h>


NTSTATUS fscc_card_init(struct fscc_card *card, WDFCMRESLIST ResourcesTranslated)
{	
	unsigned bar_num = 0;
	unsigned i = 0;

	for (i = 0; i < WdfCmResourceListGetCount(ResourcesTranslated); i++) {
		PCM_PARTIAL_RESOURCE_DESCRIPTOR descriptor;
		
		descriptor = WdfCmResourceListGetDescriptor(ResourcesTranslated, i);
		
		if (!descriptor)
			return STATUS_DEVICE_CONFIGURATION_ERROR;
			
		switch (descriptor->Type) {
		case CmResourceTypePort:
			switch (i) {
			case 0:
				bar_num = 2;
				break;

			case 2:
				bar_num = 0;
				break;
			}

			card->bar[bar_num].address = ULongToPtr(descriptor->u.Port.Start.LowPart);
			card->bar[bar_num].memory_mapped = FALSE;
			break;
		}
	}

	return STATUS_SUCCESS;
}

NTSTATUS fscc_card_delete(struct fscc_card *card, WDFCMRESLIST ResourcesTranslated)
{	
	unsigned bar_counter = 0;
	unsigned i = 0;

    for (i = 0; i < WdfCmResourceListGetCount(ResourcesTranslated); i++) {		
		PCM_PARTIAL_RESOURCE_DESCRIPTOR descriptor;
		
		descriptor = WdfCmResourceListGetDescriptor(ResourcesTranslated, i);
		
		if (!descriptor)
			return STATUS_DEVICE_CONFIGURATION_ERROR;
		
		switch (descriptor->Type) {
		case CmResourceTypePort:
			bar_counter++;
			break;
			
		case CmResourceTypeMemory:
			MmUnmapIoSpace(card->bar[bar_counter].address, descriptor->u.Memory.Length);
			bar_counter++;
			break;
		}
	}

	return STATUS_SUCCESS;
}

void *fscc_card_get_BAR(struct fscc_card *card, unsigned number)
{
	if (number > 2)
		return 0;

	return card->bar[number].address;
}

UINT32 fscc_card_get_register(struct fscc_card *card, unsigned bar,
							  unsigned offset)
{
	void *address = 0;
	UINT32 value = 0;

	address = fscc_card_get_BAR(card, bar);

	value = READ_PORT_ULONG((ULONG *)((char *)address + offset));

	return value;
}

void fscc_card_set_register(struct fscc_card *card, unsigned bar,
							unsigned offset, UINT32 value)
{
	void *address = 0;

	address = fscc_card_get_BAR(card, bar);

	WRITE_PORT_ULONG((ULONG *)((char *)address + offset), value);
}

/* 
    At the card level there is no offset manipulation to get to the second port
    on each card. If you would like to pass in a register offset and get the
    appropriate address on a port basis use the fscc_port_* functions.
*/
void fscc_card_get_register_rep(struct fscc_card *card, unsigned bar,
                                unsigned offset, char *buf,
                                unsigned byte_count)
{
    void *address = 0;
    unsigned leftover_count = 0;
    UINT32 incoming_data = 0;
    unsigned chunks = 0;
	unsigned i = 0;

    return_if_untrue(card);
    return_if_untrue(bar <= 2);
    return_if_untrue(buf);
    return_if_untrue(byte_count > 0);

    address = fscc_card_get_BAR(card, bar);
    leftover_count = byte_count % 4;
    chunks = (byte_count - leftover_count) / 4;

	for (i = 0; i < chunks; i++) {
		UINT32 value = 0;

		value = READ_PORT_ULONG((ULONG *)((char *)address + offset));

		memcpy(&buf[i * 4], &value, sizeof(value));
	}

    if (leftover_count) {
        incoming_data = READ_PORT_ULONG((ULONG *)((char *)address + offset));

        memmove(buf + (byte_count - leftover_count),
                (char *)(&incoming_data), leftover_count);
    }

#ifdef __BIG_ENDIAN
    {
        unsigned i = 0;

        for (i = 0; i < (int)(byte_count / 2); i++) {
            char first, last;

            first = buf[i];
            last = buf[byte_count - i - 1];

            buf[i] = last;
            buf[byte_count - i - 1] = first;
        }
    }
#endif
}

/* 
    At the card level there is no offset manipulation to get to the second port
    on each card. If you would like to pass in a register offset and get the
    appropriate address on a port basis use the fscc_port_* functions.
*/
void fscc_card_set_register_rep(struct fscc_card *card, unsigned bar,
                                unsigned offset, const char *data,
                                unsigned byte_count)
{
    void *address = 0;
    unsigned leftover_count = 0;
    unsigned chunks = 0;
    char *reversed_data = 0;
    const char *outgoing_data = 0;
	unsigned i = 0;

    return_if_untrue(card);
    return_if_untrue(bar <= 2);
    return_if_untrue(data);
    return_if_untrue(byte_count > 0);

    address = fscc_card_get_BAR(card, bar);
    leftover_count = byte_count % 4;
    chunks = (byte_count - leftover_count) / 4;

    outgoing_data = data;

#ifdef __BIG_ENDIAN
    {
        unsigned i = 0;
		
		reversed_data = (char *)ExAllocatePoolWithTag(NonPagedPool, byte_count, 'ataD');

        for (i = 0; i < byte_count; i++)
            reversed_data[i] = data[byte_count - i - 1];

        outgoing_data = reversed_data;
    }
#endif

	for (i = 0; i < chunks; i++)
		WRITE_PORT_ULONG((ULONG *)((char *)address + offset), chars_to_u32(outgoing_data + (i * 4)));

    if (leftover_count)
		WRITE_PORT_ULONG((ULONG *)((char *)address + offset), chars_to_u32(outgoing_data + (byte_count - leftover_count)));

    if (reversed_data)
		ExFreePoolWithTag (reversed_data, 'ataD');
}

char *fscc_card_get_name(struct fscc_card *card)
{
	UNREFERENCED_PARAMETER(card);

	return "TODO2";
	
	/*
	switch (card->pci_dev->device) {
	case FSCC_ID:
		return "FSCC PCI";
	case SFSCC_ID:
		return "SuperFSCC PCI";
	case FSCC_232_ID:
		return "FSCC-232 PCI";
	case SFSCC_4_ID:
		return "SuperFSCC/4 PCI";
	case FSCC_4_ID:
		return "FSCC/4 PCI";
	case SFSCC_4_LVDS_ID:
		return "SuperFSCC/4 LVDS PCI";
	case SFSCCe_4_ID:
		return "SuperFSCC/4 PCIe";
	}

	return "Unknown Device";
	*/
}