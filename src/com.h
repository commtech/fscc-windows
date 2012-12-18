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

#ifndef COM_PORT_H
#define COM_PORT_H

#include <ntddk.h>
#include <wdf.h>

#include "card.h"

typedef struct com_port {
	WDFDEVICE device;
	
	struct fscc_port *fscc_port;
	unsigned channel;
} COM_PORT;

WDF_DECLARE_CONTEXT_TYPE(COM_PORT);

typedef struct _PDO_EXTENSION 
{
	ULONG flags;							// flags

	PULONG UartAddress;
	PULONG FcrAddress;
	UINT16 DeviceID;
	unsigned Channel;
} PDO_EXTENSION, *PPDO_EXTENSION;

typedef void (*PCOM_GET_FSCC_INFO)(IN WDFDEVICE Device, OUT PDO_EXTENSION *pdo_ext);
typedef BOOLEAN (*PCOM_IS_FSCC_OPEN)(IN WDFDEVICE Device);
typedef void (*PCOM_ENABLE_ASYNC)(IN WDFDEVICE Device);
typedef void (*PCOM_DISABLE_ASYNC)(IN WDFDEVICE Device);

typedef struct _COM_INTERFACE_STANDARD {
    INTERFACE                        InterfaceHeader;
    PCOM_GET_FSCC_INFO    GetFsccInfo;
    PCOM_IS_FSCC_OPEN    IsFsccOpen;
    PCOM_ENABLE_ASYNC    EnableAsync;
    PCOM_DISABLE_ASYNC    DisableAsync;
} COM_INTERFACE_STANDARD, *PCOM_INTERFACE_STANDARD;

NTSTATUS com_port_init(struct fscc_port *port, unsigned channel);

#endif
