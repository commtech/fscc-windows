# Ignore Timeout

When this is enabled, the card will ignore the timeouts for different things. This can be useful when setting a very low clock frequency or when attempting to purge with a slower baud rate, as an example.

###### Support
| Code | Version |
| ---- | ------- |
| fscc-windows | 2.0.0  |


## Get
```c
FSCC_GET_IGNORE_TIMEOUT
```

###### Examples
```c
#include <fscc.h>
...

unsigned status;

DeviceIoControl(h, FSCC_GET_IGNORE_TIMEOUT,
                NULL, 0,
                &status, sizeof(status),
                &temp, NULL);
```


## Enable
```c
FSCC_ENABLE_IGNORE_TIMEOUT
```

###### Examples
```c
#include <fscc.h>
...

DeviceIoControl(h, FSCC_ENABLE_IGNORE_TIMEOUT,
                NULL, 0,
                NULL, 0,
                &temp, NULL);
```


## Disable
```c
FSCC_DISABLE_IGNORE_TIMEOUT
```

###### Examples
```c
#include <fscc.h>
...

DeviceIoControl(h, FSCC_DISABLE_IGNORE_TIMEOUT,
                NULL, 0,
                NULL, 0,
                &temp, NULL);
```


### Additional Resources
- Complete example: [`examples/ignore-timeout.c`](../examples/ignore-timeout.c)
