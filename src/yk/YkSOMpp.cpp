#ifdef USE_YK

  #include "YkSOMpp.h"

  #include <cstdio>
  #include <cstdlib>

  #include "../interpreter/bytecodes.h"
  #include "../vm/Universe.h"
  #include "../vmobjects/VMMethod.h"

// --- Universe ---

YkMT* Universe::yk_mt = nullptr;

void YkUniverseInit() {
    char* yk_err = nullptr;
    Universe::yk_mt = yk_mt_new(&yk_err);
    if (yk_err != nullptr) {
        fprintf(stderr, "yk failed to initialise: %s\n", yk_err);
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

// Assign a yk location to each loop header (backward-jump target).
//
// Yk traces loops by recording execution from a control point until it cycles
// back to the same point. Only backward jumps create such cycles, so we only
// need locations at their targets. All other yklocs slots stay null.
void VMMethod::InitYkLocs() {
    // Walk the variable-length bytecode stream one instruction at a time.
    for (size_t i = 0; i < bcLength;
         i += Bytecode::GetBytecodeLength(bytecodes[i])) {
        if (bytecodes[i] == BC_JUMP_BACKWARD) {
            // Single-byte offset: target is within 255 bytes of this
            // instruction.
            yklocs[i - bytecodes[i + 1]] = yk_location_new();
        } else if (bytecodes[i] == BC_JUMP2_BACKWARD) {
            // Two-byte little-endian offset: covers loops larger than 255
            // bytes.
            yklocs[i - ComputeOffset(bytecodes[i + 1], bytecodes[i + 2])] =
                yk_location_new();
        }
    }
}

#endif
