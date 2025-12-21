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

#ifndef SKPRINTER_H
#define SKPRINTER_H

#include <Arduino.h>
#include <LittleFS.h>
#include <string>
#include "src/IECDevice/IECFileDevice.h"

class SKDisplay;
class SKConfig;

class SKPrinter : public IECDevice
{
 public: 
  SKPrinter(uint8_t devnum, const char *printerFileName);
  virtual ~SKPrinter();

  void setDisplay(SKDisplay *d) { m_display = d; }
  void setConfig(SKConfig *c)   { m_config = c; }

 protected:
  virtual void    begin();
  virtual void    task();
  virtual void    listen(uint8_t secondary);
  virtual int8_t  canWrite();
  virtual void    write(uint8_t data, bool eoi);

 private:
  uint8_t     m_channel, m_prevChannel, m_zeroCount;
  int         m_data;
  std::string m_printerFileName;
  File        m_printerFile;
  uint32_t    m_timeout;
  
  SKConfig   *m_config;
  SKDisplay  *m_display;
};


#endif
