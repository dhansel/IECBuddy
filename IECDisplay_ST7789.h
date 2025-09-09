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

  virtual void setCurrentImageName(std::string iname);
  virtual void setCurrentFileName(std::string fname);
  virtual void setStatusMessage(std::string msg);

  virtual void showMessage(std::string msg);
  virtual void showTransmitMessage(std::string msg, std::string fileName);
  virtual void showPrintStatus(bool printing);

  virtual void startProgress(int nbytestotal);
  virtual void updateProgress(int nbytes);
  virtual void endProgress();

  virtual void redraw();

  virtual uint32_t startImage(uint16_t x, uint16_t y, uint16_t w, uint16_t h);
  virtual void     addImageData(uint8_t *data, uint32_t dataSize);
  virtual void     endImage();
  virtual bool     setBackgroundImage(const std::string &filename, bool doUpdate = true);
  virtual bool     setBackgroundImageGIF(uint8_t *data, uint32_t size, int32_t x, int32_t y, bool doUpdate = true);

 private:
  bool setBackgroundImageRGB(const std::string &fname);
  bool setBackgroundImageGIF(const std::string &fname);

  void dispStatus(const std::string &s, bool clear = true);
  void dispImageName(const std::string &s, bool clear = true);
  void dispFileName(const std::string &s, bool clear = true);
  
  Arduino_ST7789m *m_display;  
};

#endif
