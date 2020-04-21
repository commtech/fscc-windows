# Blocking Write

ENABLE_FORCE_FIFO will force the drivers to utilize the FIFOs instead of DMA when operating. This can be useful if you find that DMA does not work for your set up. DISABLE_FORCE_FIFO will allow the drivers to use DMA if it is available and functioning. DMA is only available to the SuperFSCC side of the FSCC family of products.

###### Support
| Code | Version |
| ---- | ------- |
| fscc-windows | 2.8.0 |


## Get
```c
FSCC_GET_FORCE_FIFO
```

###### Examples
```c
#include <fscc.h>
...

unsigned status;

DeviceIoControl(h, FSCC_GET_FORCE_FIFO,
                NULL, 0,
                &status, sizeof(status),
                &temp, NULL);
```


## Enable
```c
FSCC_ENABLE_FORCE_FIFO
```

###### Examples
```c
#include <fscc.h>
...

DeviceIoControl(h, FSCC_ENABLE_FORCE_FIFO,
                NULL, 0,
                NULL, 0,
                &temp, NULL);
```


## Disable
```c
FSCC_DISABLE_FORCE_FIFO
```

###### Examples
```c
#include <fscc.h>
...

DeviceIoControl(h, FSCC_DISABLE_FORCE_FIFO,
                NULL, 0,
                NULL, 0,
                &temp, NULL);
```


### Additional Resources
- Complete example: [`examples/force-fifo.c`](../examples/force-fifo.c)
