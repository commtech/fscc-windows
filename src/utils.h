/*
Copyright 2023 Commtech, Inc.

Permission is hereby granted, free of charge, to any person obtaining a copy 
of this software and associated documentation files (the "Software"), to deal 
in the Software without restriction, including without limitation the rights 
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell 
copies of the Software, and to permit persons to whom the Software is 
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in 
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR 
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, 
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE 
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER 
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, 
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN 
THE SOFTWARE.
*/


#ifndef FSCC_UTILS_H
#define FSCC_UTILS_H

#include "port.h" /* struct fscc_port */
#include "config.h"
#include "defines.h"

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

#define set_timestamp(x) KeQuerySystemTime(x)
void clear_timestamp(fscc_timestamp *timestamp);
int timestamp_is_empty(fscc_timestamp *timestamp);

NTSTATUS registry_create_key(WDFKEY parent_key, PUNICODE_STRING key_str, WDFKEY *key);
/*
NTSTATUS registry_get_ulong(WDFKEY key, PCUNICODE_STRING value_name, ULONG *value);
NTSTATUS registry_get_or_create_ulong(WDFKEY key, PCUNICODE_STRING value_name,
									ULONG *value, ULONG initial_value);
NTSTATUS registry_set_ulong(WDFKEY key, PCUNICODE_STRING value_name, ULONG value);
*/
#endif