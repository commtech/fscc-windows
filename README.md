## Installation

### Downloading Driver Package
You will more than likely want to download our pre-built driver package from
the [Commtech website](http://www.commtech-fastcom.com/CommtechSoftware.html).


### Downloading Source Code
If you are installing from the pre-built driver packge you can skip ahead
to the section on loading the driver.

The source code for the Fastcom FSCC driver is hosted on Github code hosting.
To check out the latest code you will need Git and to run the following in a
terminal.

```
git clone git://github.com/commtech/fscc-windows.git fscc
```

NOTE: We prefer you use the above method for downloading the driver source code
      (because it is the easiest way to stay up to date) but you can also get 
      the driver source code from the
      [download page](https://github.com/commtech/fscc-windows/tags/).

Now that you have the latest code checked out you will more than likely want
to switch to a stable version within the code directory. To do this browse
the various tags for one you would like to switch to. Version v1.0.0 is only
listed here as an example.

```
git tag
git checkout v1.0.0
```

### Compiling Driver
Compiling the driver is relatively simple assuming you have all of the
required dependencies. You will need Windows Driver Kit 7.1.0 at a 
minimum. After assembling all of these things you can build the driver by
simply running the BLD command from within the source code directory.

```
cd fscc/src/
BLD
```

#### Port numbers
This is the key for setting the port numbering. If you want the next number to be 8 then set this to 7. If you want it to be 0 then set to 0xffffffff (actually -1).
HKEY_LOCAL_MACHINE\SYSTEM\CurrentControlSet\services\FSCC\Parameters\LastPortNumber
 
This isn't the exact key because it is for the device id in my system but it will get you close. This is the port specific number.
HKEY_LOCAL_MACHINE\SYSTEM\CurrentControlSet\Enum\MF\PCI#VEN_18F7&DEV_0014&SUBSYS_00000000&REV_04\5&2148fa65&2d&00F0#Child01\Device Parameters\PortNumber


#### CRC Calculation
http://www.zorc.breitbandkatze.de/crc.html
[CCITT Settings](http://i.imgur.com/G6zT87i.jpg)

Here is the reason why lammertbies CCITT is incorrect http://www.lammertbies.nl/forum/viewtopic.php?t=607


### Tx Modifiers

##### Options
- XF - Normal transmit - disable modifiers
- XREP - Transmit repeat
- TXT - Transmit on timer
- TXEXT - Transmit on external signal

###### Windows API
```c
#include <fscc.h>

...

unsigned modifiers = TXT | XREP;

DeviceIoControl(h, FSCC_SET_TX_MODIFIERS, 
				&modifiers, sizeof(modifiers), 
				NULL, 0, 
				&temp, NULL);

DeviceIoControl(h, FSCC_GET_TX_MODIFIERS, 
				NULL, 0, 
				&modifiers, sizeof(modifiers), 
				&temp, NULL);				
```

###### C Library
```
#include <fscc.h>
...

unsigned modifiers;

fscc_set_tx_modifiers(h, XF | XREP);
fscc_get_tx_modifiers(h, &modifiers);
```

###### C++ Library
```cpp
#include <fscc.hpp>
...

port.SetTxModifiers(XF | XREP);

unsigned modifiers = port.GetTxModifiers();
```

###### .NET Library
```csharp
using FSCC;
...

port.TxModifiers = XF | XREP;
```

###### Python Library
```python
import fscc
...

port.tx_modifiers = XF | XREP
```


### Memory Cap

###### Windows API
```c
#include <fscc.h>

...

struct fscc_memory_cap memcap;

memcap.input = 1000000; /* 1 MB */
memcap.output = 2000000; /* 2 MB */

DeviceIoControl(h, FSCC_SET_MEMORY_CAP, 
				&memcap, sizeof(memcap), 
				NULL, 0, 
				&temp, NULL);

DeviceIoControl(h, FSCC_GET_MEMORY_CAP, 
				NULL, 0, 
				&memcap, sizeof(memcap), 
				&temp, NULL);				
```

###### C Library
```
#include <fscc.h>
...

struct fscc_memory_cap memcap;

memcap.input = 1000000; /* 1 MB */
memcap.output = 2000000; /* 2 MB */

fscc_set_memory_cap(h, &memcap);
fscc_get_memory_cap(h, &memcap);
```

###### C++ Library
```cpp
#include <fscc.hpp>
...

struct fscc_memory_cap memcap;

memcap.input = 1000000; /* 1 MB */
memcap.output = 2000000; /* 2 MB */

port.SetMemoryCap(memcap);

memcap = port.GetMemoryCap();
```

###### .NET Library
```csharp
TODO
```

###### Python Library
```python
import fscc
...

port.input_memory_cap = 1000000
port.output_memory_cap = 2000000
```


### Registers

###### Windows API
```c
#include <fscc.h>

...

struct fscc_registers registers;

FSCC_REGISTERS_INIT(registers);

registers.CCR0 = 0x0011201c;
registers.BGR = 10;

DeviceIoControl(h, FSCC_SET_REGISTERS, 
				&registers, sizeof(registers), 
				NULL, 0, 
				&temp, NULL);

DeviceIoControl(h, FSCC_GET_REGISTERS, 
				NULL, 0, 
				&registers, sizeof(registers), 
				&temp, NULL);				
```

###### C Library
```
#include <fscc.h>
...

struct fscc_registers registers;

FSCC_REGISTERS_INIT(registers);

registers.CCR0 = 0x0011201c;
registers.BGR = 10;

fscc_set_memory_cap(h, &memcap);
fscc_get_memory_cap(h, &memcap);
```

###### C++ Library
```cpp
#include <fscc.hpp>
...

Registers registers;

FSCC_REGISTERS_INIT(registers);

registers.CCR0 = 0x0011201c;
registers.BGR = 10;

port.SetRegisters(registers);

registers = port.GetRegisters();
```

###### .NET Library
```csharp
using FSCC;
...

port.CCR0 = 0x0011201c;
port.BGR = 10;
```

###### Python Library
```python
import fscc
...

port.registers.CCR0 = 0x0011201c
port.registers.BGR = 10
```


### Append Status

###### Windows API
```c
#include <fscc.h>
...

BOOL status;

DeviceIoControl(h, FSCC_ENABLE_APPEND_STATUS, 
                NULL, 0, 
                NULL, 0, 
                &temp, NULL);

DeviceIoControl(h, FSCC_DISABLE_APPEND_STATUS, 
                NULL, 0, 
                NULL, 0, 
                &temp, NULL);
				
DeviceIoControl(h, FSCC_GET_APPEND_STATUS, 
                NULL, 0, 
                &status, sizeof(status), 
                &temp, NULL);
```

###### C Library
```c
#include <fscc.h>
...

BOOL status;

fscc_enable_append_status(h);
fscc_disable_append_status(h);

fscc_get_append_status(h, &status);
```

###### C++ Library
```cpp
#include <fscc.hpp>
...

port.EnableAppendStatus();
port.DisableAppendStatus();

bool status = port.GetAppendStatus();
```

###### .NET Library
```csharp
using FSCC;
...

port.AppendStatus = true;
```

###### Python Library
```python
import fscc
...

port.append_status = True;
```


### Ignore Timeout

###### Windows API
```c
#include <fscc.h>
...

BOOL status;

DeviceIoControl(h, FSCC_ENABLE_IGNORE_TIMEOUT, 
                NULL, 0, 
                NULL, 0, 
                &temp, NULL);

DeviceIoControl(h, FSCC_DISABLE_IGNORE_TIMEOUT, 
                NULL, 0, 
                NULL, 0, 
                &temp, NULL);
				
DeviceIoControl(h, FSCC_GET_IGNORE_TIMEOUT, 
                NULL, 0, 
                &status, sizeof(status), 
                &temp, NULL);
```

###### C Library
```c
#include <fscc.h>
...

BOOL status;

fscc_enable_ignore_timeout(h);
fscc_disable_ignore_timeout(h);

fscc_get_ignore_timeout(h, &status);
```

###### C++ Library
```cpp
#include <fscc.hpp>
...

port.EnableIgnoreTimeout();
port.DisableIgnoreTimeout();

bool status = port.GetIgnoreTimeout();
```

###### .NET Library
```csharp
using FSCC;
...

port.IgnoreTimeout = true;
```

###### Python Library
```python
import fscc
...

port.ignore_timeout = True
```


### Purge

###### Windows API
```c
#include <fscc.h>
...
DeviceIoControl(h, FSCC_PURGE_TX, 
                NULL, 0, 
                NULL, 0, 
                &temp, NULL);

DeviceIoControl(h, FSCC_PURGE_RX, 
                NULL, 0, 
                NULL, 0, 
                &temp, NULL);
```

###### C Library
```c
#include <fscc.h>
...

fscc_purge(h, TRUE, TRUE);
```

###### C++ Library
```cpp
#include <fscc.hpp>
...

port.Purge(TRUE, TRUE);
```

###### .NET Library
```csharp
using FSCC;
...

port.Purge(true, true);
```

###### Python Library
```python
import fscc
...

port.purge(True, True)
```


### Clock Frequency

###### Windows API
```c
#include <fscc.h>
TODO// calculate clock bits?
...

unsigned char clock_bits[20];

calculate_clock_bits(18432000, 2, &clock_bits);

DeviceIoControl(h, FSCC_SET_CLOCK_BITS, 
		        &clock_bits, sizeof(clock_bits), 
				NULL, 0, 
				&temp, NULL);
```

###### C Library
```c
#include <fscc.h>
...

fscc_set_clock_frequency(h, 18432000, 2);
```

###### C++ Library
```cpp
#include <fscc.hpp>
...

port.SetClockFrequency(18432000, 2);
```

###### .NET Library
```csharp
using FSCC;
...

port.SetClockFrequency(18432000, 2);
```

###### Python Library
```python
TODO
```
