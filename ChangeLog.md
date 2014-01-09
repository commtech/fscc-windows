# ChangeLog

## v2.7.2 _(01/09/2014)_
- Added checks to prevent status/timestamp while in streaming mode
- Updated SerialFC to 
[v2.4.4](https://github.com/commtech/serialfc-windows/blob/master/ChangeLog.txt)

## v2.7.1 _(12/12/2013)_
- Updated SerialFC to
[v2.4.3](https://github.com/commtech/serialfc-windows/blob/master/ChangeLog.txt)

## v2.7.0 _(12/12/2013)_
- Added ability to track interrupts

## v2.6.4 _(11/26/2013)_
- Fixed incorrectly setting the clock on 232 and green cards
- Included latest library versions
- Updated SerialFC to 
[v2.4.2](https://github.com/commtech/serialfc-windows/blob/master/ChangeLog.txt)

## v2.6.3 _(11/11/2013)_
- Fixed a bug that prevented small frame sizes from being calculated correctly when approaching the memory limit
      
## v2.6.2 _(11/11/2013)_
- Fixed a regresion from v2.5.0 where the computer would freeze while reading transparent data
- Fixed a bug where outgoing frames wouldn't get cleared fast enough

## v2.6.1 _(10/25/2013)_
- Updated SerialFC to 
[v2.4.1](https://github.com/commtech/serialfc-windows/blob/master/ChangeLog.txt)
      
## v2.6.0 _(10/25/2013)_
- Updated SerialFC to 
[v2.4.0](https://github.com/commtech/serialfc-windows/blob/master/ChangeLog.txt)

## v2.5.0 _(10/7/2013)_
- Added wait on write support

## v2.4.3 _(9/24/2013)_
- Fixed the test program not compiling correctly

## v2.4.2 _(9/20/2013)_
- Switch to using external code libraries
- Updated SerialFC to 
[v2.2.2](https://github.com/commtech/serialfc-windows/blob/master/ChangeLog.txt)

## v2.4.1 _(8/28/2013)_
- Fixed a bug that would allow you to return more data than your buffer size (introduced in 2.4.0)
- Documentation and library improvements

## v2.4.0 _(8/27/2013)_
- Added support for timestamps

## v2.3.1 _(8/21/2013)_
- Added missing device ID's 22 and 27

## v2.3.0 _(8/13/2013)_
- Updated SerialFC to 
[v2.2.0](https://github.com/commtech/serialfc-windows/blob/master/ChangeLog.txt)

## v2.2.10 _(7/23/2013)_
- Updated SerialFC to 
[v2.1.6](https://github.com/commtech/serialfc-windows/blob/master/ChangeLog.txt)

## v2.2.9 _(6/21/2013)_
- Added support for returning multiple frames per read call
- Added support for new card ID's
- Fixed a bug that allowed users to incorrectly be in a stream based mode when only terminating using NTB
- Updated SerialFC to 
[v2.1.5](https://github.com/commtech/serialfc-windows/blob/master/ChangeLog.txt)

## v2.2.8 _(5/21/2013)_
- Fixed a bug that allowed users to reset another port's FCR settings when opening an adjacent port.

## v2.2.7 _(5/8/2013)_
- Fixed a bug that allowed the user to open the FSCC and COM ports at the same time by manually clearing the FCR register
- Updated SerialFC to 
[v2.1.4](https://github.com/commtech/serialfc-windows/blob/master/ChangeLog.txt)

## v2.2.6 _(4/26/2013)_
- Fixed a memory leak when using a frame based reception mode
- General memory improvements

## v2.2.5 _(4/19/2013)_
- Added a default PPM value to the .NET library
- Fixed a bug that caused the channel to be set incorrectly (cause FCR and clock operations to not work on 2nd channel)
- Several small performance improvements when handling incoming frames
- Increase timeout duration to reduce the frequency of timeout errors at lower data rates
- Fixed a bug where streaming data wasn't terminated on memory constraint

## v2.2.4 _(4/10/2013)_
- Made the CMDR register write only in the .NET library
- Fixed regression buf causing memory to be calculated incorrectly (caused runaway memory use)
- Added initial support for memory-mapped firmware
- Fixed miscellaneous little bugs

## v2.2.3 _(3/25/2013)_
- Fixed a bug that caused random crashes when using a framing based mode
- Fixed a bug that caused 'append status' to be ignored (full fix)

## v2.2.2 _(3/13/2013)_
- Fixed a bug in the .NET library that caused extra data to be returned from the Read function
- Fixed a bug that caused 'append status' to be ignored
- Fixed a bug in the .NET library that caused registers to be set incorrectly
- Update SerialFC to 
- [v2.1.2](https://github.com/commtech/serialfc-windows/blob/master/ChangeLog.txt)

## v2.2.1 _(2/25/2013)_
- Performance improvements
- Add Python support
- Update SerialFC to 
[v2.1.1](https://github.com/commtech/serialfc-windows/blob/master/ChangeLog.txt)

## v2.2.0 _(2/1/2013)_
- Update SerialFC to 
[v2.1.0](https://github.com/commtech/serialfc-windows/blob/master/ChangeLog.txt)
- Libraries are now built for XP support
- fscc_read_with_timeout now uses CancelIo instead of CancelIoEx to support XP.

## v2.1.0 _(1/11/2013)_
- Add asynchronous support

## v2.0.2 _(11/16/2012)_
- Improve port location naming in the Device Manager
- Fixed a bug in fscc_read_with_timeout where a frame wasn't cancelled upon timeout
- Fixed a bug that caused X-Sync mode to not be considered streaming data if there wasn't any framing
- Improved debug prints
- IRQ and memory optimizations
- Fixed a bug causing data loss in transparent mode

## v2.0.1 _(11/05/2012)_
- Fixed a bug that prevented initiating a read before data had arrived
- Improve debug prints
- Changed each port's priority level to SERIAL_PORT
- Miscellaneous BSOD bug fixes
- Miscellaneous data loss bug fixes
- Change SDDL permissions to not require administrator privileges
- Add more explicit exceptions to the C++ library
- Fixed a C++ library connection bug 
- Fixed a generic library bug preventing setting the clock frequency
        
## v2.0.0 _(10/08/2012)_
This is the initial release of the 2.0 driver series.
