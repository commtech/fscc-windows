# Memory Cap

The FSCC possess internal driver buffering that is required for DMA operation, and serves as a secondary buffering location for incoming data when operating in FIFO mode. This buffering is made up of X buffers of Y size, for both transmit and receive. By default, both transmit and receive uses 200 buffers of 256 bytes each.

To see these values, the ports first have to be installed. These values can then be modified, and the new modified values will take effect on the next reboot and thereafter until they are changed again. 

In previous versions of the drivers, these sizes could be adjusted while the driver was in operation, however because of new limitations that is no longer possible. Instead, the default values can be adjusted by modifying the registry:
Number of transmit buffers: `HKEY_LOCAL_MACHINE\SYSTEM\CurrentControlSet\Enum\MF\PCI#VEN_18F7&DEV_00XXXXXXXXXXXXXXXXXXXX#Child0X\Device Parameters\TxNum`
Number of receive buffers: `HKEY_LOCAL_MACHINE\SYSTEM\CurrentControlSet\Enum\MF\PCI#VEN_18F7&DEV_00XXXXXXXXXXXXXXXXXXXX#Child0X\Device Parameters\RxNum`
Size of transmit buffers: `HKEY_LOCAL_MACHINE\SYSTEM\CurrentControlSet\Enum\MF\PCI#VEN_18F7&DEV_00XXXXXXXXXXXXXXXXXXXX#Child0X\Device Parameters\TxSize`
Size of receive buffers: `HKEY_LOCAL_MACHINE\SYSTEM\CurrentControlSet\Enum\MF\PCI#VEN_18F7&DEV_00XXXXXXXXXXXXXXXXXXXX#Child0X\Device Parameters\RxSize`

The limitations of these values are a minimum of 2 buffers, and the buffer size must be evenly divisble by 4. The maximum is based off your system, but higher maximums will not increase throughput and instead just prevent lost data. The buffer size typically has no particular impact, except when using XREP to repeatedly transmit frames while in DMA mode. When using XREP to repeatedly transmit frames in DMA mode, the TxSize must be larger than the frame you wish to transmit. Otherwise, the sizes of the buffers can be any size and will allow the transmission and reception of frames larger or smaller than the size.

###### Support
| Code  | Version |
| ----- | ------- |
| fscc-windows | 3.0.1.x |
