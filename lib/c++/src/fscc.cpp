#include <cstdio>

#include <fscc.h>

#include "fscc.hpp"

namespace FSCC {

/// <param name="port_num">Used to indicate status.</param>
Port::Port(unsigned port_num)
{
	init(port_num, false);
}

Port::Port(unsigned port_num, bool overlapped)
{
	init(port_num, overlapped);
}

Port::~Port(void)
{
	cleanup();
}

void Port::init(unsigned port_num, bool overlapped, HANDLE h)
{
	_port_num = port_num;
	_overlapped = overlapped;
	_h = h;
} 

void Port::init(unsigned port_num, bool overlapped)
{
	_port_num = port_num;
	_overlapped = overlapped;

	int e = fscc_connect(port_num, overlapped, &_h);

	switch (e) {
	case ERROR_SUCCESS:
		break;

	case ERROR_FILE_NOT_FOUND:
		throw PortNotFoundException(port_num);

	case ERROR_ACCESS_DENIED:
		throw InsufficientPermissionsException();

	default:
		throw SystemException(e);
	}
} 

void Port::cleanup(void)
{
	int e = fscc_disconnect(_h);

	if (e)
			throw SystemException(e);
} 

Port::Port(const Port &other)
{
	init(other._port_num, other._overlapped);
}

Port& Port::operator=(const Port &other)
{
	if (this != &other) {
		HANDLE h2;
		int e = fscc_connect(other._port_num, other._overlapped, &h2);

		if (e) {
				throw SystemException(e);
		}
		else {
				cleanup();
				init(other._port_num, other._overlapped, h2);
		}
	}

	return *this;
}

unsigned Port::Write(const char *buf, unsigned size, OVERLAPPED *o)
{
        unsigned bytes_written;

        int e = fscc_write(_h, (char *)buf, size, &bytes_written, o);

        if (e)
                throw SystemException(e);

        return bytes_written;
}

unsigned Port::Write(const char *buf, unsigned size) 
{
        return Write(buf, size, 0);
}

unsigned Port::Write(const std::string &s)
{
        return Write(s.c_str(), s.length());
}

unsigned Port::Read(char *buf, unsigned size, OVERLAPPED *o)
{
        unsigned bytes_read;

        int e = fscc_read(_h, buf, size, &bytes_read, o);

        if (e)
                throw SystemException(e);

        return bytes_read;
}

unsigned Port::Read(char *buf, unsigned size)
{
        return Read(buf, size, (OVERLAPPED *)0);
}

void Port::SetTxModifiers(unsigned modifiers)
{
	int e = fscc_set_tx_modifiers(_h, modifiers);

	if (e)
		throw SystemException(e);
}

void Port::SetMemoryCap(const struct fscc_memory_cap &memcap) throw(SystemException)
{
	int e = fscc_set_memory_cap(_h, &memcap);

	if (e)
		throw SystemException(e);
}

struct fscc_memory_cap Port::GetMemoryCap(void) throw(SystemException)
{
	struct fscc_memory_cap memcap;

	int e = fscc_get_memory_cap(_h, &memcap);

	if (e)
		throw SystemException(e);

	return memcap;
}

void Port::SetRegisters(const Registers &regs) throw(SystemException)
{
	struct fscc_registers r;

	memcpy(&r, &regs, sizeof(r));

	int e = fscc_set_registers(_h, &r);

	if (e)
		throw SystemException(e);
}

Registers Port::GetRegisters(void) throw(SystemException)
{
	Registers regs;

	int e = fscc_get_registers(_h, (struct fscc_registers *)&regs);

	if (e)
		throw SystemException(e);

	return regs;
}

void Port::SetClockFrequency(unsigned frequency, unsigned ppm) throw(SystemException)
{
	int e = fscc_set_clock_frequency(_h, frequency, ppm);

	if (e)
		throw SystemException(e);
}

bool Port::GetAppendStatus(void) throw(SystemException)
{
	BOOL append_status;

	int e = fscc_get_append_status(_h, &append_status);

	if (e)
		throw SystemException(e);

	return append_status != 0;
}

void Port::EnableAppendStatus(void) throw(SystemException)
{
	int e = fscc_enable_append_status(_h);

	if (e)
		throw SystemException(e);
}

void Port::DisableAppendStatus(void) throw(SystemException)
{
	int e = fscc_disable_append_status(_h);

	if (e)
		throw SystemException(e);
}

bool Port::GetIgnoreTimeout(void) throw(SystemException)
{
	BOOL ignore_timeout;

	int e = fscc_get_ignore_timeout(_h, &ignore_timeout);

	if (e)
		throw SystemException(e);

	return ignore_timeout != 0;
}

void Port::EnableIgnoreTimeout(void) throw(SystemException)
{
	int e = fscc_enable_ignore_timeout(_h);

	if (e)
		throw SystemException(e);
}

void Port::DisableIgnoreTimeout(void) throw(SystemException)
{
	int e = fscc_disable_ignore_timeout(_h);

	if (e)
		throw SystemException(e);
}

bool Port::GetRxMultiple(void) throw(SystemException)
{
	BOOL rx_multiple;

	int e = fscc_get_rx_multiple(_h, &rx_multiple);

	if (e)
		throw SystemException(e);

	return rx_multiple != 0;
}

void Port::EnableRxMultiple(void) throw(SystemException)
{
	int e = fscc_enable_rx_multiple(_h);

	if (e)
		throw SystemException(e);
}

void Port::DisableRxMultiple(void) throw(SystemException)
{
	int e = fscc_disable_rx_multiple(_h);

	if (e)
		throw SystemException(e);
}

void Port::Purge(bool tx, bool rx) throw(SystemException)
{
	int e = fscc_purge(_h, tx, rx);

	if (e)
		throw SystemException(e);
}

Registers::Registers()
{
    FIFOT = FSCC_UPDATE_VALUE;

    CMDR = FSCC_UPDATE_VALUE;
    STAR = FSCC_UPDATE_VALUE; /* Read-only */
    CCR0 = FSCC_UPDATE_VALUE;
    CCR1 = FSCC_UPDATE_VALUE;
    CCR2 = FSCC_UPDATE_VALUE;
    BGR = FSCC_UPDATE_VALUE;
    SSR = FSCC_UPDATE_VALUE;
    SMR = FSCC_UPDATE_VALUE;
    TSR = FSCC_UPDATE_VALUE;
    TMR = FSCC_UPDATE_VALUE;
    RAR = FSCC_UPDATE_VALUE;
    RAMR = FSCC_UPDATE_VALUE;
    PPR = FSCC_UPDATE_VALUE;
    TCR = FSCC_UPDATE_VALUE;
    VSTR = FSCC_UPDATE_VALUE;

    IMR = FSCC_UPDATE_VALUE;
    DPLLR = FSCC_UPDATE_VALUE;

    /* BAR 2 */
    FCR = FSCC_UPDATE_VALUE;
}

} /* namespace FSCC */