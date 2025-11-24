#include "IECDrive.h"
#include "IECConfig.h"
#include "IECDisplay.h"
#include "Pins.h"
#include <LittleFS.h>
#include <algorithm>

using namespace std;

#define E_OK          0
#define E_SCRATCHED   1
#define E_READ       20
#define E_WRITEPROT  26
#define E_WRITE      28
#define E_INVCMD     31
#define E_INVNAME    33
#define E_NOTFOUND   62
#define E_EXISTS     63
#define E_MISMATCH   64
#define E_SPLASH     73
#define E_NOTREADY   74
#define E_MEMEXE     97
#define E_TOOMANY    98
#define E_VDRIVE     99

#define FT_NONE      0x00
#define FT_PRG       0x01
#define FT_SEQ       0x02
#define FT_ANY       0xFF

#ifndef min
#define min(a,b) ((a)<(b) ? (a) : (b))
#endif

#ifdef PIN_DRIVE_LED_IS_NEOPIXEL_RGB
#include <Adafruit_NeoPixel.h>
static Adafruit_NeoPixel s_led(1, 16, NEO_RGB + NEO_KHZ800);
#define BRIGHTNESS 25
#endif

#define LED_OFF   0
#define LED_RED   1
#define LED_GREEN 2
#define LED_BLUE  3

// ----------------------------------------------------------------------------------------------

static string tolower(string s)
{
  transform(s.begin(), s.end(), s.begin(), [](unsigned char c){ return std::tolower(c); });
  return s;
}

static bool changeDisk = false;
static void diskChangeButtonFcn()
{
  static unsigned long debounceTime = 0;
  static bool debounceState = true;

  bool state = digitalRead(PIN_BUTTON);
  if( state!=debounceState && millis()>debounceTime )
    {
      debounceState = state;
      debounceTime  = millis()+20;
      if( !state ) changeDisk = true;
    }
}


IECDrive::IECDrive(uint8_t devnum, uint8_t pinLED) :
  IECFileDevice(devnum)
{
  m_pinLED = pinLED;
  m_dirOpen = false;
  m_drive = NULL;
  m_curFileChannel = -1;
  m_display = NULL;
  m_lastActivity = 0;
  m_showExt = false;
}


void IECDrive::begin()
{
  pinMode(PIN_BUTTON, INPUT_PULLUP);
  attachInterrupt(PIN_BUTTON, diskChangeButtonFcn, CHANGE);

  unsigned long ledTestEnd = millis() + 500;

  if( m_pinLED<0xFF ) 
    {
#ifdef PIN_DRIVE_LED_IS_NEOPIXEL_RGB
      s_led.setPin(m_pinLED); 
      s_led.begin();
#else
      pinMode(m_pinLED, OUTPUT); 
#endif
    }

  setLEDState(LED_GREEN);
  m_errorCode = E_SPLASH;
  m_drive = new VDrive(0);

  // read configuration settings
  int d = std::atoi(m_config->getValue("device").c_str());
  if( d>=8 && d<=15 && d!=m_devnr ) setDeviceNumber(d);
  m_diskFlushTimeout = std::atoi(m_config->getValue("diskflush").c_str());
  m_showExt = std::atoi(m_config->getValue("showext").c_str())!=0;

  m_display->redraw();
  updateDisplayStatus();

  // call this after m_display->begin() since this sets all IEC and parallel cable
  // pins to GPIO mode, ensuring that any pins that were previously set as I2C/SPI
  // (because of defaults) get set to GPIO mode.
  IECFileDevice::begin();

  if( m_pinLED<0xFF ) 
    {
      unsigned long t = millis();
      if( t<ledTestEnd ) delay(ledTestEnd-t);
      setLEDState(LED_OFF);
    }
}


