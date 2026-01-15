# IECBuddy

IECBuddy is a USB plug-in for the [C64 RAD Expansion Unit](https://github.com/frntc/RAD), giving the RAD
access to the C64's IEC bus. The IECBuddy is based on my [IECDevice](https://github.com/dhansel/IECDevice)
and [VDrive](https://github.com/dhansel/VDrive) libraries, allowing the RAD to support various disk image 
formats (D64, G64, D71, D81) and fast-load protocols (JiffyDos, Epyx FastLoad, Final Cartridge 3, Action Replay 6,
DolphinDos and SpeedDos). Additionally, the IECBuddy acts as a virtual printer. Printed content can be viewed
on the C64 screen from within the RAD menu.

<br>
  <div align="center">
  <a href="images/IECBuddy-Barebones2.jpg"><img src="images/IECBuddy.jpg" height="300"></a>
  </div>
<br>

The IECBuddy comes in four different variants, with differing amounts of components and build effort required:
  * [Barebones](IECBuddy-Barebones) (no PCB, requires only a RP2040-One board)
  * [Micro](IECBuddy-Micro) (like Barebones but with a PCB and disk change button)
  * [Mini](IECBuddy-Mini) (like Micro but with a display and better bus interface)
  * [Max](IECBuddy-Max) (like Mini but with parallel cable connector)

All variants can plug directly into a RAD powered by a Raspberry Pi 3. If your RAD uses a Raspberry Pi Zero then 
you will need [an adapter cable](https://www.raspberrypi.com/products/micro-usb-male-to-usb-a-female-cable/) 
to connect the IECBuddy since the Zero only has a micro-USB port.

To build an IECBuddy, select and build whichever variant appeals to you and then follow the instructions in the
[Uplodading the firmware](#uploading-the-firmware) section below.

## IECBuddy Barebones

The barebones variant is the simplest version, requiring no manufactured PCB, just a [RP2040-One](https://www.amazon.com/RP2040-One-Pico-Like-Raspberry-Dual-Core-Processor/dp/B0BMM7SS99)
board and a Commodore [serial cable](https://www.c64-wiki.com/wiki/Serial_Port). Either solder the cable directly to the RP2040-One or set it up on a breadboard:

<br>
  <div align="center">
  <a href="images/IECBuddy-Barebones1.jpg"><img src="images/IECBuddy-Barebones1.jpg" height="300"></a>
  <a href="images/IECBuddy-Barebones2.jpg"><img src="images/IECBuddy-Barebones2.jpg" height="300"></a>
  </div>
<br>

Connect the Commodore serial cable to the RP2040-One as follows:

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

## IECBuddy Micro

<br>
  <div align="center">
  <a href="images/IECBuddy-Barebones1.jpg"><img src="images/IECBuddy-Micro1.jpg" height="300"></a>
  <a href="images/IECBuddy-Barebones2.jpg"><img src="images/IECBuddy-Micro2.jpg" height="300"></a>
  </div>
<br>

If you would like a somewhat cleaner and more permanent build but still want to go with a very small
footprint and minimal component count, use the "IECBuddy Micro" variant. You can either solder
the serial cable directly onto the board (connections are labeled on the board) or solder a 
proper IEC connector onto the board and use a standard serial cable. This also comes with
space for a pushbutton switch. No display though.

The same caveats regarding voltage conversion and line drivers apply as described in the "Barebones" section above.

A Gerber file for PCB production can be downloaded [here](https://github.com/dhansel/IECBuddy/raw/refs/heads/main/hardware/IECBuddy-micro-gerber.zip).<br>
A PDF file with the schematics is available [here](https://github.com/dhansel/IECBuddy/raw/refs/heads/main/hardware/IECBuddy-micro-schematic.pdf).<br>
KiCad files for the board are [here](hardware/IECBuddy-micro).

You will need the following components (the given links are just suggestions, I do not get any kickbacks for them).

Designator | Component 
-----------|-----------
U1         | [RP2040-One](https://www.amazon.com/RP2040-One-Pico-Like-Raspberry-Dual-Core-Processor/dp/B0BMM7SS99)
SW1        | [Pushbutton Switch](https://www.digikey.com/en/products/detail/c-k/PTS645VH58-2-LFS/1146783)
IEC1       | [IEC Bus Connector (6 Pin)](https://www.aliexpress.us/item/3256807108500271.html)

You can skip the IEC1 connector if you solder the serial cable directly to the board (connections on the board are labeled).

## IECBuddy Mini

  <div align="center">
  <a href="images/IECBuddy-Mini1.jpg"><img src="images/IECBuddy-Mini1.jpg" height="300"></a>
  <a href="images/IECBuddy-Mini2.jpg"><img src="images/IECBuddy-Mini2.jpg" height="300"></a>
  </div>

The Mini variant is slightly larger than the Micro version and requires more components besides
the RP2040-One. As a result it comes with the following features that are not present in the smaller versions:

First, it has space and connections on the PCB for a [0.96" TFT display](https://www.aliexpress.us/item/2251832810664524.html).
The display shows the currently mounted disk image as well as disk status and progress bars while loading.

Second, it uses 7406 and 74LVC04 ICs for voltage conversion and properly interfacing with and driving the Commodore IEC bus lines.
This is very similar to the way original hardware (like the 1541) interfaces to the IEC bus. It also protects the RP2040
from the possible overcurrent and overvoltage conditions described in the "Barebones" section above.

A Gerber file for PCB production can be downloaded [here](https://github.com/dhansel/IECBuddy/raw/refs/heads/main/hardware/IECBuddy-mini-gerber.zip).<br>
A PDF file with the schematics is available [here](https://github.com/dhansel/IECBuddy/raw/refs/heads/main/hardware/IECBuddy-mini-schematic.pdf).<br>
KiCad files for the board are [here](hardware/IECBuddy-mini).

You will need the following components (the given links are just suggestions, I do not get any kickbacks for them).

Designator | Component 
-----------|-----------
R1,R2,R3,R4| [Resistor 1kOhm SMD0805](https://www.digikey.com/en/products/detail/stackpole-electronics-inc/RMCF0805FT1K00/1760090)
C1,C2      | [Ceramic Capacitor 100uF SMD0805](https://www.digikey.com/en/products/detail/kyocera-avx/KGM21NR71H104KT/563505)
U1         | [RP2040-One](https://www.amazon.com/RP2040-One-Pico-Like-Raspberry-Dual-Core-Processor/dp/B0BMM7SS99)
U2         | [74LVC04AD SOIC](https://www.digikey.com/en/products/detail/nexperia-usa-inc/74LVC04AD-118/946673)
U3         | [7406DR SOIC](https://www.digikey.com/en/products/detail/texas-instruments/SN7406DR/276661)
SW1        | [Pushbutton Switch](https://www.digikey.com/en/products/detail/c-k/PTS645VH58-2-LFS/1146783)
ST7789     | [TFT Display](https://www.aliexpress.us/item/2251832810664524.html)
IEC1       | [IEC Bus Connector (6 Pin)](https://www.aliexpress.us/item/3256807108500271.html)

Various components can be left out if desired:
  * You can leave out the IEC1 connector if you solder the serial cable directly to the board (connections on the board are labeled).
  * You can leave out the ST7789 display if you don't want a display.
  * If you don't want to use the IEC bus driver ICs then you can place solder on the JP1-JP5 solder jumpers and leave out R1-R4, C1, C2, U2 and U3 (in this case use the IECBuddyMicro.uf2 firmware).

I recommend uploading the firmware **before** soldering on the display. The display sits on top of the RP2040-One
makes accessing the "Boot" button (required for firmware upload) tricky - still possible, but a bit fiddly.

## IECBuddy Max

The IECBuddy Max variant has a much larger PCB layout and uses a Raspberry Pi Pico (version 1 or 2).
It has all the features of the Mini version and additionally a second IEC port for daisy-chaining 
and a connector for a parallel cable to be used with Dolphin Dos and Speed Dos.
Descriptions on how to make a compatible parallel cable and user port connector can be found in
various places over the internet, for example
[here](https://github.com/dhansel/IECDevice/tree/main/hardware#user-port-breakout-board),
[here](https://github.com/svenpetersen1965/1541-parallel-adapter-SpeedDOS)
or [here](https://github.com/FraEgg/commodore-1541-parallel-port-adapter-c64-c128-speeddos-dolphindos)

  <div align="center">
  <a href="images/IECBuddy-Max1.jpg"><img src="images/IECBuddy-Max1.jpg" height="300"></a>
  <a href="images/IECBuddy-Max2.jpg"><img src="images/IECBuddy-Max2.jpg" height="300"></a>
  </div>

A Gerber file for PCB production can be downloaded [here](https://github.com/dhansel/IECBuddy/raw/refs/heads/main/hardware/IECBuddy-max-gerber.zip).<br>
A PDF file with the schematics is available [here](https://github.com/dhansel/IECBuddy/raw/refs/heads/main/hardware/IECBuddy-max-schematic.pdf).<br>
KiCad files for the board are [here](hardware/IECBuddy-max).

You will need the following components (the given links are just suggestions, I do not get any kickbacks for them).

Designator | Component 
-----------|-----------
R1,R2,R3,R4| [Resistor 1kOhm SMD0805](https://www.digikey.com/en/products/detail/stackpole-electronics-inc/RMCF0805FT1K00/1760090)
C1,C2,C3   | [Ceramic Capacitor 100uF SMD0805](https://www.digikey.com/en/products/detail/kyocera-avx/KGM21NR71H104KT/563505)
U1         | [Raspberry Pi Pico](https://www.microcenter.com/product/661033/raspberry-pi-pico-microcontroller-development-board)
U2         | [74CBTD3861DW](https://www.digikey.com/en/products/detail/texas-instruments/SN74CBTD3861DW/378015)
U3         | [74LVC04AD SOIC](https://www.digikey.com/en/products/detail/nexperia-usa-inc/74LVC04AD-118/946673)
U4         | [7406DR SOIC](https://www.digikey.com/en/products/detail/texas-instruments/SN7406DR/276661)
Reset,DiskChg | [Pushbutton Switch](https://www.digikey.com/en/products/detail/same-sky-formerly-cui-devices-/TS02-66-60-BK-100-LCR-D/15634327)
ST7789     | [TFT Display](https://www.aliexpress.us/item/2251832810664524.html)
IEC1       | [IEC Bus Connector (6 Pin)](https://www.aliexpress.us/item/3256807108500271.html)
Parallel1, Parallel2 | [10-position IDC Connector](https://www.digikey.com/en/products/detail/on-shore-technology-inc/302-S101/2178422)


## Uploading the firmware

Pre-compiled versions of the firmware are available for all four versions of the IECBuddy. Programming the 2040 is easy:

  1) Download the UF2 file appropriate for your version of the board:
     - Barebones and Micro: [IECBuddyMicro.uf2](https://github.com/dhansel/IECBuddy/raw/refs/heads/main/software/IECBuddyMicro.uf2) (also for Mini if if not using the bus driver ICs)
     - Mini: [IECBuddyMini.uf2](https://github.com/dhansel/IECBuddy/raw/refs/heads/main/software/IECBuddyMini.uf2)
     - Max using PiPico 1: [IECBuddyMax1.uf2](https://github.com/dhansel/IECBuddy/raw/refs/heads/main/software/IECBuddyMax1.uf2)
     - Max using PiPico 2: [IECBuddyMax2.uf2](https://github.com/dhansel/IECBuddy/raw/refs/heads/main/software/IECBuddyMax2.uf2)
  2) Connect the RP2040-One to your computer in "boot" mode. This can be done in two ways:
     - Connect the RP2040-One to the computer **while holding down the "Boot" button on the device**. Note that this can be tricky
       for the Mini version if you have already soldered on the display since the display obstructs access to the RP2040-One.
     - Connect the RP2040-One to the computer. This will register a new serial (COM) port on your computer. 
       Then open a terminal program (e.g. TeraTerm, Putty or even the serial monitor in the Arduino IDE) and connect to the new COM
       port with a baud rate of 1200.
     As a result of either of these, your computer should now show a new external drive.
  3) Copy the downloaded UF2 file to the root directory of the new drive.
  4) Disconnect the RP2040-One from your computer.

If you would like to compile the firmware yourself, instructions can be found [here](software/IECBuddy/README.md).

## Usage

When it is connected to the RAD (via USB) and C64 (via serial cable), the IECBuddy behaves like a disk drive.
The initial device number is 8 but can be configured from within the RAD menu system.

Loading the directory shows all files and disk images currently on the IECBuddy file system. You can use "CD"
to enter and exit disk images. If you have a DOS wedge (like in JiffyDos), `@CD:GAMES.D64` will enter the GAMES.D64
Disk image, `@CD/` will exit the image and go back to the top-level directory. If you do not have a DOS wedge, 
doing a `LOAD"GAMES.D64",8` will automatically switch into the GAMES.D64 disk image and load its directory,
`LOAD"/",8` will return to the top level.

If desired, a new disk image can be created by using the "N" command, for examplem, executing `@N:GAMES2.D64`
in the top-level dierctory will create a new disk image named GAMES2.D64.

Files (either PRG files or disk images) can be copied between the IECBuddy and the RAD's SD card via the RAD menu system.

The IECBuddy also emulates a printer, more specifically a STAR NL-10 printer with device number 4. The STAR NL-10 was
compatible with Commodore MPS-801 commands as well as the EPSON FX-80 command set. Much of the existing C64 software
supports either one of these (if not specifically the NL-10) and should therefore be compatible.

After printing, enter the RAD menu to see a preview of the printout and/or copy a BMP or PDF version of the printed
content to the RAD's SD card.

For more information on the RAD menu structure relating to the IECBuddy see [here]().
