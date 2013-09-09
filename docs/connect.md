# Connect

Opening a handle using this API will only give you access to the
synchronous functionality of the card. You will need to use the COM ports
if you would like to use the asynchronous functionality.

###### Driver Support
| Code           | Version
| -------------- | --------
| `fscc-windows` | `v2.0.0` 


## Connect
The Windows [`CreateFile`](http://msdn.microsoft.com/en-us/library/windows/desktop/aa363858.aspx)
is used to connect to the port.

###### Examples
Connect to port 0.
```c
#include <Windows.h>
...

HANDLE h;

h = CreateFile("\\\\.\\FSCC0", GENERIC_READ | GENERIC_WRITE, 0, NULL, 
                  OPEN_EXISTING, 0, NULL);
```


### Additional Resources
- Complete example: [`examples\tutorial.c`](https://github.com/commtech/fscc-windows/blob/master/examples/tutorial.c)