void IECDrive::task()
{
  // handle status LED
  if( m_pinLED<0xFF )
    {
      static unsigned long nextblink = 0;
      if( m_errorCode==E_OK || m_errorCode==E_SPLASH || m_errorCode==E_SCRATCHED )
        {
          bool active = m_dirOpen || (bool) m_file;
          active |= m_drive->getNumOpenChannels()>0;
          setLEDState(active ? LED_GREEN : LED_OFF);
        }
      else if( millis()>nextblink )
        {
          setLEDState((nextblink&1) ? LED_RED : LED_OFF);
          nextblink += (m_errorCode==E_MEMEXE) ? 101 : 251;
        }
    }

  if( changeDisk )
    {
      changeDisk = false;

      string current;
      if( m_drive->isOk() ) 
        {
          current = m_drive->getDiskImageFilename();
          m_drive->closeDiskImage();
          m_lastActivity = 0;
        }

      m_display->showMessage("Searching...");

      string favlist = m_config->getValue("favlist");
      if( favlist.length()%32==0 && favlist.find('.')!=string::npos )
        {
          // we have a properly sized favlist containing at least one item
          int len = favlist.length()/32;

          // find the currently mounted image in the favlist
          int i = 0;
          for(i=0; i<len; i++)
            {
              string next = favlist.substr(i*32,32);
              if( next.find('|')!=string::npos ) next = next.substr(0, next.find('|'));
              if( strcasecmp(next.c_str(), current.c_str())==0 ) break;
            }

          // if we couldn't find the current image then start at the beginning, otherwise go to the next
          if( i==len )
            i = 0;
          else
            i++;

          // find the next image in the favlist
          for(int j=i; j<len; j++)
            {
              string next = favlist.substr(j*32,32);
              if( next.find('|')!=string::npos ) next = next.substr(0, next.find('|'));
              if( !next.empty() && m_drive->openDiskImage(next.c_str()) )
                break;
            }

          // if we haven't found one yet then re-start at the beginning
          if( !m_drive->isOk() )
            {
              for(int j=0; j<i; j++)
                {
                  string next = favlist.substr(j*32,32);
                  if( next.find('|')!=string::npos ) next = next.substr(0, next.find('|'));
                  if( m_drive->openDiskImage(next.c_str()) )
                    break;
                }
            }
        }
      else
        {
          bool found = false;
          Dir dir = LittleFS.openDir("/");
          if( !current.empty() )
            {
              while( !found && dir.next() )
                found = strcmp(dir.fileName().c_str(), current.c_str())==0;

              // if we can't find the current image then just find the first one
              if( !found ) dir = LittleFS.openDir("/");
            }

          // keep trying to mount files as disk images and stop if one found
          while( !m_drive->isOk() && dir.next() )
            m_drive->openDiskImage(dir.fileName().c_str());
        }

      const char *iname = m_drive->getDiskImageFilename();
      m_display->setCurrentImageName(iname ? iname : "");
    }

  // handle IEC serial bus communication, the open/read/write/close/execute 
  // functions will be called from within this when required
  IECFileDevice::task();

  // check whether VDrive cache needs to be flushed
  if( m_diskFlushTimeout>=0 && m_lastActivity>0 && (millis()-m_lastActivity) >= m_diskFlushTimeout )
    {
      setLEDState(LED_BLUE);
      m_drive->flushCache();
      m_lastActivity = 0;
      setLEDState(LED_OFF);
    }
}


#if defined(IEC_FP_EPYX) && defined(IEC_FP_EPYX_SECTOROPS)
bool IECDrive::epyxReadSector(uint8_t track, uint8_t sector, uint8_t *buffer)
{
  bool res = false;

  if( m_drive->isOk() )
    {
      setLEDState(LED_GREEN);
      res = m_drive->readSector(track, sector, buffer);
      m_lastActivity = millis();
      setLEDState(LED_OFF);
    }

  // for debug log
  if( res ) IECFileDevice::epyxReadSector(track, sector, buffer);

  return res;
}


bool IECDrive::epyxWriteSector(uint8_t track, uint8_t sector, uint8_t *buffer)
{
  bool res = false;

  // for debug log
  IECFileDevice::epyxWriteSector(track, sector, buffer);

  if( m_drive->isOk() )
    {
      setLEDState(LED_GREEN);
      res = m_drive->writeSector(track, sector, buffer);
      m_lastActivity = millis();
      setLEDState(LED_OFF);
    }

  return res;
}
#endif


string IECDrive::stripFileName(const char *cname)
{
  if( cname[0]=='@' ) cname++;

  if( isdigit(cname[0]) && cname[1]==':' )
    cname += 2;
  else if( cname[0]==':' )
    cname++;

  string name(cname);
  size_t comma = name.find_first_of(',');
  return comma==string::npos ? name : name.substr(0, comma);
}


bool IECDrive::isHiddenFile(const char *name)
{
  if( name==NULL )
    return false;
  else
    {
      int len = strlen(name);
      return len>2 && name[0]=='$' && name[len-1]=='$';
    }
}


