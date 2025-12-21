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

#include "SKPrinter.h"
#include "SKConfig.h"
#include "SKDisplay.h"

#define DEBUG 0

#if DEBUG>0

#define Serial Serial1

static void print_hex(uint8_t data)
{
  static const PROGMEM char hex[16] = {'0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'A', 'B', 'C', 'D', 'E', 'F'};
  Serial.write(pgm_read_byte_near(hex+(data/16)));
  Serial.write(pgm_read_byte_near(hex+(data&15)));
}

#endif


SKPrinter::SKPrinter(uint8_t devnum, const char *printerFileName) : IECDevice(devnum)
{
  m_printerFileName = printerFileName;
}


SKPrinter::~SKPrinter()
{
}


void SKPrinter::begin()
{
#if DEBUG>0
  Serial.begin(115200);
  Serial.print("PRINTER-BEGIN: "); Serial.println(m_printerFileName.c_str());
#endif

  std::string s = m_config->getValue("prdevice");
  int d = std::atoi(s.c_str());

  if( s=="0" )
    setActive(false);
  else if( d>=4 && d<=7 && d!=m_devnr ) 
    setDeviceNumber(d);

  if( m_printerFile ) m_printerFile.close();
  LittleFS.remove(m_printerFileName.c_str());
  m_data = -1;
}


void SKPrinter::listen(uint8_t secondary)
{
  m_channel = secondary & 0x0F;
  m_data = -1;
}


int8_t SKPrinter::canWrite()
{
  return m_data<0 ? 1 : -1;
}


void SKPrinter::write(uint8_t data, bool eoi)
{
  m_data = data;
}


void SKPrinter::task()
{
  char buf[5];

  if( m_data>=0 )
    {
      bool writeHeader = false;
      uint8_t data = m_data;

      m_display->showPrintStatus(true);
      if( !m_printerFile )
        {
          m_printerFile = LittleFS.open(m_printerFileName.c_str(), "a");
          m_zeroCount = 0;
          writeHeader = true;
        }
      else if( m_channel != m_prevChannel )
        {
          // double-zero marks end of data block (channel has changed)
          buf[0]=0; buf[1]=0;
          m_printerFile.write(buf, 2);
          writeHeader = true;
        }

      if( m_printerFile )
        {
          if( writeHeader )
            {
              // write print data block header, including channel number (secondary address)
              sprintf(buf,"PDB%01X", m_channel & 0x0F);
              m_printerFile.write(buf, 4);
              m_prevChannel = m_channel;
            }
          
          if( data==0 )
            {
              // compress 0 sequences by using run-length encoding
              if( m_zeroCount<255 )
                m_zeroCount++;
              else
                {
                  // already have 255 zeros => write it out and start new
                  buf[0] = 0; buf[1] = 255;
                  m_printerFile.write(buf, 2);
                  m_zeroCount = 1;
                }
            }
          else
            {
              // non-zero data byte => if we have zeros to write then do so now
              if( m_zeroCount>0 )
                {
                  buf[0] = 0; buf[1] = m_zeroCount;
                  m_printerFile.write(buf, 2);
                }

              m_zeroCount = 0;
              m_printerFile.write(&data, 1);
            }

          m_timeout = millis() + 500;
        }
      
#if DEBUG>0 && 0
      print_hex(data); Serial.write(' ');
#endif
      m_data = -1;
    }
  else if( m_printerFile && millis()>m_timeout )
    {
      // closing file after one second of inactivity
      // => if we have zeros to write then do so now
      if( m_zeroCount>0 )
        {
          buf[0] = 0; buf[1] = m_zeroCount;
          m_printerFile.write(buf, 2);
        }

      // double-zero marks end of data block
      buf[0]=0; buf[1]=0;
      m_printerFile.write(buf, 2);

      m_printerFile.close();
      m_display->showPrintStatus(false);
    }
  
  IECDevice::task();
}
