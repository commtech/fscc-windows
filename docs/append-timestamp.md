# Append Timestamp

[`KeQuerySystemTime`](http://msdn.microsoft.com/en-us/library/windows/hardware/ff553068.aspx) is used to acquire the timestamp upon complete reception of a frame.
This data will be appended to the end of your frame.

###### Support
| Code | Version |
| ---- | ------- |
| fscc-windows | 2.4.0 |


## Get
```c
FSCC_GET_APPEND_TIMESTAMP
```

###### Examples
```c
#include <fscc.h>
...

unsigned timestamp;

DeviceIoControl(h, FSCC_GET_APPEND_TIMESTAMP,
                NULL, 0,
                &timestamp, sizeof(timestamp),
                &temp, NULL);
```


## Enable
```c
FSCC_ENABLE_APPEND_TIMESTAMP
```

###### Examples
```c
#include <fscc.h>
...

DeviceIoControl(h, FSCC_ENABLE_APPEND_TIMESTAMP,
                NULL, 0,
                NULL, 0,
                &temp, NULL);
```


## Disable
```c
FSCC_DISABLE_APPEND_TIMESTAMP
```

###### Examples
```c
#include <fscc.h>
...

DeviceIoControl(h, FSCC_DISABLE_APPEND_TIMESTAMP,
                NULL, 0,
                NULL, 0,
                &temp, NULL);
```


### Additional Resources
- Complete example: [`examples/append-timestamp.c`](../examples/append-timestamp.c)