uint8_t IECDrive::openDir(const char *name)
{
  m_dir = LittleFS.openDir("/");
  m_dirOpen = true;
  m_dir.rewind();
  m_dirBuffer[0] = 0x01;
  m_dirBuffer[1] = 0x08;
  m_dirBuffer[2] = 1;
  m_dirBuffer[3] = 1;
  m_dirBuffer[4] = 0;
  m_dirBuffer[5] = 0;
  m_dirBuffer[6] = 18;
  m_dirBuffer[7] = '"';
  strcpy(m_dirBuffer+8, "DRIVE");
  size_t n = strlen(m_dirBuffer+8);
  while( n<16 ) { m_dirBuffer[8+n] = ' '; n++; }
  strcpy(m_dirBuffer+24, "\" 00 2A");
  m_dirBufferLen = 32;
  m_dirBufferPtr = 0;

  if( isdigit(name[1]) && name[2]==':' )
    m_dirPattern = name+3;
  else if( name[1] == ':' || isdigit(name[1]) )
    m_dirPattern = name+2;
  else
    m_dirPattern = name+1;

  if( *m_dirPattern==0 ) m_dirPattern="*";

  return E_OK;
}


bool IECDrive::readDir(uint8_t *data)
{
  if( m_dirBufferPtr==m_dirBufferLen && m_dirOpen )
    {
      bool repeat = true;
      while( repeat )
        {
          m_dirBufferPtr = 0;
          m_dirBufferLen = 0;

          bool ok = m_dir.next();
          while( ok && !isMatch(m_dir.fileName().c_str(), m_dirPattern, FT_ANY) || isHiddenFile(m_dir.fileName().c_str()) )
            ok = m_dir.next();

          if( ok )
            {
              uint16_t size = m_dir.fileSize()==0 ? 0 : min(m_dir.fileSize()/254+1, 9999);
              m_dirBuffer[m_dirBufferLen++] = 1;
              m_dirBuffer[m_dirBufferLen++] = 1;
              m_dirBuffer[m_dirBufferLen++] = size&255;
              m_dirBuffer[m_dirBufferLen++] = size/256;
              if( size<10 )    m_dirBuffer[m_dirBufferLen++] = ' ';
              if( size<100 )   m_dirBuffer[m_dirBufferLen++] = ' ';
              if( size<1000 )  m_dirBuffer[m_dirBufferLen++] = ' ';

              m_dirBuffer[m_dirBufferLen++] = '"';
              String name = m_dir.fileName();
              size_t n = min(name.length(), 16);
              if( n>0 )
                {
                  strncpy(m_dirBuffer+m_dirBufferLen, name.c_str(), n);
                  m_dirBuffer[m_dirBufferLen+n] = 0;

                  char ftype[4];
                  strcpy(ftype, "???");
                  int dot = name.lastIndexOf('.');
                  if( dot>=0 && dot==name.length()-4 && isalphanum(name[dot+1]) && isalphanum(name[dot+2]) && isalphanum(name[dot+3]) )
                    {
                      ftype[0] = toupper(name[dot+1]);
                      ftype[1] = toupper(name[dot+2]);
                      ftype[2] = toupper(name[dot+3]);
                      if( dot<n && !m_showExt ) n = dot;
                    }
                  
                  m_dirBufferLen += n;
                  m_dirBuffer[m_dirBufferLen++] = '"';
                  m_dirBuffer[m_dirBufferLen] = 0;
                  
                  m_dirBufferLen += strlen(m_dirBuffer+m_dirBufferLen);
                  n = 17-n;
                  while(n-->0) m_dirBuffer[m_dirBufferLen++] = ' ';
                  strcpy(m_dirBuffer+m_dirBufferLen, ftype!=NULL ? ftype : (m_dir.isDirectory() ? "DIR" : "PRG"));
                  m_dirBufferLen+=3;
                  while( m_dirBufferLen<31 ) m_dirBuffer[m_dirBufferLen++] = ' ';
                  m_dirBuffer[m_dirBufferLen++] = 0;
                  repeat = false;
                }
            }
          else
            {
              struct FSInfo info;
              LittleFS.info(info);

              uint64_t free = min(65535, (info.totalBytes-info.usedBytes)/254);
              m_dirBuffer[0] = 1;
              m_dirBuffer[1] = 1;
              m_dirBuffer[2] = free&255;
              m_dirBuffer[3] = free/256;
              strcpy(m_dirBuffer+4, "BLOCKS FREE.             ");
              m_dirBuffer[29] = 0;
              m_dirBuffer[30] = 0;
              m_dirBuffer[31] = 0;
              m_dirBufferLen = 32;
              repeat = false;
              m_dirOpen = false;
            }
        }
    } 
      
  if( m_dirBufferPtr<m_dirBufferLen )
    {
      *data = m_dirBuffer[m_dirBufferPtr++];
      return true;
    }
  else
    return false;
}


