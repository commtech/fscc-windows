# ChangeLog

## [3.0.2](https://github.com/commtech/fscc-windows/releases/tag/v3.0.2) (03/06/2023)
- Updated files for Windows 10 Universal driver compatibility, including but not limited to:
	* Changing from ExAllocPoolWithTag to ExAllocPool2
	* Changing several size_t to UINT32 when dealing with registers or DMA.
	* Removed the coinstallers and class installers
	* Added FriendlyName renaming to the driver core instead of relying on class installers
	* Changing the FSCC Port class from a custom class to a member of the Multiport Serial Devices
	* Changing the release script to remove references to build scripts
	* Updating project files for VS2022 compatibility
- Removed support for Windows 7, Windows 8, Windows 8.1, and anything else before Windows 10. This should have been noted and done on the 3.0.0 release, but those releases were pushed forward to resolve some customer issues. We will be deprecating 3.0.0 and 3.0.1 as a result.
	

## [3.0.1](DEPRECATED)(https://github.com/commtech/fscc-windows/releases/tag/v3.0.1) (01/09/2023)
- Removed the ability to adjust the memory buffers while in the driver is in operation.
- Added the ability to adjust the default memory buffer sizes and numbers to the registry.
- Added default register settings to the registry.
- Registry values take effect on reboot or reinstall.

## [3.0.0](DEPRECATED)(UNRELEASED)
- Further DMA work.
- Added DSTAR and DMACCR to the register structure.
- As much as I'd like to claim this is perfectly backwards compatible, the change to the register structure breaks backwards compatibility. Older software can be 'fixed' by using the new fscc.h header (with the updated register structure) and recompiling your software. Otherwise, the changes should be invisible.

## [2.8.2](https://github.com/commtech/fscc-windows/releases/tag/v2.8.2) (08/11/2022)
- This is another release of the DMA rework, with lower default values for the DMA allocations and more safety checks for failed DMA allocations.
- **The most significant change is that instead of allocated as needed, memory buffers are now pre-allocated.** This means that when ports are installed or memory cap is changed, there may be slight delays caused by the allocation of large chunks of memory. If these values are set too high, it can cause a DPC Watchdog BSOD to occur because of how long memory allocation may take.

## [2.8.1](https://github.com/commtech/fscc-windows/releases/tag/v2.8.1) (07/18/2022)
- This is a complete rewrite of the internal IO for both DMA and FIFO. The DMA is automatically enabled for Super\* family cards, but can be disabled with ENABLE_FORCE_FIFO. DMA can be enabled again with DISABLE_FORCE_FIFO.
- **The most significant change is that instead of allocated as needed, memory buffers are now pre-allocated.** This means that when ports are installed or memory cap is changed, there may be slight delays caused by the allocation of large chunks of memory. If these values are set too high, it can cause a DPC Watchdog BSOD to occur because of how long memory allocation may take.

## [2.8.0](https://github.com/commtech/fscc-windows/releases/tag/v2.8.0) (04/21/2020)
- This version fully implements DMA. While it should be entirely transparent for the average user, it is advised to do thorough testing with your software to verify that your software is fully compatible with the DMA changes. If you find that the DMA changes don't work for your software, DMA can be disabled by using the IOCTL ENABLE_FORCED_FIFO. DMA is only available on the Super side of the SuperFSCC family of products (for example, the SuperFSCC/4-PCIe but not the FSCC/4-PCIe).

## [2.7.8](https://github.com/commtech/fscc-windows/releases/tag/v2.7.8) (12/12/2018)
- Updated to serialfc-2.4.7.

## [2.7.7](https://github.com/commtech/fscc-windows/releases/tag/v2.7.7) (02/14/2017)
- Removed sent_oframes. It caused memory to grow infinitely when frames were back to back, and seemed to serve no real purpose.
- Changed memory calculations to use buffer_size instead of data_length.
- Added BLOCKING_WRITE IOCTL.

## [2.7.6](https://github.com/commtech/fscc-windows/releases/tag/v2.7.6) (03/15/2016)
- Removed test.exe from releases - it was unreliable in Clock Mode 7 because of cable length.
- Fixed the original cause of the bug check fixed in version 2.7.5, rxcnt should no longer be invalid.

## [2.7.5](https://github.com/commtech/fscc-windows/releases/tag/v2.7.5) (10/30/2015)
- Fixed the blue screen issue when receive_length is negative.
- Added tools for more thorough debugging.
- Updated copyright years.

