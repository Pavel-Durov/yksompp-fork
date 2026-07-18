#pragma once

#include <cstddef>
#include <vector>

#include "../misc/defs.h"
#include "Heap.h"

class MarkSweepHeap : public Heap<MarkSweepHeap> {
    friend class MarkSweepCollector;
    struct FreeListEntry {
        FreeListEntry* next;
    };
    // NOLINTNEXTLINE(altera-struct-pack-align):
    struct Page {
        char* memory;
        size_t sweptEpoch;
    };

public:
    explicit MarkSweepHeap(size_t objectSpaceSize = 1048576);
    ~MarkSweepHeap();
    AbstractVMObject* AllocateObject(size_t size);

private:
    static size_t sizeClassIndex(size_t size);
    // Grab a new page from the OS and put all its cells on the class' free
    // list.
    void carveNewPage(size_t classIndex);
    // Sweep one page; reclaim unmarked cells, or free the page if none are
    // live.
    bool sweepPageAt(size_t classIndex, size_t pageIndex);
    // Sweep pages of the class until its free list has a cell.
    bool sweepNextPage(size_t classIndex);
    AbstractVMObject* allocateLargeObject(size_t size);
    void accountAllocation(size_t bytes);

    std::vector<FreeListEntry*> freeLists;
    std::vector<std::vector<Page*>> classPages;
    std::vector<size_t> sweepCursor;
    std::vector<AbstractVMObject*> largeObjects;

    size_t epoch{0};
    size_t spcAlloc{0};
    size_t collectionLimit;
    size_t minCollectionLimit;
};
