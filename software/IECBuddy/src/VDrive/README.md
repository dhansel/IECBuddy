# Virtual disk drive (VDrive) library

This library provides fairly simple access to Commodore disk 
images (D64, D71, D71, G64, G71) for C++ projects using SDFat in the Arduino environment.

## Credits

Most of this code was extracted from [VICE](https://vice-emu.sourceforge.io/). 
VICE is distributed under the GPL-2 license. All credits go to the VICE developers.

The VDriveClass C++ wrapper code was written by me and is distributed here under the GPL-3 license.

## Installation

To install this library, click the green "Code" button on the top of this page and select "Download ZIP".
Then in the Arduino IDE select "Sketch->Include Library->Add ZIP Library" and select the downloaded ZIP file.

## Integration

The VDrive library can be integrated into different environments by adapting the
implementation for architecture-dependent functions in the archdep-*.c files.

As is, it is set up for the Arduino environment (using the SDFat library to access an SD card). 
It should be fairly simple to adjust to other environments.

## VDrive class reference

- ```VDrive(uint8_t unit)```

  Constructor. Creates a new VDrive. The ```unit``` parameter defines the device
  number starting at 0, i.e. drive #8 should have unit 0.

-  ```bool openDiskImage(const char *filename, bool readOnly = false)```
  
  Opens a new disk image on the host file system, either read/write or read-only.
  Returns true if filename was recognized to be a valid, supported disk image
  and false otherwise.

- ```void closeDiskImage()```
  
  Closes the disk image currently in use.

- ```bool isOk()```
  
  Returns true if a disk image has been successfuly opened for this drive.

- ```bool openFile(uint8_t channel, const char *name, bool convertNameToPETSCII = false)```
  
  Opens a file within the current disk image on the given channel, returning true if
  the file was successfully opened. If "convertNameToPETSCII" is false then the "name"
  parameter is assumed to be PETSCII characters, otherwise it is assumed ASCII and will
  be converted to PETSCII before attempting to open the file.

- ```bool closeFile(uint8_t channel)```
  
  Closes the file that is currently open on a channel (if any).

- ```void closeAllChannels()```
  
  Closes all currently open files on all channels.

- ```int getNumOpenChannels()```
  
  Returns the number of currently active channels (i.e. channels that have a file opened).

- ```bool isFileOk(uint8_t channel)```
  
  Returns true if the file on the given channel is ok to read and/or write.

- ```bool read(uint8_t channel, uint8_t *buffer, size_t *nbytes, bool *eoi)```
  
  Reads data from the file on the given channel. On entry, nbytes should contain
  the maximum number of bytes to read, on exit, nbytes contains the number of bytes
  actually read (can be different due to error or EOF).
  Returns false if an error occurred while reading (EOF is not an error) and true otherwise.
  The "eoi" parameter will be set to "true" if an EOF condition was encountered
  during the current call, otherwise it will remain unchanged.
  If an error occurred, call getStatusString to return the error message.

- ```bool write(uint8_t channel, uint8_t *buffer, size_t *nbytes)```
  
  Write data to the file on the given channel. On entry, nbytes should contain
  the number of bytes to write, on exit, nbytes contains the number of bytes
  actually written (can be different due to error).
  Returns false if an error occurred while writing and true otherwise.
  If an error occurred, call getStatusString to return the error message.

- ```int execute(const char *cmd, size_t cmdLen, bool convertToPETSCII = false)```
  
  Executes the given DOS command, returns true on success false otherwise.
  - returns 0 if there was an error (call getStatusString to retrieve error message)
  - returns 1 if the command succeeded
  - returns 2 if the command succeeded AND there is return data in the status buffer (call getStatusBuffer to read the data)

- ```const char *getStatusString()```
  
  Returns the current status message from the error message buffer
  calling this if read/write/execute fails gives the standard CBMDOS error messages

- ```size_t getStatusBuffer(void *buf, size_t bufSize)```
  
  Copies the contents of the drive's status buffer to "buf", not exceeding
  the given bufSize length.

- ```static bool createDiskImage(const char *filename, const char *itype, const char *name, bool convertNameToPETSCII)```
  
  Creates and optionally formats a new disk image. Parameters
  - filename: name of the created image file on the host file system, required
  - itype: image type ("d64", "g64", ...), if NULL use extension from filename parameter
  - name: disk name and id ("NAME,ID") used when formatting the disk image. If NULL, disk image will not be formatted,
    if ",ID" is missing then "00" will be used as the ID.
  - convertNameToPETSCII: if false then the name argument is assumed to be PETSCII, otherwise it will be converted from ASCII to PETSCII
