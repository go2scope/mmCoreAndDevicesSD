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
#pragma once
#include "MMDevice.h"
#include "DeviceBase.h"

//////////////////////////////////////////////////////////////////////////////
// Error codes
//
//////////////////////////////////////////////////////////////////////////////
#define ERR_PARAMETER_ERROR         144001
#define ERR_INTERNAL						144002

//////////////////////////////////////////////////////////////////////////////
// Cache configuration
//
//////////////////////////////////////////////////////////////////////////////
#define MAX_CACHE_SIZE					1024
#define CACHE_HARD_LIMIT				0

static const char* g_Go2Scope = "Go2Scope";
static const char* g_MMV1Storage = "MMV1Storage";
static const char* g_AcqZarrStorage = "AcquireZarrStorage";
static const char* g_BigTiffStorage = "BigTiffStorage";

/**
 * Dataset dimension descriptor
 * @author Miloš Jovanović <milos@tehnocad.rs>
 * @version 1.0
 */
struct G2SDimensionInfo
{
	/**
	 * Default initializer
	 * @param vname Axis name
	 * @param ndim Axis size
	 */
	G2SDimensionInfo(int ndim = 0) noexcept : Name(""), Coordinates(ndim) { }

	/**
	 * Set dimensions size
	 * @param sz Number of axis coordinates
	 */
	void setSize(std::size_t sz) noexcept { Coordinates.resize(sz); }
	/**
	 * Get dimension size
	 * @return Number of axis coordinates
	 */
	std::size_t getSize() const noexcept { return Coordinates.size(); }

	std::string													Name;												///< Axis name
	std::string													Metadata;										///< Axis metadata
	std::vector<std::string>								Coordinates;									///< Axis coordinates
};

/**
 * Storage entry descriptor
 * @author Miloš Jovanović <milos@tehnocad.rs>
 * @version 1.0
 */
struct G2SStorageEntry
{
	/**
	 * Default initializer
	 * @param vpath Absoulute path on disk
	 * @param vname Dataset name
	 * @param ndim Number of dimensions
	 * @param shape Axis sizes
	 * @param vmeta Dataset metadata
	 */
	G2SStorageEntry(const char* vpath, const char* vname, int ndim, const int* shape = nullptr, const char* vmeta = nullptr) noexcept : Path(vpath), Name(vname), Dimensions(ndim)
	{
		if(shape != nullptr)
		{
			for(std::size_t i = 0; i < Dimensions.size(); i++)
				Dimensions[i].setSize((std::size_t)shape[i]);
		}
		if(vmeta != nullptr)
			Metadata = std::string(vmeta);
		ImageWidth = 0;
		ImageHeight = 0;
		FileHandle = nullptr;
	}

	/**
	 * Check if file handle is open
	 * @return Is file handle open
	 */
	bool isOpen() noexcept { return FileHandle != nullptr; }
	/**
	 * Get number of dimensions
	 * @return Number of dataset dimensions
	 */
	std::size_t getDimSize() const noexcept { return Dimensions.size(); }

	std::string													Path;												///< Absoulute path on disk
	std::string													Name;												///< Dataset name
	std::string													Metadata;										///< Dataset metadata
	std::vector<G2SDimensionInfo>							Dimensions;										///< Dataset dimensions vector
	std::uint32_t												ImageWidth;										///< Image width (in pixels)
	std::uint32_t												ImageHeight;									///< Image height (in pixels)
	void*															FileHandle;										///< File handle
};