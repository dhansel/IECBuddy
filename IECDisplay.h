#ifndef IECDISPLAY
#define IECDISPLAY


#include <string>
#include <stdint.h>

class IECDisplay
{
 public:
  IECDisplay();
  virtual ~IECDisplay();

  virtual void begin(uint32_t rotation);

  virtual void setCurrentImageName(std::string iname);
  virtual void setCurrentFileName(std::string fname);
  virtual void setStatusMessage(std::string msg);

  virtual void showMessage(std::string msg);
  virtual void showTransmitMessage(std::string msg, std::string fileName);
  virtual void showPrintStatus(bool printing);

  virtual void startProgress(int nbytestotal);
  virtual void updateProgress(int nbytes);
  virtual void endProgress();

  virtual void setRotation(uint32_t rotation) {}
  virtual void redraw();

  virtual uint32_t startImage(uint16_t x, uint16_t y, uint16_t w, uint16_t h) { return 1; }
  virtual void     addImageData(uint8_t *data, uint32_t dataSize) {}
  virtual void     endImage() {}
  virtual bool     setBackgroundImage(const std::string &filename, bool doUpdate = true) { return false; }
  virtual bool     setBackgroundImageGIF(uint8_t *data, uint32_t size, int32_t x, int32_t y, bool doUpdate = true)  { return false; }

  static IECDisplay *Create(std::string displayType);

 protected:
  bool isErrorStatus(const char *statusMessage);

  std::string m_curImageName;
  std::string m_curFileName;
  std::string m_statusMessage;

  int         m_curFileSize;
  int         m_curFileBytesRead;
  int         m_progressWidth;
};

#include "IECDisplay_SSD1306.h"
#include "IECDisplay_ST7789.h"

#endif
