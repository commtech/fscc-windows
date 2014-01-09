/*
    Copyright (C) 2014  Commtech, Inc.

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


//
// Define an Interface Guid so that app can find the device and talk to it.
//

#include <initguid.h>

// {0b99e9a4-1460-4856-9c13-72fca9fa92a3}
DEFINE_GUID (GUID_DEVINTERFACE_FSCC,
    0x0b99e9a4,0x1460,0x4856,0x9c,0x13,0x72,0xfc,0xa9,0xfa,0x92,0xa3);

// {4d36e878-e325-11ce-bfc1-08002be11319}
DEFINE_GUID (GUID_DEVCLASS_FSCC,
    0x4d36e878,0xe325,0x11ce,0xbf,0xc1,0x08,0x00,0x2b,0xe1,0x13,0x19);

// {0b99e9a4-1460-4856-9c13-72fca9fa92a5}
DEFINE_GUID (GUID_WMI_FSCC,
    0x0b99e9a4,0x1460,0x4856,0x9c,0x13,0x72,0xfc,0xa9,0xfa,0x92,0xa5);

DEFINE_GUID (GUID_COM_INTERFACE_STANDARD,
        0xe0b27630, 0x5434, 0x11d3, 0xb8, 0x90, 0x0, 0xc0, 0x4f, 0xad, 0x51, 0x72);