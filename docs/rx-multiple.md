# RX Multiple

###### Support
| Code           | Version
| -------------- | --------
| `fscc-windows` | `v2.2.9` 


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
- Complete example: [`examples\rx-multiple.c`](https://github.com/commtech/fscc-windows/blob/master/examples/rx-multiple.c)
