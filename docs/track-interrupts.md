# Track Interrupts

###### Support
| Code | Version |
| ---- | ------- |
| fscc-windows | 2.7.0 |


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

interrupts = 0x00000100; /* TIN interrupt */

DeviceIoControl(h, FSCC_TRACK_INTERRUPTS,
                &interrupts, sizeof(interrupts),
                &matches, sizeof(matches),
                &temp, NULL);
```

### Additional Resources
- Complete example: [`examples/track-interrupts.c`](../examples/track-interrupts.c)
