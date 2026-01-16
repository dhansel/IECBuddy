#include "commfun.h"
#include "utilfun.h"
#include "ceserial.h"
#include "../IECBuddy/protocol.h"
#include <sstream>
#include <vector>
#include <string>

#define DEBUG 0

static ceSerial com;

#ifdef WIN32

#include <windows.h>
#include <setupapi.h>
#include <devguid.h>
#include <regstr.h>
#include <iostream>
#include <initguid.h>

// Link with setupapi.lib
// GUID_DEVINTERFACE_COMPORT definition
DEFINE_GUID(GUID_DEVINTERFACE_COMPORT,
    0x86E0D1E0, 0x8089, 0x11D0,
    0x9C, 0xE4, 0x08, 0x00, 0x3E, 0x30, 0x1F, 0x73);

string autodetectPort()
{
  vector<string> comPorts;

  // Get the device info set for all present devices (GUID_DEVINTERFACE_COMPORT)
  HDEVINFO deviceInfoSet = SetupDiGetClassDevs(&GUID_DEVINTERFACE_COMPORT, nullptr, nullptr,
                                               DIGCF_PRESENT | DIGCF_DEVICEINTERFACE);
  if( deviceInfoSet == INVALID_HANDLE_VALUE ) false;

  SP_DEVINFO_DATA devInfoData;
  devInfoData.cbSize = sizeof(SP_DEVINFO_DATA);

  string found;
  for(DWORD i = 0; found.empty() && SetupDiEnumDeviceInfo(deviceInfoSet, i, &devInfoData); ++i)
    {
      char portName[256];
      if( SetupDiGetDeviceRegistryPropertyA(deviceInfoSet, &devInfoData,
                                            SPDRP_FRIENDLYNAME, nullptr,
                                            (PBYTE)portName, sizeof(portName), nullptr) )
        {
          // Parse out actual "COMx" from friendly name, e.g. "USB Serial Device (COM3)"
          string friendly(portName);
          size_t start = friendly.find("(");
          size_t end = friendly.find(")", start);
          if (start != string::npos && end != string::npos && end > start) 
            {
              string port = friendly.substr(start + 1, end - start - 1);
              // Ensure it starts with "COM"
              if( port.find("COM") == 0 )
                {
                  printf("Checking %s...\n", port.c_str());
                  port = "\\\\.\\" + port;
                  HANDLE h = CreateFileA(port.c_str(), GENERIC_READ | GENERIC_WRITE, 0, 0, OPEN_EXISTING, 0, 0);
                  if( h != INVALID_HANDLE_VALUE )
                    {
                      PurgeComm(h, PURGE_TXABORT | PURGE_RXABORT | PURGE_TXCLEAR | PURGE_RXCLEAR);

                      DCB dcb;
                      GetCommState(h, &dcb);
                      dcb.ByteSize = 8;
                      dcb.Parity   = NOPARITY;
                      dcb.StopBits = ONESTOPBIT;
                      dcb.BaudRate = 115200;
                      dcb.fOutxCtsFlow = false;
                      dcb.fOutxDsrFlow = false;
                      dcb.fOutX = false;
                      dcb.fDtrControl = DTR_CONTROL_ENABLE;
                      dcb.fRtsControl = RTS_CONTROL_ENABLE;
                      SetCommState(h, &dcb);

                      COMMTIMEOUTS timeouts = { 0 };
                      timeouts.ReadIntervalTimeout         = 1;
                      timeouts.ReadTotalTimeoutConstant    = 100;
                      timeouts.ReadTotalTimeoutMultiplier  = 1;
                      timeouts.WriteTotalTimeoutConstant   = 100;
                      timeouts.WriteTotalTimeoutMultiplier = 1;
                      SetCommTimeouts(h, &timeouts);

                      DWORD n;
                      uint8_t data[4];
                      data[0] = (MAGIC_PING_TO_DEVICE >>  0) & 0xFF;
                      data[1] = (MAGIC_PING_TO_DEVICE >>  8) & 0xFF;
                      data[2] = (MAGIC_PING_TO_DEVICE >> 16) & 0xFF;
                      data[3] = (MAGIC_PING_TO_DEVICE >> 24) & 0xFF;

                      if( WriteFile(h, data, 4, &n, NULL) && n==4 )
                        {
                          Sleep(100);
                          bool b = ReadFile(h, data, 4, &n, NULL);
                          if( b && n==4 && 
                              data[0]==((MAGIC_PING_FROM_DEVICE >>  0)&0xFF) &&
                              data[1]==((MAGIC_PING_FROM_DEVICE >>  8)&0xFF) &&
                              data[2]==((MAGIC_PING_FROM_DEVICE >> 16)&0xFF) &&
                              data[3]==((MAGIC_PING_FROM_DEVICE >> 24)&0xFF) )
                            {
                              found = port;
                            }
                        }
                      
                      CloseHandle(h);
                    }
                }
            }
        }
    }

  SetupDiDestroyDeviceInfoList(deviceInfoSet);
  return found;
}

