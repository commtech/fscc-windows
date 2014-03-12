# Memory Cap

If your system has limited memory available, there are safety checks in place to prevent spurious incoming data from overrunning your system. Each port has an option for setting it's input and output memory cap.

###### Support
| Code  | Version |
| ----- | ------- |
| fscc-windows | 2.0.0 |


## Structure
```c
struct fscc_memory_cap {
    int input;
    int output;
};
```


## Macros
```c
FSCC_MEMORY_CAP_INIT(memcap)
```

| Parameter | Type | Description |
| --------- | ---- | ----------- |
| `memcap` | `struct fscc_memory_cap *` | The memory cap structure to initialize |

The `FSCC_MEMORY_CAP_INIT` macro should be called each time you use the  `struct fscc_memory_cap` structure. An initialized structure will allow you to only set/receive the memory cap you need.


## Get
```c
FSCC_GET_MEMORY_CAP
```

###### Examples
```
#include <fscc.h>
...

struct fscc_memory_cap memcap;

FSCC_MEMORY_CAP_INIT(memcap);

DeviceIoControl(port, FSCC_GET_MEMORY_CAP,
                NULL, 0,
                &memcap, sizeof(memcap),
                &tmp, (LPOVERLAPPED)NULL);
```

At this point `memcap.input` and `memcap.output` would be set to their respective values.


## Set
```c
FSCC_SET_MEMORY_CAP
```

###### Examples
```
#include <fscc.h>
...

struct fscc_memory_cap memcap;

FSCC_MEMORY_CAP_INIT(memcap);

memcap.input = 1000000; /* 1 MB */
memcap.output = 1000000; /* 1 MB */

DeviceIoControl(port, FSCC_SET_MEMORY_CAP,
                &memcap, sizeof(memcap),
                NULL, 0,
                &tmp, (LPOVERLAPPED)NULL);
```


### Additional Resources
- Complete example: [`examples/memory-cap.c`](../examples/memory-cap.c)
