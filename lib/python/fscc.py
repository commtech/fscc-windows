"""
        Copyright (C) 2012 Commtech, Inc.

        This file is part of fscc-linux.

        fscc-linux is free software: you can redistribute it and/or modify
        it under the terms of the GNU General Public License as published by
        the Free Software Foundation, either version 3 of the License, or
        (at your option) any later version.

        fscc-linux is distributed in the hope that it will be useful,
        but WITHOUT ANY WARRANTY; without even the implied warranty of
        MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
        GNU General Public License for more details.

        You should have received a copy of the GNU General Public License
        along with fscc-linux.  If not, see <http://www.gnu.org/licenses/>.

"""

import struct
import select
import errno
import io
import os
import win32file

def CTL_CODE(DeviceType, Function, Method, Access):
    return (DeviceType<<16) | (Access << 14) | (Function << 2) | Method

FSCC_IOCTL_MAGIC = 0x8018
METHOD_BUFFERED = 0
FILE_ANY_ACCESS = 0


FSCC_GET_REGISTERS = CTL_CODE(FSCC_IOCTL_MAGIC, 0x800, METHOD_BUFFERED, FILE_ANY_ACCESS)
FSCC_SET_REGISTERS = CTL_CODE(FSCC_IOCTL_MAGIC, 0x801, METHOD_BUFFERED, FILE_ANY_ACCESS)

FSCC_PURGE_TX = CTL_CODE(FSCC_IOCTL_MAGIC, 0x802, METHOD_BUFFERED, FILE_ANY_ACCESS)
FSCC_PURGE_RX = CTL_CODE(FSCC_IOCTL_MAGIC, 0x803, METHOD_BUFFERED, FILE_ANY_ACCESS)

FSCC_ENABLE_APPEND_STATUS = CTL_CODE(FSCC_IOCTL_MAGIC, 0x804, METHOD_BUFFERED, FILE_ANY_ACCESS)
FSCC_DISABLE_APPEND_STATUS = CTL_CODE(FSCC_IOCTL_MAGIC, 0x805, METHOD_BUFFERED, FILE_ANY_ACCESS)
FSCC_GET_APPEND_STATUS = CTL_CODE(FSCC_IOCTL_MAGIC, 0x80D, METHOD_BUFFERED, FILE_ANY_ACCESS)

FSCC_SET_MEMORY_CAP = CTL_CODE(FSCC_IOCTL_MAGIC, 0x806, METHOD_BUFFERED, FILE_ANY_ACCESS)
FSCC_GET_MEMORY_CAP = CTL_CODE(FSCC_IOCTL_MAGIC, 0x807, METHOD_BUFFERED, FILE_ANY_ACCESS)

FSCC_SET_CLOCK_BITS = CTL_CODE(FSCC_IOCTL_MAGIC, 0x808, METHOD_BUFFERED, FILE_ANY_ACCESS)

FSCC_ENABLE_IGNORE_TIMEOUT = CTL_CODE(FSCC_IOCTL_MAGIC, 0x80A, METHOD_BUFFERED, FILE_ANY_ACCESS)
FSCC_DISABLE_IGNORE_TIMEOUT = CTL_CODE(FSCC_IOCTL_MAGIC, 0x80B, METHOD_BUFFERED, FILE_ANY_ACCESS)
FSCC_GET_IGNORE_TIMEOUT = CTL_CODE(FSCC_IOCTL_MAGIC, 0x80F, METHOD_BUFFERED, FILE_ANY_ACCESS)

FSCC_SET_TX_MODIFIERS = CTL_CODE(FSCC_IOCTL_MAGIC, 0x80C, METHOD_BUFFERED, FILE_ANY_ACCESS)
FSCC_GET_TX_MODIFIERS = CTL_CODE(FSCC_IOCTL_MAGIC, 0x80E, METHOD_BUFFERED, FILE_ANY_ACCESS)

FSCC_UPDATE_VALUE = -2

XF, XREP, TXT, TXEXT = 0, 1, 2, 4


