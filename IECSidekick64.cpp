#include "IECSidekick64.h"
#include <LittleFS.h>

#define E_OK          0
#define E_SCRATCHED   1
#define E_READ       20
#define E_WRITE      28
#define E_INVCMD     31
#define E_INVNAME    33
#define E_NOTFOUND   62
#define E_EXISTS     63
#define E_SPLASH     73
#define E_NOTREADY   74
#define E_TOOMANY    98
#define E_VDRIVE     255

#ifndef min
#define min(a,b) ((a)<(b) ? (a) : (b))
#endif

// ----------------------------------------------------------------------------------------------


IECSidekick64::IECSidekick64(uint8_t devnum, uint8_t pinChipSelect, uint8_t pinLED) :
  IECFileDevice(devnum)
{
  m_pinLED = pinLED;
  m_pinChipSelect = pinChipSelect;
  m_dirOpen = false;
  m_drive = NULL;
}


void IECSidekick64::begin()
{
  if( m_pinChipSelect<0xFF ) pinMode(m_pinChipSelect, OUTPUT);

  unsigned long ledTestEnd = millis() + 500;
  if( m_pinLED<0xFF ) { pinMode(m_pinLED, OUTPUT); digitalWrite(m_pinLED, HIGH); }

  m_errorCode = E_SPLASH;
  LittleFS.begin();

  if( m_pinLED<0xFF ) 
    {
      unsigned long t = millis();
      if( t<ledTestEnd ) delay(ledTestEnd-t);
      digitalWrite(m_pinLED, LOW); 
    }

  m_drive = new VDrive(0);

  IECFileDevice::begin();
}


bool IECSidekick64::checkCard()
{
  return true;
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

  // handle IEC serial bus communication, the open/read/write/close/execute 
  // functions will be called from within this when required
  IECFileDevice::task();
}


uint8_t IECSidekick64::openDir()
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
  strcpy_P(m_dirBuffer+24, PSTR("\" 00 2A"));
  m_dirBufferLen = 32;
  m_dirBufferPtr = 0;
  
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

          if( m_dir.next() )
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
                  strcpy_P(m_dirBuffer+m_dirBufferLen, ftype!=NULL ? ftype : (m_dir.isDirectory() ? "DIR" : "PRG"));
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
              strcpy_P(m_dirBuffer+4, PSTR("BLOCKS FREE.             "));
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
          if( (extmatch & 1) && strcasecmp_P(name+i+1, PSTR("prg"))==0 )
            found = 1;
          else if( (extmatch & 2) && strcasecmp_P(name+i+1, PSTR("seq"))==0 )
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
      found = !m_dir.isDirectory() && isMatch(name, pattern, ftype=='P' ? 1 : 2);
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

  strcpy(name, constName);
  char *c = strchr(name, '\xa0');
  if( c!=NULL ) *c = 0;

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
          
          //strcat_P(name, ftype=='P' ? PSTR(".prg") : PSTR(".seq"));
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
  if( !checkCard() )
    m_errorCode = E_NOTREADY;
  else if( m_drive->isOk() )
    m_errorCode = m_drive->openFile(channel, name) ? E_OK : E_VDRIVE;
  else if( channel==0 && name[0]=='$' )
    m_errorCode = openDir();
  else if ( !m_file )
    m_errorCode = openFile(channel, name);
  else
    {
      // we can only have one file open at a time
      m_errorCode = E_TOOMANY;
    }

  // clear the status buffer so getStatus() is called again next time the buffer is queried
  clearStatus();

  return m_errorCode==E_OK;
}


uint8_t IECSidekick64::read(uint8_t channel, uint8_t *buffer, uint8_t bufferSize, bool *eoi)
{
  if( m_drive->isFileOk(channel) )
    {
      size_t n = bufferSize;
      if( !m_drive->read(channel, buffer, &n, eoi) )
        {
          m_errorCode = E_VDRIVE;
          return 0;
        }

      return n;
    }
  else if( m_file )
    return m_file.read(buffer, bufferSize);
  else
    return readDir(buffer) ? 1 : 0;
}


uint8_t IECSidekick64::write(uint8_t channel, uint8_t *buffer, uint8_t bufferSize, bool eoi)
{
  if( m_drive->isFileOk(channel) )
    {
      size_t n = bufferSize;
      if( !m_drive->write(channel, buffer, &n) )
        {
          m_errorCode = E_VDRIVE;
          return 0;
        }

      return n;
    }
  else if( m_file )
    return m_file.write(buffer, bufferSize);
  else
    return 0;
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
}


