# ChangeLog

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