#else
#define Sleep(ms) usleep(ms*1000)
#define fopen_s(pfile, name, mode) ((*(pfile)=fopen(name, mode))!=NULL)

string autodetectPort()
{
  return "";
}

#endif


int comOpen(const char *port)
{
  if( port==NULL )
    {
      // attempt to auto-detect
      string portName = autodetectPort();
      if( portName.empty() )
        return -1;
      else
        {
          printf("Found IECBuddy on port %s\n", portName.c_str());
          com.SetPortName(portName);
          return com.Open();
        }
    }
  else
    {
#ifdef WIN32
      com.SetPortName("\\\\.\\" + string(port));
#else
      com.SetPortName("/dev/" + string(port));
#endif
      return com.Open();
    }
}


void comClose()
{
  com.Close();
}


bool send_data(uint32_t length, const uint8_t *buffer)
{
  uint32_t n = com.Write((const char *) buffer, (long) length) ? length : 0;

#if DEBUG>=2
  if( n!=length )
    printf("=> ERROR (%i/%i): ", n, length);
  else
    printf("=> %i: ", length);

  for(uint32_t i=0; i<length; i++) 
    {
      if( DEBUG==2 && i==10 ) { printf("[...]"); break; }
      printf("%02X ", buffer[i]);
    }

  printf("\n");
#endif

  return n==length;
}


bool recv_data(uint32_t length, uint8_t *buffer)
{
  uint32_t n = 0;

  if( length>0 )
    while( n==0 ) 
      { 
        n = com.Read((char *) buffer, (long) length) ? length : 0;
        if( n==0 ) Sleep(10);
      }

#if DEBUG>=2
  if( n!=length )
    printf("<= ERROR (%i/%i)", n, length); 
  else
    printf("<= %i: ", length);

  for(uint32_t i=0; i<n; i++) 
    {
      if( DEBUG==2 && i==10 ) { printf("[...]"); break; }
      printf("%02X ", buffer[i]);
    }

  printf("\n");
#endif

  return n==length;
}



StatusType printDir(const vector<string> &patterns)
{
  StatusType status = ST_OK;
  uint32_t available;

#if DEBUG>0
  printf("printDir()\n");
#endif

  if( !send_command(CMD_DIR) ) 
    status = ST_COM_ERROR;

  if( status==ST_OK )
    status = recv_status();

  if( status==ST_OK )
    if( !recv_uint(available) )
      status = ST_COM_ERROR;

  if( status==ST_OK )
    {
      bool ok = true;

      uint32_t flags, size;
      string   name;

      while( (ok=recv_uint(flags)) && flags!=0xFFFFFFFF )
        {
          if( ok ) ok = recv_uint(size);
          if( ok ) ok = recv_string(name);

          if( ok && patterns.empty() || isMatch(patterns, name.c_str()) || isMatch(patterns, fromPETSCII(name).c_str()) )
            printf("%7u %02X %s\n", size, flags, fromPETSCII(name).c_str());
        }

      printf("%7u bytes free.\n\n", available);
      if( !ok ) status = ST_COM_ERROR;
    }
  
  return status;
}


