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
#include <filesystem>
#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_generators.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <boost/lexical_cast.hpp>
#include "BigTiffStorage.h"
#include "tiff.h"
#include "tiffio.h"

/**
 * Default class constructor
 */
BigTiffStorage::BigTiffStorage() : initialized(false)
{
   InitializeDefaultErrorMessages();

   // set device specific error messages
   SetErrorText(ERR_INTERNAL, "Internal driver error, see log file for details");

   // create pre-initialization properties                                   
   // ------------------------------------
   //
   
   // Name                                                                   
   CreateProperty(MM::g_Keyword_Name, g_BigTiffStorage, MM::String, true);
   //
   // Description
   std::ostringstream os;
   os << "BigTIFF Storage v" << TIFFGetVersion();
   CreateProperty(MM::g_Keyword_Description, os.str().c_str(), MM::String, true);
}

/**
 * Get device name
 * @param Name String buffer [out]
 */
void BigTiffStorage::GetName(char* Name) const
{
   CDeviceUtils::CopyLimitedString(Name, g_MMV1Storage);
}

/**
 * Device driver initialization routine
 */
int BigTiffStorage::Initialize()
{
   if(initialized)
      return DEVICE_OK;

   int ret(DEVICE_OK);

   UpdateStatus();

   initialized = true;
   return DEVICE_OK;
}

/**
 * Device driver shutdown routine
 * During device shutdown cache will be emptied, 
 * and all open file handles will be closed
 */
int BigTiffStorage::Shutdown()
{
   initialized = false;
   for(auto it = cache.begin(); it != cache.end(); it++)
   {
      if(it->second.isOpen())
         TIFFClose((TIFF*)it->second.FileHandle);
      it->second.FileHandle = nullptr;
   }
   cache.clear();
   return DEVICE_OK;
}

/**
 * Create storage entry
 * Dataset storage descriptor will open a file handle, to close a file handle call Close()
 * Dataset storage descriptor will reside in device driver cache
 * If the file already exists this method will fail with 'DEVICE_DUPLICATE_PROPERTY' status code
 * @param path Absolute file path (TIFF file)
 * @param name Dataset name
 * @param numberOfDimensions Number of dimensions
 * @param shape Axis sizes
 * @param meta Metadata
 * @param handle Entry GUID [out]
 * @return Status code
 */
int BigTiffStorage::Create(const char* path, const char* name, int numberOfDimensions, const int shape[], const char* meta, char* handle)
{
   if(path == nullptr || numberOfDimensions <= 0)
      return DEVICE_INVALID_INPUT_PARAM;

   // Check cache size limits
   if(cache.size() >= MAX_CACHE_SIZE)
   {
      cacheReduce();
      if(CACHE_HARD_LIMIT && cache.size() >= MAX_CACHE_SIZE)
         return DEVICE_OUT_OF_MEMORY;
   }

   // Check if the file already exists
   if(std::filesystem::exists(std::filesystem::u8path(path)))
      return DEVICE_DUPLICATE_PROPERTY;
   
   // Create a file on disk and store the file handle
   TIFF* fhandle = TIFFOpen(path, "w");
   if(fhandle == nullptr)
      return DEVICE_OUT_OF_MEMORY;
   
   // Create dataset storage descriptor
   std::string guid = boost::lexical_cast<std::string>(boost::uuids::random_generator()());           // Entry UUID
   G2SStorageEntry sdesc(path, name, numberOfDimensions, shape, meta);
   sdesc.FileHandle = fhandle;

   // Append dataset storage descriptor to cache
   auto it = cache.insert(std::make_pair(guid, sdesc));
   if(!it.second)
      return DEVICE_OUT_OF_MEMORY;
   if(guid.size() > MM::MaxStrLength)
      return DEVICE_INVALID_PROPERTY_LIMTS;

   // Copy UUID string to the GUID buffer
   guid.copy(handle, guid.size());
   return DEVICE_OK;
}

/**
 * Load dataset from disk
 * Dataset storage descriptor will be read from file
 * Dataset storage descriptor will open a file handle, to close a file handle call Close()
 * Dataset storage descriptor will reside in device driver cache
 * @param path Absolute file path (TIFF file)
 * @param name Dataset name
 * @param handle Entry GUID [out]
 * @return Status code
 */
