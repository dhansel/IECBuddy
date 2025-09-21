#include "IECDrive.h"
#include "IECPrinter.h"
#include "IECDisplay.h"
#include "IECConfig.h"
#include "Pins.h"
#include "src/IECDevice/IECBusHandler.h"
#include "protocol.h"
#include <algorithm>
using namespace std;

#define DEBUG 0

/*
--- to debug LittleFS, add the following to the top of
  C:/Users/[..]/AppData/Local/Arduino15/packages/rp2040/hardware/rp2040/[version]/libraries/LittleFS/lib/littlefs/lfs_util.h
  #define LFS_YES_TRACE
  #undef LFS_NO_DEBUG
  #undef LFS_NO_WARN
  #undef LFS_NO_ERROR
*/


#if DEBUG>0
extern "C" int printf(const char* format, ... )
{
  int res = 0;

  char buffer[512];
  va_list args;

  va_start(args, format);
  res = vsnprintf(buffer, 512, format, args);
  va_end(args);
  
  for(int i=0; i<512 && buffer[i]; i++)
    { 
      if( buffer[i]=='\n' ) Serial1.write('\r');
      Serial1.write(buffer[i]);
    }

  return res;
}

#include <malloc.h>

static uint32_t getTotalHeap()
{
   extern char __StackLimit, __bss_end__;
   return &__StackLimit  - &__bss_end__;
}

static uint32_t getFreeHeap()
{
  struct mallinfo m = mallinfo();
  return getTotalHeap() - m.uordblks;
}

#endif


// IEC bus device number
#define DEVICE_NUMBER  8

IECConfig   iecConfig;
IECDrive    iecDrive(DEVICE_NUMBER, PIN_LED);
IECPrinter  iecPrinter(4, PRINTDATAFILE);
IECDisplay *iecDisplay = NULL;

// if RESET pin is not defined, set it to 0xFF (not assigned)
#ifndef PIN_IEC_RESET
#define PIN_IEC_RESET 0xFF
#endif

#ifdef IEC_USE_LINE_DRIVERS
IECBusHandler iecBus(PIN_IEC_ATN, PIN_IEC_CLK, PIN_IEC_CLK_OUT, PIN_IEC_DATA, PIN_IEC_DATA_OUT, PIN_IEC_RESET);
#else
IECBusHandler iecBus(PIN_IEC_ATN, PIN_IEC_CLK, PIN_IEC_DATA, PIN_IEC_RESET);
#endif


// the "mytime" function is used to implement tracking of whether a file was
// modified by the C64. Whenever a file that was opened for writing is closed,
// LittleFS updates its modification time with the result returned by the
// function set via LittleFS.setTimeCallback(). After opening a file via our
// SK64 communication we make sure that s_modTime!=0 before closing it, 
// therefore resetting it to good (NOT modified). Otherwise we make sure that
// s_modTime==0 (i.e. dirty, modified). 
// We use "1" for "not modified" because if files are uploaded via the Arduino
// "LittleFS data upload" function they get a non-zero modification date.
static time_t s_modTime = 0;
time_t mytime()
{
  //Serial1.printf("mytime:%i\r\n", s_modTime);
  return s_modTime;
}


bool send_data(uint32_t length, const uint8_t *buffer)
{
  uint32_t n = Serial.write(buffer, length);

#if DEBUG>=2
  if( n!=length )
    Serial1.printf("=> ERROR (%i/%i): ", n, length);
  else
    Serial1.printf("=> %i: ", length);

  for(uint32_t i=0; i<length; i++) 
    {
      if( DEBUG==2 && i==10 ) { Serial1.printf("[...]"); break; }
      Serial1.printf("%02X ", buffer[i]);
    }

  Serial1.printf("\r\n");
#elif DEBUG>0
  if( n!=length )
    Serial1.printf("SEND ERROR (%i of %i sent)\r\n", n, length);
#endif

  return n==length;
}


bool recv_data(uint32_t length, uint8_t *buffer)
{
  uint32_t n = Serial.readBytes(buffer, length);

#if DEBUG>=2
  if( n!=length )
    Serial1.printf("<= ERROR (%i/%i)", n, length);
  else
    Serial1.printf("<= %i: ", length);

  for(uint32_t i=0; i<n; i++) 
    {
      if( DEBUG==2 && i==10 ) { Serial1.printf("[...]"); break; }
      Serial1.printf("%02X ", buffer[i]);
    }
  Serial1.printf("\r\n");
#elif DEBUG>0
  if( n!=length )
    Serial1.printf("RECEIVE ERROR (%i of %i received)\r\n", n, length);
#endif
  
  return n==length;
}


