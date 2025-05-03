#include "IECSidekick64.h"
#include "IECDisplay_SSD1306.h"
#include "IECDisplay_ST7789.h"
#include <LittleFS.h>
#include <algorithm>

using namespace std;

#define BUTTON_PIN       22

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
#define E_TOOMANY    98
#define E_VDRIVE     99

#define CONFIGFILENAME "$CONFIG$"

#ifndef min
#define min(a,b) ((a)<(b) ? (a) : (b))
#endif

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

  bool state = digitalRead(BUTTON_PIN);
  if( state!=debounceState && millis()>debounceTime )
    {
      debounceState = state;
      debounceTime  = millis()+20;
      if( !state ) changeDisk = true;
    }
}


IECSidekick64::IECSidekick64(uint8_t devnum, uint8_t pinLED) :
  IECFileDevice(devnum)
{
  m_pinLED = pinLED;
  m_dirOpen = false;
  m_drive = NULL;
  m_curFileChannel = -1;
  m_display = new IECDisplay(); // no display
}


void IECSidekick64::begin()
{
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  attachInterrupt(BUTTON_PIN, diskChangeButtonFcn, CHANGE);

  unsigned long ledTestEnd = millis() + 500;
  if( m_pinLED<0xFF ) { pinMode(m_pinLED, OUTPUT); digitalWrite(m_pinLED, HIGH); }

  m_errorCode = E_SPLASH;
  LittleFS.begin();
  readConfig();

  int d = std::atoi(getConfigValue("device").c_str());
  if( d>=8 && d<=15 && d!=m_devnr ) setDeviceNumber(d);

  if( m_pinLED<0xFF ) 
    {
      unsigned long t = millis();
      if( t<ledTestEnd ) delay(ledTestEnd-t);
      digitalWrite(m_pinLED, LOW); 
    }


  string displayType = getConfigValue("display");
  if( displayType.empty() )
    {
#if defined(SUPPORT_SSD1306)
      displayType = "SSD1306";
#elif defined(SUPPORT_ST7789)
      displayType = "ST7789";
#else
      displayType = "NONE";
#endif
      setConfigValue("display", displayType);
    }

  delete m_display;

  if( displayType=="NONE" )
    m_display = new IECDisplay(); // no display
#if defined(SUPPORT_SSD1306)
  else if( displayType=="SSD1306" )
    m_display = new IECDisplay_SSD1306();
#endif
#if defined(SUPPORT_ST7789)
  else if( displayType=="ST7789" )
    m_display = new IECDisplay_ST7789();
#endif
  else
    m_display = new IECDisplay(); // no display

  m_drive = new VDrive(0);

  IECFileDevice::begin();
  m_display->begin();
  updateDisplay();
}


void IECSidekick64::task()
{
  // handle status LED
  if( m_pinLED<0xFF )
    {
      static unsigned long nextblink = 0;
      if( m_errorCode==E_OK || m_errorCode==E_SPLASH || m_errorCode==E_SCRATCHED )
        {
          bool active = m_dirOpen || (bool) m_file;
          active |= m_drive->getNumOpenChannels()>0;
          digitalWrite(m_pinLED, active);
        }
      else if( millis()>nextblink )
        {
          digitalWrite(m_pinLED, !digitalRead(m_pinLED));
          nextblink += 500;
        }
    }

  if( changeDisk )
    {
      changeDisk = false;

      updateDisplay();
      string current;
      if( m_drive->isOk() ) 
        {
          current = m_drive->getDiskImageFilename();
          m_drive->closeDiskImage();
        }

      m_display->showMessage("Searching...");

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

      const char *iname = m_drive->getDiskImageFilename();
      m_display->setCurrentImageName(iname ? iname : "");
      updateDisplay();
    }

  // handle IEC serial bus communication, the open/read/write/close/execute 
  // functions will be called from within this when required
  IECFileDevice::task();
}



void IECSidekick64::clearConfig()
{
  LittleFS.remove("/" CONFIGFILENAME);
  m_config.clear();
}


