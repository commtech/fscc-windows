/*! \file */ 
#include <stdio.h>

#include "fscc.h"
#include "calculate-clock-bits.h"

#define MAX_NAME_LENGTH 25

/******************************************************************************/
/*!

  \brief Opens a handle to an FSCC port.

  \param[in] port_num 
    the FSCC port number
  \param[in] overlapped 
    whether you would like to use the port in overlapped mode
  \param[out] h 
    user variable that the port's HANDLE will be assigned to
      
  \return 0 
    if the operation completed successfully
  \return >= 1 
    if the operation failed (see MSDN 'System Error Codes')

  \note
    Opening a handle using this API will only give you access to the
	synchronous functionality of the card. You will need to use the COM ports
	if you would like to use the asynchronous functionality.

*/
/******************************************************************************/
int fscc_connect(unsigned port_num, BOOL overlapped, HANDLE *h)
{
	char name[MAX_NAME_LENGTH];
	DWORD flags_and_attributes = FILE_ATTRIBUTE_NORMAL;

	sprintf_s(name, MAX_NAME_LENGTH, "\\\\.\\FSCC%u", port_num);
        
	if (overlapped)
		flags_and_attributes |= FILE_FLAG_OVERLAPPED;

	*h = CreateFile(name,
			GENERIC_READ | GENERIC_WRITE,
			FILE_SHARE_READ | FILE_SHARE_WRITE,
			NULL,
			OPEN_EXISTING,
			flags_and_attributes,
			NULL
	);

	return (*h != INVALID_HANDLE_VALUE) ? ERROR_SUCCESS : GetLastError();
}

/******************************************************************************/
/*!

  \brief Sets the transmit modifiers for the port.

  \param[in] h 
    HANDLE to the port
  \param[in] modifiers 
    bit mask of the transmit modifier values
      
  \return 0 
    if the operation completed successfully
  \return >= 1 
    if the operation failed (see MSDN 'System Error Codes')

  \note
    XF - Normal transmit - disable modifiers
    XREP - Transmit repeat
    TXT - Transmit on timer
    TXEXT - Transmit on external signal

  \snippet set-tx-modifiers.c Set TXT | XREP
  \snippet set-tx-modifiers.c Set XF

*/
/******************************************************************************/
int fscc_set_tx_modifiers(HANDLE h, unsigned modifiers)
{
	DWORD temp;
	BOOL result;

	result = DeviceIoControl(h, (DWORD)FSCC_SET_TX_MODIFIERS, 
		                     &modifiers, sizeof(modifiers), 
							 NULL, 0, 
							 &temp, (LPOVERLAPPED)NULL);

	return (result == TRUE) ? ERROR_SUCCESS : GetLastError();
}

/******************************************************************************/
/*!

  \brief Gets the transmit modifiers for the port.

  \param[in] h 
    HANDLE to the port
  \param[out] modifiers 
    bit mask of the transmit modifier values
      
  \return 0 
    if the operation completed successfully
  \return >= 1 
    if the operation failed (see MSDN 'System Error Codes')

  \note
    XF - Normal transmit - disable modifiers
    XREP - Transmit repeat
    TXT - Transmit on timer
    TXEXT - Transmit on external signal

  \snippet get-tx-modifiers.c Setup variables
  \snippet get-tx-modifiers.c Get modifiers

*/
/******************************************************************************/
int fscc_get_tx_modifiers(HANDLE h, unsigned *modifiers)
{
	DWORD temp;
	BOOL result;

	result = DeviceIoControl(h, (DWORD)FSCC_GET_TX_MODIFIERS, 
		                     NULL, 0, 
							 modifiers, sizeof(*modifiers), 
							 &temp, (LPOVERLAPPED)NULL);

	return (result == TRUE) ? ERROR_SUCCESS : GetLastError();
}

