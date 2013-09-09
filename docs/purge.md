# Purge
Between the hardware FIFO and the driver's software buffers there are multiple places data could 
be stored, excluding your application code. If you ever need to clear this data and start fresh, 
there are a couple of methods you can use.

###### Driver Support
| Code           | Version
| -------------- | --------
| `fscc-windows` | `v2.0.0` 


## Execute
```c
FSCC_PURGE_TX, FSCC_PURGE_RX
```

###### Examples
Purge the transmit data.
```c
#include <fscc.h>
...

DeviceIoControl(h, FSCC_PURGE_TX, 
                NULL, 0, 
                NULL, 0, 
                &temp, NULL);
```

Purge the receive data.
```c
#include <fscc.h>
...

DeviceIoControl(h, FSCC_PURGE_RX, 
                NULL, 0, 
                NULL, 0, 
                &temp, NULL);
```


### Additional Resources
- Complete example: [`examples\purge.c`](https://github.com/commtech/fscc-windows/blob/master/examples/purge.c)
