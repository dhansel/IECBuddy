#ifndef IECDISPLAY
#define IECDISPLAY


#include <string>

class IECDisplay
{
 public:
  IECDisplay();
  virtual ~IECDisplay();

  void setCurrentImageName(std::string iname);
  void setCurrentFileName(std::string fname);

  virtual void begin();

  virtual void showMessage(std::string msg);
  virtual void showTransmitMessage(std::string msg, std::string fileName);
  virtual void showPrintStatus(bool printing);

  virtual void startProgress(int nbytestotal);
  virtual void updateProgress(int nbytes);
  virtual void update(const char *statusMessage);

  static IECDisplay *Create(std::string displayType);

 protected:
  bool isErrorStatus(const char *statusMessage);

  std::string m_curImageName;
  std::string m_curFileName;

  int         m_curFileSize;
  int         m_curFileBytesRead;
  int         m_progressWidth;
};

#include "IECDisplay_SSD1306.h"
#include "IECDisplay_ST7789.h"

#endif
