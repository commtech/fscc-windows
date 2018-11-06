/*
    Copyright (C) 2016  Commtech, Inc.

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


#include "card.h"
#include "port.h"
#include "utils.h"
#include "isr.h"
#include "driver.h"

#include <ntddser.h>
#include <ntstrsafe.h>

#if defined(EVENT_TRACING)
#include "card.tmh"
#endif

INT
PCIReadConfigWord(
    IN PDEVICE_OBJECT DeviceObject,
    IN ULONG          Offset,
    IN PVOID          Value
    );

NTSTATUS fscc_card_init(struct fscc_card *card,
                        WDFCMRESLIST ResourcesTranslated,
                        WDFDEVICE port_device)
{	
    unsigned bar_num = 0;
    unsigned i = 0;
    PDEVICE_OBJECT pdo;

    card->device_id = 0;

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

            card->bar[bar_num].address =
                ULongToPtr(descriptor->u.Port.Start.LowPart);

            card->bar[bar_num].memory_mapped = FALSE;
            break;

        case CmResourceTypeMemory:
            switch (i) {
            case 0:
                bar_num = 2;
                break;

            case 2:
                bar_num = 0;
                break;
            }

            card->bar[bar_num].address =
                MmMapIoSpace(descriptor->u.Memory.Start,
                             descriptor->u.Memory.Length,
                             MmNonCached);

            card->bar[bar_num].memory_mapped = TRUE;
            break;
        }
    }

    pdo = WdfDeviceWdmGetDeviceObject(port_device);

    PCIReadConfigWord(pdo, 0x02, &card->device_id);

    return STATUS_SUCCESS;
}

NTSTATUS fscc_card_delete(struct fscc_card *card,
                          WDFCMRESLIST ResourcesTranslated)
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
            MmUnmapIoSpace(card->bar[bar_counter].address,
                descriptor->u.Memory.Length);

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

UINT32 io_read(struct fscc_card *card, unsigned bar, PULONG Address)
{
    if (card->bar[bar].memory_mapped)
        return READ_REGISTER_ULONG(Address);
    else
        return READ_PORT_ULONG(Address);
}

void io_write(struct fscc_card *card, unsigned bar, PULONG Address,
              ULONG Value)
{
    if (card->bar[bar].memory_mapped)
        WRITE_REGISTER_ULONG(Address, Value);
    else
        WRITE_PORT_ULONG(Address, Value);
}

UINT32 fscc_card_get_register(struct fscc_card *card, unsigned bar,
                              unsigned offset)
{
    void *address = 0;

    address = fscc_card_get_BAR(card, bar);

    return io_read(card, bar, (ULONG *)((char *)address + offset));
}

void fscc_card_set_register(struct fscc_card *card, unsigned bar,
                            unsigned offset, UINT32 value)
{
    void *address = 0;

    address = fscc_card_get_BAR(card, bar);

    io_write(card, bar, (ULONG *)((char *)address + offset), value);
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

        value = io_read(card, bar, (ULONG *)((char *)address + offset));

        memcpy(&buf[i * 4], &value, sizeof(value));
    }

    if (leftover_count) {
        incoming_data = io_read(card, bar, (ULONG *)((char *)address + offset));

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

        reversed_data = (char *)ExAllocatePoolWithTag(NonPagedPool, byte_count,
                                                      'ataD');

        for (i = 0; i < byte_count; i++)
            reversed_data[i] = data[byte_count - i - 1];

        outgoing_data = reversed_data;
    }
#endif

    for (i = 0; i < chunks; i++)
        io_write(card, bar, (ULONG *)((char *)address + offset),
                 chars_to_u32(outgoing_data + (i * 4)));

    if (leftover_count)
        io_write(card, bar, (ULONG *)((char *)address + offset),
                 chars_to_u32(outgoing_data + (byte_count - leftover_count)));

    if (reversed_data)
        ExFreePoolWithTag (reversed_data, 'ataD');
}

char *fscc_card_get_name(struct fscc_card *card)
{
    switch (card->device_id) {
    case FSCC_ID:
    case FSCC_UA_ID:
        return "FSCC PCI";
    case SFSCC_ID:
    case SFSCC_UA_ID:
        return "SuperFSCC PCI";
    case SFSCC_104_LVDS_ID:
        return "SuperFSCC-104-LVDS PC/104+";
    case SFSCC_104_UA_ID:
        return "SuperFSCC-104 PC/104+";
    case FSCC_232_ID:
        return "FSCC-232 PCI";
    case SFSCC_4_UA_ID:
        return "SuperFSCC/4 PCI";
    case FSCC_4_UA_ID:
        return "FSCC/4 PCI";
    case SFSCC_4_LVDS_ID:
    case SFSCC_4_UA_LVDS_ID:
        return "SuperFSCC/4-LVDS PCI";
    case SFSCC_LVDS_ID:
    case SFSCC_UA_LVDS_ID:
        return "SuperFSCC-LVDS PCI";
    case SFSCCe_4_ID:
        return "SuperFSCC/4 PCIe";
    case SFSCC_4_CPCI_ID:
    case SFSCC_4_UA_CPCI_ID:
        return "SuperFSCC/4 cPCI";
    case FSCCe_4_UA_ID:
        return "FSCC/4 PCIe";
    case SFSCCe_4_LVDS_UA_ID:
        return "SuperFSCC/4-LVDS PCIe";
    }

    return "Unknown Device";
}


/*****************************************************************************
 * Direct R/W from config space.
 *****************************************************************************/
INT
PCIReadConfigWord(
    IN PDEVICE_OBJECT DeviceObject,
    IN ULONG          Offset,
    IN PVOID          Value
    )
{
    PDEVICE_OBJECT TargetObject;
    PIRP pIrp;
    IO_STATUS_BLOCK IoStatusBlock;
    PIO_STACK_LOCATION IrpStack;
    KEVENT ConfigReadWordEvent;
    INT error = 0;

    TargetObject = IoGetAttachedDeviceReference(DeviceObject);
    KeInitializeEvent(&ConfigReadWordEvent, NotificationEvent, FALSE);

    pIrp = IoBuildSynchronousFsdRequest(IRP_MJ_PNP, TargetObject, NULL,
        0, NULL, &ConfigReadWordEvent, &IoStatusBlock );

    if (pIrp) {
        /* Create the config space read IRP */
        IrpStack = IoGetNextIrpStackLocation(pIrp);
        IrpStack->MinorFunction = IRP_MN_READ_CONFIG;
        IrpStack->Parameters.ReadWriteConfig.WhichSpace = \
            PCI_WHICHSPACE_CONFIG;
        IrpStack->Parameters.ReadWriteConfig.Offset = Offset;
        IrpStack->Parameters.ReadWriteConfig.Length = 0x2;
        IrpStack->Parameters.ReadWriteConfig.Buffer = Value;
        pIrp->IoStatus.Status = STATUS_NOT_SUPPORTED ;

        /* Send the IRP */
        if (IoCallDriver(TargetObject, pIrp)==STATUS_PENDING) {
            KeWaitForSingleObject(&ConfigReadWordEvent, Executive, \
                KernelMode, FALSE, NULL);
        }
    } else {
        error = -1;
    }

    ObDereferenceObject(TargetObject);
    return error;
}