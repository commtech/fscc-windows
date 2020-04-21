# fscc-windows
This README file is best viewed [online](http://github.com/commtech/fscc-windows/).

## Installing Driver

##### Downloading Driver Package
You can download a pre-built driver package directly from our [website](http://www.commtech-fastcom.com/CommtechSoftware.html).


## Quick Start Guide
There is documentation for each specific function listed below, but lets get started with a quick programming example for fun.

_This tutorial has already been set up for you at_ [`fscc/examples/tutorial.c`](examples/tutorial.c).

Create a new C file (named tutorial.c) with the following code.

```c
#include <stdio.h>
#include <stdlib.h>
#include <Windows.h>

int main(void)
{
	HANDLE h = 0;
	DWORD tmp;
	char odata[] = "Hello world!";
	char idata[20];

	/* Open port 0 in a blocking IO mode */
	h = CreateFile("\\\\.\\FSCC0", GENERIC_READ | GENERIC_WRITE, 0, NULL,
	                  OPEN_EXISTING, 0, NULL);

	if (h == INVALID_HANDLE_VALUE) {
        fprintf(stderr, "CreateFile failed with %d\n", GetLastError());
		return EXIT_FAILURE;
	}

	/* Send "Hello world!" text */
	WriteFile(h, odata, sizeof(odata), &tmp, NULL);

	/* Read the data back in (with our loopback connector) */
	ReadFile(h, idata, sizeof(idata), &tmp, NULL);

	fprintf(stdout, "%s\n", idata);

	CloseHandle(h);

	return EXIT_SUCCESS;
}

```

For this example I will use the Visual Studio command line compiler, but you can use your compiler of choice.

```
# cl tutorial.c
```

Now attach the included loopback connector.

```
# tutorial.exe
Hello world!
```

You have now transmitted and received an HDLC frame!


## API Reference

There are likely other configuration options you will need to set up for your  own program. All of these options are described on their respective documentation page.

- [Connect](docs/connect.md)
- [Append Status](docs/append-status.md)
- [Append Timestamp](docs/append-timestamp.md)
- [Clock Frequency](docs/clock-frequency.md)
- [Ignore Timeout](docs/ignore-timeout.md)
- [Memory Cap](docs/memory-cap.md)
- [Purge](docs/purge.md)
- [Read](docs/read.md)
- [Registers](docs/registers.md)
- [RX Multiple](docs/rx-multiple.md)
- [Track Interrupts](docs/track-interrupts.md)
- [TX Modifiers](docs/tx-modifiers.md)
- [Write](docs/write.md)
- [Disconnect](docs/disconnect.md)

There are also multiple code libraries to make development easier.
- [C](https://github.com/commtech/cfscc/)
- [C++](https://github.com/commtech/cppfscc/)
- [.NET](https://github.com/commtech/netfscc/)
- [Python](https://github.com/commtech/pyfscc/)


## Visual Studio Integration
The process for using the library will differ slightly depending on the version of Visual Studio.

1. Configuration Properties -> C/C++ -> General -> Additional Include Directories: (folder containing fscc.h)


## Asynchronous Communication
The FSCC driver includes a slightly modified version of the Windows serial driver for handling the asynchronous communication for our UARTs. The Windows serial driver is highly tested and likely more stable than anything we could produce in any reasonable amount of time.

The FSCC and SerialFC drivers work together to automatically switch between synchronous and asynchronous modes by modifying the FCR register for you. All you need to do is open the FSCC handle to be in synchronous mode and the COM handle to be in asynchronous mode.

More information about using the UARTs is available in the
[SerialFC driver README](https://github.com/commtech/serialfc-windows/blob/master/README.md) file.


### FAQ

##### Can I use the old legacy utilties with the new driver?
No. The old utilities use a different API and will not work with the new driver.

##### Why does executing a purge without a clock put the card in a broken state?
When executing a purge on either the transmitter or receiver there is a TRES or RRES (command from the CMDR register) happening behind the scenes. If there isn't a clock available, the command will stall until a clock is available. This should work in theory but doesn't in practice. So whenever you need to execute a purge without a clock: first put it into clock mode 7, execute your purge, and then return to your other clock mode.


##### The CRC-CCITT generated is not what I expected.
There are many resources online that say they can calculate CRC-CCITT, but most don't use the correct formula defined by the HDLC specification.

An example of one that doesn't is [lammertbies.nl](http://www.lammertbies.nl/comm/info/crc-calculation.html) and the [forum post](http://www.lammertbies.nl/forum/viewtopic.php?t=607) explaining why it isn't generated correctly.

We recommend using this [CRC generator](http://www.zorc.breitbandkatze.de/crc.html) to calculate the correct value and to use [these settings](http://i.imgur.com/G6zT87i.jpg).


##### My port numbering is incorrect, how do I fix it?
It is possible, but unfortunately there isn't a good way at the moment.  You will need to modify the following registry keys.

This is the key for setting automatically generated port numbering. If you want the next number to be 8, then set this to 7. If you want it to be 0, then set to 0xffffffff (actually -1).
`HKEY_LOCAL_MACHINE\SYSTEM\CurrentControlSet\services\FSCC\Parameters\LastPortNumber`

This is the key for setting the port's specific number after it has already been assigned. `HKEY_LOCAL_MACHINE\SYSTEM\CurrentControlSet\Enum\MF\PCI#VEN_18F7&DEV_00XXXXXXXXXXXXXXXXXXXX#Child0X\Device Parameters\PortNumber`


##### Which resitors are for termination?
Each receive differential pair is terminated with a 100 ohm resistor between the + and - pins. These resistors are on the back of the card and are labeled '101'.

Each single receive pin is either pulled up or pulled down by a 1k ohm resister (+ is pulled up and - is pulled down).  This ensures that the receiver will stay at a logic 1 in the presence of transient noise and is weak enough to allow a real signal through. These resistors are near the connector and are labeled '102'.

If you would like to send the card to us, we will gladly remove them for you. Please note, removing them yourself will void the warranty.

```
+5
 |
 |
1000 Ω
 |
 |
 ---- Rx(+)
 |
 |
100 Ω
 |
 |
 ---- Rx(-)
 |
 |
1000 Ω
 |
 |
 ⏚
```


##### What does each LED color mean?
| Color | Version |
| ----- | ------- |
| `Red` | Receive line |
| `Green or yellow` | Transmit line |
| `Blue` | Transmit & receive lines simultaneously (4-port cards) |


These are not bi-color LEDs, they are only red and only green (or yellow).

##### Why are there extra bits surrounding the data while using RS485?
When operating in RS-485 mode the physical driver chip is in a disabled state when data is not being actively transmitted. It is necessary to re-enable the driver prior to the start of data transmission. This is to ensure that the driver is fully enabled and ready to receive data when the first bit of data arrives. Likewise it is necessary for the driver to remain enabled for a brief time after the last bit of data has been transmitted. This is to ensure that the data has completely propagated through the driver before it is disabled. The FCore will enable and disable the RS-422 driver chip one clock period before the first bit of data and one clock period after the last bit of data. As a result you will see an extra 1 bit (if idling ones) appear just before the first bit of your data and just after the last bit of your data. This is not erroneous data bit and will be outside of your normal data frame and will not be confused as valid data.

##### How long after CTS until the data is transmitted?
Transmission happens within a fixed time frame after CTS of around 1 - 1.5 clock cycles.

##### How do I get past the 'Windows Logo testing' check?

1. Right-click on My Computer and click Properties
2. Click the Hardware tab, then click Driver Signing under Drivers
3. Select 'Warn: Prompt me each time to choose an action'

Once you're done, restart the workstation and install the driver as usual. Windows shouldn't bother you about logo testing this time.


##### How do I receive parse raw data without any framing or sync signal?
The difficulty in using transparent mode without a data sync signal, is that you have no idea where valid receive data starts and stops.  You must shift through the bit stream until you find valid data.  Once you have found valid data and noted the correct bit alignment, if the data reception remains constant, you should be able to shift all incoming data by the correct number of bits and you will then have correct data.

Lets start by sending `0xa5a5` (`10100101` `10100101`) with LSB being transmitted first.
`10100101` `10100101`

Now the idle is 1's so the message will be received (depending on how many 1's are clocked in prior to the actual data) the possibilities are:
```
11111111 10100101 10100101 11111111
1111111 10100101 10100101 111111111
111111 10100101 10100101 1111111111
11111 10100101 10100101 11111111111
1111 10100101 10100101 111111111111
111 10100101 10100101 1111111111111
11 10100101 10100101 11111111111111
1 10100101 10100101 111111111111111
```

Re-aligning this to byte boundaries:
```
11111111 10100101 10100101 11111111
11111111 01001011 01001011 11111111
11111110 10010110 10010111 11111111
11111101 00101101 00101111 11111111
11111010 01011010 01011111 11111111
11110100 10110100 10111111 11111111
11101001 01101001 01111111 11111111
11010010 11010010 11111111 11111111
```

Remembering that LSB was transmitted first, changing it to "normal" (MSB) gives possible received messages
```
11111111 10100101 10100101 11111111 = 0xffa5a5ff
11111111 11010010 11010010 11111111 = 0xffd2d2ff
01111111 01101001 11101001 11111111 = 0x7f69e9ff
10111111 10110100 11110100 11111111 = 0xbfb4f4ff
01011111 01011010 11111010 11111111 = 0x5f5afaff
00101111 00101101 11111101 11111111 = 0x2f2dfdff
10010111 10010110 11111110 11111111 = 0x9796feff
01001011 01001011 11111111 11111111 = 0x4b4bffff
```

They are all representative of the original 0xa5a5 that was sent, however only 1 of them directly shows that upon receive without shifting.

And once you determine how many bits you need to shift to get valid data, everything that you read in will need to be shifted by that number of bits until you stop transmitting data.  If you stop transmitting, then you have to do the same thing over again once you start transmitting data again.

This can be avoided by utilizing a receive data strobe in an appropriate clock mode.  If the transmitting device can supply a strobe signal that activates at the beginning of the of the data and deactivates at the end, the receiver will only be activated during the active phase of this signal, and hopefully the data will have the correct alignment.


##### What was changed in 2.X?
You can view an up-to-date list of driver changes in our [ChangeLog](ChangeLog.md).


##### How do I upgrade driver versions?
1. Open the 'Device Manager'
3. Right click & select 'Properties' on each Commtech port
4. Switch to the 'Driver' tab
5. Click on the 'Update Driver...' button
6. Then browse to the location of the new driver files


##### How do I build a custom version of the driver source code?
The source code for the Fastcom FSCC driver is hosted on Github code hosting. To check out the latest code you will need Git and to run the following command in a terminal:

```
git clone git://github.com/commtech/fscc-windows.git fscc
```

We prefer you use the above method for downloading the driver source code (because it is the easiest way to stay up to date), but you can also get the driver source code from the [download page](https://github.com/commtech/fscc-windows/tags/).

Now that you have the latest code checked out, you will probably want to switch to a stable version within the code directory. You can do this by browsing the various tags for one you would like to switch to. Version v2.2.8 is only listed here as an example.

```
git tag
git checkout v2.2.8
```

Provided you have Windows Driver Kit 7.1.0 installed you can build the driver by running the BLD command from within the source code directory.

```
cd fscc/src/
BLD
```


##### Should I migrate from 1.x to 2.x?
There are multiple benefits of using the 2.x driver: amd64 support, intuitive [`DeviceIoControl`](http://msdn.microsoft.com/en-us/library/windows/desktop/aa363216.aspx) calls, backend support for multiple languages (C, C++, Python, .NET), and dynamic memory management, to name a few.

The 1.x driver and the 2.x driver are very similar, so porting from one to the other should be rather painless.

All `DeviceIoControl` values have changed even if their new names match their old names. This means even if you use a `DeviceIoControl` with an identical name, it will not work correctly.

| Old IOCTL | New IOCTL | Notes |
| --------- | --------- | ----- |
| `IOCTL_FSCCDRV_TX_ACTIVE` | | No longer available |
| `IOCTL_FSCCDRV_RX_READY` | | No longer available |
| `IOCTL_FSCCDRV_SETUP` | `FSCC_SET_REGISTERS` |
| `IOCTL_FSCCDRV_STATUS` | | No longer available |
| `IOCTL_FSCCDRV_FLUSH_RX` | `FSCC_PURGE_RX` | Name change |
| `IOCTL_FSCCDRV_FLUSH_TX` | `FSCC_PURGE_TX` | Name change |
| `IOCTL_FSCCDRV_READ_REGISTER` | `FSCC_GET_REGISTERS` | Single register specified with `FSCC_UPDATE_VALUE` |
| `IOCTL_FSCCDRV_WRITE_REGISTER` | `FSCC_SET_REGISTERS`  | Single register set after `FSCC_REGISTERS_INIT` |
| | `FSCC_GET_REGISTERS` | Added support for getting multiple registers at once |
| `IOCTL_FSCCDRV_IMMEDIATE_STATUS` | | No longer available |
| `IOCTL_FSCCDRV_WRITE_REGISTER2` | `FSCC_SET_REGISTERS` | Single register set after `FSCC_REGISTERS_INIT` |
| `IOCTL_FSCCDRV_READ_REGISTER3` | `FSCC_GET_REGISTERS` | Single register specified with `FSCC_UPDATE_VALUE` |
| `IOCTL_FSCCDRV_WRITE_REGISTER3` | `FSCC_SET_REGISTERS` | Single register set after `FSCC_REGISTERS_INIT` |
| `IOCTL_FSCCDRV_READ_REGISTER2` | `FSCC_GET_REGISTERS` | Single register specified with `FSCC_UPDATE_VALUE` |
| `IOCTL_FSCCDRV_CANCEL_RX` | `CancelIo, CancelIoEx` | Can use Windows API Cancel functions |
| `IOCTL_FSCCDRV_CANCEL_TX` | `CancelIo, CancelIoEx` | Can use Windows API Cancel functions |
| `IOCTL_FSCCDRV_CANCEL_STATUS` | | No longer available |
| `IOCTL_FSCCDRV_SET_FREQ` | `FSCC_SET_CLOCK_BITS` | Only takes clock bits now |
| `IOCTL_FSCCDRV_SET_FEATURES` | `FSCC_SET_REGISTERS` | Use the FCR member of the registers structure |
| `IOCTL_FSCCDRV_GET_FEATURES` | `FSCC_GET_REGISTERS` | Use the FCR member of the registers structure |
| `IOCTL_FSCCDRV_GET_FREQ` | | No longer available |
| `IOCTL_FSCCDRV_BLOCK_MULTIPLE_IO` | | No longer available |
| `IOCTL_FSCCDRV_GET_SETUP` | `FSCC_GET_REGISTERS` | |
| `IOCTL_FSCCDRV_TIMED_TRANSMIT` | `FSCC_SET_TX_MODIFIERS` | TXT option |
| `IOCTL_FSCCDRV_TRANSMIT_REPEAT` | `FSCC_SET_TX_MODIFIERS` | XREP option |
| `IOCTL_FSCCDRV_DELAY_WRITE_START` | | No longer available |
| `IOCTL_FSCCDRV_ALLOW_READ_CUTOFF` | | No longer required (handled automatically) |
| `IOCTL_FSCCDRV_SET_RX_IRQ_RATE` | | No longer available |
| `IOCTL_FSCCDRV_SET_TX_IRQ_RATE` | | No longer available |
| `IOCTL_FSCCDRV_SET_DMA` | | No longer available |
| `IOCTL_FSCCDRV_SET_RECEIVE_MULTIPLE` | `FSCC_SET_RX_MULTIPLE` | No longer available |
| `IOCTL_FSCCDRV_GET_RECEIVE_MULTIPLE` | `FSCC_GET_RX_MULTIPLE` | No longer available |
| `IOCTL_FSCCDRV_SET_CHARACTER_MAP_ENABLE` | | No longer available |
| `IOCTL_FSCCDRV_GET_CHARACTER_MAP_ENABLE` | | No longer available |
| `IOCTL_FSCCDRV_SET_CHARACTER_MAP` | | No longer available |
| `IOCTL_FSCCDRV_GET_CHARACTER_MAP` | | No longer available |
| `IOCTL_FSCCDRV_SET_UCHARACTER_MAP` | | No longer available |
| `IOCTL_FSCCDRV_GET_UCHARACTER_MAP` | | No longer available |
| `IOCTL_FSCCDRV_EXTERNAL_TRANSMIT` | `FSCC_SET_TX_MODIFIERS` | TXEXT option |
| `IOCTL_FSCCDRV_GET_TIMED_TRANSMIT` | `FSCC_GET_TX_MODIFIERS` | Check for TXT |
| `IOCTL_FSCCDRV_GET_TRANSMIT_REPEAT` | `FSCC_GET_TX_MODIFIERS` | Check for XREP |
| `IOCTL_FSCCDRV_GET_EXTERNAL_TRANSMIT` | `FSCC_GET_TX_MODIFIERS` | Check for TXEXT |
| `IOCTL_FSCCDRV_SET_PROGBITS` | `FSCC_SET_CLOCK_BITS` | Only takes clock bits now |
| `IOCTL_FSCCDRV_GET_PROGBITS` | | No longer available |
| | `FSCC_ENABLE_APPEND_STATUS`  | |
| | `FSCC_DISABLE_APPEND_STATUS` | |
| | `FSCC_GET_APPEND_STATUS`     | |

##### Status Bytes
Getting the frame status has now been designed to be configurable. When using the 1.x driver, you will always have the frame status appended to your data on a read. When using the 2.x driver, this can be toggled and defaults to not appending the status to the data.

All of the 2.2.X releases will not break API compatability. If a function in the 2.2.X series returns an incorrect value, it can be fixed to return the correct value in a later release.

When and if we switch to a 2.3 release there will only be minor API changes.

##### I need to run software using the older API. How do I revert back to the 1.X driver?
Start by uninstalling all of the entries in the 'Device Manager'.

1. Open the 'Device Manager'
2. Expand the 'Ports (COM & LPT)' section
3. Right click & select 'Uninstall' on each Commtech port
4. If available, select 'Delete the driver software for this device'
5. Repeat steps 2-4 with the 'Ports (FSCC)' section
5. Repeat steps 2-4 with the 'Multi-port serial adapaters' section

To ensure Windows doesn't use a cached version in the future we will remove the .inf files.

1. Open 'Windows Exporer' to 'C:\Windows\inf'
2. Delete any .inf and .PNF files that contain the word Commtech. These are usually labeled oemX.inf and oemX.PNF
3. Open the folder with the drivers from Commtech
4. Delete all folders except for 'legacy'

At this point you can install the 'legacy' version of the driver without the newer version interfering.


##### Do I need to do anything special to use DMA?
No! If you have a board with DMA enabled (the SuperFSCC family), the drivers will automatically utilize DMA for both transmit and receive (as of version 2.8.0). If you find you do not want to use DMA, you can disable it by using ENABLE_FORCE_FIFO. You can switch back to DMA by using DISABLE_FORCE_FIFO. 

It is advisable to purge after switching between DMA and FIFO mode.

DMA allocates an additional 1MB of space for transmit and 1MB of space for receive in addition to the memory cap.


## Build Dependencies
- Windows Driver Kit (7.1.0 used internally to support XP)


## Run-time Dependencies
- OS: Windows XP+


## API Compatibility
We follow [Semantic Versioning](http://semver.org/) when creating releases.


## License

Copyright (C) 2019 [Commtech, Inc.](https://fastcomproducts.com/)

Licensed under the [MIT license](https://opensource.org/licenses/MIT).
