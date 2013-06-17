#ifndef FSCC_HPP
#define FSCC_HPP

#pragma warning(disable:4290)

#include <Windows.h>
#include <string>

#include "sys_exception.hpp"

namespace FSCC {

	struct Registers;

	class __declspec(dllexport) Port {
	public:
		Port(unsigned port_num) throw(SystemException);
		Port(unsigned port_num, bool overlapped) throw(SystemException);

		Port(const Port &other) throw(SystemException); /* Untested */
		Port& operator=(const Port &other) throw(SystemException); /* Untested */

		virtual ~Port(void) throw(SystemException);

		void SetTxModifiers(unsigned modifiers);
		void SetMemoryCap(const struct fscc_memory_cap &memcap) throw(SystemException);
		struct fscc_memory_cap GetMemoryCap(void) throw(SystemException);
		void SetRegisters(const Registers &regs) throw(SystemException);
		Registers GetRegisters(void) throw(SystemException);
		void SetClockFrequency(unsigned frequency, unsigned ppm=2) throw(SystemException);
		bool GetAppendStatus(void) throw(SystemException);
		void EnableAppendStatus(void) throw(SystemException);
		void DisableAppendStatus(void) throw(SystemException);
		bool GetIgnoreTimeout(void) throw(SystemException);
		void EnableIgnoreTimeout(void) throw(SystemException);
		void DisableIgnoreTimeout(void) throw(SystemException);
		bool GetRxMultiple(void) throw(SystemException);
		void EnableRxMultiple(void) throw(SystemException);
		void DisableRxMultiple(void) throw(SystemException);
		void Purge(bool tx, bool rx) throw(SystemException);

		unsigned Write(const char *buf, unsigned size, OVERLAPPED *o) throw(SystemException);
		unsigned Write(const char *buf, unsigned size) throw(SystemException);
		unsigned Write(const std::string &s) throw(SystemException);
		unsigned Read(char *buf, unsigned size, OVERLAPPED *o) throw(SystemException);
		unsigned Read(char *buf, unsigned size) throw(SystemException);

	protected:
		void init(unsigned port_num, bool overlapped) throw(SystemException);
		void init(unsigned port_num, bool overlapped, HANDLE h) throw();
		void cleanup(void) throw(SystemException);

	private:
		HANDLE _h;
		unsigned _port_num;
		bool _overlapped;
	};
		
	typedef INT64 fscc_register;

	struct __declspec(dllexport) Registers {
		Registers(void);

		/* BAR 0 */
		fscc_register __reserved1[2];
	
		fscc_register FIFOT;
	
		fscc_register __reserved2[2];
	
		fscc_register CMDR;
		fscc_register STAR; /* Read-only */
		fscc_register CCR0;
		fscc_register CCR1;
		fscc_register CCR2;
		fscc_register BGR;
		fscc_register SSR;
		fscc_register SMR;
		fscc_register TSR;
		fscc_register TMR;
		fscc_register RAR;
		fscc_register RAMR;
		fscc_register PPR;
		fscc_register TCR;
		fscc_register VSTR; /* Read-only */
	
		fscc_register __reserved4[1];
	
		fscc_register IMR;
		fscc_register DPLLR;

		/* BAR 2 */
		fscc_register FCR;
	};

} /* namespace FSCC */

#endif