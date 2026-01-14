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

#include "SKConfig.h"
#include <LittleFS.h>
#include <algorithm>

using namespace std;

static string tolower(string s)
{
  transform(s.begin(), s.end(), s.begin(), [](unsigned char c){ return std::tolower(c); });
  return s;
}


SKConfig::SKConfig()
{
}


void SKConfig::begin(const string &filename)
{
  m_filename = filename;
  read();
}


void SKConfig::clear()
{
  LittleFS.remove(m_filename.c_str());
  m_config.clear();
}


const string &SKConfig::getValue(const string &key)
{
  return m_config[tolower(key)];
}


void SKConfig::setValue(const string &key, const string &value, bool writeNow)
{
  m_config[tolower(key)] = value;
  if( writeNow ) write();
}


void SKConfig::read()
{
  bool ok = false;

  m_config.clear();
  File f = LittleFS.open(m_filename.c_str(), "r");
  if( f )
    {
      char *s = (char *) calloc(1, f.size()+1);
      if( s )
        {
          ok = f.read((uint8_t *) s, f.size())==f.size();
          if( ok )
            {
              char *p;
              for(char *line=strtok_r(s, "\n", &p); line!=NULL; line=strtok_r(NULL, "\n", &p))
                {
                  char *eq = strchr(line, '=');
                  if( eq!=NULL )
                    {
                      *eq = 0;
                      m_config[tolower(string(line))]=string(eq+1);
                    }
                }
            }
          free(s);
        }
      
      f.close();
    }

  if( !ok )
    {
      m_config.clear();
      //m_config["device"] = std::to_string(m_devnr);
      write();
    }
}


void SKConfig::write()
{
  File f = LittleFS.open(m_filename.c_str(), "w");
  if( f )
    {
      for(const std::pair<const string, string>&cfg : m_config)
        {
          string line = cfg.first + "=" + cfg.second + "\n";
          f.write((uint8_t *) line.c_str(), line.length());
        }
      f.close();
    }
}
