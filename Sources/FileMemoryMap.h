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

// Uses mapped_file with flags instead of mapped_file_source
// so that the data array can be modified, and the changes won't be saved to the file.
class FileMemoryMap : public boost::noncopyable
{
public:
  // Throws OrthancException if could not read at all
  // If length = 0, the whole file after offset.
  // On overflow of end-of-file pointer, trims. Please call length() to check for this.
  FileMemoryMap(const std::string& location, uintmax_t offset = 0, uintmax_t length = 0);

  char *data();
  // equal to "length" in constructor unless
  // 1) "length" was 0 (constructor deduces length)
  // 2) offset + length is greater than file size
  uintmax_t length();
  ~FileMemoryMap();

private:
  static int alignment;

  char *data_start;
  uintmax_t data_length;

  bool using_mapping;
  boost::iostreams::mapped_file mapped_data;
  std::string non_mapped_data;
};
