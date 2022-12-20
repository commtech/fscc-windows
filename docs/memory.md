# Memory Cap

NOT YET IMPLEMENTED.

The memory structure determines how much space is available in the drivers for storing input and output data. The total input memory is equal to rx_size \* rx_num while the total output memory is equal to tx_size \* tx_num. These values should be big enough to hold as much data as necessary before the user can get handle emptying or filling them again.

###### Support
| Code  | Version |
| ----- | ------- |
| fscc-windows | -.-.- |


## Structure
```c
struct fscc_memory {
	UINT32 tx_size;
	UINT32 tx_num;
	UINT32 rx_size;
	UINT32 rx_num;
};
```


## Macros
```c
FSCC_MEMORY_INIT(mem)
```

| Parameter | Type | Description |
| --------- | ---- | ----------- |
| `mem` | `struct fscc_memory *` | The memory structure to initialize |

The `FSCC_MEMORY_INIT` macro should be called each time you use the  `struct fscc_memory` structure. An initialized structure will allow you to only set/receive the memory cap you need.


## Get
```c
FSCC_GET_MEMORY
```

###### Examples
```
#include <fscc.h>
...

struct fscc_memory memory;

FSCC_MEMORY_INIT(memory);

DeviceIoControl(port, FSCC_GET_MEMORY,
                NULL, 0,
                &memory, sizeof(memory),
                &tmp, (LPOVERLAPPED)NULL);
```

At this point `memory.rx_size`, `memory.rx_num`, `memory.tx_size`, and `memory.tx_num` would be set to their respective values.


## Set
```c
FSCC_SET_MEMORY
```

###### Examples
```
#include <fscc.h>
...

struct fscc_memory memory;

FSCC_MEMORY_INIT(memory);
memory.rx_size = 256; 
memory.rx_num = 200; 
memory.tx_size = 256; 
memory.tx_num = 200; 

DeviceIoControl(h, FSCC_SET_MEMORY,
				&memory, sizeof(memory),
				NULL, 0,
				&tmp, (LPOVERLAPPED)NULL);
DeviceIoControl(h, FSCC_GET_MEMORY,
				NULL, 0,
				&memory, sizeof(memory),
				&tmp, (LPOVERLAPPED)NULL);
```


### Additional Resources
- Complete example: [`examples/memory.c`](../examples/memory.c)
