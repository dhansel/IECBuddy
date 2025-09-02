#include "IECDisplay_ST7789.h"
#include "Pins.h"

#ifdef SUPPORT_ST7789
#include <Arduino_GFX_Library.h>
#include <LittleFS.h>
#include <algorithm>
#include <cctype>

#define DRIVE_IMAGE         "$DRIVE_240X140.BIN$"
#define DEFAULT_DISK_IMAGE  "$DISK_140X140.BIN$"

#define IMAGE_REGION_X      0
#define IMAGE_REGION_Y      65
#define IMAGE_REGION_WIDTH  240
#define IMAGE_REGION_HEIGHT 145

using namespace std;

// derive our own ST7789 class so we can access parameters that are 
// not available otherwise (e.g. the SPI mode)
class Arduino_ST7789m : public Arduino_ST7789
{
public:
  Arduino_ST7789m(Arduino_DataBus *bus, int8_t rst = GFX_NOT_DEFINED) : 
    Arduino_ST7789(bus, rst, 0, false, 240, 240) { m_row = IMAGE_REGION_HEIGHT; }

  virtual ~Arduino_ST7789m()
    {}

  bool begin(int32_t speed, int8_t spiMode)
    { 
      _override_datamode = spiMode;
      if( Arduino_TFT::begin(speed) )
        { fillScreen(RGB565_BLACK); return true; }
      else
        return false;
    }

  void clearDisplay()
    {
      if( (IMAGE_REGION_Y)>0 )
        fillRect(0, 0, width(), IMAGE_REGION_Y, RGB565_BLACK);
      if( (IMAGE_REGION_Y)+(IMAGE_REGION_HEIGHT) < height() )
        fillRect(0, IMAGE_REGION_Y+IMAGE_REGION_HEIGHT, width(), height()-(IMAGE_REGION_Y)-(IMAGE_REGION_HEIGHT), RGB565_BLACK);
      if( (IMAGE_REGION_X)>0 )
        fillRect(0, 0, IMAGE_REGION_X, height(), RGB565_BLACK);
      if( (IMAGE_REGION_X)+(IMAGE_REGION_WIDTH) < width() )
        fillRect(IMAGE_REGION_X+IMAGE_REGION_WIDTH, 0, width()-(IMAGE_REGION_X)-(IMAGE_REGION_WIDTH), height(), RGB565_BLACK);
    }

  void clearCurrentLine()
    { 
      fillRect(0, cursor_y, width(), getTextLineHeight(), RGB565_BLACK);
    }

  int getTextLineHeight()
    { return (gfxFont==NULL ? 8 : gfxFont->yAdvance) * textsize_y; }

  uint32_t startImage(uint16_t x, uint16_t y, uint16_t w, uint16_t h)
    {
      uint32_t dw = IMAGE_REGION_WIDTH, dh = IMAGE_REGION_HEIGHT;

      // center image
      if( x==0xFFFF ) x = w < dw ? (dw-w)/2 : 0;
      if( y==0xFFFF ) y = h < dh ? (dh-h)/2 : 0;

      // cannot currently crop horizontally
      if( x+w > dw ) return 0;

      // if image does not fill whole region then clear first
      if( x>0 || w<dw || y>0 || h<dh )
        fillRect(IMAGE_REGION_X, IMAGE_REGION_Y, IMAGE_REGION_WIDTH, IMAGE_REGION_HEIGHT, RGB565_BLACK);

      m_width  = w;
      m_height = min(h, dh);
      m_col    = x;
      m_row    = y;
      return 2; // bytes per pixel
    }

  uint32_t addImageData(uint16_t *data, uint32_t npixels)
    {
      uint32_t nrows = min(npixels / m_width, m_height-m_row);
      if( m_row == m_height )
        return npixels; // we already have all the data we need
      else if( nrows>0 )
        {
          startWrite();
          writeAddrWindow((IMAGE_REGION_X)+m_col, (IMAGE_REGION_Y)+m_row, m_width, nrows);
          _bus->writePixels(data, nrows*m_width);
          m_row += nrows;
          endWrite();
          return nrows * m_width;
        }
      else
        return 0;
    }

  void endImage()
    {}

private:
  uint32_t  m_row, m_col, m_width, m_height;
};


IECDisplay_ST7789::IECDisplay_ST7789()
{
  Arduino_DataBus *bus = new Arduino_HWSPI(PIN_ST7789_DC, GFX_NOT_DEFINED, &PIN_ST7789_SPI);
  gpio_set_function(PIN_ST7789_SPI_SCL,  GPIO_FUNC_SPI);
  gpio_set_function(PIN_ST7789_SPI_COPI, GPIO_FUNC_SPI);

  m_display = new Arduino_ST7789m(bus, PIN_ST7789_RES);
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
  showImage(DRIVE_IMAGE);
  m_doClear = false;
}


void IECDisplay_ST7789::setCurrentImageName(std::string iname)
{
  IECDisplay::setCurrentImageName(iname);

  bool found = false;

  size_t dot = iname.rfind('.');
  if( dot!=string::npos )
    {
      string basename = iname.substr(0,dot);
      if( !showImage("$"+basename+".GIF$") )
        {
          Dir dir = LittleFS.openDir("/");
          while( !found && dir.next() )
            {
              string name(dir.fileName().c_str());
              if( name.front()=='$' && name.substr(name.length()-5)==".BIN$" )
                {
                  size_t dash = name.rfind('_');
                  if( name.substr(1, dash-1)==basename )
                    {
                      showImage(name);
                      found = true;
                    }
                }
            }
        }
    }

  if( !found ) showImage(iname.empty() ? DRIVE_IMAGE : DEFAULT_DISK_IMAGE);
}


