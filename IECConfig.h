#ifndef IECCONFIGFILE_H
#define IECCONFIGFILE_H

#include <string>
#include <unordered_map>

typedef std::unordered_map<std::string, std::string> ConfigType;

class IECConfig
{
 public:
  IECConfig();
  
  void begin(const std::string &filename);
  const std::string &getValue(const std::string &key);
  void setValue(const std::string &key, const std::string &value, bool write = true);
  void clear();

 private:
  std::string m_filename;
  ConfigType m_config;

  void read();
  void write();
};

#endif