## [2.7.4](https://github.com/commtech/fscc-windows/releases/tag/v2.7.4) (10/14/2014)
- Fixed memory cap issues with outgoing data.
- Fixed memory cap issues with incoming data.

## [2.7.3](https://github.com/commtech/fscc-windows/releases/tag/v2.7.3) (02/13/2014)
- Updated SerialFC to
[v2.4.5](https://github.com/commtech/serialfc-windows/releases/tag/v2.4.5)
- Minor optimizations

## [2.7.2](https://github.com/commtech/fscc-windows/releases/tag/v2.7.2) (01/09/2014)
- Added checks to prevent status/timestamp while in streaming mode
- Updated SerialFC to 
[v2.4.4](https://github.com/commtech/serialfc-windows/releases/tag/v2.4.4)

## [2.7.1](https://github.com/commtech/fscc-windows/releases/tag/v2.7.1) (12/12/2013)
- Updated SerialFC to
[v2.4.3](https://github.com/commtech/serialfc-windows/releases/tag/v2.4.3)

## [2.7.0](https://github.com/commtech/fscc-windows/releases/tag/v2.7.0) (12/12/2013)
- Added ability to track interrupts

## [v2.6.4](https://github.com/commtech/fscc-windows/releases/tag/v2.6.4) (11/26/2013)
- Fixed incorrectly setting the clock on 232 and green cards
- Included latest library versions
- Updated SerialFC to 
[v2.4.2](https://github.com/commtech/serialfc-windows/releases/tag/v2.4.2)

## [2.6.3](https://github.com/commtech/fscc-windows/releases/tag/v2.6.3) (11/11/2013)
- Fixed a bug that prevented small frame sizes from being calculated correctly when approaching the memory limit
      
## [2.6.2](https://github.com/commtech/fscc-windows/releases/tag/v2.6.2) (11/11/2013)
- Fixed a regresion from v2.5.0 where the computer would freeze while reading transparent data
- Fixed a bug where outgoing frames wouldn't get cleared fast enough

## [2.6.1](https://github.com/commtech/fscc-windows/releases/tag/v2.6.1) (10/25/2013)
- Updated SerialFC to 
[v2.4.1](https://github.com/commtech/serialfc-windows/releases/tag/v2.4.1)
      
## [2.6.0](https://github.com/commtech/fscc-windows/releases/tag/v2.6.0) (10/25/2013)
- Updated SerialFC to 
[v2.4.0](https://github.com/commtech/serialfc-windows/releases/tag/v2.4.0)

## [2.5.0](https://github.com/commtech/fscc-windows/releases/tag/v2.5.0) (10/7/2013)
- Added wait on write support

## [2.4.3](https://github.com/commtech/fscc-windows/releases/tag/v2.4.3) (9/24/2013)
- Fixed the test program not compiling correctly

## [2.4.2](https://github.com/commtech/fscc-windows/releases/tag/v2.4.2) (9/20/2013)
- Switch to using external code libraries
- Updated SerialFC to 
[v2.2.2](https://github.com/commtech/serialfc-windows/releases/tag/v2.2.2)

## [2.4.1](https://github.com/commtech/fscc-windows/releases/tag/v2.4.1) (8/28/2013)
- Fixed a bug that would allow you to return more data than your buffer size (introduced in 2.4.0)
- Documentation and library improvements

## [2.4.0](https://github.com/commtech/fscc-windows/releases/tag/v2.4.0) (8/27/2013)
- Added support for timestamps

## [2.3.1](https://github.com/commtech/fscc-windows/releases/tag/v2.3.1) (8/21/2013)
- Added missing device ID's 22 and 27

## [2.3.0](https://github.com/commtech/fscc-windows/releases/tag/v2.3.0) (8/13/2013)_
- Updated SerialFC to 
[v2.2.0](https://github.com/commtech/serialfc-windows/releases/tag/v2.2.0)

## [2.2.10](https://github.com/commtech/fscc-windows/releases/tag/v2.2.10) (7/23/2013)
- Updated SerialFC to 
[v2.1.6](https://github.com/commtech/serialfc-windows/releases/tag/v2.1.6)

## [2.2.9](https://github.com/commtech/fscc-windows/releases/tag/v2.2.9) (6/21/2013)
- Added support for returning multiple frames per read call
- Added support for new card ID's
- Fixed a bug that allowed users to incorrectly be in a stream based mode when only terminating using NTB
- Updated SerialFC to 
[v2.1.5](https://github.com/commtech/serialfc-windows/releases/tag/v2.1.5)

## [2.2.8](https://github.com/commtech/fscc-windows/releases/tag/v2.2.8) (5/21/2013)
- Fixed a bug that allowed users to reset another port's FCR settings when opening an adjacent port.

## [2.2.7](https://github.com/commtech/fscc-windows/releases/tag/v2.2.7) (5/8/2013)
- Fixed a bug that allowed the user to open the FSCC and COM ports at the same time by manually clearing the FCR register
- Updated SerialFC to 
[v2.1.4](https://github.com/commtech/serialfc-windows/releases/tag/v2.1.4)

## [2.2.6](https://github.com/commtech/fscc-windows/releases/tag/v2.2.6) (4/26/2013)
- Fixed a memory leak when using a frame based reception mode
- General memory improvements

## [2.2.5](https://github.com/commtech/fscc-windows/releases/tag/v2.2.5) (4/19/2013)
- Added a default PPM value to the .NET library
- Fixed a bug that caused the channel to be set incorrectly (cause FCR and clock operations to not work on 2nd channel)
- Several small performance improvements when handling incoming frames
- Increase timeout duration to reduce the frequency of timeout errors at lower data rates
- Fixed a bug where streaming data wasn't terminated on memory constraint

## [2.2.4](https://github.com/commtech/fscc-windows/releases/tag/v2.2.4) (4/10/2013)
- Made the CMDR register write only in the .NET library
- Fixed regression buf causing memory to be calculated incorrectly (caused runaway memory use)
- Added initial support for memory-mapped firmware
- Fixed miscellaneous little bugs

## [2.2.3](https://github.com/commtech/fscc-windows/releases/tag/v2.2.3) (3/25/2013)
- Fixed a bug that caused random crashes when using a framing based mode
- Fixed a bug that caused 'append status' to be ignored (full fix)

## [2.2.2](https://github.com/commtech/fscc-windows/releases/tag/v2.2.2) (3/13/2013)
- Fixed a bug in the .NET library that caused extra data to be returned from the Read function
- Fixed a bug that caused 'append status' to be ignored
- Fixed a bug in the .NET library that caused registers to be set incorrectly
- Update SerialFC to 
- [v2.1.2](https://github.com/commtech/serialfc-windows/releases/tag/v2.1.2)

## [2.2.1](https://github.com/commtech/fscc-windows/releases/tag/v2.2.1) (2/25/2013)
- Performance improvements
- Add Python support
- Update SerialFC to 
[v2.1.1](https://github.com/commtech/serialfc-windows/releases/tag/v2.1.1)

## [2.2.0](https://github.com/commtech/fscc-windows/releases/tag/v2.2.0) (2/1/2013)
- Libraries are now built for XP support
- fscc_read_with_timeout now uses CancelIo instead of CancelIoEx to support XP.
- Update SerialFC to 
[v2.1.0](https://github.com/commtech/serialfc-windows/releases/tag/v2.1.0)

## [2.1.0](https://github.com/commtech/fscc-windows/releases/tag/v2.1.0) (1/11/2013)
- Add asynchronous support

## [2.0.2](https://github.com/commtech/fscc-windows/releases/tag/v2.0.2) (11/16/2012)
- Improve port location naming in the Device Manager
- Fixed a bug in fscc_read_with_timeout where a frame wasn't cancelled upon timeout
- Fixed a bug that caused X-Sync mode to not be considered streaming data if there wasn't any framing
- Improved debug prints
- IRQ and memory optimizations
- Fixed a bug causing data loss in transparent mode

## [2.0.1](https://github.com/commtech/fscc-windows/releases/tag/v2.0.1) (11/05/2012)
- Fixed a bug that prevented initiating a read before data had arrived
- Improve debug prints
- Changed each port's priority level to SERIAL_PORT
- Miscellaneous BSOD bug fixes
- Miscellaneous data loss bug fixes
- Change SDDL permissions to not require administrator privileges
- Add more explicit exceptions to the C++ library
- Fixed a C++ library connection bug 
- Fixed a generic library bug preventing setting the clock frequency
        
## [2.0.0](https://github.com/commtech/fscc-windows/releases/tag/v2.0.0) (10/08/2012)
This is the initial release of the 2.0 driver series.