int BigTiffStorage::Load(const char* path, const char* name, char* handle)
{
   if(path == nullptr)
      return DEVICE_INVALID_INPUT_PARAM;

   // Check if the file exists
   if(!std::filesystem::exists(std::filesystem::u8path(path)))
      return DEVICE_INVALID_INPUT_PARAM;

   // Open a file on disk and store the file handle
   TIFF* fhandle = TIFFOpen(path, "r+");
   if(fhandle == nullptr)
      return DEVICE_OUT_OF_MEMORY;
   
   // Determine dataset format
   int ndim = 0;
   std::vector<int> shape;
   std::string meta = "";
   while(TIFFReadDirectory(fhandle))
   {
      // TODO:
   }


   // Create dataset storage descriptor
   std::string guid = boost::lexical_cast<std::string>(boost::uuids::random_generator()());
   G2SStorageEntry sdesc(path, name, ndim, &shape[0], meta.c_str());
   sdesc.FileHandle = fhandle;

   // Append dataset storage descriptor to cache
   auto it = cache.insert(std::make_pair(guid, sdesc));
   if(!it.second)
      return DEVICE_OUT_OF_MEMORY;
   if(guid.size() > MM::MaxStrLength)
      return DEVICE_INVALID_PROPERTY_LIMTS;

   // Copy UUID string to the GUID buffer
   guid.copy(handle, guid.size());
   return DEVICE_OK;
}

/**
 * Close the dataset
 * File handle will be closed
 * Metadata will be discarded
 * Storage entry descriptor will remain in cache
 * @param handle Entry GUID
 * @return Status code
 */
int BigTiffStorage::Close(const char* handle)
{
   auto it = cache.find(handle);
   if(it == cache.end())
      return DEVICE_INVALID_INPUT_PARAM;
   if(it->second.isOpen())
   {
      TIFFClose((TIFF*)it->second.FileHandle);
      it->second.FileHandle = nullptr;
   }
   it->second.Metadata.clear();
   return DEVICE_OK;
}

/**
 * Delete existing dataset (file on disk)
 * If the file doesn't exist this method will return 'DEVICE_NO_PROPERTY_DATA' status code
 * Dataset storage descriptor will be removed from cache
 * @param handle Entry GUID
 * @return Status code
 */
int BigTiffStorage::Delete(char* handle)
{
   if(handle == nullptr)
      return DEVICE_INVALID_INPUT_PARAM;
   auto it = cache.find(handle);
   if(it == cache.end())
      return DEVICE_INVALID_INPUT_PARAM;

   // Check if the file exists
   auto fp = std::filesystem::u8path(it->second.Path);
   if(!std::filesystem::exists(fp))
      return DEVICE_NO_PROPERTY_DATA;

   // Close the file handle
   if(it->second.isOpen())
   {
      TIFFClose((TIFF*)it->second.FileHandle);
      it->second.FileHandle = nullptr;
   }
   
   // Delete the file
   if(!std::filesystem::remove(fp))
      return DEVICE_ERR;

   // Discard the cache entry
   cache.erase(it);
   return DEVICE_OK;
}

/**
 * List datasets in the specified folder / path
 * @param path Folder path
 * @param listOfDatasets Dataset path list [out]
 * @param maxItems Max dataset count
 * @param maxItemLength Max dataset path length
 * @return Status code
 */
