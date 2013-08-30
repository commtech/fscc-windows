# Disconnect


###### Driver Support
| Code           | Version
| -------------- | --------
| `fscc-windows` | `v2.0.0` 


## Disconnect
The Windows [`CloseHandle`](http://msdn.microsoft.com/en-us/library/windows/apps/ms724211.aspx)
is used to connect to the port.


###### Examples
```c
#include <Windows.h>
...

CloseHandle(h);
```


### Additional Resources
- Complete example: [`examples\tutorial.c`](https://github.com/commtech/cfscc/blob/master/examples/tutorial/tutorial.c)