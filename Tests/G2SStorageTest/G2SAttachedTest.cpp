///////////////////////////////////////////////////////////////////////////////
// FILE:          G2SAttachedTest.cpp
// PROJECT:       Micro-Manager
// SUBSYSTEM:     Device Driver Tests
//-----------------------------------------------------------------------------
// DESCRIPTION:   Go2Scope storage driver acquisition test
//
// AUTHOR:        Nenad Amodaj <nenad@amodaj.com>
//
// COPYRIGHT:     Nenad Amodaj, 2025
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
///////////////////////////////////////////////////////////////////////////////
#include <iostream>
#include <iomanip>
#include <string>
#include <vector>
#include <chrono>
#include <thread>
#include "MMCore.h"

extern std::string generateImageMeta(CMMCore& core, int imgind);

/**
 * Storage acquisition test
 * @param core MM Core instance
 * @param path Data folder path
 * @param name Dataset name
 * @param c Channel count
 * @param t Time points
 * @param p Positions count
 * @throws std::runtime_error
 */
void testAttachedStorage(CMMCore& core, const std::string& path, const std::string& name, int c, int t, int p)
{
	std::cout << std::endl << "Starting G2SStorage driver attached storage test" << std::endl;

	// Take one image to "warm up" the camera and get actual image dimensions
	core.snapImage();
	int w = (int)core.getImageWidth();
	int h = (int)core.getImageHeight();
	int imgSize = 2 * w * h;
	double imgSizeMb = (double)imgSize / (1024.0 * 1024.0);

	// Shape convention: Z, T, C, Y, X
	std::vector<long> shape = { p, t, c, h, w };
	auto handle = core.createDataset(path.c_str(), name.c_str(), shape, MM::StorageDataType_GRAY16, "", 0);

	std::cout << "Dataset handle: " << handle << std::endl;
	std::cout << "Dataset shape (P-T-C-H-W): " << p << " x " << t << " x " << c << " x " << h << " x " << w << " x 16-bit" << std::endl << std::endl;

	// attach storage to circular buffer
	core.clearCircularBuffer(); // clear buffer before attaching storage to avoid adding previously acquired images
	core.attachDatasetToCircularBuffer(handle); // at this point the saving thread is active and looking for new images 

	std::cout << "START OF ACQUISITION TO ATTACHED STORAGE" << std::endl;
	
	// Start acquisition
	auto start = std::chrono::high_resolution_clock::now();
	int numberOfImages = c * t * p;
	core.startSequenceAcquisition(numberOfImages, 0.0, true);
	Sleep(100);

	while (core.isSequenceRunning())
	{
		int savedImages = core.getDatasetImageCount(handle);
		int imagesInBuffer = core.getRemainingImageCount();
		std::cout << "Saved images: " << savedImages << ", remaining in buffer: " << imagesInBuffer << std::endl;
		Sleep(300);
	}

	// We are done so close the dataset
	core.closeDataset(handle);
	auto end = std::chrono::high_resolution_clock::now();
	std::cout << "END OF ACQUISITION" << std::endl << std::endl;

	// Calculate storage driver bandwidth
	double totalTimeS = (end - start).count() / 1000000000.0;
	double totalSizemb = (double)imgSize * p * t * c / (1024.0 * 1024.0);
	double totbw = totalSizemb / totalTimeS;

	std::cout << std::fixed << std::setprecision(1) << "Dataset size " << totalSizemb << " MB" << std::endl;
	std::cout << std::fixed << std::setprecision(3) << "Acquisition completed in " << totalTimeS << " sec" << std::endl;
	std::cout << std::fixed << std::setprecision(1) << "Acquisition bandwidth " << totbw << " MB/s" << std::endl;
}