const string &IECSidekick64::getConfigValue(const string &key)
{
  return m_config[tolower(key)];
}


void IECSidekick64::setConfigValue(const string &key, const string &value, bool write)
{
  m_config[tolower(key)] = value;
  if( write ) writeConfig();
}


void IECSidekick64::readConfig()
{
  bool ok = false;

  m_config.clear();
  File f = LittleFS.open("/" CONFIGFILENAME, "r");
  if( f )
    {
      char *s = (char *) calloc(1, f.size()+1);
      if( s )
        {
          ok = f.read((uint8_t *) s, f.size())==f.size();
          if( ok )
            {
              char *p;
              for(char *line=strtok_r(s, "\n", &p); line!=NULL; line=strtok_r(NULL, "\n", &p))
                {
                  char *eq = strchr(line, '=');
                  if( eq!=NULL )
                    {
                      *eq = 0;
                      m_config[tolower(string(line))]=string(eq+1);
                    }
                }
            }
          free(s);
        }
      
      f.close();
    }

  if( !ok )
    {
      m_config.clear();
      m_config["device"] = std::to_string(m_devnr);
      writeConfig();
    }
}


void IECSidekick64::writeConfig()
{
  File f = LittleFS.open("/" CONFIGFILENAME, "w");
  if( f )
    {
      for(const std::pair<const string, string>&cfg : m_config)
        {
          string line = cfg.first + "=" + cfg.second + "\n";
          f.write((uint8_t *) line.c_str(), line.length());
        }
      f.close();
    }
}


#if defined(SUPPORT_EPYX) && defined(SUPPORT_EPYX_SECTOROPS)
bool IECSidekick64::epyxReadSector(uint8_t track, uint8_t sector, uint8_t *buffer)
{
  bool res = false;

  if( m_drive->isOk() )
    res = m_drive->readSector(track, sector, buffer);

  // for debug log
  if( res ) IECFileDevice::epyxReadSector(track, sector, buffer);

  return res;
}


bool IECSidekick64::epyxWriteSector(uint8_t track, uint8_t sector, uint8_t *buffer)
{
  bool res = false;

  // for debug log
  IECFileDevice::epyxWriteSector(track, sector, buffer);

  if( m_drive->isOk() )
    res = m_drive->writeSector(track, sector, buffer);

  return res;
}
#endif


string IECSidekick64::stripFileName(const char *cname)
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


bool IECSidekick64::isHiddenFile(const char *name)
{
  if( name==NULL )
    return false;
  else
    {
      int len = strlen(name);
      return len>2 && name[0]=='$' && name[len-1]=='$';
    }
}


uint8_t IECSidekick64::openDir(const char *name)
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
  strcpy(m_dirBuffer+8, "SIDEKICK64");
  size_t n = strlen(m_dirBuffer+8);
  while( n<16 ) { m_dirBuffer[8+n] = ' '; n++; }
  strcpy(m_dirBuffer+24, "\" 00 2A");
  m_dirBufferLen = 32;
  m_dirBufferPtr = 0;

  if( isdigit(name[1]) && name[2]==':' )
    m_dirPattern = name+3;
  else if( name[1] == ':' )
    m_dirPattern = name+2;
  else
    m_dirPattern = name+1;

  if( *m_dirPattern==0 ) m_dirPattern="*";

  return E_OK;
}


