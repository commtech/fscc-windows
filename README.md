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

### Changing Register Values
The FSCC driver is a swiss army knife of sorts with communication. It can
handle many different situations if configured correctly. Typically to
configure it to handle your specific situation you need to modify the card's
register values.

There are multiple ways of modifying the card's registers varying from using
the Windows API to an FSCC specific API. Here are a few ways of doing this.

NOTE: For a listing of all of the configuration options please see the manual.

Use the `FSCC_SET_REGISTERS` ioctl to set the values of any registers you
need to modify from within C code. This ioctl can be found within
`<fscc/fscc.h>`.

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
```

Use the various APIs to easily set the values of any registers you need to
modify from within your code.

###### C Library
```
#include <fscc.h>
...

struct fscc_registers registers;

FSCC_REGISTERS_INIT(registers);

registers.CCR0 = 0x0011201c;
registers.BGR = 10;

fscc_set_registers(h, &registers);
```

NOTE: A complete example of how to do this can be found in the file
      fscc\lib\fscc\c\examples\set-registers.c.

###### C++ Library
```cpp
#include <fscc.hpp>
...

Registers registers;

FSCC_REGISTERS_INIT(registers);

registers.CCR0 = 0x0011201c;
registers.BGR = 10;

port.SetRegisters(registers);
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


### Reading Register Values
There are multiple ways of reading the card's registers varying from using
the Windows API to an FSCC specific API. Here are a few ways of doing this.

Use the `FSCC_GET_REGISTERS` ioctl to get the values of any registers you
need to read from within code. This ioctl can be found within
`<fscc/fscc.h>`.

```c
#include <fscc.h>

...

struct fscc_registers registers;

FSCC_REGISTERS_INIT(registers);

registers.CCR0 = FSCC_UPDATE_VALUE;
registers.BGR = FSCC_UPDATE_VALUE;

DeviceIoControl(h, FSCC_GET_REGISTERS, 
				NULL, 0, 
				&registers, sizeof(registers), 
				&temp, NULL);				
```

At this point 'regs.BGR' and 'regs.FCR' would be set to their respective
values.

NOTE: A complete example of how to do this can be found in the file
      fscc\lib\fscc\c\examples\get-registers.c.

Use the various APIs to easily get the values of any registers you need 
from within your code.

###### C Library
```
#include <fscc.h>
...

struct fscc_registers registers;

FSCC_REGISTERS_INIT(registers);

registers.CCR0 = FSCC_UPDATE_VALUE;
registers.BGR = FSCC_UPDATE_VALUE;

fscc_get_registers(h, &registers);
```

NOTE: A complete example of how to do this can be found in the file
      fscc\lib\fscc\c\examples\set-registers.c.

###### C++ Library
```cpp
#include <fscc.hpp>
...

Registers registers = port.GetRegisters();
```

###### .NET Library
```csharp
using FSCC;
...

var ccr0 = port.CCR0;
var bgr = port.BGR;
```

###### Python Library
```python
import fscc
...

ccr0 = port.registers.BGR
bgr = port.registers.FCR
```


### Asynchronous Communication
The FSCC driver includes a slightly modified version of the Windows serial 
driver for handling the asynchronous communication for our UARTs. The Windows
serial driver is highly tested and likely more stable than anything we could 
produce in any reasonably amount of time.

The FSCC and SerialFC drivers work together to automatically switch between 
synchronous and asynchronous modes by modifying the FCR register for you. 
All you need to do is open the FSCC handle to be in synchronous mode and the 
COM handle to be in asychronous mode.

