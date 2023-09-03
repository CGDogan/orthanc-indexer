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

#include <boost/noncopyable.hpp>
#include <boost/iostreams/device/mapped_file.hpp>

// Currently uses "mapped_file_source" which is read-only.
// The array can be modified and changes won't be saved to the file.
class FileMemoryMap : public boost::noncopyable
{
public:
  // Throws OrthancException
  // If length = 0, full file
  FileMemoryMap(const std::string& location, uintmax_t offset = 0, uintmax_t length = 0);

  char *data();
  // equal to "length" in constructor unless
  // 1) "length" was 0 (constructor deduces length)
  // 2) offset + length is greater than file size
  uintmax_t readable_length();
  void ~FileMemoryMap();

private:
  static int aligment;
  int reserve_for_padding_offset;
  uintmax_t length;

  bool using_mapping;
  boost::iostreams::mapped_file_sink mapped_data;
  std::string non_mapped_data;
};
