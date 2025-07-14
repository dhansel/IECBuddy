#ifndef IECDISPLAY_ST7789
#define IECDISPLAY_ST7789

#include "IECDisplay.h"
#include "Pins.h"

#ifdef PIN_ST7789_SPI
#define SUPPORT_ST7789
#endif

class Arduino_ST7789m;

class IECDisplay_ST7789 : public IECDisplay
{
 public:
  IECDisplay_ST7789();
  virtual ~IECDisplay_ST7789();

  virtual void begin();

  virtual void showMessage(std::string msg);
  virtual void showTransmitMessage(std::string msg, std::string fileName);

  virtual void showPrintStatus(bool printing);
  virtual void startProgress(int nbytestotal);
  virtual void updateProgress(int nbytes);
  virtual void update(const char *statusMessage);

 private:
  Arduino_ST7789m *m_display;  
  bool             m_doClear;
};

#endif
