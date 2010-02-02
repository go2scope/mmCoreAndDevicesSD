///////////////////////////////////////////////////////////////////////////////
// FILE:          DemoStreamingCamera.h
// PROJECT:       Micro-Manager
// SUBSYSTEM:     DeviceAdapters
//-----------------------------------------------------------------------------
// DESCRIPTION:   The fast, straming camera simulator.
//                
// AUTHOR:        Nenad Amodaj, nenad@amodaj.com, 06/05/2007
//
// COPYRIGHT:     University of California, San Francisco, 2007
//                100X Imaging Inc, 2008
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
// CVS:           $Id: DemoCamera.h 73 2007-04-19 00:11:35Z nenad $
//

#include "DemoStreamingCamera.h"
#include <cstdio>
#include <string>
#include <math.h>
#include "../../MMDevice/ModuleInterface.h"
#include <sstream>
#include <algorithm>

using namespace std;
const int DemoStreamingCamera::imageSize_ = 512;
const double DemoStreamingCamera::nominalPixelSizeUm_ = 1.0;

const char* g_CameraDeviceName = "DStreamCam";
const char* g_NoiseProcessorName = "DNoiseProcessor";
const char* g_SignalGeneratorName = "DSignalGenerator";

// constants for naming pixel types (allowed values of the "PixelType" property)
const char* g_PixelType_8bit = "8bit";
const char* g_PixelType_16bit = "16bit";

// constants for naming color modes
const char* g_ColorMode_Grayscale = "Grayscale";
const char* g_ColorMode_RGB = "RGB-32bit";

// windows DLL entry code
#ifdef WIN32
BOOL APIENTRY DllMain( HANDLE /*hModule*/, 
                      DWORD  ul_reason_for_call, 
                      LPVOID /*lpReserved*/
                      )
{
   switch (ul_reason_for_call)
   {
   case DLL_PROCESS_ATTACH:
      break;
   case DLL_THREAD_ATTACH:
      break;
   case DLL_THREAD_DETACH:
      break;
   case DLL_PROCESS_DETACH:
      break;
   }
   return TRUE;
}
#endif

// mutex
static MMThreadLock g_lock;

///////////////////////////////////////////////////////////////////////////////
// Exported MMDevice API
///////////////////////////////////////////////////////////////////////////////

MODULE_API void InitializeModuleData()
{
   AddAvailableDeviceName(g_CameraDeviceName, "Demo streaming camera");
   AddAvailableDeviceName(g_NoiseProcessorName, "Demo processor: adds noise to images in real time");
   AddAvailableDeviceName(g_SignalGeneratorName, "Demo signal generator: real-time signal output");
}

MODULE_API MM::Device* CreateDevice(const char* deviceName)
{
   if (deviceName == 0)
      return 0;

   // decide which device class to create based on the deviceName parameter
   if (strcmp(deviceName, g_CameraDeviceName) == 0)
   {
      // create camera
      return new DemoStreamingCamera();
   }
   else if (strcmp(deviceName, g_NoiseProcessorName) == 0)
   {
      // create processor
      return new DemoNoiseProcessor();
   }
   else if (strcmp(deviceName, g_SignalGeneratorName) == 0)
   {
      // create processor
      return new DemoSignalGenerator();
   }

   // ...supplied name not recognized
   return 0;
}

MODULE_API void DeleteDevice(MM::Device* pDevice)
{
   delete pDevice;
}

///////////////////////////////////////////////////////////////////////////////
// DemoStreamingCamera implementation
// ~~~~~~~~~~~~~~~~~~~~~~~~~~

/**
* DemoStreamingCamera constructor.
* Setup default all variables and create device properties required to exist
* before intialization. In this case, no such properties were required. All
* properties will be created in the Initialize() method.
*
* As a general guideline Micro-Manager devices do not access hardware in the
* the constructor. We should do as little as possible in the constructor and
* perform most of the initialization in the Initialize() method.
*/
DemoStreamingCamera::DemoStreamingCamera() : 
CCameraBase<DemoStreamingCamera> (),
initialized_(false),
busy_(false),
readoutUs_(0),
color_(false),
rawBuffer_(0),
stopOnOverflow_(true)
{
   // call the base class method to set-up default error codes/messages
   InitializeDefaultErrorMessages();
   readoutStartTime_ = GetCurrentMMTime();
}

