///////////////////////////////////////////////////////////////////////////////
// FILE:          Go2Scope.cpp
// PROJECT:       Micro-Manager
// SUBSYSTEM:     DeviceAdapters
//-----------------------------------------------------------------------------
// DESCRIPTION:   Go2Scope devices. Includes the experimental StorageDevice
//
// AUTHOR:        Milos Jovanovic <milos@tehnocad.rs>
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
#include "BigTiffStorage.h"

BigTiffStorage::BigTiffStorage() : initialized(false)
{
   InitializeDefaultErrorMessages();

   // set device specific error messages
   SetErrorText(ERR_INTERNAL, "Internal driver error, see log file for details");

   auto ver = 0; //  Zarr_get_api_version();

   // create pre-initialization properties                                   
   // ------------------------------------
   //

   // Name                                                                   
   CreateProperty(MM::g_Keyword_Name, g_BigTiffStorage, MM::String, true);
   //
   // Description
   std::ostringstream os;
   os << "BigTIFF Storage v" << ver;
   CreateProperty(MM::g_Keyword_Description, os.str().c_str(), MM::String, true);
}

BigTiffStorage::~BigTiffStorage()
{
   Shutdown();
}

void BigTiffStorage::GetName(char* Name) const
{
   CDeviceUtils::CopyLimitedString(Name, g_MMV1Storage);
}

int BigTiffStorage::Initialize()
{
   if(initialized)
      return DEVICE_OK;

   int ret(DEVICE_OK);

   UpdateStatus();

   initialized = true;
   return DEVICE_OK;
}

int BigTiffStorage::Shutdown()
{
   if(initialized)
   {
      initialized = false;
   }
   return DEVICE_OK;
}

// Never busy because all commands block
bool BigTiffStorage::Busy()
{
   return false;
}

int BigTiffStorage::Create(const char* path, const char* name, int numberOfDimensions, const int shape[], const char* meta, char* handle)
{
   return 0;
}

int BigTiffStorage::ConfigureDimension(const char* handle, int dimension, const char* name, const char* meaning)
{
   return 0;
}

int BigTiffStorage::ConfigureCoordinate(const char* handle, int dimension, int coordinate, const char* name)
{
   return 0;
}

int BigTiffStorage::Close(const char* handle)
{
   return 0;
}

int BigTiffStorage::Load(const char* path, const char* name, char* handle)
{
   return 0;
}

int BigTiffStorage::Delete(char* handle)
{
   return 0;
}

int BigTiffStorage::List(const char* path, char** listOfDatasets, int maxItems, int maxItemLength)
{
   return 0;
}

int BigTiffStorage::AddImage(const char* handle, unsigned char* pixels, int width, int height, int depth, int coordinates[], int numCoordinates, const char* imageMeta)
{
   return 0;
}

int BigTiffStorage::GetSummaryMeta(const char* handle, char* meta, int bufSize)
{
   return 0;
}

int BigTiffStorage::GetImageMeta(const char* handle, int coordinates[], int numCoordinates, char* meta, int bufSize)
{
   return 0;
}

const unsigned char* BigTiffStorage::GetImage(const char* handle, int coordinates[], int numCoordinates)
{
   return nullptr;
}

int BigTiffStorage::GetNumberOfDimensions(const char* handle, int& numDimensions)
{
   return 0;
}

int BigTiffStorage::GetDimension(const char* handle, int dimension, char* name, int nameLength, char* meaning, int meaningLength)
{
   return 0;
}

int BigTiffStorage::GetCoordinate(const char* handle, int dimension, int coordinate, char* name, int nameLength)
{
   return 0;
}