bool IECSidekick64::readDir(uint8_t *data)
{
  if( m_dirBufferPtr==m_dirBufferLen && m_dirOpen )
    {
      bool repeat = true;
      while( repeat )
        {
          m_dirBufferPtr = 0;
          m_dirBufferLen = 0;

          bool ok = m_dir.next();
          while( ok && !isMatch(m_dir.fileName().c_str(), m_dirPattern, 1+2) || isHiddenFile(m_dir.fileName().c_str()) )
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

                  const char *ftype = NULL;
                  if( n>4 && strcasecmp(m_dirBuffer+m_dirBufferLen+n-4, ".prg")==0 )
                    { n -= 4; ftype = "PRG"; }
                  else if( n>4 && strcasecmp(m_dirBuffer+m_dirBufferLen+n-4, ".seq")==0 )
                    { n -= 4; ftype = "SEQ"; }
                  else if( n>17 )
                    n = 17;
                  
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


bool IECSidekick64::isMatch(const char *name, const char *pattern, uint8_t extmatch)
{
  signed char found = -1;

  for(uint8_t i=0; found<0; i++)
    {
      if( pattern[i]=='*' )
        found = 1;
      else if( pattern[i]==0 && name[i]=='.' )
        {
          if( (extmatch & 1) && strcasecmp(name+i+1, "prg")==0 )
            found = 1;
          else if( (extmatch & 2) && strcasecmp(name+i+1, "seq")==0 )
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


const char *IECSidekick64::findFile(const char *pattern, char ftype)
{
  bool found = false;
  static char name[22];

  m_dir = LittleFS.openDir("/");
  while( !found && m_dir.next() )
    {
      strcpy(name, m_dir.fileName().substring(0,21).c_str());
      found = !m_dir.isDirectory() && isMatch(name, pattern, ftype=='P' ? 1 : 2) && !isHiddenFile(name);
    }

  return found ? name : NULL;
}


uint8_t IECSidekick64::openFile(uint8_t channel, const char *constName)
{
  uint8_t res = E_OK;
  char ftype = 'P';
  char mode  = 'R';
  char *name = m_dirBuffer;

  // skip "0:" prefix
  if( isdigit(constName[0]) && constName[1]==':' ) constName+=2;

  // file name ends at the first 0xA0 ("shifted space")
  strcpy(name, constName);
  char *c = strchr(name, '\xa0');
  if( c!=NULL ) *c = 0;

  // convert "/" and "\" to "-" (we don't support subdirectories)
  for(c=name; *c!=0; c++)
    if( *c=='/' || *c=='\\' ) *
      c = '-';

  char *comma = strchr(name, ',');
  if( comma!=NULL )
    {
      char *c = comma;
      do { *c-- = 0; } while( c!=name && ((*c) & 0x7f)==' ');
      ftype  = toupper(*(comma+1));
      if( ftype=='R' || ftype=='W' )
        {
          mode  = ftype;
          ftype = 'P';
        }
      else
        {
          comma  = strchr(comma+1, ',');
          if( comma!=NULL )
            mode = toupper(*(comma+1));
        }
    }
  else if( channel==0 )
    mode = 'R';
  else if( channel==1 )
    mode = 'W';
  
  if( (ftype!='P' && ftype!='S') || (mode!='R' && mode!='W') )
    res = E_INVNAME;

  if( res == E_OK )
    {
      if( mode=='R' )
        {
          if( isHiddenFile(name) )
            return E_NOTFOUND;
          
          if( name[0]==':' ) name++;
          m_file = LittleFS.open(name, "r");
          if( !m_file )
            {
              const char *fn = findFile(name, ftype);
              if( fn ) m_file = LittleFS.open(fn, "r");
            }
          
          res = m_file && (m_file.size()>0) ? E_OK : E_NOTFOUND;
          if( res != E_OK ) m_file.close();
        }
      else
        {
          if( isHiddenFile(name) )
            return E_WRITEPROT;

          bool overwrite = false;
          if( name[0]=='@' && name[1]==':' )
            { name+=2; overwrite = true; }
          else if( name[0]=='@' && name[1]!=0 && name[2]==':' )
            { name+=3; overwrite = true; }
          else if( name[0]!=0 && name[1]==':' )
            { name+=2; }

          // if we are overwriting a file whose name exists without PRG/SEQ extension 
          // then delete the existing version (so we don't get two files with the same name)
          if( overwrite && findFile(name, '\0')!=NULL )
            LittleFS.remove(name);
          
          if( !overwrite && LittleFS.exists(name) )
            res = E_EXISTS;
          else 
            {
              m_file = LittleFS.open(name, "w");
              res = m_file ? E_OK : res = E_WRITE;
            }
        }
    }

  return res;
}


bool IECSidekick64::open(uint8_t channel, const char *name)
{
  if( m_drive->isOk() )
    m_errorCode = m_drive->openFile(channel, name) ? E_OK : E_VDRIVE;
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
      updateDisplay();
      if( m_drive->isOk() )
        m_display->startProgress(m_drive->getFileNumBlocks(sname.c_str()) * 254);
      else if( m_file )
        m_display->startProgress(m_file.size());
      else
        m_display->startProgress(-1);
    }
  else
    updateDisplay();

  // clear the status buffer so getStatus() is called again next time the buffer is queried
  clearStatus();

  return m_errorCode==E_OK;
}


uint8_t IECSidekick64::read(uint8_t channel, uint8_t *buffer, uint8_t bufferSize, bool *eoi)
{
  uint8_t n = 0;

  if( m_drive->isFileOk(channel) )
    {
      size_t nn = bufferSize;
      if( m_drive->read(channel, buffer, &nn, eoi) )
        n = nn;
      else
        m_errorCode = E_VDRIVE;
    }
  else if( m_file )
    n = m_file.read(buffer, bufferSize);
  else
    n = readDir(buffer) ? 1 : 0;

  if( channel==m_curFileChannel )
    m_display->updateProgress(n);

  return n;
}


uint8_t IECSidekick64::write(uint8_t channel, uint8_t *buffer, uint8_t bufferSize, bool eoi)
{
  uint8_t n = 0;

  if( m_drive->isFileOk(channel) )
    {
      size_t nn = bufferSize;
      if( m_drive->write(channel, buffer, &nn) )
        n = nn;
      else
        m_errorCode = E_VDRIVE;
    }
  else if( m_file )
    n = m_file.write(buffer, bufferSize);

  if( channel==m_curFileChannel )
    m_display->updateProgress(n);

  return n;
}


void IECSidekick64::close(uint8_t channel)
{
  if( m_drive->isFileOk(channel) )
    {
      m_drive->closeFile(channel);
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
      m_display->setCurrentFileName("");
      m_curFileChannel = -1;
      updateDisplay();
    }
}


void IECSidekick64::execute(const char *command, uint8_t len)
{
  // clear the status buffer so getStatus() is called again next time the buffer is queried
  clearStatus();
  digitalWrite(m_pinLED, HIGH);

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

  if( command[0]=='X' || command[0]=='E' )
    {
      if( isdigit(command[1]) )
        {
          int d = atoi(command+1);
          if( d>=4 && d<=15 )
            {
              setConfigValue("device", std::to_string(d));
              setDeviceNumber(d);
              m_errorCode = E_OK;
            }
          else
            m_errorCode = E_INVCMD;
        }
      else
        m_errorCode = E_INVCMD;
    }
  else if( m_drive->isOk() )
    {
      if( cdcmd<0 )
        {
          // CD "up"
          m_drive->closeDiskImage(); 
          m_errorCode = E_OK; 
          m_display->setCurrentImageName("");
          updateDisplay();
        }
      else 
        {
          int r = m_drive->execute(command, len);
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

          // when executing a "P" (position relative file) command we need
          // to clear the read buffer of the channel for which this command
          // is issued, otherwise remaining characters in the buffer will be
          // prefixed to the data from the new record
          if( command[0]=='P' && len>=2 )
            clearReadBuffer(command[1] & 0x0f);
        }
    }
  else if( strncmp(command, "S:", 2)==0 )
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
          if( name.length()>0 && isMatch(name.c_str(), pattern, 1+2) && !isHiddenFile(name.c_str()) )
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
  else if( strncmp(command, "M-R\xfa\x02\x03", 6)==0 )
    {
      // hack: DolphinDos' MultiDubTwo reads 02FA-02FC to determine
      // number of free blocks => pretend we have 664 (0298h) blocks available
      uint8_t data[3] = {0x98, 0, 0x02};
      setStatus((char *) data, 3);
      m_errorCode = E_OK;
    }
  else if( strncmp(command, "M-R", 3)==0 && len>=6 && command[5]<=32 )
    {
      // memory read not supported => always return 0xFF
      uint8_t n = command[5];
      char buf[32];
      memset(buf, 0xFF, n);
      setStatus(buf, n);
      m_errorCode = E_OK;
    }
  else if( strncmp(command, "M-W", 3)==0 )
    {
      // memory write not supported => ignore
      m_errorCode = E_OK;
    }
  else if( cdcmd<0 )
    {
      // CD "up" (no effect since no disk image mounted)
    }
  else if( cdcmd>0 )
    {
      strncpy(m_dirBuffer, command+cdcmd, IEC_BUFSIZE);
      m_dirBuffer[IEC_BUFSIZE-1]=0;

      if( !LittleFS.exists(m_dirBuffer) )
        {
          const char *name = findFile(m_dirBuffer, 0);
          if( name!=NULL ) strcpy(m_dirBuffer, name);
        }

      if( mountDiskImage(m_dirBuffer) )
        m_errorCode = E_OK;
      else if( LittleFS.exists(m_dirBuffer) )
        m_errorCode = E_MISMATCH;
      else
        m_errorCode = E_NOTFOUND;

      updateDisplay();
    }
  else if( strcmp(command, "I")==0 || strcmp(command, "X+\x0dUJ")==0 )
    m_errorCode = E_OK;
  else if( strncmp(command, "CFG:", 4)==0 )
    {
      command += 4;
      char *c = strchr(command, '=');
      if( c==NULL )
        {
          string v = getConfigValue(command) + "\r";
          setStatus(v.c_str(), v.size());
        }
      else
        setConfigValue(string(command).substr(0, c-command), string(c+1));

      m_errorCode = E_OK;
    }
  else
    m_errorCode = E_INVCMD;

  if( m_errorCode!=E_OK ) updateDisplay();
  digitalWrite(m_pinLED, LOW);
}


const char *IECSidekick64::getStatusMessage(uint8_t statusCode)
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
    case E_TOOMANY:              { message = "TOO MANY OPEN FILES"; break; }
    case E_SPLASH:               { message = "IEC-SIDEKICK64 V0.1"; break; }
    default:                     { message = "UNKNOWN"; break; }
    }

  return message;
}


void IECSidekick64::getStatus(char *buffer, uint8_t bufferSize)
{
  if( m_errorCode==E_VDRIVE )
    {
      strncpy(buffer, m_drive->getStatusString(), bufferSize);
      buffer[bufferSize-1] = '\r';
      m_errorCode = E_OK;
      return;
    }

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
      updateDisplay();
    }
}


void IECSidekick64::unmountDiskImage()
{
  if( m_drive->isOk() )
    m_drive->closeDiskImage();

  m_display->setCurrentImageName("");
  updateDisplay();
}


bool IECSidekick64::mountDiskImage(const char *name)
{
  if( m_drive->isOk() )
    m_drive->closeDiskImage();

  bool res = m_drive->openDiskImage(name);
  m_display->setCurrentImageName(res ? name : "");
  updateDisplay();

  return res;
}


const char *IECSidekick64::getMountedImageName()
{
  return m_drive->getDiskImageFilename();
}


void IECSidekick64::reset()
{
  unsigned long ledTestEnd = millis() + 250;
  if( m_pinLED<0xFF ) digitalWrite(m_pinLED, HIGH);

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
      digitalWrite(m_pinLED, LOW); 
    }

  updateDisplay();
}


void IECSidekick64::updateDisplay()
{
  char buf[22];
  if( m_errorCode==E_VDRIVE )
    strncpy(buf, m_drive->getStatusString(), 22);
  else
    snprintf(buf, 22, "%02i,%s,00,00", m_errorCode, getStatusMessage(m_errorCode));

  buf[21] = 0;
  m_display->update(buf);
}
