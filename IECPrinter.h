#ifndef IECPRINTER_H
#define IECPRINTER_H

#include <Arduino.h>
#include <LittleFS.h>
#include <string>
#include "src/IECDevice/IECFileDevice.h"

class IECDisplay;
class IECConfig;

class IECPrinter : public IECDevice
{
 public: 
  IECPrinter(uint8_t devnum, const char *printerFileName);
  virtual ~IECPrinter();

  void setDisplay(IECDisplay *d) { m_display = d; }
  void setConfig(IECConfig *c)   { m_config = c; }

 protected:
  virtual void    begin();
  virtual void    task();
  virtual void    listen(uint8_t secondary);
  virtual int8_t  canWrite();
  virtual void    write(uint8_t data, bool eoi);

 private:
  uint8_t     m_channel, m_prevChannel, m_zeroCount;
  int         m_data;
  std::string m_printerFileName;
  File        m_printerFile;
  uint32_t    m_timeout;
  
  IECConfig  *m_config;
  IECDisplay *m_display;
};


#endif