void sendDriveStatus()
{
#if DEBUG>0
  Serial1.printf("sendDriveStatus()\r\n");
#endif

  char buffer[255];
  iecDrive.getStatus(buffer, 255);
  send_string(buffer);
}


void execDriveCommand()
{
#if DEBUG>0
  Serial1.printf("execDriveCommand()\r\n");
#endif

  string cmd;
  if( recv_string(cmd) )
    {
#if DEBUG>0
      Serial1.printf("command: %s\r\n", cmd.c_str());
#endif
      if( iecDrive.getMountedImageName()==NULL )
        iecDrive.execute(cmd.c_str(), cmd.length());
      else
        {
          // execute command on SD card (not in currently mounted image)
          string imageName(iecDrive.getMountedImageName());
          iecDrive.unmountDiskImage();
          iecDrive.execute(cmd.c_str(), cmd.length());
          iecDrive.mountDiskImage(imageName.c_str());
        }

      send_status(ST_OK);
    }
}


void rmDirRecursive(string dirName)
{
  Dir dir = LittleFS.openDir(dirName.c_str());
  while( dir.next() )
    {
      string fullName = dirName+string("/")+string(dir.fileName().c_str());
#if DEBUG>0
      Serial1.printf("remove: %s\r\n", fullName.c_str());
#endif
      if( dir.isDirectory() )
        {
          rmDirRecursive(fullName);
          LittleFS.rmdir(fullName.c_str());
        }
      else
        LittleFS.remove(fullName.c_str());
    }
}


void sendDir()
{
  string s = "/";

#if DEBUG>0
  Serial1.printf("sendDir()\r\n");
#endif

  // check whether directory exists
  File f = LittleFS.open(s.c_str(), "r");
  bool ok = f && f.isDirectory();
  f.close();
  
  // send status code
  ok = send_status(ok ? ST_OK : ST_INVALID_DIR);

  if( ok )
    {
      // send available space
      struct FSInfo info;
      LittleFS.info(info);
      ok = send_uint(info.totalBytes-info.usedBytes);
    }

  if( ok )
    {
      // send directory
      Dir dir = LittleFS.openDir(s.c_str());
      while( dir.next() && ok )
        {
#if DEBUG>0
          Serial1.printf("%s %i %u\r\n", dir.fileName().c_str(), dir.fileSize(), dir.fileTime());
#endif
          uint32_t flags = 0;

          // we set the files' lastWrite() time to "0" whenever the file 
          // is modified and set it back to "1" when a file is retrieved
          // or originally uploaded
          if( dir.fileTime()==0 ) flags |= FF_MODIFIED;

          // send flags, file size and file name
          if( ok ) ok = send_uint(flags);
          if( ok ) ok = send_sint(dir.fileSize());
          if( ok ) ok = send_string(dir.fileName().c_str());
        }
      
      // send end-of-dir marker
      if( ok ) send_uint(0xFFFFFFFF);
    }
}