void IECSidekick64::execute(const char *command, uint8_t len)
{
  // clear the status buffer so getStatus() is called again next time the buffer is queried
  clearStatus();
  digitalWrite(m_pinLED, HIGH);
  if( m_drive->isOk() )
    {
      if( strcmp(command, "CD:..")==0 || strcmp(command, "CD_")==0 )
        { 
          m_drive->closeDiskImage(); 
          m_errorCode = E_OK; 
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
      
      strncpy(pattern, command+2, 16);
      pattern[16]=0;
      
      m_dir = LittleFS.openDir("/");
      while( m_dir.next() )
        {
          String name = m_dir.fileName();
          if( name.length()>0 && isMatch(name.c_str(), pattern, 1+2) && LittleFS.remove(name.c_str()) )
            {
              m_scratched++;
              m_dir.rewind();
            }
        }
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
  else if( strncmp_P(command, PSTR("M-R\xfa\x02\x03"), 6)==0 )
    {
      // hack: DolphinDos' MultiDubTwo reads 02FA-02FC to determine
      // number of free blocks => pretend we have 664 (0298h) blocks available
      uint8_t data[3] = {0x98, 0, 0x02};
      setStatus((char *) data, 3);
      m_errorCode = E_OK;
    }
  else if( strncmp_P(command, PSTR("M-R"), 3)==0 && len>=6 && command[5]<=32 )
    {
      // memory read not supported => always return 0xFF
      uint8_t n = command[5];
      char buf[32];
      memset(buf, 0xFF, n);
      setStatus(buf, n);
      m_errorCode = E_OK;
    }
  else if( strncmp_P(command, PSTR("M-W"), 3)==0 )
    {
      // memory write not supported => ignore
      m_errorCode = E_OK;
    }
  else if( strncmp_P(command, PSTR("CD:"),3)==0 )
    {
      strncpy(m_dirBuffer, command+3, IEC_BUFSIZE);
      m_dirBuffer[IEC_BUFSIZE-1]=0;
      m_errorCode = m_drive->openDiskImage(m_dirBuffer) ? E_OK : E_NOTFOUND;
    }
  else if( strcmp(command, "I")==0 || strcmp_P(command, PSTR("X+\x0dUJ"))==0 )
    m_errorCode = E_OK;
  else if( command[0]=='X' || command[0]=='E' )
    {
      command++;
      if( command[0]>='1' && command[0]<='9' )
        {
          const char *c = command;
          uint8_t devnr = *c++ - '0';
          if( *c>='0' && *c<='9' ) devnr = devnr*10 + *c++ - '0';
          if( *c!=0 ) devnr = 0;

          if( devnr>2 && devnr<16 )
            {
              m_devnr = devnr;
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

  digitalWrite(m_pinLED, LOW);
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

  const char *message = NULL;
  switch( m_errorCode )
    {
    case E_OK:                   { message = PSTR(" OK"); break; }
    case E_READ:                 { message = PSTR("READ ERROR"); break; }
    case E_WRITE:                { message = PSTR("WRITE ERROR"); break; }
    case E_SCRATCHED:            { message = PSTR("FILES SCRATCHED"); break; }
    case E_NOTREADY:             { message = PSTR("DRIVE NOT READY"); break; }
    case E_NOTFOUND:             { message = PSTR("FILE NOT FOUND"); break; }
    case E_EXISTS:               { message = PSTR("FILE EXISTS"); break; }
    case E_INVCMD:
    case E_INVNAME:              { message = PSTR("SYNTAX ERROR"); break; }
    case E_TOOMANY:              { message = PSTR("TOO MANY OPEN FILES"); break; }
    case E_SPLASH:               { message = PSTR("IEC-SIDEKICK64 V0.1"); break; }
    default:                     { message = PSTR("UNKNOWN"); break; }
    }

  uint8_t i = 0;
  buffer[i++] = '0' + (m_errorCode / 10);
  buffer[i++] = '0' + (m_errorCode % 10);
  buffer[i++] = ',';
  strcpy_P(buffer+i, message);
  i += strlen_P(message);

  if( m_errorCode!=E_SCRATCHED ) m_scratched = 0;
  buffer[i++] = ',';
  buffer[i++] = '0' + (m_scratched / 10);
  buffer[i++] = '0' + (m_scratched % 10);
  strcpy_P(buffer+i, PSTR(",00\r"));


  m_errorCode = E_OK;
}


void IECSidekick64::reset()
{
  unsigned long ledTestEnd = millis() + 250;
  if( m_pinLED<0xFF ) digitalWrite(m_pinLED, HIGH);

  IECFileDevice::reset();

  m_errorCode = E_SPLASH;
  m_drive->closeAllChannels();
  if( !checkCard() ) m_errorCode = E_NOTREADY;

  m_file.close();
  m_dirOpen = false;

  if( m_pinLED<0xFF ) 
    { 
      unsigned long t = millis();
      if( t<ledTestEnd ) delay(ledTestEnd-t);
      digitalWrite(m_pinLED, LOW); 
    }
}
