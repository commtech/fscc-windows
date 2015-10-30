/*
    Copyright (C) 2015  Commtech, Inc.

    This file is part of fscc-windows.

    fscc-windows is free software: you can redistribute it and/or modify it
    under the terms of the GNU General Public License as published bythe Free
    Software Foundation, either version 3 of the License, or (at your option)
    any later version.

    fscc-windows is distributed in the hope that it will be useful, but WITHOUT
    ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
    FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
    more details.

    You should have received a copy of the GNU General Public License along
    with fscc-windows.  If not, see <http://www.gnu.org/licenses/>.

*/


#ifndef FSCC_UTILS_H
#define FSCC_UTILS_H

#include "port.h" /* struct fscc_port */
#include "config.h"

#define UNUSED(x) (void)(x)

#define warn_if_untrue(expr) \
    if (expr) {} else \
    { \
        KdPrint((DEVICE_NAME " %s %s\n", #expr, "is untrue.")); \
    }

#define return_if_untrue(expr) \
    if (expr) {} else \
    { \
        KdPrint((DEVICE_NAME " %s %s\n", #expr, "is untrue.")); \
        return; \
    }

#define return_val_if_untrue(expr, val) \
    if (expr) {} else \
    { \
        KdPrint((DEVICE_NAME " %s %s\n", #expr, "is untrue.")); \
        return val; \
    }

UINT32 chars_to_u32(const char *data);
unsigned is_read_only_register(unsigned offset);
unsigned port_offset(struct fscc_port *port, unsigned bar, unsigned offset);

NTSTATUS registry_create_key(WDFKEY parent_key, PUNICODE_STRING key_str, WDFKEY *key);

NTSTATUS registry_get_ulong(WDFKEY key, PCUNICODE_STRING value_name, ULONG *value);
NTSTATUS registry_get_or_create_ulong(WDFKEY key, PCUNICODE_STRING value_name,
                                      ULONG *value, ULONG initial_value);
NTSTATUS registry_set_ulong(WDFKEY key, PCUNICODE_STRING value_name, ULONG value);

#endif