/**
* DemoStreamingCamera destructor.
* If this device used as intended within the Micro-Manager system,
* Shutdown() will be always called before the destructor. But in any case
* we need to make sure that all resources are properly released even if
* Shutdown() was not called.
*/
DemoStreamingCamera::~DemoStreamingCamera()
{
   delete[] rawBuffer_;
}

/**
* Obtains device name.
* Required by the MM::Device API.
*/
void DemoStreamingCamera::GetName(char* name) const
{
   // We just return the name we use for referring to this
   // device adapter.
   CDeviceUtils::CopyLimitedString(name, g_CameraDeviceName);
}

/**
* Tells us if device is still processing asynchronous command.
* Required by the MM:Device API.
*/
bool DemoStreamingCamera::Busy()
{
   // TODO: this is controversial!
   // Should camera appear as busy during acquistion ???
   // is mutex neccessary ???
   //ACE_Guard<ACE_Mutex> guard(g_lock);
   return IsCapturing();
}

/**
* Intializes the hardware.
* Required by the MM::Device API.
* Typically we access and initialize hardware at this point.
* Device properties are typically created here as well, except
* the ones we need to use for defining initialization parameters.
* Such pre-initialization properties are created in the constructor.
* (This device does not have any pre-initialization properties)
*/
int DemoStreamingCamera::Initialize()
{
   if (initialized_)
      return DEVICE_OK;

   // set property list
   // -----------------

   // Name
   int nRet = CreateProperty(MM::g_Keyword_Name, g_CameraDeviceName, MM::String, true);
   if (DEVICE_OK != nRet)
      return nRet;

   // Description
   nRet = CreateProperty(MM::g_Keyword_Description, "Demo Streaming Camera Device Adapter", MM::String, true);
   if (DEVICE_OK != nRet)
      return nRet;

   // CameraName
   nRet = CreateProperty(MM::g_Keyword_CameraName, "Demo Streaming Camera", MM::String, true);
   assert(nRet == DEVICE_OK);

   // CameraID
   nRet = CreateProperty(MM::g_Keyword_CameraID, "V1.0", MM::String, true);
   assert(nRet == DEVICE_OK);

   // binning
   CPropertyAction *pAct = new CPropertyAction (this, &DemoStreamingCamera::OnBinning);
   nRet = CreateProperty(MM::g_Keyword_Binning, "1", MM::Integer, false, pAct);
   assert(nRet == DEVICE_OK);

   vector<string> binValues;
   binValues.push_back("1");
   binValues.push_back("2");
   binValues.push_back("4");
   binValues.push_back("8");
   nRet = SetAllowedValues(MM::g_Keyword_Binning, binValues);
   if (nRet != DEVICE_OK)
      return nRet;

   // pixel type
   pAct = new CPropertyAction (this, &DemoStreamingCamera::OnPixelType);
   nRet = CreateProperty(MM::g_Keyword_PixelType, g_PixelType_8bit, MM::String, false, pAct);
   assert(nRet == DEVICE_OK);

   vector<string> pixelTypeValues;
   pixelTypeValues.push_back(g_PixelType_8bit);
   pixelTypeValues.push_back(g_PixelType_16bit);
   nRet = SetAllowedValues(MM::g_Keyword_PixelType, pixelTypeValues);
   if (nRet != DEVICE_OK)
      return nRet;

   // exposure
   nRet = CreateProperty(MM::g_Keyword_Exposure, "100.0", MM::Float, false);
   assert(nRet == DEVICE_OK);

   // scan mode
   nRet = CreateProperty("ScanMode", "1", MM::Integer, false);
   assert(nRet == DEVICE_OK);

   // camera gain
   nRet = CreateProperty(MM::g_Keyword_Gain, "0", MM::Integer, false);
   assert(nRet == DEVICE_OK);
   SetPropertyLimits(MM::g_Keyword_Gain, 0.0, 10.0);

   // camera offset
   nRet = CreateProperty(MM::g_Keyword_Offset, "0", MM::Integer, false);
   assert(nRet == DEVICE_OK);

   // camera temperature
   nRet = CreateProperty(MM::g_Keyword_CCDTemperature, "0", MM::Float, false);
   assert(nRet == DEVICE_OK);
   SetPropertyLimits(MM::g_Keyword_CCDTemperature,  5.0, 100.0);

   // readout time
   pAct = new CPropertyAction (this, &DemoStreamingCamera::OnReadoutTime);
   nRet = CreateProperty(MM::g_Keyword_ReadoutTime, "0", MM::Float, false, pAct);
   assert(nRet == DEVICE_OK);

   // actual interval
   nRet = CreateProperty(MM::g_Keyword_ActualInterval_ms, "0.0", MM::Float, false);
   assert(nRet == DEVICE_OK);

   // color mode
   pAct = new CPropertyAction (this, &DemoStreamingCamera::OnColorMode);
   nRet = CreateProperty(MM::g_Keyword_ColorMode, g_ColorMode_Grayscale, MM::String, false, pAct);
   color_ = false;
   assert(nRet == DEVICE_OK);
   vector<string> colorValues;
   colorValues.push_back(g_ColorMode_Grayscale);
   colorValues.push_back(g_ColorMode_RGB);
   nRet = SetAllowedValues(MM::g_Keyword_ColorMode, colorValues);


   // synchronize all properties
   // --------------------------
   nRet = UpdateStatus();
   if (nRet != DEVICE_OK)
      return nRet;

   // setup the buffer
   // ----------------
   nRet = ResizeImageBuffer();
   if (nRet != DEVICE_OK)
      return nRet;

   initialized_ = true;

   // initialize image buffer
   return SnapImage();
}

