#ifndef SYSEXEPTION_HPP
#define SYSEXEPTION_HPP

#include <stdexcept>

class SystemException : public std::runtime_error
{
public:
        SystemException(unsigned error_code); //TODO throw(?)

        unsigned error_code(void) const throw();

private:
        unsigned _error_code;
};

#endif