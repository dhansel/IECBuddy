#include <stdio.h>
#include <iostream>
#include <algorithm>
#include "ceserial.h"
#include "../protocol.h"
using namespace std;

extern "C" {
#include "drv-nl10.h"
}

#define DEBUG 0

// ------------ Printer data file reader class

class PrinterDataFile
{
public:
  PrinterDataFile(string fname);
  ~PrinterDataFile();

  bool    IsNewDataBlock() { bool b = m_newBlock; m_newBlock = false; return b; }
  uint8_t GetChannel()     { return m_channel; }
  int     GetNextByte();

private:
  void ReadHeader();

  FILE   *m_file;
  uint8_t m_zeroCount;
  uint8_t m_channel;
  bool    m_newBlock;
};


PrinterDataFile::PrinterDataFile(string fname)
{
  m_file      = fopen(fname.c_str(), "rb");
  m_channel   = 0xFF;
  m_newBlock  = false;
  m_zeroCount = 0;
  ReadHeader();
}

PrinterDataFile::~PrinterDataFile()
{
  fclose(m_file);
}

PrinterDataFile::GetNextByte()
{
  if( m_file )
    {
      uint8_t data;
        
      if( m_zeroCount>0 )
        {
          m_zeroCount--;
          return 0;
        }
      else if( fread(&data, 1, 1, m_file)==1 )
        {
          if( data==0 )
            {
              fread(&m_zeroCount, 1, 1, m_file);
              if( m_zeroCount==0 )
                {
                  ReadHeader();
                  return GetNextByte();
                }
              else
                {
                  m_zeroCount--;
                  return 0;
                }
            }
          else
            return data;
        }
    }

  return -1;
}

void PrinterDataFile::ReadHeader()
{
  uint8_t buffer[4];
  if( fread(buffer, 1, 4, m_file)==4 &&
      buffer[0]=='P' && buffer[1]=='D' && buffer[2]=='B' )
    {
      m_newBlock = true;
      if( buffer[3]>='0' && buffer[3]<='9' ) 
        m_channel = buffer[3]-'0';
      else if( buffer[3]>='A' && buffer[3]<='F' ) 
        m_channel = buffer[3]-'A'+10;
      else
        fseek(m_file, 0, SEEK_END);
    }
  else
    fseek(m_file, 0, SEEK_END);
}


// ------------------ helper functions -----------------

static ceSerial com;

#define SHOW_LOWERCASE 0
uint8_t toPETSCII(uint8_t c)
{
  if( c>=65 && c<=90 && SHOW_LOWERCASE )
    c += 32;
  else if( c>=97 && c<=122 )
    c -= 32;
  
  return c;
}


string toPETSCII(const string &s)
{
  string rs;
  rs.reserve(s.length());

  for(size_t i=0; i<s.length(); i++)
    rs += char(toPETSCII(s[i]));
  
  return rs;
}


uint8_t fromPETSCII(uint8_t c)
{
  if( c==0xFF )
    c = '~';
  else if( c>=192 ) 
    c -= 96;
  
  if( c>=65 && c<=90 )
    c += 32;
  else if( c>=97 && c<=122 )
    c -= 32;
  
  return c;
}


