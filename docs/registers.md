# Registers

The FSCC driver is a swiss army knife of sorts with communication. It can
handle many different situations, if configured correctly. Typically to
configure it to handle your specific situation you need to modify the card's
register values.

_For a complete listing of all of the configuration options please see the 
manual._

In HDLC mode some settings are fixed at certain values. If you are in
HDLC mode and after setting/getting your registers some bits don't look correct,
then they are likely fixed. A complete list of the fixed values can be found in 
the CCR0 section of the manual.

All of the registers, except FCR, are tied to a single port. FCR on the other hand 
is shared between two ports on a card. You can differentiate between which FCR 
settings affects what port by the A/B labels. A for port 0 and B for port 1.

You should purge the data stream after changing the registers.
Settings like CCR0 will require being purged for the changes to take 
effect.

###### Driver Support
| Code           | Version
| -------------- | --------
| `fscc-windows` | `v2.0.0` 


## Structure
```c
struct fscc_registers {
    /* BAR 0 */
    fscc_register reserved1[2];

    fscc_register FIFOT;

    fscc_register reserved2[2];

    fscc_register CMDR;
    fscc_register STAR; /* Read-only */
    fscc_register CCR0;
    fscc_register CCR1;
    fscc_register CCR2;
    fscc_register BGR;
    fscc_register SSR;
    fscc_register SMR;
    fscc_register TSR;
    fscc_register TMR;
    fscc_register RAR;
    fscc_register RAMR;
    fscc_register PPR;
    fscc_register TCR;
    fscc_register VSTR; /* Read-only */

    fscc_register reserved3[1];

    fscc_register IMR;
    fscc_register DPLLR;

    /* BAR 2 */
    fscc_register FCR;
};
```


## Macros
```c
FSCC_REGISTERS_INIT(regs)
```

| Parameter | Type                      | Description
| --------- | ------------------------- | -----------------------
| `regs`    | `struct fscc_registers *` | The registers structure to initialize

The `FSCC_REGISTERS_INIT` macro should be called each time you use the 
`struct fscc_registers` structure. An initialized structure will allow you to 
only set/receive the registers you need.


## Set
```c
FSCC_SET_REGISTERS
```

###### Examples
```
#include <fscc.h>
...

struct fscc_registers registers;

FSCC_REGISTERS_INIT(registers);

registers.CCR0 = 0x0011201c;
registers.BGR = 10;

DeviceIoControl(h, FSCC_SET_REGISTERS, 
				&registers, sizeof(registers), 
				NULL, 0, 
				&temp, NULL);	
```


## Get
```c
FSCC_GET_REGISTERS
```

###### Examples
```
#include <fscc.h>
...

struct fscc_registers registers;

FSCC_REGISTERS_INIT(registers);

registers.CCR0 = FSCC_UPDATE_VALUE;
registers.BGR = FSCC_UPDATE_VALUE;

DeviceIoControl(h, FSCC_GET_REGISTERS, 
				&registers, sizeof(registers), 
				&registers, sizeof(registers), 
				&temp, NULL);	
```

At this point `regs.CCR0` and `regs.BGR` would be set to their respective
values.


### Additional Resources
- Complete example: [`examples\registers.c`](https://github.com/commtech/cfscc/blob/master/examples/registers/registers.c)
