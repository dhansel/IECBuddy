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

#ifndef SKCONFIG_H
#define SKCONFIG_H

#include <string>
#include <unordered_map>

typedef std::unordered_map<std::string, std::string> ConfigType;

class SKConfig
{
 public:
  SKConfig();
  
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