void IECDisplay_ST7789::showMessage(std::string msg)
{
  m_display->setTextSize(3);
  m_display->setCursor(0, 0);
  m_display->println();
  m_display->clearCurrentLine();
  m_display->print(msg.c_str());
  m_doClear = true;
}


void IECDisplay_ST7789::showTransmitMessage(std::string msg, std::string fileName)
{
  m_display->clearDisplay();
  m_display->setCursor(0,0);
  m_display->setTextSize(3);
  m_display->println(msg.c_str());
  m_display->print(fileName.c_str());
  m_doClear = true;
}


void IECDisplay_ST7789::startProgress(int nbytestotal)
{
  IECDisplay::startProgress(nbytestotal);
}


void IECDisplay_ST7789::updateProgress(int nbytes)
{
  m_curFileBytesRead += nbytes;

  if( m_curFileSize>0 )
    {
      int w = (m_display->width() * m_curFileBytesRead) / m_curFileSize;
      if( (w-m_progressWidth)>0 )
        {
          m_display->fillRect(m_progressWidth, m_display->height()-5, w-m_progressWidth, 5, RGB565_WHITE);
          m_progressWidth = w;
        }
    }
  else
    {
      const int markerSize = 16;
      int width = m_display->width()-markerSize;
      int np = ((width*2) * (m_curFileBytesRead & 4095)) / 4096;
      int pp = m_progressWidth;
      if( np != pp )
        {
          m_progressWidth = np;
          if( np >= width ) np = width*2-1-np;
          if( pp >= width ) pp = width*2-1-pp;

          // draw marker
          m_display->fillRect(np, m_display->height()-5, markerSize, 5, RGB565_WHITE);

          // erase pixels before/after marker
          if( np>pp )
            m_display->fillRect(pp, m_display->height()-5, np-pp, 5, RGB565_BLACK);
          else if( np<pp )
            m_display->fillRect(np+markerSize, m_display->height()-5, pp-np, 5, RGB565_BLACK);
        }
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
      m_display->setTextColor(RGB565_YELLOW);
      m_display->setCursor(0,0);
      m_display->clearCurrentLine();
      m_display->println(image.c_str());
      s_image = image;
    }

  if( m_curFileName!=s_file )
    {
      m_display->setTextSize(3);
      m_display->setTextColor(RGB565_WHITE);
      m_display->setCursor(0, m_display->getTextLineHeight());
      m_display->clearCurrentLine();
      m_display->print(m_curFileName.c_str());
      s_file = m_curFileName;
    }

  string status(statusMessage==NULL ? "" : statusMessage);
  if( status!=s_status )
    {
      int color = RGB565_WHITE;
      if( isdigit(statusMessage[0]) && isdigit(statusMessage[1]) )
        {
          int code = (statusMessage[0]-'0')*10 + (statusMessage[1]-'0');
          color = code < 20 ? RGB565_GREEN : RGB565_RED;
        }

      m_display->setTextSize(2);
      m_display->setTextColor(color);
      m_display->setCursor(0, m_display->height()-m_display->getTextLineHeight()-10);
      m_display->clearCurrentLine();
      m_display->print(status.c_str());
      s_status = status;
    }
}


void IECDisplay_ST7789::showPrintStatus(bool printing)
{
  static int spin = 0;
  static unsigned long spinto = 0;
  const char spinchr[] = "|/-\\";

  int w = 25;
  int x = m_display->width()-w;

  if( printing )
    {
      if( millis()>spinto )
        {
          m_display->fillRect(x, 0, x+w, m_display->getTextLineHeight(), RGB565_BLACK);
          m_display->setTextSize(3);
          m_display->setTextColor(RGB565_YELLOW);
          m_display->setCursor(x, 0);
          m_display->write(spinchr[spin]);
          spin   = (spin+1) & 3;
          spinto = millis()+250;
        }
    }
  else
    {
      m_display->fillRect(x, 0, x+w, m_display->getTextLineHeight(), RGB565_BLACK);
      spinto = 0;
      spin   = 0;
    }
}


uint32_t IECDisplay_ST7789::startImage(uint16_t x, uint16_t y, uint16_t w, uint16_t h)
{
  return m_display->startImage(x, y, w, h);
}


uint32_t IECDisplay_ST7789::addImageData(uint8_t *data, uint32_t dataSize)
{
  return m_display->addImageData((uint16_t *) data, dataSize/2) * 2;
}


void IECDisplay_ST7789::endImage()
{
  m_display->endImage();
}


bool IECDisplay_ST7789::showImage(const string &filename)
{
  bool res = false;

  string s(filename);
  std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c){return std::tolower(c);});

  if( (s.front()=='$' && s.substr(s.length()-5)==".bin$") || s.substr(s.length()-4)==".bin" )
    {
      int w = 240, h = 240;
      size_t i = s.rfind('_');
      if( i!=string::npos )
        sscanf(s.substr(i).c_str(), "_%ix%i", &w, &h);

      File f = LittleFS.open(filename.c_str(), "r");
      if( f )
        {
          uint32_t n = startImage(0xFFFF, 0xFFFF, w, h);
          if( n>0 )
            {
              uint8_t *buffer = new uint8_t[w*20*2];
              while( (n=f.read(buffer, w*20*2))>0 ) addImageData(buffer, n);
              endImage();
              delete [] buffer;
              res = true;
            }
        }
    }
  
  return res;
}

#endif