/******************************************************************************/
/*!

  \brief Sets the FSCC driver's memory caps.

  \param[in] h 
    HANDLE to the port
  \param[in] memcap 
    input and output memory cap values
      
  \return 0 
    if the operation completed successfully
  \return >= 1 
    if the operation failed (see MSDN 'System Error Codes')

  \snippet set-memory-cap.c Setup variables
  \snippet set-memory-cap.c Set memory cap

*/
/******************************************************************************/
int fscc_set_memory_cap(HANDLE h, const struct fscc_memory_cap *memcap)
{
	DWORD temp;
	BOOL result;

	result = DeviceIoControl(h, (DWORD)FSCC_SET_MEMORY_CAP, 
		                     (struct fscc_memory_cap *)memcap, sizeof(*memcap), 
							 NULL, 0, 
							 &temp, (LPOVERLAPPED)NULL);

	return (result == TRUE) ? ERROR_SUCCESS : GetLastError();
}

/******************************************************************************/
/*!

  \brief Gets the FSCC driver's memory caps.

  \param[in] h 
    HANDLE to the port
  \param[in] memcap 
    input and output memory cap values
      
  \return 0 
    if the operation completed successfully
  \return >= 1 
    if the operation failed (see MSDN 'System Error Codes')

  \snippet get-memory-cap.c Setup variables
  \snippet get-memory-cap.c Get memory cap

*/
/******************************************************************************/
int fscc_get_memory_cap(HANDLE h, struct fscc_memory_cap *memcap)
{
	DWORD temp;
	BOOL result;

	result = DeviceIoControl(h, (DWORD)FSCC_GET_MEMORY_CAP, 
		                     NULL, 0, 
							 memcap, sizeof(*memcap), 
							 &temp, (LPOVERLAPPED)NULL);

	return (result == TRUE) ? ERROR_SUCCESS : GetLastError();
}

/******************************************************************************/
/*!

  \brief Sets a port's register values.

  \param[in] h 
    HANDLE to the port
  \param[in] regs
    the new register values
      
  \return 0 
    if the operation completed successfully
  \return >= 1 
    if the operation failed (see MSDN 'System Error Codes')

  \snippet set-registers.c Setup variables
  \snippet set-registers.c Set registers

*/
/******************************************************************************/
int fscc_set_registers(HANDLE h, const struct fscc_registers *regs)
{
	DWORD temp;
	BOOL result;

	result = DeviceIoControl(h, (DWORD)FSCC_SET_REGISTERS, 
		                     (struct fscc_registers *)regs, sizeof(*regs), 
							  NULL, 0, 
							  &temp, (LPOVERLAPPED)NULL);

	return (result == TRUE) ? ERROR_SUCCESS : GetLastError();
}

/******************************************************************************/
/*!

  \brief Gets a port's register values.

  \param[in] h 
    HANDLE to the port
  \param[out] regs
    the register values
      
  \return 0 
    if the operation completed successfully
  \return >= 1 
    if the operation failed (see MSDN 'System Error Codes')

  \snippet get-registers.c Setup variables
  \snippet get-registers.c Get registers

*/
/******************************************************************************/
int fscc_get_registers(HANDLE h, struct fscc_registers *regs)
{
	DWORD temp;
	BOOL result;

	result = DeviceIoControl(h, (DWORD)FSCC_GET_REGISTERS, 
		                     regs, sizeof(*regs), 
							 regs, sizeof(*regs), 
							 &temp, (LPOVERLAPPED)NULL);

	return (result == TRUE) ? ERROR_SUCCESS : GetLastError();
}

/******************************************************************************/
/*!

  \brief Gets a port's append status value.

  \param[in] h 
    HANDLE to the port
  \param[out] status
    the append status value
      
  \return 0 
    if the operation completed successfully
  \return >= 1 
    if the operation failed (see MSDN 'System Error Codes')

  \snippet get-append-status.c Setup variables
  \snippet get-append-status.c Get append status

*/
/******************************************************************************/
int fscc_get_append_status(HANDLE h, BOOL *status)
{
	DWORD temp;
	BOOL result;

	result = DeviceIoControl(h, (DWORD)FSCC_GET_APPEND_STATUS, 
		                     NULL, 0, 
							 status, sizeof(*status), 
							 &temp, (LPOVERLAPPED)NULL);

	return (result == TRUE) ? ERROR_SUCCESS : GetLastError();
}