StatusType printDir(const char *pattern)
{
  vector<string> patterns;
  if( pattern!=NULL ) patterns.push_back(pattern);
  return printDir(patterns);
}


StatusType getDirFiles(vector<string> &filenames, const vector<string> &patterns)
{
  uint32_t n;
  StatusType status = ST_OK;

  if( !send_command(CMD_DIR) ) 
    status = ST_COM_ERROR;
  
  if( status==ST_OK )
    status = recv_status();
  
  // available space
  if( status==ST_OK && !recv_uint(n) )
    status = ST_COM_ERROR;
              
  filenames.clear();
  while( status==ST_OK )
    {
      string name;
      if( status==ST_OK && !recv_uint(n) ) status = ST_COM_ERROR;  // flags
      if( n==0xFFFFFFFF ) break;
      
      if( status==ST_OK && !recv_uint(n) ) status = ST_COM_ERROR;  // size
      if( status==ST_OK && !recv_string(name) ) status = ST_COM_ERROR;
      
      if( status==ST_OK )
        if( patterns.empty() || isMatch(patterns, name.c_str()) || isMatch(patterns, fromPETSCII(name).c_str()) )
          filenames.push_back(name);
    }

  return status;
}


StatusType getDirFiles(vector<string> &filenames, const char *pattern)
{
  vector<string> patterns;
  if( pattern!=NULL ) patterns.push_back(pattern);
  return getDirFiles(filenames, patterns);
}



StatusType getDriveStatus(string &drivestatus)
{
  StatusType status = ST_OK;

#if DEBUG>0
  printf("getdrivestatus()\n");
#endif

  // send command
  if( status==ST_OK )
    if( !send_command(CMD_DRIVESTATUS) )
      status = ST_COM_ERROR;
  
  if( status==ST_OK )
    if( !recv_string(drivestatus) )
      status = ST_COM_ERROR;

  return status;
}


StatusType sendDriveCommand(const string &cmd)
{
  StatusType status = ST_OK;

  // send command
  if( status==ST_OK )
    if( !send_command(CMD_DRIVECMD) )
      status = ST_COM_ERROR;

  // send command string
  if( status==ST_OK )
    if( !send_string(toPETSCII(cmd)) )
      status = ST_COM_ERROR;
  
  // receive status
  if( status==ST_OK )
    status = recv_status();

  return status;
}


StatusType putFile(const string &fname, string dstfname)
{
  StatusType status = ST_OK;

#if DEBUG>0
  printf("putFile(%s)", fname.c_str());
#endif

  FILE* file;
  if( fopen_s(&file, fname.c_str(), "rb")==0 )
    {
      // send command
      if( status==ST_OK )
        if( !send_command(CMD_PUTFILE) ) 
          status = ST_COM_ERROR;

      // send file name
      if( status==ST_OK )
        if( !send_string(dstfname.empty() ? toPETSCII(fname) : dstfname) )
          status = ST_COM_ERROR;

      // send file length
      fseek(file, 0, SEEK_END);
      uint32_t length = ftell(file);
      if( status==ST_OK )
        if( !send_uint(length) )
          status = ST_COM_ERROR;

#if DEBUG>0
      printf(" %u bytes ", length); fflush(stdout);
#endif
          
      // receive status
      if( status==ST_OK )
        status = recv_status();
      
      // send data
      uint8_t buf[1024];
      fseek(file, 0, SEEK_SET);
      while( status==ST_OK && length>0 )
        {
          uint32_t n = min(1024u, length);
          uint32_t i = (uint32_t) fread(buf, 1, n, file);
#if DEBUG>0
          printf(".");
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
          
          length -= i;
        }
    }
  else
    status = ST_READ_ERROR;

#if DEBUG>0
  printf("\n");
#endif

  return status;
}


