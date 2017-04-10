# Append Status

ENABLE_BLOCKING_WRITE will allow the drivers to block when the output memory cap is full until the outgoing data can fit into the memory cap. This will allow the user to continuously refill the output memory cap without polling when it becomes full.
DISABLE_BLOCKING_WRITE will return the drivers to their original state, where the drivers will return an error condituion when the output memory cap is full.

###### Support
| Code | Version |
| ---- | ------- |
| fscc-windows | 2.7.7 |


## Get
```c
FSCC_GET_BLOCKING_WRITE
```

###### Examples
```c
#include <fscc.h>
...

unsigned status;

DeviceIoControl(h, FSCC_GET_BLOCKING_WRITE,
                NULL, 0,
                &status, sizeof(status),
                &temp, NULL);
```


## Enable
```c
FSCC_ENABLE_BLOCKING_WRITE
```

###### Examples
```c
#include <fscc.h>
...

DeviceIoControl(h, FSCC_ENABLE_BLOCKING_WRITE,
                NULL, 0,
                NULL, 0,
                &temp, NULL);
```


## Disable
```c
FSCC_DISABLE_BLOCKING_WRITE
```

###### Examples
```c
#include <fscc.h>
...

DeviceIoControl(h, FSCC_DISABLE_BLOCKING_WRITE,
                NULL, 0,
                NULL, 0,
                &temp, NULL);
```


### Additional Resources
- Complete example: [`examples/blocking-write.c`](../examples/blocking-write.c)
