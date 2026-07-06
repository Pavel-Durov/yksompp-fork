#include "MarkSweepCollector.h"

#include <cstddef>
#include <utility>
#include <vector>

#include "../memory/Heap.h"
#include "../misc/debug.h"
#include "../vm/Universe.h"
#include "../vmobjects/AbstractObject.h"
#include "../vmobjects/IntegerBox.h"
#include "../vmobjects/ObjectFormats.h"
#include "../vmobjects/VMFrame.h"
#include "MarkSweepHeap.h"

static size_t s_epoch = 0;
static size_t s_markedBytes = 0;
static std::vector<AbstractVMObject*> s_markStack;

void MarkSweepCollector::Collect() {
    DebugLog("MarkSweep Collect\n");

    auto* heap = GetHeap<MarkSweepHeap>();
    Timer::GCTimer.Resume();
    heap->resetGCTrigger();

    // New cycle. The epoch only increases, so a survivor marked in an older
    // cycle is never mistaken for live now -- no mark reset needed.
    heap->epoch++;
    s_epoch = heap->epoch;
    s_markedBytes = 0;

    for (auto& list : heap->freeLists) {
        list = nullptr;
    }
    for (auto& cursor : heap->sweepCursor) {
        cursor = 0;
    }

    markReachableObjects();

    // Sweep the large-object space.
    std::vector<AbstractVMObject*> survivingLarge;
    for (auto* obj : heap->largeObjects) {
        if (obj->GetGCField() == heap->epoch) {
            survivingLarge.push_back(obj);
        } else {
            heap->FreeObject(obj);
        }
    }
    heap->largeObjects = std::move(survivingLarge);


    heap->spcAlloc = s_markedBytes;
    // Collect again after allocating ~max(live, a heap's worth). The floor
    // makes the heap size (-H / objectSpaceSize) actually govern GC frequency,
    // like the copying collector, instead of collecting every ~live bytes.
    size_t const grown = 2 * s_markedBytes;
    heap->collectionLimit =
        grown > heap->minCollectionLimit ? grown : heap->minCollectionLimit;
    Timer::GCTimer.Halt();
}

// Marks an object with the current epoch and queues it. Iterative (worklist),
// not recursive, so deep object graphs can't overflow the native stack.
static gc_oop_t mark_object(gc_oop_t oop) {
    if (IS_TAGGED(oop)) {
        return oop;
    }

    AbstractVMObject* obj = AS_OBJ(oop);

    if (obj->GetGCField() == s_epoch) {
        return oop;
    }

    obj->SetGCField(s_epoch);
    s_markedBytes += obj->GetObjectSize();
    s_markStack.push_back(obj);
    return oop;
}

void MarkSweepCollector::markReachableObjects() {
    s_markStack.clear();
    Universe::WalkGlobals(mark_object);

    while (!s_markStack.empty()) {
        AbstractVMObject* obj = s_markStack.back();
        s_markStack.pop_back();
        obj->WalkObjects(mark_object);
    }
}