bool IECDrive::isMatch(const char *name, const char *pattern, uint8_t ftypes)
{
  signed char found = -1;

  for(uint8_t i=0; found<0; i++)
    {
      if( pattern[i]=='*' )
        {
          if( ftypes==FT_ANY )
            found = 1;
          else
            {
              while( name[i]!=0 && name[i]!='.' ) i++;
              if( name[i]==0 )
                found = 0;
              else if( (ftypes & FT_PRG) && strcasecmp(name+i+1, "prg")==0 )
                found = 1;
              else if( (ftypes & FT_SEQ) && strcasecmp(name+i+1, "seq")==0 )
                found = 1;
              else
                found = 0;
            }
        }
      else if( pattern[i]==0 && name[i]=='.' )
        {
          if( ftypes==FT_ANY )
            found = 1;
          else if( (ftypes & FT_PRG) && strcasecmp(name+i+1, "prg")==0 )
            found = 1;
          else if( (ftypes & FT_SEQ) && strcasecmp(name+i+1, "seq")==0 )
            found = 1;
          else
            found = 0;
        }
      else if( pattern[i]==0 || name[i]==0 )
        found = (pattern[i]==name[i]) ? 1 : 0;
      else if( pattern[i]!='?' && tolower(pattern[i])!=tolower(name[i]) && !(name[i]=='~' && (pattern[i] & 0xFF)==0xFF) )
        found = 0;
    }

  return found==1;
}


const char *IECDrive::findFile(const char *pattern, uint8_t ftypes)
{
  bool found = false;
  static String name;

  m_dir = LittleFS.openDir("/");
  while( !found && m_dir.next() )
    {
      name = m_dir.fileName();
      found = !m_dir.isDirectory() && isMatch(name.c_str(), pattern, ftypes) && !isHiddenFile(name.c_str());
    }

  return found ? name.c_str() : NULL;
}


uint8_t IECDrive::openFile(uint8_t channel, const char *constName)
{
  uint8_t res = E_OK;
  uint8_t ftype = FT_PRG;
  char mode  = 0;
  char *name = m_dirBuffer;
  bool overwrite = false;

  // file name ends at the first 0xA0 ("shifted space")
  strcpy(name, constName);
  char *c = strchr(name, '\xa0');
  if( c!=NULL ) *c = 0;

  // ignore anything before and including the first ':'
  // and check for 'overwrite' ("@") character
  if( strchr(name, ':')!=NULL )
    {
      while( *name != ':' )
        {
          if( *name=='@' ) overwrite = true;
          name++;
        }
      name++;
    }

  // convert "/" and "\" to "-" (we don't support subdirectories)
  for(c=name; *c!=0; c++)
    if( *c=='/' || *c=='\\' )
      *c = '-';

  char *comma = strchr(name, ',');
  if( comma!=NULL )
    {
      char *c = comma;
      do { *c-- = 0; } while( c!=name && ((*c) & 0x7f)==' ');
      char cc = toupper(*(comma+1));
      if( cc=='R' || cc=='W' )
        mode = cc;
      else
        {
          if( cc=='S' )
            ftype = FT_SEQ;
          else if( cc!='P' )
            ftype = FT_NONE;

          comma = strchr(comma+1, ',');
          if( comma!=NULL )
            mode = toupper(*(comma+1));
        }
    }
  
  if( mode==0 )
    {
      if( channel==0 )
        mode = 'R';
      else if( channel==1 )
        mode = 'W';
    }

  // if the file type is neither PRG nor SEQ or mode is neither R nor W
  // then the file name is invalid
  if( ftype==FT_NONE || (mode!='R' && mode!='W') )
    res = E_INVNAME;

  if( res == E_OK )
    {
      if( mode=='R' )
        {
          const char *fn = findFile(name, ftype);
          if( fn==NULL )
            {
              fn = findFile(name, FT_ANY);
              if( fn==NULL || isHiddenFile(fn) )
                {
                  // can't find the file with any extension => file doesn't exist
                  res = E_NOTFOUND;
                }
              else if( channel==0 && mountDiskImage(fn) )
                {
                  // the file is a mountable disk image => mount it and read its directory
                  res = m_drive->openFile(channel, "$") ? E_OK : E_VDRIVE;
                }
              else
                {
                  // can't load this type of file
                  res = E_MISMATCH;
                }
            }
          else
            {
              // found the file, try to open, reject if size is 0
              m_file = LittleFS.open(fn, "r");
              res = m_file && (m_file.size()>0) ? E_OK : E_NOTFOUND;
              if( res != E_OK ) m_file.close();
            }
        }
      else
        {
          if( strchr(name, '*')!=NULL || strchr(name, '?')!=NULL )
            res = E_INVNAME;
          else if( !overwrite && findFile(name, FT_ANY)!=NULL )
            res = E_EXISTS;
          else 
            {
              // creating/overwriting file => always add PRG/SEQ extension
              // this also means we can never create/overwrite a hidden file (ending in '$')
              strcat(name, ftype==FT_PRG ? ".PRG" : ".SEQ");
              m_file = LittleFS.open(name, "w");
              res = m_file ? E_OK : E_WRITE;
            }
        }
    }

  return res;
}