For more information about using the UART's take a look at the 
[SerialFC driver readme](https://github.com/commtech/serialfc-windows/blob/master/README.md).



### Setting Clock Frequency
The FSCC device has a programmable clock that can be set anywhere from
20 KHz to 200 MHz. However, this is not the full operational range of an
FSCC port, only the range that the onboard clock can be set to.

Using one of the synchronous modes you can only receive data consistently
up to 30 MHz (when you are using a external clock). If you are transmitting
data using an internal clock you can safely go up 50 MHz.

Lower clock rates (less than 1 MHz for example) can take a long time for 
the frequency generator to finish. If you run into this situation we 
recommend using a larger frequency and then dividing it down to your 
desired baud rate using the BGR register.

Use the `FSCC_SET_CLOCK_BITS` ioctl to set the frequency from within code.

```c
/* 10 MHz */
unsigned char clock_bits[20] = {0x01, 0xa0, 0x04, 0x00, 0x00, 0x00, 0x00,
                               0x00, 0x00, 0x00, 0x00, 0x9a, 0x4a, 0x41,
                               0x01, 0x84, 0x01, 0xff, 0xff, 0xff};

DeviceIoControl(h, FSCC_SET_CLOCK_BITS, 
		        &clock_bits, sizeof(clock_bits), 
				NULL, 0, 
				&temp, NULL);
```

NOTE: A complete example of how to do this along with how to calculate
      these clock bits can be found in the file
      fscc\lib\fscc\c\examples\set-clock-frequency.c.

Use the various APIs to easily get the values of any registers you need 
from within your code.

NOTE: PPM (Parts Per Million) has been deprecated and will be removed in 
a future release. This value will be ignored in the mean time.

###### Windows API
```c
#include <fscc.h>
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


###  Operating Driver
The FSCC driver typically (but not always) works in "frames". This means that
data typically is packaged together in permanent chunks. If the card received
two frames of data prior to you retrieving the data you will only get one chunk
of data back at a time when you interface with the driver to get the data.

There are multiple ways of reading/writing data to/from the card. Listed below
are only the most common.

Writing data will typically be done within C code using the 
[`WriteFile()`](http://msdn.microsoft.com/en-us/library/windows/desktop/aa365747(v=vs.85).aspx)
function found within 
[`<windows.h>`](http://msdn.microsoft.com/en-us/library/windows/desktop/aa383745(v=vs.85).aspx). 

```
result = WriteFile(handle, buf, count, (DWORD*)bytes_written, NULL);
```

In in addition to the standard errors that the 
[`WriteFile()`](http://msdn.microsoft.com/en-us/library/windows/desktop/aa365747(v=vs.85).aspx)
function returns there are a couple errors specific to the FSCC you might run 
into.

STATUS_IO_TIMEOUT: If trying to use a FSCC port without a transmit clock present.
            This check can be turned off with the 'ignore_timeout' option.

STATUS_BUFFER_TOO_SMALL: If the count parameter passed into the write() function 
          is larger
          than the output cap. If the count parameter is less than the
          output cap but the amount out of output space isn't enough the
          driver will block instead of returning this error.


Reading data will typically be done within C code using the 
[`ReadFile()`](http://msdn.microsoft.com/en-us/library/windows/desktop/aa365467(v=vs.85).aspx)
function found within the 
[`<windows.h>`](http://msdn.microsoft.com/en-us/library/windows/desktop/aa383745(v=vs.85).aspx). 

```c
result = ReadFile(handle, buf, length, (DWORD*)bytes_read, NULL);
```

The length argument of the `ReadFile()` function means different things depending
on the mode you are using.

In a frame based mode the length argument specifies the maximum frame size
to return. If the next queued frame is larger than the size you specified
the error `STATUS_BUFFER_TOO_SMALL` is returned and the data will remain 
waiting for a 
[`ReadFile()`](http://msdn.microsoft.com/en-us/library/windows/desktop/aa365467(v=vs.85).aspx)
of a larger value. If a 
[`ReadFile()`](http://msdn.microsoft.com/en-us/library/windows/desktop/aa365467(v=vs.85).aspx)
length is specified that is larger than the
length of multiple frames in queue you will still only receive one frame per
[`ReadFile()`](http://msdn.microsoft.com/en-us/library/windows/desktop/aa365467(v=vs.85).aspx)
call.

In streaming mode (no frame termination) the length argument specifies the
maximum amount of data to return. If there is 100 bytes of streaming data
in the card and you 
[`ReadFile()`](http://msdn.microsoft.com/en-us/library/windows/desktop/aa365467(v=vs.85).aspx)
with a length of 50, you will receive 50 bytes.
If you were to do a 
[`ReadFile()`](http://msdn.microsoft.com/en-us/library/windows/desktop/aa365467(v=vs.85).aspx)
of 200 bytes you will receive the 100 bytes
available.

Frame based data and streaming data are kept separate within the driver.
To understand what this means first imagine this scenario. You are in a
frame based mode and receive a couple of frames. You then switch to
streaming mode and receive a stream of data. When calling 
[`ReadFile()`](http://msdn.microsoft.com/en-us/library/windows/desktop/aa365467(v=vs.85).aspx)
you will receive the the streaming data until you switch back into a frame based
mode then do a 
[`ReadFile()`](http://msdn.microsoft.com/en-us/library/windows/desktop/aa365467(v=vs.85).aspx).

In in addition to the standard errors that the 
[`ReadFile()`](http://msdn.microsoft.com/en-us/library/windows/desktop/aa365467(v=vs.85).aspx)
function returns there are a couple errors specific to the FSCC you might run into.

STATUS_BUFFER_TOO_SMALL: If the size parameter passed into the read() function is smaller
          than the next frame (in a frame based mode).


### Viewing/Setting Frame Status
It is a good idea to pay attention to the status of each frame. For example if
you want to see if the frame's CRC check succeeded or failed.

The way the FSCC reports this data to you is by appending two additional bytes
to each frame you read from the card if you opt in to see this data. There are
a few of ways of enabling this additional data.

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

NOTE: A complete example of how to do this can be found in the files
      fscc\lib\fscc\c\examples\set-append-status.c and 
      fscc\lib\fscc\c\examples\get-append-status.c.

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


XI. Viewing/Setting Memory Constraints
-------------------------------------------------------------------------------
For systems with limited memory available to them there is safety checks in
place to prevent spurious incoming data from overrunning your system. Each port
has an option for setting it's input and output memory cap.

There are multiple ways of setting this value.

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

NOTE: You can set only 1 of the 2 values by running the `FSCC_MEMORY_CAP_INIT`
      macro on the `fscc_memory_cap` struct then setting only 1 of the values
      in the structure. The `FSCC_MEMORY_CAP_INIT` structure initializes both
      values to -1 which will be ignored in the driver.

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

NOTE: You can set only 1 of the 2 values by running the `FSCC_MEMORY_CAP_INIT`
      macro on the `fscc_memory_cap` struct then setting only 1 of the values
      in the structure. The `FSCC_MEMORY_CAP_INIT` structure initializes both
      values to -1 which will be ignored in the driver.

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

```c
struct fscc_memory_cap memory_cap;

FSCC_MEMORY_CAP_INIT(memory_cap);

memory_cap.input = 5000000;
memory_cap.output = 10000;

ioctl(port_fd, FSCC_SET_MEMORY_CAP, &memory_cap);
```

NOTE: A complete example of how to do this can be found in the file
      fscc\lib\fscc\c\examples\set-memory-cap.c and 
      fscc\lib\fscc\c\examples\get-memory-cap.c

### Purging Data
Between the hardware FIFO and the driver's software buffers there are multiple
places data could be at excluding your application code. If you ever need to
clear this data out and start out fresh there are a couple ways of doing this.

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

In in addition to the standard errors that the DeviceIoControl() function returns
there is an error specific to the FSCC you might run into.

STATUS_IO_TIMEOUT: If trying to use a FSCC port without a transmit clock present.
            This check can be turned off with the 'ignore_timeout' command
            line parameter.

NOTE: A complete example of how to do this can be found in the files
      fscc\lib\fscc\c\examples\purge.c.

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

All of the following information has been copied from the linux README and has yet
to be integrated into the Windows README. It will be soon.
=================================================

### Migrating From 1.x to 2.x
There are multiple benefits of using the 2.x driver: accurate posix error
codes, intuitive ioctl calls, backend support for multiple languages (Python,
C#) and dynamic memory management are some.

The 1.x driver and the 2.x driver are very similar so porting from one to the
other should be rather painless.

NOTE: All ioctl values have changed even if their new names match their old
      names. This means even if you use an ioctl with an identical name, it
      will not work correctly.

Setting register values was split into two different ioctl's in the 1.x
driver, setting all the registers at once and one at a time. In the 2.x
driver these two scenarios have been combined into one ioctl.

Change the following ioctl's to the current ioctl `FSCC_SET_REGISTERS` (see
section V).

`FSCC_WRITE_REGISTER` (setting a single register at a time)
`FSCC_INIT` (setting all registers at a time)

Getting register values was limited to one at a time in the 1.x driver. In
the 2.x driver it has been made more convenient to read multiple register
values.

Change the following ioctl to the current ioctl `FSCC_GET_REGISTERS` (see
section VI).

`FSCC_READ_REGISTER` (reading a single register at a time)

Purging transmit and receive data has not changed. Continue using
`FSCC_PURGE_TX` and `FSCC_PURGE_RX`.

For more information on these ioctl's see section XII.

Getting the frame status has now been designed to be configurable. In the
1.x driver you would always have the frame status appended to your data on a
read. In the 2.x driver this can be toggled, and defaults to not appending
the status to the data.

For more information on the ioctl's to toggle this feature see section X.

Changing the clock frequency is basically the same but the data structure
and ioctl name are different.

Change the following ioctl to the current ioctl `FSCC_SET_CLOCK_BITS` (see
section VIII).

`FSCC_SET_FREQ` (setting the clock frequency)

In the 1.x driver you passed in a structure composed of both the desired
frequency and the clock bits that represent the frequency. In the 2.x driver
this has been simplified down to just the clock bits.

### FAQ
Q: Why are the /dev/fscc* ports not created even though the driver has
   loaded?

A: There are a couple of possibilities but you should first check
   /var/log/messages for any helpful information. If that doesn't help you
   out then continue reading.

   One possibility is that there is another driver loaded that has claimed
   our cards. For example if your kernel is patched to use our card for
   asynchronous transmission the linux serial driver has more than likely
   claimed the card and in turn we won't receive the appropriate 'probe'
   notifications to load our card.

   Another possibility is that you have accidentally tried insmod'ing with
   the 'hot_plug' option enabled and your cards are not actually present.
   Double check that your card shows up in the output of 'lspci' and make
   sure to use hot_plug=0.

Q: What does poll() and select() base their information on?

A: Whether or not you can read data will be based on if there is at least 1
   byte of data available to be read in your current mode of operation. For
   example, if there is streaming data it will not be considered when in
   a frame based mode.

   Whether or not you can write data will be based on if you have hit your
   output memory cap. (see section XI).

Q: Why does executing a purge without a clock put the card in a broken
   state?

A: When executing a purge on either the transmitter or receiver there is
   a TRES or RRES (command from the CMDR register) happening behind the
   scenes. If there is no clock available the command will stall until
   a clock is available. This should work in theory but doesn't in
   practice. So whenever you need to execute a purge without a clock, first
   put it into clock mode 7, execute your purge then return to your other
   clock mode.

Q: Why am I receiving the error message 'Couldn't register serial port'
   when loading the driver.

A: When loading, the driver will attempt to register the board's UARTs with
   the built in serial driver. The serial driver statically defines how
   many UARTs can be registered and will report this error if there isn't
   enough room.

   There are multiple ways of allowing more available UART room which can
   be found in section VII.

Q: Why am I not seeing my card in sysfs?

A: There are a couple possibilities but you should first check what kernel
   version you are using. Due to the way we register our card with sysfs it
   won't appear in kernel versions prior to 2.6.25.

   Another possibility is that it is located in a different directory than
   you are checking. Typically fscc/ appears in /sys/class/ but it might
   appear elsewhere. If it isn't in /sys/class/ do a search in /sys/ for
   our fscc/ directory.


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
