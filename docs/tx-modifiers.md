# TX Modifiers

- XF - Normal transmit - disable modifiers
- XREP - Transmit repeat
- TXT - Transmit on timer
- TXEXT - Transmit on external signal

###### Support
| Code           | Version
| -------------- | --------
| `fscc-windows` | `v2.0.0` 

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

DeviceIoControl(h, FSCC_SET_TX_MODIFIERS, 
				&modifiers, sizeof(modifiers), 
				NULL, 0, 
				&temp, NULL);
```


### Additional Resources
- Complete example: [`examples\tx-modifiers.c`](https://github.com/commtech/fscc-windows/blob/master/examples/tx-modifiers.c)