bool IECDrive::open(uint8_t channel, const char *name, uint8_t nameLen)
{
  if( m_drive->isOk() )
    {
      if( channel==0 && (strcmp(name, "..")==0 || strcmp(name, "/")==0) )
        {
          // loading ".." or "/" while a disk image is mounted
          // => unmount image and load SD card directory
          unmountDiskImage();
          m_errorCode = openDir("$");
        }
      else
        m_errorCode = m_drive->openFile(channel, name, nameLen) ? E_OK : E_VDRIVE;

      m_lastActivity = millis();
    }
  else if( channel==0 && name[0]=='$' )
    m_errorCode = openDir(name);
  else if ( !m_file )
    m_errorCode = openFile(channel, name);
  else
    {
      // we can only have one file open at a time when accessing the SD directory
      m_errorCode = E_TOOMANY;
    }

  if( m_errorCode==E_OK )
    {
      string sname = stripFileName(name);
      m_display->setCurrentFileName(sname);

      m_curFileChannel = channel;
      if( m_drive->isOk() )
        m_display->startProgress(m_drive->getFileNumBlocks(sname.c_str()) * 254);
      else if( m_file )
        m_display->startProgress(m_file.size());
      else
        m_display->startProgress(-1);
    }

  updateDisplayStatus();

  // clear the status buffer so getStatus() is called again next time the buffer is queried
  clearStatus();

  return m_errorCode==E_OK;
}


uint8_t IECDrive::read(uint8_t channel, uint8_t *buffer, uint8_t bufferSize, bool *eoi)
{
  uint8_t n = 0;

  if( m_drive->isFileOk(channel) )
    {
      size_t nn = bufferSize;
      if( m_drive->read(channel, buffer, &nn, eoi) )
        n = nn;
      else
        m_errorCode = E_VDRIVE;

      m_lastActivity = millis();
    }
  else if( m_file )
    n = m_file.read(buffer, bufferSize);
  else
    n = readDir(buffer) ? 1 : 0;

  if( channel==m_curFileChannel )
    m_display->updateProgress(n);

  return n;
}


uint8_t IECDrive::write(uint8_t channel, uint8_t *buffer, uint8_t bufferSize, bool eoi)
{
  uint8_t n = 0;

  if( m_drive->isFileOk(channel) )
    {
      size_t nn = bufferSize;
      if( m_drive->write(channel, buffer, &nn) )
        n = nn;
      else
        m_errorCode = E_VDRIVE;

      m_lastActivity = millis();
    }
  else if( m_file )
    n = m_file.write(buffer, bufferSize);

  if( channel==m_curFileChannel )
    m_display->updateProgress(n);

  return n;
}


void IECDrive::close(uint8_t channel)
{
  if( m_drive->isFileOk(channel) )
    {
      m_drive->closeFile(channel);
      m_lastActivity = millis();
    }
  else if( m_dirOpen )
    { 
      m_dirOpen = false;
      m_dirBufferLen = 0;
    }
  else 
    m_file.close(); 

  if( m_curFileChannel==channel )
    {
      m_display->endProgress();
      m_display->setCurrentFileName("");
      updateDisplayStatus();
      m_curFileChannel = -1;
    }
}


