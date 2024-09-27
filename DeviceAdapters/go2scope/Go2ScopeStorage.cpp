///////////////////////////////////////////////////////////////////////////////
// FILE:          Go2Scope.cpp
// PROJECT:       Micro-Manager
// SUBSYSTEM:     DeviceAdapters
//-----------------------------------------------------------------------------
// DESCRIPTION:   Go2Scope devices. Includes the experimental StorageDevice
//
// AUTHORS:       Milos Jovanovic <milos@tehnocad.rs>
//						Nenad Amodaj <nenad@go2scope.com>
//
// COPYRIGHT:     Nenad Amodaj, Chan Zuckerberg Initiative, 2024
//
// LICENSE:       This file is distributed under the BSD license.
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
// NOTE:          Storage Device development is supported in part by
//                Chan Zuckerberg Initiative (CZI)
// 
///////////////////////////////////////////////////////////////////////////////
#include "Go2ScopeStorage.h"
#include "BigTiffStorage.h"
#include "AcqZarrStorage.h"

///////////////////////////////////////////////////////////////////////////////
// Exported MMDevice API
///////////////////////////////////////////////////////////////////////////////
MODULE_API void InitializeModuleData()
{
   RegisterDevice(g_BigTiffStorage, MM::StorageDevice, "Storage for BigTIFF format");
   RegisterDevice(g_MMV1Storage, MM::StorageDevice, "Storage for old MM format");
}

MODULE_API MM::Device* CreateDevice(const char* deviceName)
{
   if(deviceName == 0)
      return 0;

   if(strcmp(deviceName, g_MMV1Storage) == 0)
      return new AcqZarrStorage();
   if(strcmp(deviceName, g_BigTiffStorage) == 0)
      return new BigTiffStorage();

   return nullptr;
}

MODULE_API void DeleteDevice(MM::Device* pDevice)
{
   delete pDevice;
}