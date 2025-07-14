#include "IECDisplay.h"

#include <Wire.h>
#include <Adafruit_GFX.h>

using namespace std;


IECDisplay::IECDisplay()
{
  m_curFileSize = 0;
  m_curFileBytesRead = 0;
  m_curFileName = string();
}


IECDisplay::~IECDisplay()
{
}


void IECDisplay::begin()
{
}


void IECDisplay::setCurrentImageName(string iname)
{
  m_curImageName = iname;
}


void IECDisplay::setCurrentFileName(string fname)
{
  m_curFileName = fname;
}


void IECDisplay::startProgress(int nbytestotal)
{
  m_progressWidth = 0;
  m_curFileBytesRead = 0;
  m_curFileSize = nbytestotal;
}


void IECDisplay::updateProgress(int nbytes) 
{
}


void IECDisplay::showMessage(string msg) 
{
}


void IECDisplay::showTransmitMessage(string msg, string fileName) 
{
}


bool IECDisplay::isErrorStatus(const char *statusMessage)
{
  bool res = false;

  if( statusMessage!=NULL && isdigit(statusMessage[0]) && isdigit(statusMessage[1]) && statusMessage[2]==',' )
    {
      int n = (statusMessage[0]-'0')*10+(statusMessage[1]-'0');
      if( n>=20 && n!=73 ) res = true;
    }
  
  return res;
}


void IECDisplay::showPrintStatus(bool printing)
{
}


void IECDisplay::update(const char *statusMessage) 
{
}


IECDisplay *IECDisplay::Create(string displayType)
{
  IECDisplay *display;

  if( displayType=="NONE" )
    display = new IECDisplay(); // no display
#if defined(SUPPORT_SSD1306)
  else if( displayType=="SSD1306" )
    display = new IECDisplay_SSD1306();
#endif
#if defined(SUPPORT_ST7789)
  else if( displayType=="ST7789" )
    display = new IECDisplay_ST7789();
#endif
  else
    display = new IECDisplay(); // no display

  return display;
}