StatusType getFile(const string &fname, string dstfname)
{
  StatusType status = ST_OK;

#if DEBUG>0
  printf("getFile(%s)", fname.c_str()); fflush(stdout);
#endif

  FILE *file;
      
  if( fopen_s(&file, dstfname.empty() ? fromPETSCII(fname).c_str() : dstfname.c_str(), "wb")==0 )
    {
      uint32_t length;

      // send command
      if( status==ST_OK )
        if( !send_command(CMD_GETFILE) )
          status = ST_COM_ERROR;
      
      // send file name
      if( status==ST_OK )
        if( !send_string(fname) )
          status = ST_COM_ERROR;

      // receive file length
      if( status==ST_OK )
        if( !recv_uint(length) )
          status = ST_COM_ERROR;

#if DEBUG>0
      printf(" %u bytes ", length); fflush(stdout);
#endif

      // receive status
      if( status==ST_OK )
        status = recv_status();
      
      // receive data
      uint8_t buf[1024];
      while( status==ST_OK && length>0 )
        {
#if DEBUG>0
          printf(".");
#endif
          uint32_t n = min(1024u, length);
          
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
                if( fwrite(buf, 1, n, file) != n )
                  status = ST_WRITE_ERROR;

              // send our status
              if( !send_status(status) )
                status = ST_COM_ERROR;
            }

          length -= n;
        }

      // close file
      fclose(file);

      // if there was a transmission error then remove 
      // received (incomplete) file
      if( status!=ST_OK )
        remove(fname.c_str());
    }

#if DEBUG>0
  printf("\n");
#endif

  return status;
}


StatusType mountDisk(const string &name)
{
  StatusType status = ST_OK;

  // send command
  if( status==ST_OK )
    if( !send_command(CMD_MOUNT) )
      status = ST_COM_ERROR;

  // send image name
  if( status==ST_OK )
    if( !send_string(toPETSCII(name)) )
      status = ST_COM_ERROR;
  
  // receive status
  if( status==ST_OK )
    status = recv_status();
  
  return status;
}


StatusType unmountDisk()
{
  StatusType status = ST_OK;

  // send command
  if( status==ST_OK )
    if( !send_command(CMD_UNMOUNT) )
      status = ST_COM_ERROR;

  // receive status
  if( status==ST_OK )
    status = recv_status();
  
  return status;
}


StatusType getMountedDisk()
{
  string name;
  StatusType status = ST_OK;

  // send command
  if( status==ST_OK )
    if( !send_command(CMD_GET_MOUNTED) )
      status = ST_COM_ERROR;

  // receive image name
  if( status==ST_OK )
    {
      if( recv_string(name) )
        name = fromPETSCII(name);
      else
        status = ST_COM_ERROR;
    }
  
  if( status==ST_OK )
    {
      if( name.empty() )
        status = ST_NOT_MOUNTED;
      else
        printf("Currently mounted disk image: %s\n", name.c_str());
    }

  return status;
}


StatusType setConfigValue(const string &key, const string &value)
{
  StatusType status = ST_OK;

  // send command
  if( status==ST_OK )
    if( !send_command(CMD_SET_CONFIG_VAL) )
      status = ST_COM_ERROR;

  // send key and value
  if( status==ST_OK )
    if( !send_string(key) || !send_string(value) )
      status = ST_COM_ERROR;

  // receive status
  if( status==ST_OK )
    status = recv_status();

  return status;
}


StatusType getConfigValue(const string &key)
{
  string value;
  StatusType status = ST_OK;

  // send command
  if( status==ST_OK )
    if( !send_command(CMD_GET_CONFIG_VAL) )
      status = ST_COM_ERROR;

  // send key
  if( status==ST_OK )
    if( !send_string(key) )
      status = ST_COM_ERROR;

  // receive value
  if( status==ST_OK )
    if( !recv_string(value) )
      status = ST_COM_ERROR;

  if( status==ST_OK )
    printf("Value of config '%s' is '%s'\n", key.c_str(), value.c_str());

  return status;
}


StatusType clearConfig()
{
  StatusType status = ST_OK;

  // send command
  if( status==ST_OK )
    if( !send_command(CMD_CLEAR_CONFIG) )
      status = ST_COM_ERROR;

  // receive status
  if( status==ST_OK )
    status = recv_status();

  return status;
}