/**
* Returns the number of physical channels in the image.
*/
unsigned int DemoStreamingCamera::GetNumberOfComponents() const
{
   if (color_)
      return 4; // rgb
   else
      return 1; // grayscale
}

int DemoStreamingCamera::GetComponentName(unsigned int channel, char* name)
{
   if (!color_ && channel > 0)
      return DEVICE_NONEXISTENT_CHANNEL;

   switch (channel)
   {
   case 0:
      CDeviceUtils::CopyLimitedString(name, "R");
      break;

   case 1:
      CDeviceUtils::CopyLimitedString(name, "G");
      break;

   case 2:
      CDeviceUtils::CopyLimitedString(name, "B");
      break;

   default:
      return DEVICE_NONEXISTENT_CHANNEL;
      break;
   }
   return DEVICE_OK;
}


/**
* Shuts down (unloads) the device.
* Required by the MM::Device API.
* Ideally this method will completely unload the device and release all resources.
* Shutdown() may be called multiple times in a row.
* After Shutdown() we should be allowed to call Initialize() again to load the device
* without causing problems.
*/
int DemoStreamingCamera::Shutdown()
{
   initialized_ = false;
   StopSequenceAcquisition();
   delete[] rawBuffer_;
   rawBuffer_ = 0;
   return DEVICE_OK;
}

/**
* Performs exposure and grabs a single image.
* Required by the MM::Camera API.
*/
int DemoStreamingCamera::SnapImage()
{
   MM::MMTime startTime = GetCurrentMMTime();
   double exp = GetExposure();
   //double expUs = exp * 1000.0;
   if (color_)
   {
      GenerateSyntheticImage(img_[0], exp); // r
      GenerateSyntheticImage(img_[1], exp); // g
      GenerateSyntheticImage(img_[2], exp); // b
   }
   else
      GenerateSyntheticImage(img_[0], exp);

   CopyToRawBuffer();

   MM::ImageProcessor* ip = GetCoreCallback()->GetImageProcessor(this);
   if (ip)
   {
      if (color_)
      {
         for (int i=0; i<3; i++)
         {
            int ret = ip->Process(const_cast<unsigned char*>(img_[i].GetPixels()), GetImageWidth(), GetImageHeight(), GetImageBytesPerPixel());
            if (ret != DEVICE_OK)
               return ret;
         }
      }
      else
      {
         int ret = ip->Process(const_cast<unsigned char*>(img_[0].GetPixels()), GetImageWidth(), GetImageHeight(), GetImageBytesPerPixel());
         if (ret != DEVICE_OK)
            return ret;
      }
   }

   readoutStartTime_ = GetCurrentMMTime();
   CDeviceUtils::SleepMs((long)GetExposure());

   return DEVICE_OK;
}

