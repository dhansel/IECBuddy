# IECBuddy

IECBuddy is a USB plug-in for the [C64 RAD Expansion Unit](https://github.com/frntc/RAD), giving the RAD
access to the C64's IEC bus. The IECBuddy is based on my [IECDevice](https://github.com/dhansel/IECDevice)
and [VDrive](https://github.com/dhansel/VDrive) libraries, allowing the RAD to support various disk image 
formats (D64, G64, D71, D81) and fast-load protocols (JiffyDos, Epyx FastLoad, Final Cartridge 3, Action Replay 6,
DolphinDos, SpeedDos).

## Hardware

The IECBuddy comes in several different variants, with differing amounts of components and build effort required.

### Barebones

The barebones version is the simplest version, requiring only a [RP2040-One](https://www.amazon.com/RP2040-One-Pico-Like-Raspberry-Dual-Core-Processor/dp/B0BMM7SS99)
board and a Commodore [serial cable/connector](https://www.c64-wiki.com/wiki/Serial_Port).

Simply solder the serial cable to the RP2040-One as follows:

IEC Bus Pin | Signal   | RP2040-One
------------|----------|-----------
1           | SRQ      | Not connected 
2           | GND      | GND
3           | ATN      | 2
4           | CLK      | 3 
5           | DATA     | 4 
6           | RESET    | 5 

Then upload the IECBuddy Micro firmware to the RP2040-One and you're good to go.
Downsides are that there is no display and no "Disk Change" button.
If you would like a "Disk Change" button, simply wire a pushbutton switch between pins GND and 8 on the RP2040-One.

Note that this connects the 5V IEC bus lines directly to the RP2040 inputs. There are varying
opinions online on whether or not this can damage the RP2040 and/or whether the RP2040 is
capable of properly driving the IEC bus lines (especially when multiple devices are connected).
In my testing I have not had any problems, even with multiple other devices connected. YMMV.

If you would prefer proper voltage conversion and line drivers then use the "Mini" version below.

### Micro

If you'd like a somewhat cleaner and more permanent build but still have a very small
footprint and minimal component count, use the "IECBuddy Micro" PCB. You can either solder
the serial cable directly onto the board (connections are labeled on the board) or solder a 
proper IEC connector onto the board and use a standard serial cable. This also comes with
space for a pushbutton switch. No display though.

The same caveats aregarding voltage conversion and line drivers apply as described in the "Barebones" section above.

### Mini

Only slightly larger than the Micro version, but with two major differences:
- has space and connections for a [0.96" TFT display](https://www.aliexpress.us/item/2251832810664524.html).
- uses 7406 and 74LVC04 ICs for voltage conversion and properly interfacing with and driving the Commodore IEC bus lines.

### Max



