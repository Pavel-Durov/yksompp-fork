#include "MarkSweepHeap.h"

#include <cstdlib>
#include <cstring>
#include <string>

#include "../memory/Heap.h"
#include "../vm/Print.h"
#include "../vmobjects/AbstractObject.h"
#include "MarkSweepCollector.h"

MarkSweepHeap::MarkSweepHeap(size_t objectSpaceSize)
    : Heap<MarkSweepHeap>(new MarkSweepCollector(this)),
      collectionLimit((size_t)((double)objectSpaceSize * 0.9)),
      minCollectionLimit((size_t)((double)objectSpaceSize * 0.9)) {}

void MarkSweepHeap::accountAllocation(size_t bytes) {
    spcAlloc += bytes;
    if (spcAlloc >= collectionLimit) {
        requestGC();
    }
}

AbstractVMObject* MarkSweepHeap::AllocateObject(size_t size) {
    auto* newObject = (AbstractVMObject*)malloc(size);
    if (newObject == nullptr) {
        ErrorPrint("\nFailed to allocate " + to_string(size) + " Bytes.\n");
        Quit(-1);
    }
    memset((void*)newObject, 0, size);
    objects.push_back(newObject);

    accountAllocation(size);
    return newObject;
}