void IECDrive::executeData(const uint8_t *command, uint8_t len)
{
  // clear the status buffer so getStatus() is called again next time the buffer is queried
  clearStatus();
  setLEDState(LED_GREEN);

  if( memcmp(command, "M-W\x77\x00\x02", 6)==0 )
    {
      // temporarily set device number
      m_devnr = command[6] & 0x0F;
      m_errorCode = E_OK;
    }
  else if( memcmp(command, "M-E", 3)==0 || memcmp(command, "B-E", 3)==0 || 
           (command[0]=='U' && command[1]>='3' && command[1]<='8') ||
           (command[0]=='U' && command[1]>='C' && command[1]<='H') )
    {
      m_errorCode = E_MEMEXE;
      if( m_pinLED<0xFF )
        {
          // block and blink LED for 2 seconds - blinking will continue after
          // but this should ensure that the user sees the blinking before the
          // computer sends another command
          updateDisplayStatus();
          for(int i=0; i<2*5; i++)
            { setLEDState(LED_RED); delay(101); setLEDState(LED_OFF); delay(101); }
        }
    }
  else if( m_drive->isOk() && (command[0]!='C' || command[1]!='D') && command[0]!='X' && command[0]!='E' )
    {
      // "CD", "X" and "E" commands are processed outside of the VDrive (in function "execute" below)

      int r = m_drive->execute((const char *) command, len);
      if( r==2 )
        {
          // this was a command that placed its response in the status buffer
          uint8_t buf[IECFILEDEVICE_STATUS_BUFFER_SIZE];
          size_t len = m_drive->getStatusBuffer(buf, IECFILEDEVICE_STATUS_BUFFER_SIZE);
          IECFileDevice::setStatus((const char *) buf, len);
          m_errorCode = E_OK;
        }
      else
        m_errorCode = r==0 ? E_VDRIVE : E_OK;

      // when executing commands that read data into a buffer or reposition
      // the pointer we need to clear the our internal read buffer of the channel
      // for which this command is issued, otherwise remaining characters in the
      // buffer will be prefixed to the data from the new record or buffer location
      if( command[0]=='P' && len>=2 )
        clearReadBuffer(command[1] & 0x0f);
      else if( memcmp(command, "U1", 2)==0 || memcmp(command, "B-P", 3)==0 || memcmp(command, "B-R", 3)==0 )
        {
          int i = command[0]=='U' ? 2 : 3;
          while( i<len && !isdigit(command[i]) ) i++;
          if( i<len ) 
            {
              uint8_t channel = command[i]-'0';
              if( i+1<len && isdigit(command[i+1]) )
                channel = 10*channel + (command[i+1]-'0');
              
              clearReadBuffer(channel);
            }
        }

      m_lastActivity = millis();
    }
  else if( memcmp(command, "M-R\xfa\x02\x03", 6)==0 )
    {
      // hack: DolphinDos' MultiDubTwo reads 02FA-02FC to determine
      // number of free blocks => pretend we have 664 (0298h) blocks available
      uint8_t data[3] = {0x98, 0, 0x02};
      setStatus((char *) data, 3);
      m_errorCode = E_OK;
    }
  else if( memcmp(command, "M-R", 3)==0 && len>=6 && command[5]<=32 )
    {
      // memory read not supported => always return 0xFF
      uint8_t n = command[5];
      char buf[32];
      memset(buf, 0xFF, n);
      setStatus(buf, n);
      m_errorCode = E_OK;
    }
  else if( memcmp(command, "M-W", 3)==0 )
    {
      // memory write not supported => ignore
      m_errorCode = E_OK;
    }
  else
    {
      // calling IECFileDevice::executeData will strip off trailing CRs, make sure the 
      // command is 0-terminated and then call IECSD::execute(command) below
      IECFileDevice::executeData(command, len);
    }
  
  setLEDState(LED_OFF);
}