int BigTiffStorage::List(const char* path, char** listOfDatasets, int maxItems, int maxItemLength)
{
   if(path == nullptr || listOfDatasets == nullptr || maxItems <= 0 || maxItemLength <= 0)
      return DEVICE_INVALID_INPUT_PARAM;

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


/**
 * Configure metadata for a given dimension
 * @param handle Entry GUID
 * @param dimension Dimension index
 * @param name Name of the dimension
 * @param meaning Z,T,C, etc. (physical meaning)
 * @return Status code
 */
int BigTiffStorage::ConfigureDimension(const char* handle, int dimension, const char* name, const char* meaning)
{
   if(handle == nullptr || dimension < 0)
      return DEVICE_INVALID_INPUT_PARAM;
   auto it = cache.find(handle);
   if(it == cache.end())
      return DEVICE_INVALID_INPUT_PARAM;
   if((std::size_t)dimension >= it->second.getDimSize())
      return DEVICE_INVALID_INPUT_PARAM;
   it->second.Dimensions[dimension].Name = std::string(name);
   it->second.Dimensions[dimension].Metadata = std::string(meaning);
   return DEVICE_OK;
}

/**
 * Configure a particular coordinate name. e.g. channel name / position name ...
 * @param handle Entry GUID
 * @param dimension Dimension index
 * @param coordinate Coordinate index
 * @param name Coordinate name
 * @return Status code
 */
int BigTiffStorage::ConfigureCoordinate(const char* handle, int dimension, int coordinate, const char* name)
{
   if(handle == nullptr || dimension < 0 || coordinate < 0)
      return DEVICE_INVALID_INPUT_PARAM;
   auto it = cache.find(handle);
   if(it == cache.end())
      return DEVICE_INVALID_INPUT_PARAM;
   if((std::size_t)dimension >= it->second.getDimSize())
      return DEVICE_INVALID_INPUT_PARAM;
   if((std::size_t)coordinate >= it->second.Dimensions[dimension].getSize())
      return DEVICE_INVALID_INPUT_PARAM;
   it->second.Dimensions[dimension].Coordinates[coordinate] = std::string(name);
   return DEVICE_OK;
}

/**
 * Get number of dimensions
 * @param handle Entry GUID
 * @param numDimensions Number of dimensions [out]
 * @return Status code
 */
int BigTiffStorage::GetNumberOfDimensions(const char* handle, int& numDimensions)
{
   if(handle == nullptr)
      return DEVICE_INVALID_INPUT_PARAM;
   auto it = cache.find(handle);
   if(it == cache.end())
      return DEVICE_INVALID_INPUT_PARAM;
   numDimensions = (int)it->second.getDimSize();
   return DEVICE_OK;
}

/**
 * Get number of dimensions
 * @param handle Entry GUID
 * @return Status code
 */
int BigTiffStorage::GetDimension(const char* handle, int dimension, char* name, int nameLength, char* meaning, int meaningLength)
{
   if(handle == nullptr || dimension < 0 || meaningLength <= 0)
      return DEVICE_INVALID_INPUT_PARAM;
   auto it = cache.find(handle);
   if(it == cache.end())
      return DEVICE_INVALID_INPUT_PARAM;
   if((std::size_t)dimension >= it->second.getDimSize())
      return DEVICE_INVALID_INPUT_PARAM;
   if(it->second.Dimensions[dimension].Name.size() > (std::size_t)nameLength)
      return DEVICE_INVALID_PROPERTY_LIMTS;
   if(it->second.Dimensions[dimension].Metadata.size() > (std::size_t)meaningLength)
      return DEVICE_INVALID_PROPERTY_LIMTS;
   it->second.Dimensions[dimension].Name.copy(name, it->second.Dimensions[dimension].Name.size());
   it->second.Dimensions[dimension].Metadata.copy(meaning, it->second.Dimensions[dimension].Metadata.size());
   return DEVICE_OK;
}

/**
 * Get number of dimensions
 * @param handle Entry GUID
 * @return Status code
 */
int BigTiffStorage::GetCoordinate(const char* handle, int dimension, int coordinate, char* name, int nameLength)
{
   if(handle == nullptr || dimension < 0 || coordinate < 0 || nameLength <= 0)
      return DEVICE_INVALID_INPUT_PARAM;
   auto it = cache.find(handle);
   if(it == cache.end())
      return DEVICE_INVALID_INPUT_PARAM;
   if((std::size_t)dimension >= it->second.getDimSize())
      return DEVICE_INVALID_INPUT_PARAM;
   if((std::size_t)coordinate >= it->second.Dimensions[dimension].getSize())
      return DEVICE_INVALID_INPUT_PARAM;
   if(it->second.Dimensions[dimension].Coordinates[coordinate].size() > (std::size_t)nameLength)
      return DEVICE_INVALID_PROPERTY_LIMTS;
   it->second.Dimensions[dimension].Coordinates[coordinate].copy(name, it->second.Dimensions[dimension].Coordinates[coordinate].size());
   return DEVICE_OK;
}

/**
 * Discard closed dataset storage descriptors from cache
 * By default storage descriptors are preserved even after the dataset is closed
 * To reclaim memory all closed descritors are evicted from cache
 */
void BigTiffStorage::cacheReduce() noexcept
{
   for(auto it = cache.begin(); it != cache.end(); )
   {
      if(!it->second.isOpen())
         it = cache.erase(it);
      else
         it++;
   }
}