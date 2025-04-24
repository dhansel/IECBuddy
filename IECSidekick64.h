#ifndef IECSIDEKICK64_H
#define IECSIDEKICK64_H

#include <string>
#include <unordered_map>
#include <LittleFS.h>
#include "src/IECDevice/IECFileDevice.h"
#include "src/VDrive/VDriveClass.h"

#define IEC_BUFSIZE 64

class IECDisplay;
typedef std::unordered_map<std::string, std::string> ConfigType;

class IECSidekick64 : public IECFileDevice
{
 public: 
  IECSidekick64(uint8_t devnum, uint8_t pinChipSelect, uint8_t pinLED);

  virtual void getStatus(char *buffer, uint8_t bufferSize);
  virtual void execute(const char *command, uint8_t len);

  void unmountDiskImage();
  bool mountDiskImage(const char *name);
  const char *getMountedImageName();

  const std::string &getConfigValue(const std::string &key);
  void setConfigValue(const std::string &key, const std::string &value, bool write = true);

  IECDisplay *getDisplay() { return m_display; }
  void updateDisplay();

 protected:
  virtual void begin();
  virtual void task();

  virtual bool open(uint8_t channel, const char *name);
  virtual uint8_t read(uint8_t channel, uint8_t *buffer, uint8_t bufferSize, bool *eoi);
  virtual uint8_t write(uint8_t channel, uint8_t *buffer, uint8_t bufferSize, bool eoi);
  virtual void close(uint8_t channel);
  virtual void reset();

#if defined(SUPPORT_EPYX) && defined(SUPPORT_EPYX_SECTOROPS)
  virtual bool epyxReadSector(uint8_t track, uint8_t sector, uint8_t *buffer);
  virtual bool epyxWriteSector(uint8_t track, uint8_t sector, uint8_t *buffer);
#endif

  ConfigType m_config;

 private:
  uint8_t openFile(uint8_t channel, const char *name);
  uint8_t openDir();
  bool readDir(uint8_t *data);
  bool isMatch(const char *name, const char *pattern, uint8_t extmatch);
  static const char *getStatusMessage(uint8_t statusCode);

  void readConfig();
  void writeConfig();

  const char *findFile(const char *name, char ftype);
  std::string stripFileName(const char *cname);

  VDrive *m_drive;
  IECDisplay *m_display;

  File m_file;
  Dir  m_dir;

  int     m_curFileChannel;
  bool    m_dirOpen;
  uint8_t m_pinLED, m_pinChipSelect, m_errorCode, m_scratched;
  uint8_t m_dirBufferLen, m_dirBufferPtr;
  char m_dirBuffer[IEC_BUFSIZE];
};

#endif