/**
* Returns pixel data.
* Required by the MM::Camera API.
* The calling program will assume the size of the buffer based on the values
* obtained from GetImageBufferSize(), which in turn should be consistent with
* values returned by GetImageWidth(), GetImageHight() and GetImageBytesPerPixel().
* The calling program allso assumes that camera never changes the size of
* the pixel buffer on its own. In other words, the buffer can change only if
* appropriate properties are set (such as binning, pixel type, etc.)
*/
const unsigned char* DemoStreamingCamera::GetImageBuffer()
{
   return rawBuffer_;
}

/**
* Returns pixel data with interleaved RGB pixels in 32 bpp format
*/
const unsigned int* DemoStreamingCamera::GetImageBufferAsRGB32()
{
   return (unsigned int*) rawBuffer_;
}

/**
* Returns image buffer X-size in pixels.
* Required by the MM::Camera API.
*/
unsigned DemoStreamingCamera::GetImageWidth() const
{
   return img_[0].Width();
}

/**
* Returns image buffer Y-size in pixels.
* Required by the MM::Camera API.
*/
unsigned DemoStreamingCamera::GetImageHeight() const
{
   return img_[0].Height();
}

/**
* Returns image buffer pixel depth in bytes.
* Required by the MM::Camera API.
*/
unsigned DemoStreamingCamera::GetImageBytesPerPixel() const
{
   return img_[0].Depth();
} 

/**
* Returns the bit depth (dynamic range) of the pixel.
* This does not affect the buffer size, it just gives the client application
* a guideline on how to interpret pixel values.
* Required by the MM::Camera API.
*/
unsigned DemoStreamingCamera::GetBitDepth() const
{
   return 8 * GetImageBytesPerPixel();
}

/**
* Returns the size in bytes of the image buffer.
* Required by the MM::Camera API.
*/
long DemoStreamingCamera::GetImageBufferSize() const
{
   long singleChannelSize = img_[0].Width() * img_[0].Height() * GetImageBytesPerPixel();
   if (color_)
      return 4*singleChannelSize;
   else
      return singleChannelSize;
}

/**
* Sets the camera Region Of Interest.
* Required by the MM::Camera API.
* This command will change the dimensions of the image.
* Depending on the hardware capabilities the camera may not be able to configure the
* exact dimensions requested - but should try do as close as possible.
* If the hardware does not have this capability the software should simulate the ROI by
* appropriately cropping each frame.
* This demo implementation ignores the position coordinates and just crops the buffer.
* @param x - top-left corner coordinate
* @param y - top-left corner coordinate
* @param xSize - width
* @param ySize - height
*/
int DemoStreamingCamera::SetROI(unsigned /*x*/, unsigned /*y*/, unsigned xSize, unsigned ySize)
{
   if(IsCapturing())
      return ERR_BUSY_ACQIRING;

   if (xSize == 0 && ySize == 0)
      // effectively clear ROI
      ResizeImageBuffer();
   else
   {
      char buf[MM::MaxStrLength];
      int ret = GetProperty(MM::g_Keyword_Binning, buf);
      if (ret != DEVICE_OK)
         return ret;
      long binSize = atol(buf);

      ret = GetProperty(MM::g_Keyword_PixelType, buf);
      if (ret != DEVICE_OK)
         return ret;

      int byteDepth = 1;
      if (strcmp(buf, g_PixelType_16bit) == 0)
         byteDepth = 2;

      // apply ROI
      return ResizeImageBuffer(xSize*binSize, ySize*binSize, byteDepth, binSize);
   }

   return DEVICE_OK;
}

