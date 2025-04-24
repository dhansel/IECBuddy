#include "IECDisplay_ST7789.h"
#include <Arduino_GFX_Library.h>

using namespace std;

#define DISPLAY_ST7789_PIN_DC    20
#define DISPLAY_ST7789_PIN_RST   21 // GFX_NOT_DEFINED


// derive our own ST7789 class so we can access parameters that are 
// not available otherwise (e.g. the SPI mode)
class Arduino_ST7789m : public Arduino_ST7789
{
public:
  Arduino_ST7789m(Arduino_DataBus *bus, int8_t rst = GFX_NOT_DEFINED) : 
    Arduino_ST7789(bus, rst, 0, false, 240, 240) {}

  bool begin(int32_t speed, int8_t spiMode)
    { _override_datamode = spiMode; return Arduino_TFT::begin(speed); }

  void clearDisplay() 
    { fillScreen(RGB565_BLACK); }

  void clearCurrentLine()
    { fillRect(0, cursor_y, width(), getTextLineHeight(), RGB565_BLACK); }

  int getTextLineHeight()
    { return (gfxFont==NULL ? 8 : gfxFont->yAdvance) * textsize_y; }
};


IECDisplay_ST7789::IECDisplay_ST7789()
{
  Arduino_DataBus *bus = new Arduino_HWSPI(DISPLAY_ST7789_PIN_DC);
  m_display = new Arduino_ST7789m(bus, DISPLAY_ST7789_PIN_RST);
  m_doClear = true;
}


IECDisplay_ST7789::~IECDisplay_ST7789()
{
  delete m_display;
}


void IECDisplay_ST7789::begin()
{
  m_display->begin(GFX_NOT_DEFINED /* SPI speed */, SPI_MODE3);
  m_display->invertDisplay(true);
  m_display->setTextWrap(false);
  m_display->setTextColor(RGB565_WHITE);
  m_display->clearDisplay();
  m_doClear = false;

#if 0
  m_display->setCursor(0, 60);
  m_display->setTextSize(6);
  m_display->println("Hello");
  m_display->println("World!");
#endif
}


void IECDisplay_ST7789::showMessage(std::string msg)
{
  m_display->setCursor(0, 56);
  m_display->setTextSize(3);
  m_display->print(msg.c_str());
  m_doClear = true;
}


void IECDisplay_ST7789::showTransmitMessage(std::string msg, std::string fileName)
{
  m_display->clearDisplay();
  m_display->setCursor(0,0);
  m_display->setTextSize(3);
  m_display->println(msg.c_str());
  m_display->println();
  m_display->print(fileName.c_str());
  m_doClear = true;
}


void IECDisplay_ST7789::startProgress(int nbytestotal)
{
  IECDisplay::startProgress(nbytestotal);
}


void IECDisplay_ST7789::updateProgress(int nbytes)
{
  if( m_curFileSize>0 )
    {
      int w = (m_display->width() * m_curFileBytesRead) / m_curFileSize;
      if( (w-m_progressWidth)>0 )
        {
          m_display->fillRect(m_progressWidth, m_display->height()-5, w-m_progressWidth, 5, RGB565_WHITE);
          m_progressWidth = w;
        }

      m_curFileBytesRead += nbytes;
    }
}


void IECDisplay_ST7789::update(const char *statusMessage)
{
  static string s_image="^", s_file, s_status;

  if( m_doClear ) 
    {
      m_display->clearDisplay();
      s_image.clear();
      s_file.clear();
      s_status.clear();
      m_doClear = false;
    }
  else
    m_display->fillRect(0, m_display->height()-5, m_display->width(), 5, RGB565_BLACK);

  string image(m_curImageName.empty() ? "<SD>" : m_curImageName.c_str());
  if( image!=s_image )
    {
      m_display->setTextSize(3);
      m_display->setCursor(0,m_display->getTextLineHeight());
      m_display->clearCurrentLine();
      m_display->println(image.c_str());
      s_image = image;
    }

  if( m_curFileName!=s_file )
    {
      m_display->setTextSize(3);
      m_display->setCursor(0, 2*m_display->getTextLineHeight());
      m_display->clearCurrentLine();
      m_display->print(m_curFileName.c_str());
      s_file = m_curFileName;
    }

  string status(statusMessage==NULL ? "" : statusMessage);
  if( status!=s_status )
    {
      m_display->setTextSize(2);
      m_display->setCursor(0, m_display->height()-m_display->getTextLineHeight()-10);
      m_display->clearCurrentLine();
      m_display->print(status.c_str());
      s_status = status;
    }
}