class InvalidPortError(Exception):
    """Exception for the situation where a non FSCC port is opened."""
    def __init__(self, file_name):
        self.file_name = file_name

    def __str__(self):
        return "'%s' is not an FSCC port" % self.file_name


class InvalidRegisterError(Exception):
    """Exception for the situation where an invalid register is modified."""
    def __init__(self, register_name):
        self.register_name = register_name

    def __str__(self):
        return "'%s' is an invalid register" % self.register_name


class ReadonlyRegisterError(InvalidRegisterError):
    """Exception for the situation where a read only register is modified."""
    def __str__(self):
        return "'%s' is a readonly register" % self.register_name


class Port(io.FileIO):
    """Commtech FSCC port."""
    class Registers(object):
        """Registers on the FSCC port."""
        register_names = ["FIFOT", "CMDR", "STAR", "CCR0", "CCR1", "CCR2",
                          "BGR", "SSR", "SMR", "TSR", "TMR", "RAR", "RAMR",
                          "PPR", "TCR", "VSTR", "IMR", "DPLLR", "FCR"]

        readonly_register_names = ["STAR", "VSTR"]
        writeonly_register_names = ["CMDR"]

        editable_register_names = [r for r in register_names if r not in
                                   ["STAR", "VSTR"]]

        def __init__(self, port=None):
            self.port = port
            self._clear_registers()

            for register in self.register_names:
                self._add_register(register)

        def __iter__(self):
            registers = [-1, -1, self._FIFOT, -1, -1, self._CMDR, self._STAR,
                         self._CCR0, self._CCR1, self._CCR2, self._BGR,
                         self._SSR, self._SMR, self._TSR, self._TMR, self._RAR,
                         self._RAMR, self._PPR, self._TCR, self._VSTR, -1,
                         self._IMR, self._DPLLR, self._FCR]

            for register in registers:
                yield register

        def _add_register(self, register):
            """Dynamically add a way to edit a register to the port."""
            if register not in self.writeonly_register_names:
                fget = lambda self: self._get_register(register)
            else:
                fget = None

            if register not in self.readonly_register_names:
                fset = lambda self, value: self._set_register(register, value)
            else:
                fset = None

            setattr(self.__class__, register, property(fget, fset, None, ""))

        def _get_register(self, register):
            """Gets the value of a register."""
            if self.port:
                self._clear_registers()
                setattr(self, "_%s" % register, FSCC_UPDATE_VALUE)
                self._get_registers()

            return getattr(self, "_%s" % register)

        def _set_register(self, register, value):
            """Sets the value of a register."""
            if self.port:
                self._clear_registers()

            setattr(self, "_%s" % register, value)

            if self.port:
                self._set_registers()

        def _clear_registers(self):
            """Clears the stored register values."""
            for register in self.register_names:
                setattr(self, "_%s" % register, -1)

        def _get_registers(self):
            """Gets all of the register values."""
            if not self.port:
                return

            registers = list(self)

            buf_size = struct.calcsize("q" * len(registers))
            buf = win32file.DeviceIoControl(self.port.hComPort, FSCC_GET_REGISTERS, 
                             struct.pack("q" * len(registers), *registers), buf_size, None)
            regs = struct.unpack("q" * len(registers), buf)

            for i, register in enumerate(registers):
                if register != -1:
                    self._set_register_by_index(i, regs[i])

        def _set_registers(self):
            """Sets all of the register values."""
            if not self.port:
                return

            registers = list(self)

            buf_size = struct.calcsize("q" * len(registers))
            win32file.DeviceIoControl(self.port.hComPort, FSCC_SET_REGISTERS, 
                             struct.pack("q" * len(registers), *registers), buf_size, None)

        def _set_register_by_index(self, index, value):
            """Sets the value of a register by it's index."""
            data = [("FIFOT", 2), ("CMDR", 5), ("STAR", 6), ("CCR0", 7),
                    ("CCR1", 8), ("CCR2", 9), ("BGR", 10), ("SSR", 11),
                    ("SMR", 12), ("TSR", 13), ("TMR", 14), ("RAR", 15),
                    ("RAMR", 16), ("PPR", 17), ("TCR", 18), ("VSTR", 19),
                    ("IMR", 21), ("DPLLR", 22), ("FCR", 23)]

            for r, i in data:
                if i == index:
                    setattr(self, "_%s" % r, value)

        # Note: clears registers
        def import_from_file(self, import_file):
            """Reads and stores the register values from a file."""
            import_file.seek(0, os.SEEK_SET)

            for line in import_file:
                try:
                    line = str(line, encoding='utf8')
                except:
                    pass

                if line[0] != "#":
                    d = line.split("=")
                    reg_name, reg_val = d[0].strip().upper(), d[1].strip()

                    if reg_name not in self.register_names:
                        raise InvalidRegisterError(reg_name)

                    if reg_name not in self.editable_register_names:
                        raise ReadonlyRegisterError(reg_name)

                    if reg_val[0] == "0" and reg_val[1] in ["x", "X"]:
                        reg_val = int(reg_val, 16)
                    else:
                        reg_val = int(reg_val)

                    setattr(self, reg_name, reg_val)

        def export_to_file(self, export_file):
            """Writes the current register values to a file."""
            for register_name in self.editable_register_names:
                if register_name in self.writeonly_register_names:
                    continue

                value = getattr(self, register_name)

                if value >= 0:
                    export_file.write("%s = 0x%08x\n" % (register_name, value))

    def __init__(self, port_name, mode, append_status=False):
        file_name = "\\\\.\\" + port_name

        if not os.path.exists(file_name):
            raise IOError(errno.ENOENT, os.strerror(errno.ENOENT), file_name)

        self.hComPort = win32file.CreateFile(file_name,
               win32file.GENERIC_READ | win32file.GENERIC_WRITE,
               0, # exclusive access
               None, # no security
               win32file.OPEN_EXISTING,
               win32file.FILE_ATTRIBUTE_NORMAL | win32file.FILE_FLAG_OVERLAPPED,
               0)

        io.FileIO.__init__(self, file_name, mode)

        self.registers = Port.Registers(self)

        try:
        	self.append_status = append_status
       	except IOError as e:
            raise InvalidPortError(file_name)

    def purge(self, tx=True, rx=True):
        """Removes unsent and/or unread data from the card."""
        if (tx):
            try:
                win32file.DeviceIoControl(self.hComPort, FSCC_PURGE_TX, None, 0, None)
            except IOError as e:
                raise e
        if (rx):
            try:
                win32file.DeviceIoControl(self.hComPort, FSCC_PURGE_RX, None, 0, None)
            except IOError as e:
                raise e

    def _set_append_status(self, append_status):
        """Sets the value of the append status setting."""
        if append_status:
            win32file.DeviceIoControl(self.hComPort, FSCC_ENABLE_APPEND_STATUS, None, 0, None)
        else:
            win32file.DeviceIoControl(self.hComPort, FSCC_DISABLE_APPEND_STATUS, None, 0, None)

    def _get_append_status(self):
        """Gets the value of the append status setting."""
        buf_size = struct.calcsize("?")
        buf = win32file.DeviceIoControl(self.hComPort, FSCC_GET_APPEND_STATUS, None, buf_size, None)
        value = struct.unpack("?", buf)

        if (value[0]):
            return True
        else:
            return False

    append_status = property(fset=_set_append_status, fget=_get_append_status)

    def _set_memcap(self, input_memcap, output_memcap):
        """Sets the value of the memory cap setting."""
        buf_size = struct.calcsize("i" * 2)
        win32file.DeviceIoControl(self.hComPort, FSCC_SET_MEMORY_CAP, 
                   struct.pack("i" * 2, input_memcap, output_memcap), buf_size, None)

    def _get_memcap(self):
        """Gets the value of the memory cap setting."""
        buf_size = struct.calcsize("i" * 2)
        buf = win32file.DeviceIoControl(self.hComPort, FSCC_GET_MEMORY_CAP, 
                         struct.pack("i" * 2, -1, -1), buf_size, None)

        return struct.unpack("i" * 2, buf)

    def _set_imemcap(self, memcap):
        """Sets the value of the input memory cap setting."""
        self._set_memcap(memcap, -1)

    def _get_imemcap(self):
        """Gets the value of the output memory cap setting."""
        return self._get_memcap()[0]

    input_memory_cap = property(fset=_set_imemcap, fget=_get_imemcap)

    def _set_omemcap(self, memcap):
        """Sets the value of the output memory cap setting."""
        self._set_memcap(-1, memcap)

    def _get_omemcap(self):
        """Gets the value of the output memory cap setting."""
        return self._get_memcap()[1]

    output_memory_cap = property(fset=_set_omemcap, fget=_get_omemcap)

    def _set_ignore_timeout(self, ignore_timeout):
        """Sets the value of the ignore timeout setting."""
        if ignore_timeout:
            win32file.DeviceIoControl(self.hComPort, FSCC_ENABLE_IGNORE_TIMEOUT, None, 0, None)
        else:
            win32file.DeviceIoControl(self.hComPort, FSCC_DISABLE_IGNORE_TIMEOUT, None, 0, None)

    def _get_ignore_timeout(self):
        """Gets the value of the ignore timeout setting."""
        buf_size = struct.calcsize("?")
        buf = win32file.DeviceIoControl(self.hComPort, FSCC_GET_IGNORE_TIMEOUT, None, buf_size, None)
        value = struct.unpack("?", buf)

        return value[0]

    ignore_timeout = property(fset=_set_ignore_timeout,
                              fget=_get_ignore_timeout)

    def _set_tx_modifiers(self, tx_modifiers):
        """Sets the value of the transmit modifiers setting."""
        value = struct.pack("I", tx_modifiers)
        win32file.DeviceIoControl(self.hComPort, FSCC_SET_TX_MODIFIERS, value, 0, None)

    def _get_tx_modifiers(self):
        """Gets the value of the transmit modifiers setting."""
        buf_size = struct.calcsize("I")
        buf = win32file.DeviceIoControl(self.hComPort, FSCC_GET_TX_MODIFIERS, None, buf_size, None)
        value = struct.unpack("I", buf)

        return value[0]

    tx_modifiers = property(fset=_set_tx_modifiers, fget=_get_tx_modifiers)

    def read(self, max_bytes=4096):
        """Reads data from the card."""
        if max_bytes:
            return super(io.FileIO, self).read(max_bytes)

    def can_read(self, timeout=100):
        """Checks whether there is data available to read."""
        poll_obj = select.poll()
        poll_obj.register(self, select.POLLIN)

        poll_data = poll_obj.poll(timeout)

        poll_obj.unregister(self)

        if poll_data and (poll_data[0][1] | select.POLLIN):
            return True
        else:
            return False

    def can_write(self, timeout=100):
        """Checks whether there is room available to write additional data."""
        poll_obj = select.poll()
        poll_obj.register(self, select.POLLOUT)

        poll_data = poll_obj.poll(timeout)

        poll_obj.unregister(self)

        if poll_data and (poll_data[0][1] | select.POLLOUT):
            return True
        else:
            return False

if __name__ == '__main__':
    p = Port('FSCC2', 'rb')

    print("Append Status", p.append_status)
    print("Input Memory Cap", p.input_memory_cap)
    print("Output Memory Cap", p.output_memory_cap)
    print("Ignore Timeout", p.ignore_timeout)
    print("Transmit Modifiers", p.tx_modifiers)

    print("CCR0", hex(p.registers.CCR0))
    print("CCR1", hex(p.registers.CCR1))
    print("CCR2", hex(p.registers.CCR2))
    print("BGR", hex(p.registers.BGR))

    p.append_status = False
    p.input_memory_cap = 1000000
    p.output_memory_cap = 1000000
    p.ignore_timeout = False
    p.tx_modifiers = 0