/******************************************************************************/
/*!

  \brief Enable appending the status to the received data.

  \param[in] h 
    HANDLE to the port
      
  \return 0 
    if the operation completed successfully
  \return >= 1 
    if the operation failed (see MSDN 'System Error Codes')

  \snippet set-append-status.c Enable append status

*/
/******************************************************************************/
int fscc_enable_append_status(HANDLE h)
{
	DWORD temp;
	BOOL result;

	result = DeviceIoControl(h, (DWORD)FSCC_ENABLE_APPEND_STATUS, 
		                     NULL, 0, 
							 NULL, 0, 
							 &temp, (LPOVERLAPPED)NULL);

	return (result == TRUE) ? ERROR_SUCCESS : GetLastError();
}

/******************************************************************************/
/*!

  \brief Disable appending the status to the received data.

  \param[in] h 
    HANDLE to the port
      
  \return 0 
    if the operation completed successfully
  \return >= 1 
    if the operation failed (see MSDN 'System Error Codes')

  \snippet set-append-status.c Disable append status

*/
/******************************************************************************/
int fscc_disable_append_status(HANDLE h)
{
	DWORD temp;
	BOOL result;

	result = DeviceIoControl(h, (DWORD)FSCC_DISABLE_APPEND_STATUS, 
		                     NULL, 0, 
							 NULL, 0, 
							 &temp, (LPOVERLAPPED)NULL);

	return (result == TRUE) ? ERROR_SUCCESS : GetLastError();
}

/******************************************************************************/
/*!

  \brief Gets a port's append timestamp value.

  \param[in] h 
    HANDLE to the port
  \param[out] timestamp
    the append timestamp value
      
  \return 0 
    if the operation completed successfully
  \return >= 1 
    if the operation failed (see MSDN 'System Error Codes')

*/
/******************************************************************************/
int fscc_get_append_timestamp(HANDLE h, BOOL *timestamp)
{
	DWORD temp;
	BOOL result;

	result = DeviceIoControl(h, (DWORD)FSCC_GET_APPEND_TIMESTAMP, 
		                     NULL, 0, 
							 timestamp, sizeof(*timestamp), 
							 &temp, (LPOVERLAPPED)NULL);

	return (result == TRUE) ? ERROR_SUCCESS : GetLastError();
}

/******************************************************************************/
/*!

  \brief Enable appending the timestamp to the received data.

  \param[in] h 
    HANDLE to the port
      
  \return 0 
    if the operation completed successfully
  \return >= 1 
    if the operation failed (see MSDN 'System Error Codes')

*/
/******************************************************************************/
int fscc_enable_append_timestamp(HANDLE h)
{
	DWORD temp;
	BOOL result;

	result = DeviceIoControl(h, (DWORD)FSCC_ENABLE_APPEND_TIMESTAMP, 
		                     NULL, 0, 
							 NULL, 0, 
							 &temp, (LPOVERLAPPED)NULL);

	return (result == TRUE) ? ERROR_SUCCESS : GetLastError();
}

/******************************************************************************/
/*!

  \brief Disable appending the timestamp to the received data.

  \param[in] h 
    HANDLE to the port
      
  \return 0 
    if the operation completed successfully
  \return >= 1 
    if the operation failed (see MSDN 'System Error Codes')

*/
/******************************************************************************/
int fscc_disable_append_timestamp(HANDLE h)
{
	DWORD temp;
	BOOL result;

	result = DeviceIoControl(h, (DWORD)FSCC_DISABLE_APPEND_TIMESTAMP, 
		                     NULL, 0, 
							 NULL, 0, 
							 &temp, (LPOVERLAPPED)NULL);

	return (result == TRUE) ? ERROR_SUCCESS : GetLastError();
}

