#pragma once

#include <cstddef>
#include <vector>

#include "../misc/defs.h"
#include "Heap.h"

class MarkSweepHeap : public Heap<MarkSweepHeap> {
    friend class MarkSweepCollector;

public:
    explicit MarkSweepHeap(size_t objectSpaceSize = 1048576);
    AbstractVMObject* AllocateObject(size_t size);

private:
    void accountAllocation(size_t bytes);

    std::vector<AbstractVMObject*> objects;  // every live-or-not-yet-swept object

    size_t epoch{0};  // current live mark; bumped each collection
    size_t spcAlloc{0};
    size_t collectionLimit;
    // floor for collectionLimit (~the configured heap size): collect only once
    // about a heap's worth has been allocated, like the copying collector,
    // rather than every ~live-set bytes.
    size_t minCollectionLimit;
};
