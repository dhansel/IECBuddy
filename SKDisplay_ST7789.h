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

#ifndef SKDISPLAY_ST7789
#define SKDISPLAY_ST7789

#include "SKDisplay.h"
#include "Pins.h"

#ifdef PIN_ST7789_SPI
#define SUPPORT_ST7789
#endif

class Arduino_ST7789m;

class SKDisplay_ST7789 : public SKDisplay
{
 public:
  SKDisplay_ST7789();
  virtual ~SKDisplay_ST7789();

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

  virtual void setRotation(uint32_t rotation);
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
