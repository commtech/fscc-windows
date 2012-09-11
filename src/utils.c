#include "utils.h"
#include "utils.tmh"

#include "port.h" /* *_OFFSET */

UINT32 chars_to_u32(const char *data)
{
    return *((UINT32*)data);
}

unsigned port_offset(struct fscc_port *port, unsigned bar, unsigned offset)
{
	switch (bar) {
	case 0:
		return (port->channel == 0) ? offset : offset + 0x80;

	case 2:
		switch (offset) {
			case DMACCR_OFFSET:
				return (port->channel == 0) ? offset : offset + 0x04;

			case DMA_RX_BASE_OFFSET:
			case DMA_TX_BASE_OFFSET:
			case DMA_CURRENT_RX_BASE_OFFSET:
			case DMA_CURRENT_TX_BASE_OFFSET:
				return (port->channel == 0) ? offset : offset + 0x08;

			default:
				break;
		}

		break;

	default:
		break;
	}

	return offset;
}

unsigned is_read_only_register(unsigned offset)
{
	switch (offset) {
	case STAR_OFFSET:
	case VSTR_OFFSET:
			return 1;
	}

	return 0;
}

NTSTATUS registry_create_key(WDFKEY parent_key, PUNICODE_STRING key_str, WDFKEY *key)
{	
	NTSTATUS status;
	WDFKEY new_key;
	
	status = WdfRegistryCreateKey(parent_key, key_str, STANDARD_RIGHTS_ALL, REG_OPTION_NON_VOLATILE, NULL, WDF_NO_OBJECT_ATTRIBUTES, &new_key);
	if (!NT_SUCCESS(status)) {
		TraceEvents(TRACE_LEVEL_ERROR, TRACE_DEVICE, 
			"WdfRegistryCreateKey failed %!STATUS!", status);
	}

	if (key)
		*key = new_key;
	else
		WdfRegistryClose(new_key);

	return status;
}

NTSTATUS registry_get_ulong(WDFKEY key, PCUNICODE_STRING value_name, ULONG *value)
{
	NTSTATUS status;

	status = WdfRegistryQueryULong(key, value_name, value);
	if (!NT_SUCCESS(status) && status != STATUS_OBJECT_NAME_NOT_FOUND) {
		TraceEvents(TRACE_LEVEL_ERROR, TRACE_DEVICE, 
			"WdfRegistryQueryULong failed %!STATUS!", status);
		return status;
	}

	return status;
}

NTSTATUS registry_get_or_create_ulong(WDFKEY key, PCUNICODE_STRING value_name, ULONG *value, ULONG initial_value)
{
	NTSTATUS status;

	status = WdfRegistryQueryULong(key, value_name, value);
	if (!NT_SUCCESS(status) && status != STATUS_OBJECT_NAME_NOT_FOUND) {
		TraceEvents(TRACE_LEVEL_ERROR, TRACE_DEVICE, 
			"WdfRegistryQueryULong failed %!STATUS!", status);
		return status;
	}

	/* This is the first time loading the driver so initialize the starting port number to 0 */
	if (status == STATUS_OBJECT_NAME_NOT_FOUND) {
		status = WdfRegistryAssignULong(key, value_name, initial_value);
		if (!NT_SUCCESS(status)) {
			TraceEvents(TRACE_LEVEL_ERROR, TRACE_DEVICE, 
				"WdfRegistryAssignULong failed %!STATUS!", status);
			return status;
		}
		
		status = WdfRegistryQueryULong(key, value_name, value);
		if (!NT_SUCCESS(status)) {
			TraceEvents(TRACE_LEVEL_ERROR, TRACE_DEVICE, 
				"WdfRegistryQueryULong failed %!STATUS!", status);
			return status;
		}
	}

	return status;
}

NTSTATUS registry_set_ulong(WDFKEY key, PCUNICODE_STRING value_name, ULONG value)
{
	NTSTATUS status;

	status = WdfRegistryAssignULong(key, value_name, value);
	if (!NT_SUCCESS(status)) {
		TraceEvents(TRACE_LEVEL_ERROR, TRACE_DEVICE, 
			"WdfRegistryAssignULong failed %!STATUS!", status);
		return status;
	}

	return status;
}