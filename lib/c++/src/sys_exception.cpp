#include <Windows.h>

#include "sys_exception.hpp"

SystemException::SystemException(unsigned error_code, const std::string& error_msg) : _error_code(error_code), std::runtime_error(error_msg)
{
	if (error_msg.empty() == true) {
		LPVOID buffer;

		FormatMessage( 
				FORMAT_MESSAGE_ALLOCATE_BUFFER | 
				FORMAT_MESSAGE_FROM_SYSTEM | 
				FORMAT_MESSAGE_IGNORE_INSERTS,
				NULL,
				error_code,
				MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), // Default language
				(LPTSTR)&buffer,
				0,
				NULL 
		);

		static_cast<std::runtime_error&>(*this) = std::runtime_error((char *)buffer);

		LocalFree(buffer);
	}
}

unsigned SystemException::error_code(void) const throw()
{
	return _error_code;
}