/******************************************************************************/
/*!

  \brief Gets a port's ignore timeout value.

  \param[in] h 
    HANDLE to the port
  \param[out] status
    the append status value
      
  \return 0 
    if the operation completed successfully
  \return >= 1 
    if the operation failed (see MSDN 'System Error Codes')

  \snippet get-ignore-timeout.c Setup variables
  \snippet get-ignore-timeout.c Get ignore timeout

*/
/******************************************************************************/
int fscc_get_ignore_timeout(HANDLE h, BOOL *status)
{
	DWORD temp;
	BOOL result;

	result = DeviceIoControl(h, (DWORD)FSCC_GET_IGNORE_TIMEOUT, 
		                     NULL, 0, 
							 status, sizeof(*status), 
							 &temp, (LPOVERLAPPED)NULL);

	return (result == TRUE) ? ERROR_SUCCESS : GetLastError();
}

/******************************************************************************/
/*!

  \brief Ignore card timeouts.

  \param[in] h 
    HANDLE to the port
      
  \return 0 
    if the operation completed successfully
  \return >= 1 
    if the operation failed (see MSDN 'System Error Codes')

  \snippet set-ignore-timeout.c Enable ignore timeout

*/
/******************************************************************************/
int fscc_enable_ignore_timeout(HANDLE h)
{
	DWORD temp;
	BOOL result;

	result = DeviceIoControl(h, (DWORD)FSCC_ENABLE_IGNORE_TIMEOUT, 
		                     NULL, 0, 
							 NULL, 0, 
							 &temp, (LPOVERLAPPED)NULL);

	return (result == TRUE) ? ERROR_SUCCESS : GetLastError();
}

/******************************************************************************/
/*!

  \brief Disable ignore timeout.

  \param[in] h 
    HANDLE to the port
      
  \return 0 
    if the operation completed successfully
  \return >= 1 
    if the operation failed (see MSDN 'System Error Codes')

  \snippet set-ignore-timeout.c Disable ignore timeout

*/
/******************************************************************************/
int fscc_disable_ignore_timeout(HANDLE h)
{
	DWORD temp;
	BOOL result;

	result = DeviceIoControl(h, (DWORD)FSCC_DISABLE_IGNORE_TIMEOUT, 
		                     NULL, 0, 
							 NULL, 0, 
							 &temp, (LPOVERLAPPED)NULL);

	return (result == TRUE) ? ERROR_SUCCESS : GetLastError();
}

/******************************************************************************/
/*!

  \brief Gets a port's rx multiple value.

  \param[in] h 
    HANDLE to the port
  \param[out] status
    the rx multiple value
      
  \return 0 
    if the operation completed successfully
  \return >= 1 
    if the operation failed (see MSDN 'System Error Codes')

*/
/******************************************************************************/
int fscc_get_rx_multiple(HANDLE h, BOOL *status)
{
	DWORD temp;
	BOOL result;

	result = DeviceIoControl(h, (DWORD)FSCC_GET_RX_MULTIPLE, 
		                     NULL, 0, 
							 status, sizeof(*status), 
							 &temp, (LPOVERLAPPED)NULL);

	return (result == TRUE) ? ERROR_SUCCESS : GetLastError();
}

/******************************************************************************/
/*!

  \brief Receive multiple frames per read call.

  \param[in] h 
    HANDLE to the port
      
  \return 0 
    if the operation completed successfully
  \return >= 1 
    if the operation failed (see MSDN 'System Error Codes')

*/
/******************************************************************************/
int fscc_enable_rx_multiple(HANDLE h)
{
	DWORD temp;
	BOOL result;

	result = DeviceIoControl(h, (DWORD)FSCC_ENABLE_RX_MULTIPLE, 
		                     NULL, 0, 
							 NULL, 0, 
							 &temp, (LPOVERLAPPED)NULL);

	return (result == TRUE) ? ERROR_SUCCESS : GetLastError();
}

