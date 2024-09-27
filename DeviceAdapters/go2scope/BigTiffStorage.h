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
#pragma once
#include "Go2ScopeStorage.h"

/**
 * Storage writer driver for BigTIFF format
 * @author Miloš Jovanović <milos@tehnocad.rs>
 * @version 1.0
 */
class BigTiffStorage : public CStorageBase<BigTiffStorage>
{
public:
   //=========================================================================================================================
	// Constructors & destructors
	//=========================================================================================================================
	BigTiffStorage();
   virtual ~BigTiffStorage() noexcept;

public:
   //=========================================================================================================================
	// Public interface - Device API
	//=========================================================================================================================
   int                                             Initialize();
   int                                             Shutdown();
   void                                            GetName(char* pszName) const;
   bool                                            Busy();

public:
   //=========================================================================================================================
   // Public interface - Storage API
   //=========================================================================================================================
   int                                             Create(const char* path, const char* name, int numberOfDimensions, const int shape[], const char* meta, char* handle);
   int                                             ConfigureDimension(const char* handle, int dimension, const char* name, const char* meaning);
   int                                             ConfigureCoordinate(const char* handle, int dimension, int coordinate, const char* name);
   int                                             Close(const char* handle);
   int                                             Load(const char* path, const char* name, char* handle);
   int                                             Delete(char* handle);
   int                                             List(const char* path, char** listOfDatasets, int maxItems, int maxItemLength);
   int                                             AddImage(const char* handle, unsigned char* pixels, int width, int height, int depth, int coordinates[], int numCoordinates, const char* imageMeta);
   int                                             GetSummaryMeta(const char* handle, char* meta, int bufSize);
   int                                             GetImageMeta(const char* handle, int coordinates[], int numCoordinates, char* meta, int bufSize);
   const unsigned char*                            GetImage(const char* handle, int coordinates[], int numCoordinates);
   int                                             GetNumberOfDimensions(const char* handle, int& numDimensions);
   int                                             GetDimension(const char* handle, int dimension, char* name, int nameLength, char* meaning, int meaningLength);
   int                                             GetCoordinate(const char* handle, int dimension, int coordinate, char* name, int nameLength);

public:
   //=========================================================================================================================
   // Public interface - Action interface
   //=========================================================================================================================

private:
   //=========================================================================================================================
   // Data members
   //=========================================================================================================================
   bool                                            initialized;                           ///< Is driver initialized
};