#ifndef IECDRIVE_H
#define IECDRIVE_H

#include <string>
#include <unordered_map>
#include <LittleFS.h>
#include "src/IECDevice/IECFileDevice.h"
#include "src/VDrive/VDriveClass.h"

#define IEC_BUFSIZE 64

#ifndef isalphanum
#define isalphanum(c) (isalpha(c) || isdigit(c))
#endif

class IECDisplay;
class IECConfig;

class IECDrive : public IECFileDevice
{
 public: 
  IECDrive(uint8_t devnum, uint8_t pinLED);

  virtual void getStatus(char *buffer, uint8_t bufferSize);
  virtual void execute(const char *command, uint8_t len);

  void unmountDiskImage();
  bool mountDiskImage(const char *name);
  const char *getMountedImageName();

  void setDisplay(IECDisplay *d) { m_display = d; }
  void setConfig(IECConfig *c)   { m_config = c; }
  void updateDisplayStatus();

 protected:
  virtual void begin();
  virtual void task();

  virtual bool open(uint8_t channel, const char *name);
  virtual uint8_t read(uint8_t channel, uint8_t *buffer, uint8_t bufferSize, bool *eoi);
  virtual uint8_t write(uint8_t channel, uint8_t *buffer, uint8_t bufferSize, bool eoi);
  virtual void close(uint8_t channel);
  virtual void reset();

#if defined(IEC_FP_EPYX) && defined(IEC_FP_EPYX_SECTOROPS)
  virtual bool epyxReadSector(uint8_t track, uint8_t sector, uint8_t *buffer);
  virtual bool epyxWriteSector(uint8_t track, uint8_t sector, uint8_t *buffer);
#endif

 private:
  uint8_t openFile(uint8_t channel, const char *name);
  uint8_t openDir(const char *name);
  bool readDir(uint8_t *data);
  static const char *getStatusMessage(uint8_t statusCode);

  bool isMatch(const char *name, const char *pattern, uint8_t ftpes);
  const char *findFile(const char *name, uint8_t ftypes);
  bool isHiddenFile(const char *fname);

  std::string stripFileName(const char *cname);

  void setLEDState(int color);

  VDrive *m_drive;
  IECDisplay *m_display;
  IECConfig  *m_config;

  File m_file;
  Dir  m_dir;

  int     m_curFileChannel;
  bool    m_dirOpen, m_showExt;
  uint8_t m_pinLED, m_errorCode, m_scratched;
  uint8_t m_dirBufferLen, m_dirBufferPtr;
  char    m_dirBuffer[IEC_BUFSIZE];
  int32_t m_diskFlushTimeout;
  uint32_t m_lastActivity;
  const char *m_dirPattern;
};

#endif