/******************************************************************************/
/*!

  \brief Disable rx multiple.

  \param[in] h 
    HANDLE to the port
      
  \return 0 
    if the operation completed successfully
  \return >= 1 
    if the operation failed (see MSDN 'System Error Codes')

*/
/******************************************************************************/
int fscc_disable_rx_multiple(HANDLE h)
{
	DWORD temp;
	BOOL result;

	result = DeviceIoControl(h, (DWORD)FSCC_DISABLE_RX_MULTIPLE, 
		                     NULL, 0, 
							 NULL, 0, 
							 &temp, (LPOVERLAPPED)NULL);

	return (result == TRUE) ? ERROR_SUCCESS : GetLastError();
}

/******************************************************************************/
/*!

  \brief Clears the transmit and/or receive data out of the card.

  \param[in] h 
    HANDLE to the port
  \param[in] tx
    whether to clear the transmit data out
  \param[in] rx
    whether to clear the receive data out
      
  \return 0 
    if the operation completed successfully
  \return >= 1 
    if the operation failed (see MSDN 'System Error Codes')

  \note
    Any pending transmit data will not be transmited upon a purge.

  \note
    If you receive a ERROR_TIMEOUT you are likely at a speed too slow (~35 Hz)
        for this driver. You will need to contact Commtech to get a custom driver.

  \snippet purge.c Purge TX
  \snippet purge.c Purge RX
  \snippet purge.c Purge both TX & RX

*/
/******************************************************************************/
int fscc_purge(HANDLE h, BOOL tx, BOOL rx)
{        
	BOOL result;
	DWORD temp;

	if (tx) {
		result = DeviceIoControl(h, (DWORD)FSCC_PURGE_TX, 
			                     NULL, 0, 
								 NULL, 0, 
								 &temp, NULL);

		if (result == FALSE)
				return GetLastError();
	}
        
	if (rx) {
		result = DeviceIoControl(h, (DWORD)FSCC_PURGE_RX, 
			                     NULL, 0, 
								 NULL, 0, 
								 &temp, NULL);

		if (result == FALSE) {
				int e = GetLastError();

				switch (e) {
				case ERROR_INVALID_PARAMETER:
						return ERROR_TIMEOUT;

				default:
						return e;
				}
		}
	}

	return ERROR_SUCCESS;
}

/******************************************************************************/
/*!

  \brief Sets a port's clock frequency.

  \param[in] h 
    HANDLE to the port
  \param[in] frequency
    the new clock frequency
  \param[in] ppm
    See note.
      
  \return 0 
    if the operation completed successfully
  \return >= 1 
    if the operation failed (see MSDN 'System Error Codes')

  \snippet set-clock-frequency.c Set clock frequency

  \note
    PPM has been deprecated and will be removed in a future release. This value
    is ignored in the mean time.

    Lower clock rates (less than 1 MHz for example) can take a long time for 
    the frequency generator to finish. If you run into this situation we 
    recommend using a larger frequency and then dividing it down to your 
    desired baud rate using the BGR register.

*/
/******************************************************************************/
int fscc_set_clock_frequency(HANDLE h, unsigned frequency, unsigned ppm)
{
	DWORD temp;
	BOOL result;
	unsigned char clock_bits[20];

	calculate_clock_bits(frequency, ppm, clock_bits);

	result = DeviceIoControl(h, (DWORD)FSCC_SET_CLOCK_BITS, 
		                     &clock_bits, sizeof(clock_bits), 
							 NULL, 0, 
							 &temp, (LPOVERLAPPED)NULL);

	return (result == TRUE) ? ERROR_SUCCESS : GetLastError();
}
 
