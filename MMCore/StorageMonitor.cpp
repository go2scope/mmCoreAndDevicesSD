#include "StorageMonitor.h"

// The actual work function that runs in the thread
void StorageMonitorThread::StorageWorkFunction() {
   // Set running flag
   running_ = true;
   errorMessage_.clear();

   std::cout << "Storage monitor thread started" << std::endl;

   while (!shouldStop_) {
      // Do some work here
      {
         std::unique_lock<std::mutex> lock(mutex_);

         // Wait for stop signal or timeout (simulating periodic work)
         // This allows thread to respond to stop request within the timeout period
         if (cv_.wait_for(lock, std::chrono::seconds(1),
            [this] { return shouldStop_.load(); })) {
            // If we're here, we were explicitly stopped
            break;
         }
      }

      // Perform work
      std::cout << "Worker thread doing work..." << std::endl;
      int imagesInBuffer = cbuf_->GetRemainingImageCount();
      if (imagesInBuffer > 1)
      {
         for (int i = 0; i < imagesInBuffer - 1; i++)
         {
            int size = cbuf_->Width() * cbuf_->Height() * cbuf_->Depth();
            auto pBuf = cbuf_->GetNextImage();
            int ret = dataset_->first->AppendImage(dataset_->second, size, const_cast<unsigned char*>(pBuf), "", 0);
            if (ret != DEVICE_OK)
            {
               errorMessage_ = dataset_->first->GetErrorText(ret);
               break;
            }
         }
      }

      // Check stop flag again after work is done
      if (shouldStop_) {
         break;
      }
   }

   // Clear running flag
   running_ = false;
}
