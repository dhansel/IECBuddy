// -----------------------------------------------------------------------------
// Copyright (C) 2025 David Hansel
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have receikved a copy of the GNU General Public License
// along with this program; if not, write to the Free Software Foundation,
// Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301  USA
// -----------------------------------------------------------------------------

#include "SKDisplay.h"

#include <Wire.h>
#include <Adafruit_GFX.h>

using namespace std;


SKDisplay::SKDisplay()
{
  m_curFileSize = 0;
  m_curFileBytesRead = 0;
}


SKDisplay::~SKDisplay()
{
}


void SKDisplay::begin(uint32_t rotation)
{
}


void SKDisplay::setCurrentImageName(string iname)
{
  m_curImageName = iname;
}


void SKDisplay::setCurrentFileName(string fname)
{
  m_curFileName = fname;
}


void SKDisplay::setStatusMessage(string msg)
{
  m_statusMessage = msg;
}


void SKDisplay::startProgress(int nbytestotal)
{
  m_progressWidth = 0;
  m_curFileBytesRead = 0;
  m_curFileSize = nbytestotal;
}


void SKDisplay::updateProgress(int nbytes) 
{
}


void SKDisplay::endProgress() 
{
}


void SKDisplay::showMessage(string msg) 
{
}


void SKDisplay::showTransmitMessage(string msg, string fileName) 
{
}


bool SKDisplay::isErrorStatus(const char *statusMessage)
{
  bool res = false;

  if( statusMessage!=NULL && isdigit(statusMessage[0]) && isdigit(statusMessage[1]) && statusMessage[2]==',' )
    {
      int n = (statusMessage[0]-'0')*10+(statusMessage[1]-'0');
      if( n>=20 && n!=73 ) res = true;
    }
  
  return res;
}


void SKDisplay::showPrintStatus(bool printing)
{
}


void SKDisplay::redraw()
{
}


SKDisplay *SKDisplay::Create(string displayType)
{
  SKDisplay *display;

  if( displayType=="NONE" )
    display = new SKDisplay(); // no display
#if defined(SUPPORT_ST7789)
  else if( displayType=="ST7789" )
    display = new SKDisplay_ST7789();
#endif
  else
    display = new SKDisplay(); // no display

  return display;
}
