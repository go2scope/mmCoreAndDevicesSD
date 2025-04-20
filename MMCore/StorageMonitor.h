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
   std::pair<std::shared_ptr<StorageInstance>, int>* dataset_;
   std::string errorMessage_;

   // The actual work function that runs in the thread
   void StorageWorkFunction();

public:
   StorageMonitorThread(CircularBuffer* buf, std::pair<std::shared_ptr<StorageInstance>, int>* dataset) : 
      running_(false), shouldStop_(false), cbuf_(buf), dataset_(dataset) {}

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
};