StatusType reboot()
{
  StatusType status = ST_OK;

  // send command
  if( status==ST_OK )
    if( !send_command(CMD_REBOOT) )
      status = ST_COM_ERROR;

  // receive status
  if( status==ST_OK )
    status = recv_status();

  return status;
}


StatusType deleteFile(const string &fname)
{
  StatusType status = ST_OK;

  // send command
  if( status==ST_OK )
    if( !send_command(CMD_DELETE_FILE) )
      status = ST_COM_ERROR;

  // send file name
  if( status==ST_OK )
    if( !send_string(fname) )
      status = ST_COM_ERROR;
  
  // receive status
  if( status==ST_OK )
    status = recv_status();
  
  return status;
}


StatusType sendBitmap(const string &fname, int32_t x, int32_t y, uint32_t w, uint32_t h)
{
  uint32_t nbytes = w*h*2;
  uint8_t *buffer = NULL;
  StatusType status = ST_OK;

  FILE *f;
  if( fopen_s(&f, fname.c_str(), "rb")==0 )
    {
      buffer = (uint8_t *) malloc(nbytes);
      if( fread(buffer, 1, nbytes, f)!=nbytes )
        status = ST_INVALID_DATA;
      fclose(f);
    }
  else
    status = ST_FILE_NOT_FOUND;

  // send command
  if( status==ST_OK )
    if( !send_command(CMD_SHOW_BITMAP) )
      status = ST_COM_ERROR;

  // send position/size
  if( status==ST_OK )
    if( !send_sint(x) || !send_sint(y) || !send_uint(w) || !send_uint(h) )
      status = ST_COM_ERROR;

  // send image data
  if( status==ST_OK )
    {
      uint8_t *ptr = buffer;
      while( nbytes>0 && status==ST_OK && (status=recv_status())==ST_OK )
        {
          uint32_t n = min(1024u, nbytes);
          if( send_data(n, ptr) )
            {
              ptr += n;
              nbytes -= n;
            }
          else
            status = ST_COM_ERROR;
        }
    }

  // receive status
  if( status==ST_OK )
    status = recv_status();

  if( buffer!=NULL )
    free(buffer);

  return status;
}


StatusType sendGIF(const string &fname, int32_t x, int32_t y)
{
  uint32_t nbytes;
  uint8_t *buffer = NULL;
  StatusType status = ST_OK;

  FILE *f;
  if( fopen_s(&f, fname.c_str(), "rb")==0 )
    {
      fseek(f, 0, SEEK_END);
      nbytes = ftell(f);
      fseek(f, 0, SEEK_SET);

      buffer = (uint8_t *) malloc(nbytes);
      if( fread(buffer, 1, nbytes, f)!=nbytes )
        status = ST_INVALID_DATA;
      fclose(f);
    }
  else
    status = ST_FILE_NOT_FOUND;

  // send command
  if( status==ST_OK )
    if( !send_command(CMD_SHOW_GIF) )
      status = ST_COM_ERROR;

  // send position
  if( status==ST_OK )
    if( !send_sint(x) || !send_sint(y) || !send_uint(nbytes) )
      status = ST_COM_ERROR;

  // send image data
  if( status==ST_OK )
    {
      uint8_t *ptr = buffer;
      while( nbytes>0 && status==ST_OK && (status=recv_status())==ST_OK )
        {
          uint32_t n = min(1024u, nbytes);
          if( send_data(n, ptr) )
            {
              ptr += n;
              nbytes -= n;
            }
          else
            status = ST_COM_ERROR;
        }
    }

  // receive status
  if( status==ST_OK )
    status = recv_status();

  if( buffer!=NULL )
    free(buffer);

  return status;
}


bool comResetToBoot(string port)
{
  ceSerial com;

#ifdef WIN32
  if( !port.empty() && port[0]!='\\' ) port = "\\\\.\\" + port;
#else
  if( !port.empty() && port[0]!='/' ) port = "/dev/" + port;
#endif

  com.SetPortName(port);
  com.SetBaudRate(1200);
  if( com.Open()==0 )
    {
      com.Close();
      return true;
    }
  else
    return false;
}
