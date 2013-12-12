# Track Interrupts

###### Support
| Code           | Version
| -------------- | --------
| `fscc-windows` | `v2.7.0` 


## Get
```c
FSCC_TRACK_INTERRUPTS
```

###### Examples
```c
#include <fscc.h>
...

unsigned interrupts;
unsigned matches;

interrupts = 0x00000400;

DeviceIoControl(h, FSCC_GET_TRACK_INTERRUPTS, 
                &interrupts, sizeof(interrupts), 
                &matches, sizeof(matches), 
                &temp, NULL);
```

### Additional Resources
- Complete example: [`examples\track-interrupts.c`](https://github.com/commtech/fscc-windows/blob/master/examples/track-interrupts.c)
