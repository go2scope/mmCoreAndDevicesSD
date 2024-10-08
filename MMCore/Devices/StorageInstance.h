// PROJECT:       Micro-Manager
// SUBSYSTEM:     MMCore
//
// COPYRIGHT:     University of California, San Francisco, 2014,
//                Nenad Amodaj 2024
//                All Rights reserved
//
// LICENSE:       This file is distributed under the "Lesser GPL" (LGPL) license.
//                License text is included with the source distribution.
//
//                This file is distributed in the hope that it will be useful,
//                but WITHOUT ANY WARRANTY; without even the implied warranty
//                of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
//
//                IN NO EVENT SHALL THE COPYRIGHT OWNER OR
//                CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
//                INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES.
//
// AUTHOR:        Nenad Amodaj

#pragma once

#include "DeviceInstanceBase.h"


class StorageInstance : public DeviceInstanceBase<MM::Storage>
{
public:
   StorageInstance(CMMCore* core,
      std::shared_ptr<LoadedDeviceAdapter> adapter,
      const std::string& name,
      MM::Device* pDevice,
      DeleteDeviceFunction deleteFunction,
      const std::string& label,
      mm::logging::Logger deviceLogger,
      mm::logging::Logger coreLogger) :
      DeviceInstanceBase<MM::Storage>(core, adapter, name, pDevice, deleteFunction, label, deviceLogger, coreLogger) {}

   int Create(const char* path, const char* name, const std::vector<int>& shape, const char* meta, std::string& handle);
   int ConfigureDimension(const char* handle, int dimension, const char* name, const char* meaning);
   int ConfigureCoordinate(const char* handle, int dimension, int coordinate, const char* name);
   int Close(const char* handle);
   int Load(const char* path, const char* name, std::string& handle);
   int Delete(char* handle);
   int List(const char* path, std::vector<std::string>& datasets);
   int AddImage(const char* handle, unsigned char* pixels, int width, int height, int depth, std::vector<int>& coordinates, const char* imageMeta);
   int GetSummaryMeta(const char* handle, std::string& meta);
   int GetImageMeta(const char* handle, const std::vector<int>& coordinates, std::string& meta);
   const unsigned char* GetImage(const char* handle, const std::vector<int>& coordinates);
   int GetNumberOfDimensions(const char* handle, int& numDimensions);
   int GetDimension(const char* handle, int dimension, std::string& name, std::string& meaning);
   int GetCoordinate(const char* handle, int dimension, int coordinate, std::string& name);

};
