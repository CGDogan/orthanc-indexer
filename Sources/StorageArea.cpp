/**
 * Indexer plugin for Orthanc
 * Copyright (C) 2021 Sebastien Jodogne, UCLouvain, Belgium
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


#include "StorageArea.h"
#include "FileMemoryMap.h"

#include "../Resources/Orthanc/Plugins/OrthancPluginCppWrapper.h"

#include <Toolbox.h>
#include <SystemToolbox.h>
#include <OrthancException.h>

#include <boost/filesystem.hpp>


static boost::filesystem::path GetPathInternal(const std::string& root,
                                               const std::string& uuid)
{
  if (!Orthanc::Toolbox::IsUuid(uuid))
  {
    throw Orthanc::OrthancException(Orthanc::ErrorCode_ParameterOutOfRange);
  }
  else
  {
    assert(!root.empty());
      
    boost::filesystem::path path = root;
    path /= std::string(&uuid[0], &uuid[2]);
    path /= std::string(&uuid[2], &uuid[4]);
    path /= uuid;

    path.make_preferred();
    return path;
  }
}


static void CreateOrthancBuffer(OrthancPluginMemoryBuffer64 *target,
                                const char *data,
                                uintmax_t length)
{
  OrthancPluginErrorCode code = OrthancPluginCreateMemoryBuffer64(
    OrthancPlugins::GetGlobalContext(), target, length);

  if (code == OrthancPluginErrorCode_Success)
  {
    assert(length == target->size);

    if (length != 0)
    {
      memcpy(target->data, data, length);
    }
  }
  else
  {
    throw Orthanc::OrthancException(static_cast<Orthanc::ErrorCode>(code));
  }
}


void StorageArea::ReadWholeFromPath(OrthancPluginMemoryBuffer64 *target,
                                    const std::string& path)
{
  FileMemoryMap reader = FileMemoryMap(path);
  CreateOrthancBuffer(target, reader.data(), reader.length());
}   
  

void StorageArea::ReadRangeFromPath(OrthancPluginMemoryBuffer64 *target,
                                    const std::string& path,
                                    uint64_t rangeStart)
{
  FileMemoryMap reader = FileMemoryMap(path, rangeStart, target->size);
  if (reader.length() != target->size)
  {
    throw Orthanc::OrthancException(Orthanc::ErrorCode_CorruptedFile);
  }
  else if (reader.length() != 0)
  {
    memcpy(target->data, reader.data(), reader.length());
  }
}


StorageArea::StorageArea(const std::string& root) :
  root_(root)
{
  if (root_.empty())
  {
    throw Orthanc::OrthancException(Orthanc::ErrorCode_ParameterOutOfRange);
  }
}
  
  
void StorageArea::Create(const std::string& uuid,
                         const void *content,
                         int64_t size)
{
  boost::filesystem::path path = GetPathInternal(root_, uuid);
__builtin_printf("storagearea::create %s\n", path.string().c_str());
  if (boost::filesystem::exists(path.parent_path()))
  {
    if (!boost::filesystem::is_directory(path.parent_path()))
    {
      throw Orthanc::OrthancException(Orthanc::ErrorCode_DirectoryOverFile);
    }
  }
  else
  {
    if (!boost::filesystem::create_directories(path.parent_path()))
    {
      throw Orthanc::OrthancException(Orthanc::ErrorCode_FileStorageCannotWrite);
    }
  }
      
  Orthanc::SystemToolbox::WriteFile(content, size, path.string(), false);
}
  

void StorageArea::ReadWhole(OrthancPluginMemoryBuffer64 *target,
                            const std::string& uuid)
{
  ReadWholeFromPath(target, GetPath(uuid));
}
  

void StorageArea::ReadRange(OrthancPluginMemoryBuffer64 *target,
                            const std::string& uuid,
                            uint64_t rangeStart)
{
  ReadRangeFromPath(target, GetPath(uuid), rangeStart);
}
  

void StorageArea::RemoveAttachment(const std::string& uuid)
{
  boost::filesystem::path path = GetPathInternal(root_, uuid);
      __builtin_printf("storagearea::removeattachment %s\n", path.string().c_str());

  try
  {
    boost::system::error_code err;
    boost::filesystem::remove(path, err);
    boost::filesystem::remove(path.parent_path(), err);
    boost::filesystem::remove(path.parent_path().parent_path(), err);
  }
  catch (...)
  {
    // Ignore the error
  }
}


std::string StorageArea::GetPath(const std::string& uuid) const
{
  return GetPathInternal(root_, uuid).string();
}
