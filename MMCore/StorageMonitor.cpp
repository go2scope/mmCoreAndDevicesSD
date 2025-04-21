#include "StorageMonitor.h"

// The actual work function that runs in the thread
void StorageMonitorThread::StorageWorkFunction() {
   // Set running flag
   running_ = true;
   errorMessage_.clear();

   std::cout << "Storage monitor thread started" << std::endl;

   while (!shouldStop_) {
      {
         std::unique_lock<std::mutex> lock(mutex_);

         // Wait for stop signal or timeout
         // This allows thread to respond to stop request within the timeout period
         if (cv_.wait_for(lock, std::chrono::seconds(1),
            [this] { return shouldStop_.load(); })) {
            // If we're here, we were explicitly stopped
            break;
         }
      }

      // Look for images in the cb and save them to dataset
      int imagesInBuffer = cbuf_->GetRemainingImageCount();
      if (imagesInBuffer > 1)
      {
         // note: we always want to leave the last image in the buffer, so that we can monitor live
         int size = cbuf_->Width() * cbuf_->Height() * cbuf_->Depth();
         auto pBuf = cbuf_->GetNextImage();
         int ret = storageInstance_->AppendImage(datasetHandle_, size, const_cast<unsigned char*>(pBuf), "", 0);
         if (ret != DEVICE_OK)
         {
            errorMessage_ = storageInstance_->GetErrorText(ret);
            hasErrors_ = true;
            break;
         }
      }

      // Check stop flag again after work is done
      if (shouldStop_) {
         break;
      }
   }

   // at this point there could be one more image in the buffer, so save it
   if (cbuf_->GetRemainingImageCount() > 0 && !hasErrors_)
   {
      int size = cbuf_->Width() * cbuf_->Height() * cbuf_->Depth();
      auto pBuf = cbuf_->GetNextImage();
      int ret = storageInstance_->AppendImage(datasetHandle_, size, const_cast<unsigned char*>(pBuf), "", 0);
      if (ret != DEVICE_OK)
      {
         errorMessage_ = storageInstance_->GetErrorText(ret);
         hasErrors_ = true;
      }
   }

   // Clear running flag
   running_ = false;
}
