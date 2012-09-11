/*++

Module Name:

    public.h

Abstract:

    This module contains the common declarations shared by driver
    and user applications.

Environment:

    user and kernel

--*/

//
// Define an Interface Guid so that app can find the device and talk to it.
//

#include <initguid.h>

// {0b99e9a4-1460-4856-9c13-72fca9fa92a3}
DEFINE_GUID (GUID_DEVINTERFACE_FSCC,
    0x0b99e9a4,0x1460,0x4856,0x9c,0x13,0x72,0xfc,0xa9,0xfa,0x92,0xa3);

// {0b99e9a4-1460-4856-9c13-72fca9fa92a4}
DEFINE_GUID (GUID_DEVCLASS_FSCC,
    0x0b99e9a4,0x1460,0x4856,0x9c,0x13,0x72,0xfc,0xa9,0xfa,0x92,0xa4);

// {0b99e9a4-1460-4856-9c13-72fca9fa92a5}
DEFINE_GUID (GUID_WMI_FSCC,
    0x0b99e9a4,0x1460,0x4856,0x9c,0x13,0x72,0xfc,0xa9,0xfa,0x92,0xa5);