/******************************************************************************/
/*!

  \brief Transmits data out of a port.

  \param[in] h 
    HANDLE to the port
  \param[in] buf
    the buffer containing the data to transmit
  \param[in] size
    the number of bytes to transmit from 'buf'
  \param[out] bytes_written
    the input variable to store how many bytes were actually written
  \param[in,out] o
    OVERLAPPED structure for asynchronous operation
      
  \return 0 
    if the operation completed successfully
  \return >= 1 
    if the operation failed (see MSDN 'System Error Codes')

*/
/******************************************************************************/
int fscc_write(HANDLE h, char *buf, unsigned size, unsigned *bytes_written, OVERLAPPED *o)
{
	BOOL result;
        
	result = WriteFile(h, buf, size, (DWORD*)bytes_written, o);

	return (result == TRUE) ? ERROR_SUCCESS : GetLastError();
}

/******************************************************************************/
/*!

  \brief Reads data out of a port.

  \param[in] h 
    HANDLE to the port
  \param[in] buf
    the input buffer used to store the receive data
  \param[in] size
    the maximum number of bytes to read in (typically sizeof(buf))
  \param[out] bytes_read
    the user variable to store how many bytes were actually read
  \param[in,out] o
    OVERLAPPED structure for asynchronous operation
      
  \return 0 
    if the operation completed successfully
  \return >= 1 
    if the operation failed (see MSDN 'System Error Codes')

*/
/******************************************************************************/
int fscc_read(HANDLE h, char *buf, unsigned size, unsigned *bytes_read, OVERLAPPED *o)
{
	BOOL result;

	result = ReadFile(h, buf, size, (DWORD*)bytes_read, o);

	return (result == TRUE) ? ERROR_SUCCESS : GetLastError();
}


/******************************************************************************/
/*!

  \note
    Due to supporting Windows XP we have to use CancelIo() instead of 
    CancelIoEx(). As a biproduct if there is a WAIT_TIMEOUT both pending
    transmit and receive IO will be cancelled instead of just receiving. If you
    are using Vista or newer you can change this to use CancelIoEx and you will
    only cancel the receiving IO.

*/
/******************************************************************************/
int fscc_read_with_timeout(HANDLE h, char *buf, unsigned size, 
                             unsigned *bytes_read, unsigned timeout)
{
        OVERLAPPED o;
        //DWORD temp;
        BOOL result;

        memset(&o, 0, sizeof(o));

        o.hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);

        if (o.hEvent == NULL)
			return GetLastError();
        
        result = ReadFile(h, buf, size, (DWORD*)bytes_read, &o);
        
        if (result == FALSE) {
            DWORD status;
            int e;

            /* There was an actual error instead of a pending read */
            if ((e = GetLastError()) != ERROR_IO_PENDING) {
                CloseHandle(o.hEvent);
                return e;
            }

            do {
                status = WaitForSingleObject(o.hEvent, timeout);

                switch (status) {
                case WAIT_TIMEOUT:
                        *bytes_read = 0;
                        /* Switch to CancelIoEx if using Vista or higher and prefer the
                           way CancelIoEx operates. */
                        /* CancelIoEx(h, &o); */
                        CancelIo(h);
                        CloseHandle(o.hEvent);
                        return ERROR_SUCCESS;

                case WAIT_ABANDONED:
                        CloseHandle(o.hEvent);
                        return 1; //TODO: READFILE_ABANDONED;

                case WAIT_FAILED:
                        e = GetLastError();
                        CloseHandle(o.hEvent);
                        return e;
                }
            } 
            while (status != WAIT_OBJECT_0);

            GetOverlappedResult(h, &o, (DWORD *)bytes_read, TRUE);
        }

        CloseHandle(o.hEvent);

        return ERROR_SUCCESS;
}

/******************************************************************************/
/*!

  \brief Closes the handle to an FSCC port.

  \param[in] h 
    HANDLE to the port
      
  \return 0 
    if closing the port completed successfully
  \return >= 1 
    if the operation failed (see MSDN 'System Error Codes')

*/
/******************************************************************************/
int fscc_disconnect(HANDLE h)
{
	BOOL result;

	result = CloseHandle(h);

	return (result == TRUE) ? ERROR_SUCCESS : GetLastError();
}