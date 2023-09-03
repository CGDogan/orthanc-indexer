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
#include <boost/filesystem.hpp>
#include <string>

#include "../Resources/Orthanc/Plugins/OrthancPluginCppWrapper.h"
#include <SystemToolbox.h>
#include <Logging.h>

int FileMemoryMap::alignment = boost::iostreams::mapped_file::alignment();

// Special cases:
// 1) offset positive, length positive, they overflow (in this case don't throw exception,
// but trim and set length correctly. Hence the user must verify with length()
// that no overflow happened)
// 2) offset zero or positive, length 0 (deduce length)
// 3) file not readable at all: throws OrthancException
FileMemoryMap::FileMemoryMap(const std::string& location, uintmax_t offset, uintmax_t length)
{
  uintmax_t file_size = Orthanc::SystemToolbox::GetFileSize(location);

  // Handle full and partial overflow cases
  if (offset > file_size) {
    offset = file_size;
    length = 0;
  }

  if (length == 0 || offset + length > file_size)
  {
    length = file_size - offset;
  }

  // Handle the empty file case early
  if (length == 0) {
    using_mapping = false;
    data_start = 0;
    data_length = length;
    return;
  }

  boost::iostreams::mapped_file_params params;
  params.path = location.c_str();

  // If changing to use mapped_file_source, remove this line:
  params.flags = boost::iostreams::mapped_file::priv;

  // offset must be a multiple of alignment, so start from the previous page if needed.
  // reserve_for_padding_offset in range [0, alignment)
  int reserve_for_padding_offset = (alignment - (offset % alignment)) % alignment;
  params.offset = offset - reserve_for_padding_offset;
  params.length = length + reserve_for_padding_offset;

  try
  {
    mapped_data.open(params);

    // Success: use Boost mapping
    using_mapping = true;
    data_start = &mapped_data.data()[reserve_for_padding_offset];
    data_length = length;
  }
  catch (const boost::exception &e)
  {
    LOG(INFO) << "Failed mapping file, will read conventionally. Exception: " << boost::diagnostic_information(e);
    using_mapping = false;
    Orthanc::SystemToolbox::ReadFileRange(non_mapped_data, location, offset, offset + length, true);
    data_start = const_cast<char *>(&non_mapped_data[0]);
    data_length = length;
  }
}

char *FileMemoryMap::data()
{
  return data_start;
}

uintmax_t FileMemoryMap::length()
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
