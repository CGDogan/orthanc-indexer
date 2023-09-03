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
#include <boost/exception/diagnostic_information.hpp>
#include <string>
#include <stdlib.h>

#include "../Resources/Orthanc/Plugins/OrthancPluginCppWrapper.h"
#include <SystemToolbox.h>
#include <Logging.h>

int FileMemoryMap::alignment = boost::iostreams::mapped_file::alignment();

FileMemoryMap::FileMemoryMap(const std::string& location, uintmax_t offset, uintmax_t length)
{
  boost::iostreams::mapped_file_params params;
  params.path = location.c_str();
  // not for source:
  params.flags = boost::iostreams::mapped_file::priv;

  // offset must be a multiple of alignment.
  // reserve_for_padding_offset in range [0, alignment)
  reserve_for_padding_offset = (alignment - (offset % alignment)) % alignment;
  params.offset = offset - reserve_for_padding_offset;
  params.length = length + reserve_for_padding_offset;

  try
  {
    __builtin_printf("trying opening map\n");
    mapped_data.open(params);

    // Success: use Boost mapping
    using_mapping = true;
    data_length = mapped_data.size() - reserve_for_padding_offset;
    __builtin_printf("succeeded opening map\n");
  }
  catch (const boost::exception &e)
  {
    LOG(INFO) << "Failed mapping file. Exception: " << boost::diagnostic_information(e);
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
    data_length = high - low;
    non_mapped_data = std::string(non_mapped_data, data_length);
  }
  __builtin_printf("returning from constructor\n");
}

char *FileMemoryMap::data()
{
  if (using_mapping)
  {
    return const_cast<char*>(&mapped_data.data()[reserve_for_padding_offset]);
  }
  else
  {
    __builtin_printf("accessing  ...\n");

    __builtin_printf("accessing  ... %d\n", non_mapped_data[1]);
    volatile unsigned char a = 0;
    for (int i = 0; i < data_length; i++)
      a += non_mapped_data[i];
       __builtin_printf("accessed\n");

    return const_cast<char*>(&non_mapped_data[0]);
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