///////////////////////////////////////////////////////////////////////////////
// FILE:          StorageMonitor.h
// PROJECT:       Micro-Manager
// SUBSYSTEM:     MMCore
//-----------------------------------------------------------------------------
// DESCRIPTION:   Thread that monitors the circular buffer and saves any images
//                that it finds
//
// AUTHOR:        Nenad Amodaj, 2025
//
// COPYRIGHT:     Nenad Amodaj 2025
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

#pragma once
#include <iostream>
#include <thread>
#include <atomic>
#include <mutex>
#include <condition_variable>
#include <chrono>
#include "CircularBuffer.h"
#include "Devices/StorageInstance.h"

class StorageMonitorThread {
private:
   std::thread thread_;
   std::atomic<bool> running_;
   std::atomic<bool> shouldStop_;
   std::mutex mutex_;
   std::condition_variable cv_;
   CircularBuffer* cbuf_;
   std::shared_ptr<StorageInstance> storageInstance_;
   int datasetHandle_;
   std::string errorMessage_;
   std::atomic<bool> hasErrors_;

   // the function that runs in a thread
   // monitors circular buffer and saves any images that it finds
   void StorageWorkFunction();

public:
   StorageMonitorThread(CircularBuffer* buf, std::shared_ptr<StorageInstance> pStorage, int handle) : 
      running_(false), shouldStop_(false), cbuf_(buf), storageInstance_(pStorage), datasetHandle_(handle), hasErrors_(false) {}

   ~StorageMonitorThread() {
      // Ensure thread is properly stopped if not already
      if (running_) {
         stop();
      }

      // Make sure we join the thread if it's joinable
      if (thread_.joinable()) {
         thread_.join();
      }
   }

   // Start the worker thread
   void start() {
      // Only start if not already running
      if (!running_) {
         shouldStop_ = false;
         hasErrors_ = false;
         thread_ = std::thread(&StorageMonitorThread::StorageWorkFunction, this);
      }
   }

   // Gracefully stop the worker thread
   void stop() {
      if (running_) {
         // Set stop flag
         shouldStop_ = true;

         // Notify the condition variable to wake up the thread
         {
            std::lock_guard<std::mutex> lock(mutex_);
            cv_.notify_all();
         }

         // Wait for thread to finish
         if (thread_.joinable()) {
            thread_.join();
         }
      }
   }

   // Check if thread is running
   bool isRunning() const {
      return running_.load();
   }

   // check if there were any errors
   bool hasErrors() const {
      return hasErrors_.load();
   }
};

