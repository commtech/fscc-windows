# RX Multiple

RX Multiple allows you to return multiple frames from the FIFO at once to user-space. While active, every read will return N frames, where the size of all N frames combined is less than the size of the variable you're attempting to store data in. 

For example, if you have 10 frames of 128 size in your input buffer and have RX Multiple on, when you activate a read with a size of 4096, all 10 frame will be placed into the variable supplied to the read function.

###### Support
| Code | Version |
| ---- | -------- |
| fscc-windows | 2.2.9 |


## Get
```c
FSCC_GET_RX_MULTIPLE
```

###### Examples
```c
#include <fscc.h>
...

unsigned status;

DeviceIoControl(h, FSCC_GET_RX_MULTIPLE,
                NULL, 0,
                &status, sizeof(status),
                &temp, NULL);
```


## Enable
```c
FSCC_ENABLE_RX_MULTIPLE
```

###### Examples
```c
#include <fscc.h>
...

DeviceIoControl(h, FSCC_ENABLE_RX_MULTIPLE,
                NULL, 0,
                NULL, 0,
                &temp, NULL);
```


## Disable
```c
FSCC_DISABLE_RX_MULTIPLE
```

###### Examples
```c
#include <fscc.h>
...

DeviceIoControl(h, FSCC_DISABLE_RX_MULTIPLE,
                NULL, 0,
                NULL, 0,
                &temp, NULL);
```


### Additional Resources
- Complete example: [`examples/rx-multiple.c`](../examples/rx-multiple.c)
