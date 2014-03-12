# Write

###### Support
| Code | Version |
| ---- | ------- |
| fscc-windows | 2.0.0 |


## Write
The Windows [`WriteFile`](http://msdn.microsoft.com/en-us/library/windows/desktop/aa365747.aspx) is used to write data to the port.

| System Error | Value | Cause |
| ------------ | -----:| ----- |
| `ERROR_SEM_TIMEOUT` | 121 (0x79) | Command timed out (missing clock) |
| `ERROR_INSUFFICIENT_BUFFER` | 122 (0x7A) | The write size exceeds the output memory usage cap |

###### Examples
```c
#include <fscc.h>
...

char odata[] = "Hello world!";
unsigned bytes_written;

WriteFile(h, odata, sizeof(odata), (DWORD*)bytes_written, NULL);
```


### Additional Resources
- Complete example: [`examples/tutorial.c`](../examples/tutorial.c)