/**
* Returns the actual dimensions of the current ROI.
* Required by the MM::Camera API.
*/
int DemoStreamingCamera::GetROI(unsigned& x, unsigned& y, unsigned& xSize, unsigned& ySize)
{
   x = 0;
   y = 0;

   xSize = img_[0].Width();
   ySize = img_[0].Height();

   return DEVICE_OK;
}

/**
* Resets the Region of Interest to full frame.
* Required by the MM::Camera API.
*/
int DemoStreamingCamera::ClearROI()
{
   if (Busy())
      return ERR_BUSY_ACQIRING;

   ResizeImageBuffer();
   return DEVICE_OK;
}

/**
* Returns the current exposure setting in milliseconds.
* Required by the MM::Camera API.
*/
double DemoStreamingCamera::GetExposure() const
{
   char buf[MM::MaxStrLength];
   int ret = GetProperty(MM::g_Keyword_Exposure, buf);
   if (ret != DEVICE_OK)
      return 0.0;
   return atof(buf);
}

/**
* Sets exposure in milliseconds.
* Required by the MM::Camera API.
*/
void DemoStreamingCamera::SetExposure(double exp)
{

   SetProperty(MM::g_Keyword_Exposure, CDeviceUtils::ConvertToString(exp));
}

/**
* Returns the current binning factor.
* Required by the MM::Camera API.
*/
int DemoStreamingCamera::GetBinning() const
{
   char buf[MM::MaxStrLength];
   int ret = GetProperty(MM::g_Keyword_Binning, buf);
   if (ret != DEVICE_OK)
      return 1;
   return atoi(buf);
}

/**
* Sets binning factor.
* Required by the MM::Camera API.
*/
int DemoStreamingCamera::SetBinning(int binFactor)
{
   if(IsCapturing())
      return ERR_BUSY_ACQIRING;

   return SetProperty(MM::g_Keyword_Binning, CDeviceUtils::ConvertToString(binFactor));
}


///////////////////////////////////////////////////////////////////////////////
// DemoStreamingCamera Action handlers
///////////////////////////////////////////////////////////////////////////////

/**
* Handles "Binning" property.
*/
int DemoStreamingCamera::OnBinning(MM::PropertyBase* pProp, MM::ActionType eAct)
{
   int ret = DEVICE_ERR;
   switch(eAct)
   {
   case MM::AfterSet:
      {
         if(IsCapturing())
            return DEVICE_CAN_NOT_SET_PROPERTY;
         // the user just set the new value for the property, so we have to
         // apply this value to the 'hardware'.
         long binFactor;
         pProp->Get(binFactor);

         if (binFactor > 0 && binFactor < 10)
         {
            ret = ResizeImageBuffer(imageSize_, imageSize_, img_[0].Depth() , binFactor);
         }
         else
         {
            // on failure reset default binning of 1
            ResizeImageBuffer();
            pProp->Set(1L);
            return ERR_UNKNOWN_MODE;
         }
      }break;
   case MM::BeforeGet:
      {
         // the user is requesting the current value for the property, so
         // either ask the 'hardware' or let the system return the value
         // cached in the property.
         ret=DEVICE_OK;
      }break;
   }
   return ret; 
}

/**
* Handles "PixelType" property.
*/
int DemoStreamingCamera::OnPixelType(MM::PropertyBase* pProp, MM::ActionType eAct)
{
   int ret = DEVICE_ERR;
   switch(eAct)
   {
   case MM::AfterSet:
      {
         if(IsCapturing())
            return DEVICE_CAN_NOT_SET_PROPERTY;

         string pixelType;
         pProp->Get(pixelType);

         if (pixelType.compare(g_PixelType_8bit) == 0)
         {
            ret = ResizeImageBuffer(imageSize_, imageSize_, 1);
         }
         else if (pixelType.compare(g_PixelType_16bit) == 0)
         {
            ret = ResizeImageBuffer(imageSize_, imageSize_, 2);
         }
         else
         {
            // on error switch to default pixel type
            pProp->Set(g_PixelType_8bit);
            ResizeImageBuffer(imageSize_, imageSize_, 1);
            ret = ERR_UNKNOWN_MODE;
         }

      }break;
   case MM::BeforeGet:
      {
         // the user is requesting the current value for the property, so
         // either ask the 'hardware' or let the system return the value
         // cached in the property.
         ret=DEVICE_OK;
      }break;
   }
   return ret; 
}