void IECDrive::execute(const char *command)
{
  // detect whether this is a "CD" command (with some flexibility in syntax), "cdcmd" will be
  //  0: if not a "CD" command
  // -1: if this is a CD "up" command
  // >0: "cdcmd" is the index of the first character of the directory name within "command"
  int cdcmd = 0;
  if( command[0]=='C' && command[1]=='D' )
    {
      // if there is a colon then ignore anything before (and including) the colon
      const char *colon = strchr(command, ':');
      cdcmd = colon==NULL ? 2 : (colon-command+1);

      // is this a cd "up" command?
      if( strcmp(command+cdcmd, "..")==0 || strcmp(command+cdcmd, "_")==0 || strcmp(command+cdcmd, "^")==0 || strcmp(command+cdcmd, "/")==0 )
        cdcmd = -1;
    }

  if( strncmp(command, "S:", 2)==0 )
    {
      char pattern[17];
      m_errorCode = E_SCRATCHED;
      m_scratched = 0;
      int scratchFailed = 0;
      
      strncpy(pattern, command+2, 16);
      pattern[16]=0;

      m_dir = LittleFS.openDir("/");
      while( m_dir.next() )
        {
          String name = m_dir.fileName();
          if( name.length()>0 && isMatch(name.c_str(), pattern, FT_ANY) && !isHiddenFile(name.c_str()) )
            {
              if( m_dir.isDirectory() ? LittleFS.rmdir(name.c_str()) : LittleFS.remove(name.c_str()) )
                {
                  m_scratched++;
                  m_dir.rewind();
                }
              else
                scratchFailed++;
            }
        }

      if( m_scratched==0 && scratchFailed>0 )
        m_errorCode = E_WRITEPROT;
    }
  else if( strncmp(command, "N:", 2)==0 )
    {
      const char *imagename = command+2;
      const char *dot = strchr(imagename, '.');
      if( dot==NULL )
        m_errorCode = E_INVNAME;
      else
        {
          char diskname[17];
          memset(diskname, 0, 17);
          strncpy(diskname, imagename, (dot-imagename)<16 ? (dot-imagename) : 16);

          if( LittleFS.exists(imagename) )
            m_errorCode = E_EXISTS;
          else if( VDrive::createDiskImage(imagename, dot+1, diskname, false) )
            m_errorCode = E_OK;
          else
            m_errorCode = E_INVNAME;
        }
    }
  else if( cdcmd<0 )
    {
      if( m_drive->isOk() )
        {
          // CD "up"
          m_drive->closeDiskImage();
          m_display->setCurrentImageName("");
          m_lastActivity = 0;
        }

      m_errorCode = E_OK;
    }
  else if( cdcmd>0 )
    {
      if( m_drive->isOk() && command[cdcmd]=='/' )
        {
          m_drive->closeDiskImage();
          m_display->setCurrentImageName("");
          m_lastActivity = 0;
        }

      if( m_drive->isOk() )
        {
          // can't mount a disk image from within a disk image
          m_errorCode = E_MISMATCH;
        }
      else
        {
          if( command[cdcmd]=='/' ) cdcmd++;
          strncpy(m_dirBuffer, command+cdcmd, IEC_BUFSIZE);
          m_dirBuffer[IEC_BUFSIZE-1]=0;

          if( !LittleFS.exists(m_dirBuffer) )
            {
              const char *name = findFile(m_dirBuffer, FT_ANY);
              if( name!=NULL ) strcpy(m_dirBuffer, name);
            }

          if( mountDiskImage(m_dirBuffer) )
            m_errorCode = E_OK;
          else if( LittleFS.exists(m_dirBuffer) )
            m_errorCode = E_MISMATCH;
          else
            m_errorCode = E_NOTFOUND;
        }
    }
  else if( strcmp(command, "I")==0 || strcmp(command, "X+\x0dUJ")==0 )
    {
      m_dirOpen = false;
      m_file.close();
      m_curFileChannel = -1;
      m_errorCode = E_OK;
    }
  else if( strncmp(command, "CFG:", 4)==0 )
    {
      command += 4;
      char *c = strchr(command, '=');
      if( c==NULL )
        {
          string v = m_config->getValue(command) + "\r";
          setStatus(v.c_str(), v.size());
        }
      else
        {
          string key = string(command).substr(0, c-command);
          string val = string(c+1);
          m_config->setValue(key, val);

          if( tolower(key)=="diskflush" )
            m_diskFlushTimeout = std::atoi(val.c_str());
          else if( tolower(key)=="showext" )
            m_showExt = std::atoi(val.c_str())!=0;
        }

      m_errorCode = E_OK;
    }
  else if( command[0]=='X' || command[0]=='E' )
    {
      if( isdigit(command[1]) )
        {
          int d = atoi(command+1);
          if( d>=4 && d<=15 )
            {
              m_config->setValue("device", std::to_string(d));
              setDeviceNumber(d);
              m_errorCode = E_OK;
            }
          else
            m_errorCode = E_INVCMD;
        }
      else
        m_errorCode = E_INVCMD;
    }
  else
    m_errorCode = E_INVCMD;

  if( command[0]=='I' )
    {
      m_display->endProgress();
      m_display->setCurrentFileName("");
      updateDisplayStatus();
    }
  else if( m_errorCode!=E_OK )
    updateDisplayStatus();
}


const char *IECDrive::getStatusMessage(uint8_t statusCode)
{
  const char *message = NULL;

  switch( statusCode )
    {
    case E_OK:                   { message = " OK"; break; }
    case E_READ:                 { message = "READ ERROR"; break; }
    case E_WRITEPROT:            { message = "WRITE PROTECT ON"; break; }
    case E_WRITE:                { message = "WRITE ERROR"; break; }
    case E_SCRATCHED:            { message = "FILES SCRATCHED"; break; }
    case E_NOTREADY:             { message = "DRIVE NOT READY"; break; }
    case E_NOTFOUND:             { message = "FILE NOT FOUND"; break; }
    case E_EXISTS:               { message = "FILE EXISTS"; break; }
    case E_MISMATCH:             { message = "FILE TYPE MISMATCH"; break; }
    case E_INVCMD:
    case E_INVNAME:              { message = "SYNTAX ERROR"; break; }
    case E_MEMEXE:               { message = "M-E NOT SUPPORTED"; break; }
    case E_TOOMANY:              { message = "TOO MANY OPEN FILES"; break; }
    case E_SPLASH:               { message = "IEC-DRIVE V0.1"; break; }
    default:                     { message = "UNKNOWN"; break; }
    }

  return message;
}


