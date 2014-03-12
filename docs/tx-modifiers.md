# TX Modifiers

| Modifiers | Value | Description |
| --------- | -----:| ----------- |
| `XF` | 0 | Normal transmit (disable modifiers) |
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