/**
* Handles "ReadoutTime" property.
*/
int DemoStreamingCamera::OnReadoutTime(MM::PropertyBase* pProp, MM::ActionType eAct)
{

   if (eAct == MM::AfterSet)
   {
      if (Busy())
         return ERR_BUSY_ACQIRING;

      double readoutMs;
      pProp->Get(readoutMs);

      readoutUs_ = (long)(readoutMs * 1000.0);
   }
   else if (eAct == MM::BeforeGet)
   {
      pProp->Set(readoutUs_ / 1000.0);
   }

   return DEVICE_OK;
}

/**
* Handles "ColorMode" property.
*/
int DemoStreamingCamera::OnColorMode(MM::PropertyBase* pProp, MM::ActionType eAct)
{
   if (eAct == MM::AfterSet)
   {
      if(IsCapturing())
         return DEVICE_CAN_NOT_SET_PROPERTY;

      string pixelType;
      pProp->Get(pixelType);

      if (pixelType.compare(g_ColorMode_Grayscale) == 0)
      {
         color_ = false;
      }
      else if (pixelType.compare(g_ColorMode_RGB) == 0)
      {
         color_ = true;
      }
      else
      {
         // on error switch to default pixel type
         color_ = false;
         return ERR_UNKNOWN_MODE;
      }

      ResizeImageBuffer();
      if (initialized_) {
         OnPropertiesChanged(); // notify GUI to update
      }
   }
   else if (eAct == MM::BeforeGet)
   {
   }

   return DEVICE_OK;
}
///////////////////////////////////////////////////////////////////////////////
// Private DemoStreamingCamera methods
///////////////////////////////////////////////////////////////////////////////


/**
* Sync internal image buffer size to the chosen property values.
*/
int DemoStreamingCamera::ResizeImageBuffer(int imageSizeW /*= imageSize_*/, int imageSizeH /*= imageSize_*/)
{
   char buf[MM::MaxStrLength];
   int ret = GetProperty(MM::g_Keyword_Binning, buf);
   if (ret != DEVICE_OK)
      return ret;
   long binSize = atol(buf);

   ret = GetProperty(MM::g_Keyword_PixelType, buf);
   if (ret != DEVICE_OK)
      return ret;

   int byteDepth = 1;
   if (strcmp(buf, g_PixelType_16bit) == 0)
      byteDepth = 2;

   return ResizeImageBuffer(imageSizeW, imageSizeH, byteDepth, binSize);
}
/**
* Sync internal image buffer size to the chosen property values.
*/
int DemoStreamingCamera::ResizeImageBuffer(int imageSizeW, int imageSizeH, int byteDepth, int binSize /*=1*/)
{

   img_[0].Resize(imageSizeW/binSize, imageSizeH/binSize, byteDepth);
   img_[1].Resize(imageSizeW/binSize, imageSizeH/binSize, byteDepth);
   img_[2].Resize(imageSizeW/binSize, imageSizeH/binSize, byteDepth);

   delete[] rawBuffer_;
   if (color_)
   {
      rawBuffer_ = new unsigned char[img_[0].Width() * img_[0].Height() * img_[0].Depth() * 4];
   }
   else
   {
      rawBuffer_ = new unsigned char[img_[0].Width() * img_[0].Height() * img_[0].Depth()];
   }
   return DEVICE_OK;
}

