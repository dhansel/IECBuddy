#include "IECDisplay_SSD1306.h"

#ifdef SUPPORT_SSD1306

#include <Wire.h>
#include <Adafruit_SSD1306.h>
#include <Adafruit_GFX.h>

using namespace std;

#define DISPLAY_SSD1306_ADDR     0x3C
#define DISPLAY_SSD1306_PIN_SDA  20
#define DISPLAY_SSD1306_PIN_SCL  21



IECDisplay_SSD1306::IECDisplay_SSD1306() :
  IECDisplay()
{
  m_display = new Adafruit_SSD1306(128, 64, &Wire, -1);
}


IECDisplay_SSD1306::~IECDisplay_SSD1306()
{
  delete m_display;
}


void IECDisplay_SSD1306::begin()
{
  IECDisplay::begin();

  Wire.setSDA(DISPLAY_SSD1306_PIN_SDA);
  Wire.setSCL(DISPLAY_SSD1306_PIN_SCL);

  m_display->begin(SSD1306_SWITCHCAPVCC, DISPLAY_SSD1306_ADDR);
  m_display->cp437(true);
  m_display->setTextColor(SSD1306_WHITE);
  m_display->clearDisplay();
  m_display->display();
}


void IECDisplay_SSD1306::showMessage(string msg)
{
  m_display->setCursor(0, 56);
  m_display->setTextSize(1);
  m_display->print(msg.c_str());
  m_display->display();
}


void IECDisplay_SSD1306::showTransmitMessage(string msg, string fileName)
{
  m_display->clearDisplay();
  m_display->setCursor(0,0);
  m_display->setTextSize(2);
  m_display->println(msg.c_str());
  m_display->setTextSize(1);
  m_display->println();
  m_display->print(fileName.c_str());
  m_display->display();
}



void IECDisplay_SSD1306::startProgress(int nbytestotal)
{
  IECDisplay::startProgress(nbytestotal);

  if( m_curFileSize>0 )
    {
      // prepare OLED for displaying progress bar on bottom row
      m_display->ssd1306_command(0xB7); // bottom row
      m_display->ssd1306_command(0x00); // first column (low nybble)
      m_display->ssd1306_command(0x10); // first column (high nybble)
    }
}


void IECDisplay_SSD1306::updateProgress(int nbytes)
{
  m_curFileBytesRead += nbytes;

  if( m_curFileSize>0 )
    {
      int w = (m_display->width() * m_curFileBytesRead) / m_curFileSize;
      if( w>m_progressWidth )
        {
          // Drawing a progress bar via the SSD1306 library is WAY too slow,
          // severely impacting IEC transmission speed. The cause mostly is
          // that m_display->display() must be called after drawing which re-transmits
          // the entire display contents. Instead we placed the "cursor" at the
          // lower-left of the display during "open" and now are just sending 0xF0
          // data, each of which enables the bottom 4 pixels of the next column.
          Wire.beginTransmission(DISPLAY_SSD1306_ADDR);
          Wire.write(0x40); // "write data"
          while( m_progressWidth < w )
            {
              Wire.write(0xF0); // one column with bottom 4 pixels set
              m_progressWidth++;
            }
          Wire.endTransmission();
        }
    }
  else
    {
      // display is 128 pixels wide, marker is 8 pixels wide
      // => possible marker locations: 0..119
      // marker also moves backwards
      // => full marker cycle: 0..239
      // one full cycle represents 4800 bytes
      // => one pixel represents 20 bytes
      int np = (m_curFileBytesRead % 4800) / 20;
      int pp = m_progressWidth;
      if( np != pp )
        {
          m_progressWidth = np;
          if( np >= 120 ) np = 239-np;
          if( pp >= 120 ) pp = 239-pp;

          int c = pp < np ? pp : np;
          m_display->ssd1306_command(0xB7);          // bottom row
          m_display->ssd1306_command(c & 0x0F);      // column low nybble
          m_display->ssd1306_command(0x10 | (c/16)); // column high nybble

          Wire.beginTransmission(DISPLAY_SSD1306_ADDR);
          Wire.write(0x40); // "write data"

          // clear pixels before marker (if any)
          while( c<np ) { Wire.write(0x00); c++; }
          
          // draw marker
          for(int i=0; i<8; i++) Wire.write(0xF0);

          // clear pixels after marker (if any)
          pp = min(pp, 120);
          while( c<pp ) { Wire.write(0x00); c++; }

          Wire.endTransmission();
        }
    }
}


void IECDisplay_SSD1306::update(const char *statusMessage)
{
  m_display->clearDisplay();
  m_display->setTextSize(2);
  m_display->setCursor(0,0);
  m_display->println(m_curImageName.empty() ? "<SD>" : m_curImageName.c_str());

  if( !m_curFileName.empty() )
    {
      if( m_curFileName.size()>10 )
        {
          m_display->setTextSize(1);
          m_display->println();
          m_display->println(m_curFileName.substr(0, 21).c_str());
        }
      else
        m_display->println(m_curFileName.substr(0, 10).c_str());
    }

  if( isErrorStatus(statusMessage) )
    {
      m_display->setTextSize(1);
      m_display->setCursor(0, 56);
      m_display->print(statusMessage);
    }

  m_display->display();

  if( m_curFileSize>0 )
    {
      // prepare OLED for displaying progress bar on bottom row
      m_display->ssd1306_command(0xB7); // bottom row
      m_display->ssd1306_command(0x00); // first column (low nybble)
      m_display->ssd1306_command(0x10); // first column (high nybble)
      m_progressWidth = 0;
    }
}

#endif