void sendFile()
{
  string fileName;

#if DEBUG>0
  Serial1.printf("sendFile()\r\n");
#endif
  
  // receive file name
  if( recv_string(fileName) )
    {
      // convert "/" and "\" to "-" (we don't support subdirectories)
      transform(fileName.begin(), fileName.end(), fileName.begin(), [](unsigned char c){ return c=='/' || c=='\\' ? '-' : c; });

#if DEBUG>0
      Serial1.printf("name=%s\r\n", fileName.c_str());
#endif

      File file = LittleFS.open(fileName.c_str(), "r+");
      if( file )
        {
          StatusType status = ST_OK;
          uint32_t length = file.size();
#if DEBUG>0
          Serial1.printf("length=%u\r\n", length);
#endif
          // send file length
          if( status==ST_OK )
            if( !send_uint(length) )
              status = ST_COM_ERROR;

          // send status
          if( !send_status(ST_OK) ) status = ST_COM_ERROR;

          iecDisplay->showTransmitMessage("Sending", fileName);
          iecDisplay->startProgress(length);

          // send data
          uint8_t buf[1024];
          while( status==ST_OK && length>0 )
            {
              uint32_t n = min(1024u, length);
              uint32_t i = file.read(buf, n);
#if DEBUG==1
              Serial1.write('.');
#endif
              // receiver expects a full block so we send it even if read failed
              if( !send_data(n, buf) ) status = ST_COM_ERROR;
              
              // compute and send checksum for data block
              uint8_t checksum = 0;
              for(uint32_t j=0; j<i; j++) checksum ^= buf[j];
              if( status==ST_OK )
                if( !send_data(1, &checksum) )
                  status = ST_COM_ERROR;
              
              // check if we successfully read the data
              if( i!=n ) status = ST_READ_ERROR;
              
              // send status
              if( status!=ST_COM_ERROR )
                if( !send_status(status) )
                  status = ST_COM_ERROR;
              
              // receive status
              if( status==ST_OK )
                status = recv_status();
              
              iecDisplay->updateProgress(i);
              length -= i;
            }

          // close file (see comment in mytime() function above)
          s_modTime = 1;
          file.close();
          s_modTime = 0;

          iecDisplay->redraw();
        }
      else
        {
          // receiver expects a length => send 0
          send_uint(0);
          // send error
          send_status(ST_INVALID_FILE);
        }
    }

#if DEBUG==1
  Serial1.println();
#endif
}


void receiveFile()
{
  string fileName;

#if DEBUG>0
  Serial1.printf("receiveFile()");
#endif

  // receive file name
  if( recv_string(fileName) )
    {
      StatusType status = ST_OK;

      // convert "/" and "\" to "-" (we don't support subdirectories)
      transform(fileName.begin(), fileName.end(), fileName.begin(), [](unsigned char c){ return c=='/' || c=='\\' ? '-' : c; });

      // receive length
      uint32_t length;
      if( status==ST_OK )
        if( !recv_uint(length) )
          status = ST_COM_ERROR;

      if( status==ST_OK )
        {
          // make sure that either the file name has a .XYZ extension or it is of form "$...$" (hidden file)
          int len = fileName.length();
          if( len<4 || fileName[len-4]!='.' || !isalphanum(fileName[len-3]) || !isalphanum(fileName[len-2]) || !isalphanum(fileName[len-1]) )
            if( len<2 || fileName[0]!='$' || fileName[len-1]!='$' )
              status = ST_INVALID_FILE;
        }

      if( status==ST_OK )
        {
          // check whether a file with this name already exists
          if( LittleFS.exists(fileName.c_str()) )
            status = ST_FILE_EXISTS;
        }

      if( status==ST_OK )
        {
          // check available space
          // according to https://github.com/littlefs-project/littlefs/issues/533
          // LittleFS requires ~6 blocks of free storage space to work with
          struct FSInfo info;
          LittleFS.info(info);
          if( length > info.totalBytes-info.usedBytes-6*info.blockSize )
            status = ST_DRIVE_FULL;
        }

#if DEBUG>0
      Serial1.printf(" name=%s, length=%u ", fileName.c_str(), length);
#endif

      // open file
      if( status==ST_OK )
        {
          File file = LittleFS.open(fileName.c_str(), "w");
          if( file )
            {
              // send initial status
              if( !send_status(ST_OK) ) status = ST_COM_ERROR;

              iecDisplay->showTransmitMessage("Receiving", fileName);
              iecDisplay->startProgress(length);

              // receive data
              uint8_t buf[1024];
              while( status==ST_OK && length>0 )
                {
                  uint8_t checksum1, checksum2;
                  uint32_t n = min(1024u, length);
#if DEBUG>0
                  Serial1.write('.');
#endif
                  // receive data block
                  if( !recv_data(n, buf) )
                    status = ST_COM_ERROR;
          
                  // compute and receive checksum
                  if( status==ST_OK )
                    {
                      uint8_t checksum1=0, checksum2=0;
                      for(uint32_t j=0; j<n; j++) checksum1 ^= buf[j];

                      if( !recv_data(1, &checksum2) )
                        status = ST_COM_ERROR;
                      else if( checksum1!=checksum2 )
                        status = ST_CHECKSUM_ERROR;
                    }

                  // receive transmitter status
                  if( status!=ST_COM_ERROR && recv_status()==ST_OK )
                    {
                      // save data block
                      if( status==ST_OK )
                        if( file.write(buf, n) != n )
                          status = ST_WRITE_ERROR;

                      // send our status
                      if( !send_status(status) )
                        status = ST_COM_ERROR;
                    }
                  
                  iecDisplay->updateProgress(n);
                  length -= n;
                }
              
              // close file (see comment in mytime() function above)
              s_modTime = 1;
              file.close();
              s_modTime = 0;

              // if there was a transmission error then remove 
              // received (incomplete) file
              if( status!=ST_OK )
                LittleFS.remove(fileName.c_str());

              iecDisplay->redraw();
            }
          else
            send_status(ST_WRITE_ERROR);
        }
      else
        send_status(status);
    }

#if DEBUG>0
  Serial1.println();
#endif
}


