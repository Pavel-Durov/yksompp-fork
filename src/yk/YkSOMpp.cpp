#include "YkSOMpp.h"

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "../compiler/SourceCoordinate.h"
#include "../interpreter/bytecodes.h"
#include "../vm/Globals.h"
#include "../vm/Universe.h"
#include "../vmobjects/VMClass.h"
#include "../vmobjects/VMMethod.h"
#include "YkDebugStr.h"

// --- Universe ---

YkMT* Universe::yk_mt = nullptr;

void YkUniverseInit() {
    char* yk_err = nullptr;
    Universe::yk_mt = yk_mt_new(&yk_err);
    if (yk_err != nullptr) {
        (void)fprintf(stderr, "yk failed to initialise: %s\n", yk_err);
        exit(1);
    }
}

void YkUniverseShutdown() {
    yk_mt_shutdown(Universe::yk_mt);
    Universe::yk_mt = nullptr;
}

// --- VMMethod ---

void YkMethodInit(YkLocation*& yklocs, size_t bcCount) {
    yklocs = static_cast<YkLocation*>(malloc(bcCount * sizeof(YkLocation)));
    for (size_t i = 0; i < bcCount; i++) {
        yklocs[i] = yk_location_null();
    }
}

void YkMethodDestroy(YkLocation* yklocs, size_t bcLength) {
    for (size_t i = 0; i < bcLength; i++) {
        if (!yk_location_is_null(yklocs[i])) {
            yk_location_drop(yklocs[i]);
        }
    }
    free(yklocs);
}

// Give each loop header (backward-jump target) a yk location. Yk only traces
// loops, and backward jumps are the only cycles, so other slots stay null.
// The per-bytecode debug strings are built by YkDebugStr and attached here.
void VMMethod::InitYkLocs([[maybe_unused]] const SourceCoordinate* coords,
                          [[maybe_unused]] const char* sourceFile) {
#ifdef YK_DEBUG_STRS
    if (coords != nullptr) {
        instdebugstrs =
            YkBuildDebugStrs(bytecodes, bcLength, coords, sourceFile);
        // Raw copy so inlineInto can carry each bytecode's coordinate.
        instsrccoords = static_cast<SourceCoordinate*>(
            malloc(bcLength * sizeof(SourceCoordinate)));
        memcpy(instsrccoords, coords, bcLength * sizeof(SourceCoordinate));
    }
#endif

    for (size_t i = 0; i < bcLength;
         i += Bytecode::GetBytecodeLength(bytecodes[i])) {
        size_t target = SIZE_MAX;
        if (bytecodes[i] == BC_JUMP_BACKWARD) {
            target = i - bytecodes[i + 1];
        } else if (bytecodes[i] == BC_JUMP2_BACKWARD) {
            target = i - ComputeOffset(bytecodes[i + 1], bytecodes[i + 2]);
        }

        if (target != SIZE_MAX) {
            yklocs[target] = yk_location_new();
#ifdef YK_DEBUG_STRS
            if (instdebugstrs != nullptr && instdebugstrs[target] != nullptr) {
                yk_location_set_debug_str(&yklocs[target],
                                          instdebugstrs[target]);
            }
#endif
        }
    }
}

__attribute__((yk_idempotent, noinline)) uintptr_t
YkFetchBytecode(uint8_t* bytecodes, size_t index) {
    NOOP(bytecodes);
    return bytecodes[index];
}

__attribute__((yk_idempotent, noinline)) uintptr_t
YkFetchInvokable(VMClass* cls, VMSymbol* signature) {
    NOOP(cls);
    return reinterpret_cast<uintptr_t>(cls->LookupInvokable(signature));
}

__attribute__((yk_idempotent, noinline)) uintptr_t YkFetchIntegerClass() {
    return reinterpret_cast<uintptr_t>(load_ptr(integerClass));
}