string fromPETSCII(const string &s)
{
  string rs;
  rs.reserve(s.length());

  for(size_t i=0; i<s.length(); i++)
    rs += fromPETSCII(s[i]);

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


StatusType getFile(const string &fname, string dstfname = "")
{
  StatusType status = ST_OK;

#if DEBUG>0
  printf("getFile(%s)", fname.c_str()); fflush(stdout);
#endif

  FILE *file = fopen(dstfname.empty() ? fname.c_str() : dstfname.c_str(), "wb");
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


StatusType deleteFile(const string &fname)
{
  StatusType status = ST_OK;

  // send command
  if( status==ST_OK )
    if( !send_command(CMD_DELETE_FILE) )
      status = ST_COM_ERROR;

  // send file name
  if( status==ST_OK )
    if( !send_string(toPETSCII(fname)) )
      status = ST_COM_ERROR;
  
  // receive status
  if( status==ST_OK )
    status = recv_status();
  
  return status;
}


void print_data(string datafile, string driver)
{
  if( driver=="ascii" )
    {
      const char *codes1[32] =
        {"", "($1)","($2)","($3)","($4)","(WHT)","($6)","($7)",
         "(DISH)","(ENSH)","","($11)","($12)","\n","(SWLC)","($15)",
         "($16)","(DOWN)","(RVS)","(HOME)","(DEL)","($21)","($22)","($23)",
         "($24)","($25)","($26)","(ESC)","(RED)","(RGHT)","(GRN)","(BLU)"};

      const char *codes2[32] =
        {"($128)","(ORNG)","($130)","($131)","($132)","(F1)","(F3)","(F5)",
         "(F7)","(F2)","(F4)","(F6)","(F8)","(SHRT)","(SWUC)","($143)",
         "(BLK)","(UP)","(OFF)","(CLR)","(INST)","(BRN)","(LRED)","(GRY1)",
         "(GRY2)","(LGRN)","(LBLU)","(GRY3)","(PUR)","(LEFT)","(YEL)","(CYN)"};

      int data;
      PrinterDataFile f(datafile);
      while( (data=f.GetNextByte())>=0 )
        {
          if( data>=0 && data<32 )
            printf("%s", codes1[data]);
          else if( data>=128 && data<128+32 )
            printf("%s", codes2[data-128]);
          else
            printf("%c", (char) fromPETSCII(data));
        }
    }
  else if( driver.substr(0,4) == "nl10" )
    {
      int outputformat;
      string outputfile;
      if( driver.substr(0,8) == "nl10-pdf" )
        {
          outputfile   = "print.pdf";
          outputformat = OUTPUT_FORMAT_PDF;
          driver = driver.substr(8);
        }
      else
        {
          outputfile   = "printpage%02i.bmp";
          outputformat = OUTPUT_FORMAT_BMP;
          driver = driver.substr(4);
        }

      if( driver.length()>1 && driver[0]==' ' )
        outputfile = driver.substr(1);

      int secondary = 0;
      drv_nl10_init(outputfile.c_str(), outputformat);

      int data;
      PrinterDataFile f(datafile);
      while( (data=f.GetNextByte())>=0 )
        {
          if( f.IsNewDataBlock() )
            drv_nl10_open(f.GetChannel());

          drv_nl10_putc(data);
        }
  
      drv_nl10_formfeed();
      drv_nl10_close();
      drv_nl10_shutdown();
    }
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
  else if( cmd.substr(0, 4)=="del " )
    {
      status = deleteFile(cmd.substr(4));
    }
  else if( cmd.substr(0, 4)=="cmd " )
    {
      status = sendDriveCommand(cmd.substr(4));
      if( status==ST_OK ) execCommand("status");
    }
  else if( cmd=="status" || cmd=="s" )
    {
      string drivestatus;
      status = getDriveStatus(drivestatus);
      if( status==ST_OK ) printf("drive status: %s\n", drivestatus.c_str());
    }
  else if( cmd.substr(0, 6)=="mount " )
    {
      status = mountDisk(cmd.substr(6));
    }
  else if( cmd=="unmount" )
    {
      status = unmountDisk();
    }
  else if( cmd=="getmounted" )
    {
      status = getMountedDisk();
    }
  else if( cmd.substr(0, 13)=="setconfigval " )
    {
      int p1 = 13;
      while( p1<cmd.length() && cmd[p1]==' ' ) p1++;
      int p2 = cmd.find_first_of(' ', p1);
      if( p1<cmd.length() && p2<cmd.length() )
        status = setConfigValue(cmd.substr(p1, p2-p1), cmd.substr(p2+1));
      else
        status = ST_INVALID_COMMAND;
    }
  else if( cmd.substr(0, 13)=="getconfigval " )
    {
      status = getConfigValue(cmd.substr(13));
    }
  else if( cmd=="clearconfig" )
    {
      status = clearConfig();
    }
  else if( cmd=="print" )
    {
      StatusType status = getFile(PRINTDATAFILE, "printdata.dat");
      if( status==ST_OK )
        {
          print_data("printdata.dat", "ascii");
          deleteFile(PRINTDATAFILE);
        }
    }
  else if( cmd.substr(0,6)=="print " )
    {
      StatusType status = getFile(PRINTDATAFILE, "printdata.dat");
      if( status==ST_OK ) 
        {
          print_data("printdata.dat", cmd.substr(6));
          deleteFile(PRINTDATAFILE);
        }
    }
  else
    {
      status = ST_INVALID_COMMAND;
    }

  if( status!=ST_OK ) printf("Error: %s (%i)\n", get_status_msg(status), status);
}

void showCommands()
{
  printf("Commands:\n");
  printf("  dir         : display content of LittleFS filesystem\n");
  printf("  get fname   : retrieve file 'fname' from LittleFS filesystem and save to local\n");
  printf("  put fname   : copy file 'fname' from local to LittleFS filesystem\n");
  printf("  del fname   : delete file 'fname' on LittleFS filesystem\n");
  printf("  status      : display CBMDOS drive status\n");
  printf("  cmd command : execute CBMDOS 'command' on the drive\n");
  printf("  mount image : mount the given D64/G64/G81... image (image file must exist)\n");
  printf("  unmount     : unmount the currently mounted disk image (if any)\n");
  printf("  getmounted  : print the currently mounted disk image\n");
  printf("  setconfigval key value : set the value of 'key' to 'value' in the $CONFIG$ file\n");
  printf("  getconfigval key       : print the current value of 'key' in the $CONFIG$ file\n");
  printf("  clearconfig            : clear ALL settings in the $CONFIG$ file\n");
}


int main(int argc, char **argv)
{

  if( argc<2 )
    {
      printf("Usage: fstest.exe portName [command] [command] ...\n");
      showCommands();
    }
  else if( argc==2 )
    {
      string driver, fname;
      char *c = strchr(argv[1], '@');
      if( c==NULL )
        { driver="ascii"; fname = string(argv[1]); }
      else
        { *c = 0; fname=string(argv[1]); driver=string(c+1); }

      print_data(fname, driver);
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
