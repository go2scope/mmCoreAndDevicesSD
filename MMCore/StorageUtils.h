#pragma once
#include <string>
#include "Devices/StorageInstance.h"

class DatasetEntry
{
public:
   std::string adapterName;
   int handle;

   DatasetEntry() : handle(-1)
   {}
   DatasetEntry(std::string adapter, int devHandle) : adapterName(adapter), handle(devHandle)
   {}
};

class AttachedDataset
{
public:

   std::shared_ptr<StorageInstance> pStorage;
   int handle;

   AttachedDataset() : handle(-1) {}
};