void deleteFile()
{
  string fileName;

  // receive file name
  if( recv_string(fileName) )
    {
      StatusType status = ST_OK;

      if( !LittleFS.exists(fileName.c_str()) )
        status = ST_FILE_NOT_FOUND;
      else if( !LittleFS.remove(fileName.c_str()) )
        status = ST_WRITE_ERROR;

      send_status(status);
    }
}


void mountDiskImage()
{
  string fileName;

  // receive image name
  if( recv_string(fileName) )
    {
      StatusType status = ST_OK;

      // check whether a file with this name exists
      if( LittleFS.exists(fileName.c_str()) )
        status = iecDrive.mountDiskImage(fileName.c_str()) ? ST_OK : ST_INVALID_FILE;
      else
        status = ST_FILE_NOT_FOUND;
      
      send_status(status);
    }
}


void unmountDiskImage()
{
  iecDrive.unmountDiskImage();
  send_status(ST_OK);
}


void getMountedDiskImage()
{
  const char *filename = iecDrive.getMountedImageName();
  send_string(filename ? filename : "");
}



void setConfigValue()
{
#if DEBUG>0
  Serial1.printf("setConfigValue()\r\n");
#endif
  string key, value;
  if( recv_string(key) && recv_string(value) )
    {
#if DEBUG>0
      Serial1.printf("%s => %s\r\n", key.c_str(), value.c_str());
#endif
      iecConfig.setValue(key, value);
      send_status(ST_OK);
    }
}


void getConfigValue()
{
#if DEBUG>0
  Serial1.printf("getConfigValue()\r\n");
#endif
  string key;
  if( recv_string(key) )
    {
      string value = iecConfig.getValue(key);
#if DEBUG>0
      Serial1.printf("%s => %s\r\n", key.c_str(), value.c_str());
#endif
      send_string(value);
    }
}


void clearConfig()
{
  iecConfig.clear();
  send_status(ST_OK);
}


void reboot()
{
  send_status(ST_OK);
  Serial.flush();
  delay(100);
  rp2040.reboot();
}


void showBitmap()
{
  StatusType status = ST_OK;

  int32_t  x, y;
  uint32_t w, h;
  if( !recv_sint(x) || !recv_sint(y) || !recv_uint(w) || !recv_uint(h) )
    status = ST_COM_ERROR;

  if( status==ST_OK )
    {
      // startImage() returns number of bytes per pixel
      uint32_t bpp    = iecDisplay->startImage(x, y, w, h);
      uint32_t nbytes = w*h*bpp;
      uint8_t  buffer[1024];

      if( bpp==0 )
        status = ST_DRIVE_FULL;

      while( nbytes>0 && status==ST_OK )
        {
          uint32_t n = min(1024, nbytes);
          if( send_status(ST_OK) )
            {
              if( recv_data(n, buffer) )
                iecDisplay->addImageData(buffer, n);
              else
                status = ST_COM_ERROR;
            }
          else
            status = ST_COM_ERROR;

          nbytes -= n;
        }

      iecDisplay->endImage();
      iecDisplay->redraw();
    }
  
  send_status(status);
}


