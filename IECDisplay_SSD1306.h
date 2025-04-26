#ifndef IECDISPLAY_SSD1306
#define IECDISPLAY_SSD1306

#include "IECDisplay.h"

// if enabled, the "Adafruit SSD1306" library must be installed in the Arduino IDE
#define SUPPORT_SSD1306

class Adafruit_SSD1306;

class IECDisplay_SSD1306 : public IECDisplay
{
 public:
  IECDisplay_SSD1306();
  virtual ~IECDisplay_SSD1306();

  virtual void begin();

  virtual void showMessage(std::string msg);
  virtual void showTransmitMessage(std::string msg, std::string fileName);

  virtual void startProgress(int nbytestotal);
  virtual void updateProgress(int nbytes);
  virtual void update(const char *statusMessage);

 private:
  Adafruit_SSD1306 *m_display;
};

#endif
