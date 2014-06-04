# Append Status

It is a good idea to pay attention to the status of each frame. For example, you may want to see if the frame's CRC check succeeded or failed.

The FSCC reports this data to you by appending two additional bytes to each frame you read from the card, if you opt-in to see this data. There are a few methods of enabling this additional data.

These two bytes represent the STAR register. (page 67 of the manual)

###### Support
| Code | Version |
| ---- | ------- |
| fscc-windows | 2.0.0 |


## Get
```c
FSCC_GET_APPEND_STATUS
```

###### Examples
```c
#include <fscc.h>
...

unsigned status;

DeviceIoControl(h, FSCC_GET_APPEND_STATUS,
                NULL, 0,
                &status, sizeof(status),
                &temp, NULL);
```


## Enable
```c
FSCC_ENABLE_APPEND_STATUS
```

###### Examples
```c
#include <fscc.h>
...

DeviceIoControl(h, FSCC_ENABLE_APPEND_STATUS,
                NULL, 0,
                NULL, 0,
                &temp, NULL);
```


## Disable
```c
FSCC_DISABLE_APPEND_STATUS
```

###### Examples
```c
#include <fscc.h>
...

DeviceIoControl(h, FSCC_DISABLE_APPEND_STATUS,
                NULL, 0,
                NULL, 0,
                &temp, NULL);
```


### Additional Resources
- Complete example: [`examples/append-status.c`](../examples/append-status.c)
