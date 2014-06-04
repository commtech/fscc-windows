# TX Modifiers
XF: While operating in XF mode, each frame will transmit as soon as possible.
XREP: In XREP mode, the frame will not be removed from the transmit FIFO, and instead every write() operation will cause the last frame placed into the FIFO (even if it has already been output before) to be output.
TXT: In TXT mode, the next frame will not be transmitted out the transmit FIFO until the TCR regoster reaches 0. This register counts down whenever the selected clock period passes. The source of the clock period can be set in the TCR register.
TXEXT: In TXEXT mode, the next frame will not be transmitted until an external signal goes active. The source of this signal can be set in the CCR0 register. While in this mode, the last frame in the transmit FIFO will not be removed. As such, it also effectively works like XREP.

| Modifiers | Value | Description |
| --------- | -----:| ----------- |
| `XF` | 0 | Normal transmit (disable all other modifiers) |
| `XREP` | 1 | Transmit frame repeatedly |
| `TXT` | 2 | Transmit frame on timer |
| `TXEXT` | 4 | Transmit frame on external signal |

###### Support
| Code | Version |
| ---- | ------- |
| fscc-windows | 2.0.0 |

## Get
```c
FSCC_GET_TX_MODIFIERS
```

###### Examples
```
#include <fscc.h>
...

unsigned modifiers;

DeviceIoControl(h, FSCC_GET_TX_MODIFIERS,
				NULL, 0,
				&modifiers, sizeof(modifiers),
				&temp, NULL);
```


## Set
```c
FSCC_SET_TX_MODIFIERS
```

###### Examples
```
#include <fscc.h>
...

unsigned modifiers = TXT | XREP;

DeviceIoControl(h, FSCC_SET_TX_MODIFIERS,
				&modifiers, sizeof(modifiers),
				NULL, 0,
				&temp, NULL);
```


### Additional Resources
- Complete example: [`examples/tx-modifiers.c`](../examples/tx-modifiers.c)