uint8_t IECDrive::getStatusData(char *buffer, uint8_t bufferSize, bool *eoi)
{ 
  // if we have an active VDrive then just return its status
  if( m_drive->isOk() )
    {
      if( m_errorCode!=E_OK )
        {
          m_errorCode = E_OK;
          updateDisplayStatus();
        }

      return m_drive->getStatusBuffer(buffer, bufferSize, eoi);
    }

  // IECFileDevice::getStatusData will in turn call IECSD::getStatus()
  return IECFileDevice::getStatusData(buffer, bufferSize, eoi);
}


void IECDrive::getStatus(char *buffer, uint8_t bufferSize)
{
  const char *message = getStatusMessage(m_errorCode);
  uint8_t i = 0;
  buffer[i++] = '0' + (m_errorCode / 10);
  buffer[i++] = '0' + (m_errorCode % 10);
  buffer[i++] = ',';
  strcpy(buffer+i, message);
  i += strlen(message);

  if( m_errorCode!=E_SCRATCHED ) m_scratched = 0;
  buffer[i++] = ',';
  buffer[i++] = '0' + (m_scratched / 10);
  buffer[i++] = '0' + (m_scratched % 10);
  strcpy(buffer+i, ",00\r");

  if( m_errorCode!=E_OK )
    {
      m_errorCode = E_OK;
      updateDisplayStatus();
    }
}


void IECDrive::unmountDiskImage()
{
  if( m_drive->isOk() )
    m_drive->closeDiskImage();

  m_display->setCurrentImageName("");
  m_lastActivity = 0;
}


bool IECDrive::mountDiskImage(const char *name)
{
  if( m_drive->isOk() )
    m_drive->closeDiskImage();

  bool res = m_drive->openDiskImage(name);
  m_display->setCurrentImageName(res ? name : "");
  m_lastActivity = 0;

  return res;
}


const char *IECDrive::getMountedImageName()
{
  return m_drive->getDiskImageFilename();
}


void IECDrive::reset()
{
  unsigned long ledTestEnd = millis() + 250;
  setLEDState(LED_GREEN);

  IECFileDevice::reset();

  m_errorCode = E_SPLASH;
  m_drive->closeAllChannels();

  m_file.close();
  m_dirOpen = false;
  m_display->setCurrentFileName("");
  m_curFileChannel = -1;

  if( m_pinLED<0xFF ) 
    { 
      unsigned long t = millis();
      if( t<ledTestEnd ) delay(ledTestEnd-t);
      setLEDState(LED_OFF);
    }

  updateDisplayStatus();
  m_display->redraw();
}


void IECDrive::updateDisplayStatus()
{
  char buf[22];
  if( m_errorCode==E_VDRIVE )
    strncpy(buf, m_drive->getStatusString(), 22);
  else
    {
      int sec = m_errorCode==E_SCRATCHED ? m_scratched : 0;
      int code = m_errorCode==E_SPLASH ? E_OK : m_errorCode;
      snprintf(buf, 22, "%02i,%s,00,%02i", code, getStatusMessage(code),sec);
    }

  buf[21] = 0;
  m_display->setStatusMessage(buf);
}


void IECDrive::setLEDState(int color)
{
  if( m_pinLED<0xFF ) 
    {
#ifdef PIN_DRIVE_LED_IS_NEOPIXEL_RGB
      switch( color )
        {
        case LED_OFF:   s_led.clear(); break;
        case LED_RED:   s_led.setPixelColor(0,BRIGHTNESS,0,0); break;
        case LED_GREEN: s_led.setPixelColor(0,0,BRIGHTNESS,0); break;
        case LED_BLUE:  s_led.setPixelColor(0,0,0,BRIGHTNESS); break;
        default:        s_led.setPixelColor(0,BRIGHTNESS,BRIGHTNESS,BRIGHTNESS); break;
        }
      s_led.show();
#else
      digitalWrite(m_pinLED, color!=LED_OFF);
#endif
    }
}
