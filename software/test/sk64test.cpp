#include <stdio.h>
#include <iostream>
#include <algorithm>
#include <vector>
#include <string.h>

#include "../protocol.h"
#include "../SKTool/commfun.h"
#include "../SKTool/utilfun.h"

extern "C" {
#include "drv-nl10.h"
}

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
      status = printDir();
    }
  else if( cmd.substr(0, 4)=="get " )
    {
      status = getFile(toPETSCII(cmd.substr(4)));
    }
  else if( cmd.substr(0, 4)=="put " )
    {
      status = putFile(cmd.substr(4));
    }
  else if( cmd.substr(0, 4)=="del " )
    {
      status = deleteFile(toPETSCII(cmd.substr(4)));
    }
  else if( cmd.substr(0, 3)=="rm " )
    {
      status = deleteFile(toPETSCII(cmd.substr(3)));
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
  else if( cmd=="reboot" )
    {
      status = reboot();
    }
  else if( cmd=="print" )
    {
      StatusType status = getFile(toPETSCII(PRINTDATAFILE), "printdata.dat");
      if( status==ST_OK )
        {
          print_data("printdata.dat", "ascii");
          deleteFile(toPETSCII(PRINTDATAFILE));
        }
    }
  else if( cmd.substr(0,6)=="print " )
    {
      StatusType status = getFile(toPETSCII(PRINTDATAFILE), "printdata.dat");
      if( status==ST_OK ) 
        {
          print_data("printdata.dat", cmd.substr(6));
          deleteFile(toPETSCII(PRINTDATAFILE));
        }
    }
  else if( cmd.substr(0, 7)=="bitmap " )
    {
      int w, h, x, y;
      char buffer[1024];
      int n = sscanf(cmd.substr(7).c_str(), "%s %i %i %i %i", buffer, &w, &h, &x, &y);
      if( n<2 ) w = 240;
      if( n<3 ) h = 240;
      if( n<4 ) x = 0x7FFF;
      if( n<5 ) y = 0x7FFF;

      char *s = strrchr(buffer, '_');
      if( s!=NULL ) sscanf(s, "_%ix%i", &w, &h);

      status = sendBitmap(buffer, x, y, w, h);
    }
  else if( cmd.substr(0, 4)=="gif " )
    {
      int w, h, x, y;
      char buffer[1024];
      int n = sscanf(cmd.substr(4).c_str(), "%s %i %i", buffer, &x, &y);
      if( n<2 ) x = 0x7FFF;
      if( n<3 ) y = 0x7FFF;
      status = sendGIF(buffer, x, y);
    }
  else
    {
      status = ST_INVALID_COMMAND;
    }

  if( status!=ST_OK )
    printf("Error: %s (%i)\n", get_status_msg(status), status);
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
  printf("  reboot      : restarts the IECDevice\n");
  printf("  setconfigval key value : set the value of 'key' to 'value' in the $CONFIG$ file\n");
  printf("  getconfigval key       : print the current value of 'key' in the $CONFIG$ file\n");
  printf("  clearconfig            : clear ALL settings in the $CONFIG$ file\n");
  printf("  bitmap fname [w] [h] [x] [y] : send RGB565 bitmap file 'fname', if w/h missing then assumed 240, if x/y missing then center\n");
  printf("  gif fname [x] [y] : send GIF bitmap file 'fname', if x/y missing then center\n");
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
    }
  else if( comOpen(argv[1])==0 )
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

      comClose();
    }
  else
    printf("Error opening port: %s\n", argv[1]);
  
  return 0;
}
