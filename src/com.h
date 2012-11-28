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
} COM_PORT;

WDF_DECLARE_CONTEXT_TYPE(COM_PORT);

typedef struct _PDO_EXTENSION 
{
	ULONG flags;							// flags
	PDEVICE_OBJECT DeviceObject;
	PDEVICE_OBJECT Fdo;

	PULONG portbase;
	PULONG amccbase;
	PULONG control;
	ULONG vector;
	ULONG bus;
	ULONG instance;
	ULONG irql;
	KAFFINITY affin;
	ULONG irql_raw;
	KAFFINITY affin_raw;
	ULONG vector_raw;

	ULONG boardtype;
	ULONG devicenum;
	ULONG location;
	INTERFACE_TYPE  InterfaceType;
	ULONG BusNumber;
	ULONG itype;
	ULONG devid;
	PKSPIN_LOCK pboardlock;
} PDO_EXTENSION, *PPDO_EXTENSION;

typedef BOOLEAN (*PCOM_GET_MEMORY_REGION)(IN WDFDEVICE Device, OUT PDO_EXTENSION *pdo_ext);

typedef struct _COM_INTERFACE_STANDARD {
    INTERFACE                        InterfaceHeader;
    PCOM_GET_MEMORY_REGION    GetMemoryRegion;
} COM_INTERFACE_STANDARD, *PCOM_INTERFACE_STANDARD;

NTSTATUS com_port_init(struct fscc_port *port, unsigned channel);

#endif
