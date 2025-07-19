#include "IECConfig.h"
#include <LittleFS.h>
#include <algorithm>

using namespace std;

static string tolower(string s)
{
  transform(s.begin(), s.end(), s.begin(), [](unsigned char c){ return std::tolower(c); });
  return s;
}


IECConfig::IECConfig()
{
}


void IECConfig::begin(const string &filename)
{
  m_filename = filename;
  read();
}


void IECConfig::clear()
{
  LittleFS.remove(m_filename.c_str());
  m_config.clear();
}


const string &IECConfig::getValue(const string &key)
{
  return m_config[tolower(key)];
}


void IECConfig::setValue(const string &key, const string &value, bool writeNow)
{
  m_config[tolower(key)] = value;
  if( writeNow ) write();
}


void IECConfig::read()
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


void IECConfig::write()
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
