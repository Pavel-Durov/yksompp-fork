// A mark-and-sweep garbage collector. When memory runs low it finds every
// object the program can still reach ("mark"), and treats everything else as
// garbage whose memory can be reused ("sweep"). Objects are never moved.
//
// Marking starts from the roots - the globals and the interpreter's stack
// and follows references outward until the whole set of reachable objects has
// been visited.
//
// Each object is its own malloc'd allocation; sweeping walks all of them and
// frees the ones not reached this cycle.
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

// File scope so the static mark callback can reach them; the worklist is reused
// across collections.
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

    markReachableObjects();

    // Sweep: free everything not reached this cycle, keep the rest.
    std::vector<AbstractVMObject*> surviving;
    for (auto* obj : heap->objects) {
        if (obj->GetGCField() == heap->epoch) {
            surviving.push_back(obj);
        } else {
            heap->FreeObject(obj);
        }
    }
    heap->objects = std::move(surviving);

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
