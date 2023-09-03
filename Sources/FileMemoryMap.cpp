/**
 * Indexer plugin for Orthanc
 * Copyright (C) 2023 Sebastien Jodogne, UCLouvain, Belgium
 *
 * This program is free software: you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation, either version 3 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 **/

#include "FileMemoryMap.h"
#include <boost/iostreams/device/mapped_file.hpp>
#include <string>
#include <stdlib.h>

#include "../Resources/Orthanc/Plugins/OrthancPluginCppWrapper.h"

int FileMemoryMap::alignment = boost::iostreams::mapped_file::alignment();

FileMemoryMap::FileMemoryMap(const std::string& location, uintmax_t offset, uintmax_t length)
{
  boost::iostreams::mapped_file_params params;
  params.path = location.c_str();
  params.flags = boost::iostreams::mapped_file::priv;

  // offset must be a multiple of alignment.
  // reserve_for_padding_offset in range [0, aligment)
  reserve_for_padding_offset = (alignment - (offset % aligment)) % alignment;
  params.offset = offset - reserve_for_padding_offset;
  params.length = length + reserve_for_padding_offset;

  try
  {
    mapped_data.open(params);

    // Success: use Boost mapping
    using_mapping = true;
    data_length = mapped_data.size() - reserve_for_padding_offset;
  }
  catch (const boost::exception &e)
  {
    (void)e;
    using_mapping = false;

    Orthanc::SystemToolbox::ReadFile(non_mapped_data, location);

    char *low = &non_mapped_data[0];
    char *high = &non_mapped_data[non_mapped_data.size()];
    if (length != 0)
    {
      if (length > non_mapped_data.size()) {
        // Here, decision is to not throw an exception if offset + length overflows
        // but the reader should use readable_length() to verify
        length = non_mapped_data.size();
      }
      high = &non_mapped_data[length];
    }
    non_mapped_data = std::string(low, high);
    data_length = high - low;
  }
}

char *FileMemoryMap::data()
{
  if (using_mapping)
  {
    return &mapped_data.data()[reserve_for_padding_offset];
  }
  else
  {
    return &non_mapped_data[0];
  }
}

uintmax_t FileMemoryMap::readable_length()
{
  return data_length;
}

FileMemoryMap::~FileMemoryMap()
{
  if (using_mapping)
  {
    mapped_data.close();
  }
  // else: non_mapped_data.clear(); for early resource freeing
}