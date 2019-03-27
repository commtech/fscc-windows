/*
Copyright 2019 Commtech, Inc.

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