/**
* Generate a spatial sine wave.
*/
void DemoStreamingCamera::GenerateSyntheticImage(ImgBuffer& img, double exp)
{
   if (img.Height() == 0 || img.Width() == 0 || img.Depth() == 0)
      return;

   const double cPi = 3.14;
   long lPeriod = img.Width()/2;
   static double dPhase = 0.0;
   double dLinePhase = 0.0;
   const double dAmp = exp;
   const double cLinePhaseInc = 2.0 * cPi / 4.0 / img.Height();

   unsigned j, k;
   if (img.Depth() == 1)
   {
      double pedestal = 127 * exp / 100.0;
      unsigned char* pBuf = const_cast<unsigned char*>(img.GetPixels());
      for (j=0; j<img.Height(); j++)
      {
         for (k=0; k<img.Width(); k++)
         {
            long lIndex = img.Width()*j + k;
            *(pBuf + lIndex) = (unsigned char) min(255.0, (pedestal + dAmp * sin(dPhase + dLinePhase + (2.0 * cPi * k) / lPeriod)));
         }
         dLinePhase += cLinePhaseInc;
      }         
   }
   else if (img.Depth() == 2)
   {
      double pedestal = USHRT_MAX/2 * exp / 100.0;
      double dAmp16 = dAmp * USHRT_MAX/255.0; // scale to behave like 8-bit
      unsigned short* pBuf = (unsigned short*) const_cast<unsigned char*>(img.GetPixels());
      for (j=0; j<img.Height(); j++)
      {
         for (k=0; k<img.Width(); k++)
         {
            long lIndex = img.Width()*j + k;
            *(pBuf + lIndex) = (unsigned short) min((double)USHRT_MAX, pedestal + dAmp16 * sin(dPhase + dLinePhase + (2.0 * cPi * k) / lPeriod));
         }
         dLinePhase += cLinePhaseInc;
      }         
   }

   dPhase += cPi / 4.0;
}


/**
* Starts continuous acquisition.
*
*/
int DemoStreamingCamera::StartSequenceAcquisition(long numImages, double interval_ms, bool stopOnOverflow)
{
   ostringstream os;
   os << "Started camera streaming with an interval of " << interval_ms << " ms, for " << numImages << " images.\n";
   printf("%s", os.str().c_str());
   LogMessage(os.str().c_str(), true);
   if (IsCapturing())
      return ERR_BUSY_ACQIRING;

   stopOnOverflow_ = stopOnOverflow;
   int ret = GetCoreCallback()->PrepareForAcq(this);
   if (ret != DEVICE_OK)
      return ret;

   // make sure the circular buffer is properly sized
   GetCoreCallback()->InitializeImageBuffer(GetNumberOfComponents(), 1, GetImageWidth(), GetImageHeight(), GetImageBytesPerPixel());

   double actualIntervalMs = max(GetExposure(), interval_ms);
   SetProperty(MM::g_Keyword_ActualInterval_ms, CDeviceUtils::ConvertToString(actualIntervalMs)); 

   startTime_ = GetCurrentMMTime();
   thd_->Start(numImages,actualIntervalMs);

   return DEVICE_OK;
}