void showGIF()
{
  StatusType status = ST_OK;

  int32_t  x, y;
  uint32_t size;
  if( !recv_sint(x) || !recv_sint(y) || !recv_uint(size) )
    status = ST_COM_ERROR;
  
  if( status==ST_OK )
    {
      uint8_t *buffer = (uint8_t *) malloc(size);
      if( buffer )
        {
          uint32_t nbytes = size;
          uint8_t *ptr = buffer;
          while( nbytes>0 && status==ST_OK )
            {
              uint32_t n = min(1024, nbytes);
              if( !send_status(ST_OK) || !recv_data(n, ptr) )
                status = ST_COM_ERROR;
              
              nbytes -= n;
              ptr    += n;
            }
          
          if( status==ST_OK )
            iecDisplay->setBackgroundImageGIF(buffer, size, x, y);

          free(buffer);
        }
      else
        status = ST_DRIVE_FULL;
    }
  
  send_status(status);
}


void execCmd(CommandType cmd)
{
  switch( cmd )
    {
    case CMD_DIR:            sendDir();  break;
    case CMD_GETFILE:        sendFile(); break;
    case CMD_PUTFILE:        receiveFile(); break;
    case CMD_DRIVESTATUS:    sendDriveStatus(); break;
    case CMD_DRIVECMD:       execDriveCommand(); break;
    case CMD_MOUNT:          mountDiskImage(); break;
    case CMD_UNMOUNT:        unmountDiskImage(); break;
    case CMD_GET_MOUNTED:    getMountedDiskImage(); break;
    case CMD_SET_CONFIG_VAL: setConfigValue(); break;
    case CMD_GET_CONFIG_VAL: getConfigValue(); break;
    case CMD_CLEAR_CONFIG:   clearConfig(); break;
    case CMD_DELETE_FILE:    deleteFile(); break;
    case CMD_SHOW_BITMAP:    showBitmap(); break;
    case CMD_SHOW_GIF:       showGIF(); break;
    case CMD_REBOOT:         reboot(); break;

    default: 
      {
#if DEBUG>0
        Serial1.printf("INVALID COMMAND: %02X\r\n", cmd);
#endif
        send_status(ST_INVALID_COMMAND);
        break;
      }
    }
}


void setup()
{
  Serial.begin(115200);

#if DEBUG>0
  Serial1.begin(115200);
  Serial1.print("DEBUG START\r\n");
#endif

  LittleFS.begin();
  LittleFS.setTimeCallback(mytime);

  iecConfig.begin("/$CONFIG$");

  // setting fsformat==1 will format filesystem on reboot
  if( iecConfig.getValue("fsformat")=="1" )
    {
      LittleFS.format();
      iecConfig.clear();
    }

  string displayType = iecConfig.getValue("display");
  if( displayType.empty() )
    {
#if defined(SUPPORT_SSD1306)
      displayType = "SSD1306";
#elif defined(SUPPORT_ST7789)
      displayType = "ST7789";
#else
      displayType = "NONE";
#endif
      iecConfig.setValue("display", displayType);
    }

#if DEBUG>0
  Serial1.printf("Display type: %s\r\n", displayType.c_str());
#endif
  iecDisplay = IECDisplay::Create(displayType);

  iecDisplay->begin(atoi(iecConfig.getValue("rotate").c_str()));

  iecDrive.setConfig(&iecConfig);
  iecDrive.setDisplay(iecDisplay);
  iecPrinter.setConfig(&iecConfig);
  iecPrinter.setDisplay(iecDisplay);

#ifdef IEC_SUPPORT_PARALLEL
  iecBus.setParallelPins(PIN_PAR_FLAG2, PIN_PAR_PC2,
                         PIN_PAR_PB0, PIN_PAR_PB1, PIN_PAR_PB2, PIN_PAR_PB3,
                         PIN_PAR_PB4, PIN_PAR_PB5, PIN_PAR_PB6, PIN_PAR_PB7);
#endif

  iecBus.attachDevice(&iecDrive);
  iecBus.attachDevice(&iecPrinter);
  iecBus.begin();
}


void loop()
{
  iecBus.task();

  if( Serial.available()>0 )
    {
#if DEBUG>1
      Serial1.print("command receive...\r\n");
      CommandType cmd = recv_command();
      Serial1.printf("command execute %02X... (free heap=%i)\r\n", cmd, getFreeHeap());
      if( cmd != CMD_INVALID ) execCmd(cmd);
      Serial1.printf("command done (free heap=%i)\r\n", getFreeHeap());
#else
      CommandType cmd = recv_command();
      if( cmd != CMD_INVALID ) execCmd(cmd);
#endif
    }
}
