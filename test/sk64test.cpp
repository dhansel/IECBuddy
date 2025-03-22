#include <stdio.h>
#include <iostream>
#include "ceserial.h"
#include "../protocol.h"
using namespace std;

#define DEBUG 0

// ------------------ helper functions -----------------

static ceSerial com;

#define SHOW_LOWERCASE 0
string toPETSCII(const string &s)
{
  string rs;
  rs.reserve(s.length());

  for(size_t i=0; i<s.length(); i++)
    {
      char c = s[i];

      if( c>=65 && c<=90 && SHOW_LOWERCASE )
        c += 32;
      else if( c>=97 && c<=122 )
        c -= 32;

      rs += c;
    }
  
  return rs;
}


string fromPETSCII(const string &s)
{
  string rs;
  rs.reserve(s.length());

  for(size_t i=0; i<s.length(); i++)
    {
      char c = s[i];

      if( c==0xFF )
        c = '~';
      else if( c>=192 ) 
        c -= 96;

      if( c>=65 && c<=90 )
        c += 32;
      else if( c>=97 && c<=122 )
        c -= 32;

      rs += c;
    }

  return rs;
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


// ------------------ command functions -----------------


StatusType readDir()
{
  StatusType status = ST_OK;
  uint32_t available;

#if DEBUG>0
  printf("readDir()\n");
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
          if( ok )
            {
              printf("%7u %02X %s\n", size, flags, fromPETSCII(name).c_str());
            }
        }

      printf("%7u bytes free.\n\n", available);
      if( !ok ) status = ST_COM_ERROR;
    }
  
  return status;
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


StatusType putFile(const string &fname)
{
  StatusType status = ST_OK;

#if DEBUG>0
  printf("putFile(%s)", fname.c_str());
#endif

  FILE *file = fopen(fname.c_str(), "rb");
  if( file )
    {
      // send command
      if( status==ST_OK )
        if( !send_command(CMD_PUTFILE) ) 
          status = ST_COM_ERROR;

      // send file name
      if( status==ST_OK )
        if( !send_string(toPETSCII(fname)) ) 
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
          uint32_t i = fread(buf, 1, n, file);
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


StatusType getFile(const string &fname)
{
  StatusType status = ST_OK;

#if DEBUG>0
  printf("getFile(%s)", fname.c_str()); fflush(stdout);
#endif

  FILE *file = fopen(fname.c_str(), "wb");
  if( file )
    {
      uint32_t length;

      // send command
      if( status==ST_OK )
        if( !send_command(CMD_GETFILE) )
          status = ST_COM_ERROR;
      
      // send file name
      if( status==ST_OK )
        if( !send_string(toPETSCII(fname)) )
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
          uint8_t checksum1, checksum2;
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


// ------------------ main function -----------------


void execCommand(string cmd)
{
  int status = ST_OK;

  if( cmd == "dir" )
    {
      status = readDir();
    }
  else if( cmd.substr(0, 4)=="get " )
    {
      status = getFile(cmd.substr(4));
    }
  else if( cmd.substr(0, 4)=="put " )
    {
      status = putFile(cmd.substr(4));
    }
  else if( cmd.substr(0, 4)=="cmd " )
    {
      status = sendDriveCommand(cmd.substr(4));
    }
  else if( cmd=="status" || cmd=="s" )
    {
      string drivestatus;
      status = getDriveStatus(drivestatus);
      if( status==ST_OK ) printf("drive status: %s\n", drivestatus.c_str());
    }
  else
    {
      status = ST_INVALID_COMMAND;
    }

  printf("Status: %s (%i)\n", get_status_msg(status), status);
}

void showCommands()
{
  printf("Commands:\n");
  printf("  dir         : display content of LittleFS filesystem\n");
  printf("  get fname   : retrieve file 'fname' from LittleFS filesystem and save to local\n");
  printf("  put fname   : copy file 'fname' from local to LittleFS filesystem\n");
  printf("  status      : display CBMDOS drive status\n");
  printf("  cmd command : execute CBMDOS 'command' on the drive\n");
}


int main(int argc, char **argv)
{

  if( argc<2 )
    {
      printf("Usage: fstest.exe portName [command] [command] ...\n");
      showCommands();
      return 0;
    }
  else
    {
#ifdef WIN32
      com.SetPortName("\\\\.\\" + string(argv[1]));
#else
      com.SetPortName("/dev/" + string(argv[1]));
#endif
    }

  if( com.Open() == 0 )
    {
      if( argc<3 )
        {
          // interactive loop
          while(1)
            {
              string cmd;
              printf("sk64test> ");
              getline(cin, cmd);
              if( cmd=="x" || cmd=="exit" || cmd=="quit" )
                break;
              else if( cmd=="h" || cmd=="?" || cmd=="help" )
                showCommands();
              else if( cmd.length()>0 )
                execCommand(cmd);
            }
        }
      else
        {
          // execute commands from parameters
          for(int i=2; i<argc; i++)
            {
              if( argc>3 ) printf("Executing: %s\n", argv[i]);
              execCommand(argv[i]);
            }
        }

      com.Close();
    }
  else
    printf("Error opening port: %s\n", com.GetPort().c_str());
  
  return 0;
}