int DemoStreamingCamera::PushImage()
{
   // TODO: call core to prepare for image snap
   if (color_)
   {
      GenerateSyntheticImage(img_[0], GetExposure());
      GenerateSyntheticImage(img_[1], GetExposure());
      GenerateSyntheticImage(img_[2], GetExposure());
   }
   else
      GenerateSyntheticImage(img_[0], GetExposure());

   // process image
   MM::ImageProcessor* ip = GetCoreCallback()->GetImageProcessor(this);
   if (ip)
   {
      if (color_)
      {
         for (int i=0; i<3; i++)
         {
            int ret = ip->Process(const_cast<unsigned char*>(img_[i].GetPixels()), GetImageWidth(), GetImageHeight(), GetImageBytesPerPixel());
            if (ret != DEVICE_OK)
               return ret;
         }
      }
      else
      {
         int ret = ip->Process(const_cast<unsigned char*>(img_[0].GetPixels()), GetImageWidth(), GetImageHeight(), GetImageBytesPerPixel());
         if (ret != DEVICE_OK)
            return ret;
      }
   }

   // insert image into the circular buffer
   CopyToRawBuffer(); // this effectively copies images to rawBuffer_

   // create metadata
   char label[MM::MaxStrLength];
   GetLabel(label);

   MM::MMTime timestamp = GetCurrentMMTime();
   Metadata md;

   MetadataSingleTag mstStartTime(MM::g_Keyword_Metadata_StartTime, label, true);
	mstStartTime.SetValue(CDeviceUtils::ConvertToString(startTime_.getMsec()));
   md.SetTag(mstStartTime);

   MetadataSingleTag mst(MM::g_Keyword_Elapsed_Time_ms, label, true);
   mst.SetValue(CDeviceUtils::ConvertToString(timestamp.getMsec()));
   md.SetTag(mst);

   MetadataSingleTag mstCount(MM::g_Keyword_Metadata_ImageNumber, label, true);
   mstCount.SetValue(CDeviceUtils::ConvertToString(thd_->GetImageCounter()));
   md.SetTag(mstCount);

   // insert all three channels at once
   int ret = GetCoreCallback()->InsertMultiChannel(this, rawBuffer_, GetNumberOfComponents(), GetImageWidth(), GetImageHeight(), GetImageBytesPerPixel(), &md);
   if (!stopOnOverflow_ && ret == DEVICE_BUFFER_OVERFLOW)
   {
      // do not stop on overflow - just reset the buffer
      GetCoreCallback()->ClearImageBuffer(this);
      // repeat the insert
      return GetCoreCallback()->InsertMultiChannel(this, rawBuffer_, GetNumberOfComponents(), GetImageWidth(), GetImageHeight(), GetImageBytesPerPixel(), &md);
   } else
      return ret;
}
int DemoStreamingCamera::CopyToRawBuffer()
{
   int ret = DEVICE_ERR;
   unsigned long singleChannelSize = img_[0].Width() * img_[0].Height() * img_[0].Depth();
   try
   {
      MMThreadGuard(this->rawBufferLock_);
      if (color_)
      {
         if (img_[0].Depth() == 1)
         {
            for (unsigned i=0; i<img_[0].Width(); i++)
            {
               unsigned lineOffset = img_[0].Width() * i;
               for (unsigned j=0; j<img_[0].Height(); j++)
               {
                  unsigned char* pBuf = rawBuffer_ + (lineOffset + j) * 4;
                  *(pBuf+3) = 0xff;
                  *(pBuf) =  * const_cast<unsigned char*>(img_[2].GetPixels() + lineOffset + j);
                  *(pBuf+1) = * const_cast<unsigned char*>(img_[1].GetPixels() + lineOffset + j);
                  *(pBuf+2) = * const_cast<unsigned char*>(img_[0].GetPixels() + lineOffset + j);
               }
            }
         }
         /*
         memset(rawBuffer_, 0xff, singleChannelSize);
         memcpy(rawBuffer_ + singleChannelSize, img_[0].GetPixels(), singleChannelSize);
         memcpy(rawBuffer_ + 2 * singleChannelSize, img_[1].GetPixels(), singleChannelSize);
         memcpy(rawBuffer_ + 3 * singleChannelSize, img_[2].GetPixels(), singleChannelSize);
         */
      }
      else
      {
         memcpy(rawBuffer_, img_[0].GetPixels(), singleChannelSize);
      }
      ret = DEVICE_OK;
   }catch(...)
   {
      LogMessage("Exception in DemoStreamingCamera::CopyToRawBuffer()\n");
   }
   return ret;
}

int DemoStreamingCamera::ThreadRun()
{
   LogMessage("Pushing image in thread", true);
   printf ("Pushing image\n");
   int ret = PushImage();
   if (ret != DEVICE_OK)
   {
      // error occured so the acquisition must be stopped
      LogMessage("DemoStreamingCamera::ThreadRun(): Error\n");
   }
   CDeviceUtils::SleepMs((long)GetExposure());
   return ret;
}
