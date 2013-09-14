# Append Timestamp

###### Support
| Code           | Version
| -------------- | --------
| `fscc-windows` | `v2.4.0` 


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
                &status, sizeof(status), 
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
                &status, sizeof(status), 
                &temp, NULL);
```


### Additional Resources
- Complete example: [`examples\append-timestamp.c`](https://github.com/commtech/fscc-windows/blob/master/examples/append-timestamp.c)
