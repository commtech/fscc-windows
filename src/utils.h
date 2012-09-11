#ifndef FSCC_UTILS_H
#define FSCC_UTILS_H

#include "port.h" /* struct fscc_port */
#include "config.h"

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
NTSTATUS registry_get_or_create_ulong(WDFKEY key, PCUNICODE_STRING value_name, ULONG *value, ULONG initial_value);
NTSTATUS registry_set_ulong(WDFKEY key, PCUNICODE_STRING value_name, ULONG value);

#endif