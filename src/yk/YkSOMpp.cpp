#ifdef USE_YK

  #include "YkSOMpp.h"

  #include <climits>
  #include <cstddef>
  #include <cstdint>
  #include <cstdio>
  #include <cstdlib>
  #include <cstring>

  #include "../interpreter/bytecodes.h"
  #include "../vm/Universe.h"
  #include "../vmobjects/VMClass.h"
  #include "../vmobjects/VMMethod.h"

// --- Universe ---

YkMT* Universe::yk_mt = nullptr;
uintptr_t Universe::globalsEpoch = 0;
uintptr_t Universe::invokablesEpoch = 0;

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

  #ifdef YK_DEBUG_STRS

static char** buildDebugStrs(const uint8_t* bytecodes, size_t bcLen,
                             const size_t* lineNums, const char* sourceFile) {
    char** strs = static_cast<char**>(calloc(bcLen, sizeof(char*)));
    const char* slash = strrchr(sourceFile, '/');
    if (slash != nullptr) {
        sourceFile = slash + 1;
    }
    for (size_t i = 0; i < bcLen;
         i += Bytecode::GetBytecodeLength(bytecodes[i])) {
        char tmp[256];
        if (lineNums[i] != 0) {
            snprintf(tmp, sizeof(tmp), "%s:%zu:%s", sourceFile, lineNums[i],
                     Bytecode::GetBytecodeName(bytecodes[i]));
        } else {
            snprintf(tmp, sizeof(tmp), "%s:<unknown>:%s", sourceFile,
                     Bytecode::GetBytecodeName(bytecodes[i]));
        }
        strs[i] = static_cast<char*>(malloc(strlen(tmp) + 1));
        strcpy(strs[i], tmp);
    }
    return strs;
}

void YkDestroyDebugStrs(char** strs, size_t bcLen) {
    if (strs == nullptr) {
        return;
    }
    for (size_t i = 0; i < bcLen; i++) {
        free(strs[i]);
    }
    free(strs);
}

  #endif  // YK_DEBUG_STRS

#define NOOPT_VAL(X) asm volatile("" : "+r,m"(X) : : "memory");

__attribute__((yk_idempotent))
uint8_t load_bc(uint8_t* bc, size_t big) {
  NOOPT_VAL(bc);
  NOOPT_VAL(big);
  return bc[big];
}

// Idempotent lookup wrappers. At call sites where every argument is a trace
// constant (all promoted), yk's j2 compiler replaces the call with the return
// value recorded while tracing — the runtime lookup vanishes from compiled
// traces. The fold emits no guard of its own, so each wrapper takes the
// matching epoch (Universe::invokablesEpoch / globalsEpoch) as an argument:
// callers promote it, and any table mutation bumps it, turning a stale fold
// into a failed promote-guard and a clean deopt.
//
// Return type is uintptr_t, not a pointer: ykllvm's idempotent-recorder pass
// only supports integer returns. NOOPT_VAL pins `epoch` as used so LTO's
// dead-argument elimination cannot strip it (that would silently unsound the
// fold).
__attribute__((yk_idempotent))
uintptr_t lookup_invokable_idem(VMClass* cls, VMSymbol* signature,
                                uintptr_t epoch) {
  NOOPT_VAL(epoch);
  return reinterpret_cast<uintptr_t>(cls->LookupInvokable(signature));
}

__attribute__((yk_idempotent))
uintptr_t get_global_idem(VMSymbol* name, uintptr_t epoch) {
  NOOPT_VAL(epoch);
  return reinterpret_cast<uintptr_t>(Universe::GetGlobal(name));
}

// Block classes are created once per arg count and then cached; a lazy first
// load goes through SetGlobal, which bumps globalsEpoch, so that epoch also
// guards this fold.
__attribute__((yk_idempotent))
uintptr_t get_block_class_idem(uintptr_t numArgs, uintptr_t epoch) {
  NOOPT_VAL(epoch);
  return reinterpret_cast<uintptr_t>(
      Universe::GetBlockClassWithArgs((uint8_t)numArgs));
}
// Assign a yk location to each loop header (backward-jump target).
//
// Yk traces loops by recording execution from a control point until it cycles
// back to the same point. Only backward jumps create such cycles, so we only
// need locations at their targets. All other yklocs slots stay null.
void VMMethod::InitYkLocs(const size_t* lineNums, const char* sourceFile) {
  #ifdef YK_DEBUG_STRS
    instdebugstrs = buildDebugStrs(bytecodes, bcLength, lineNums, sourceFile);
  #else
    (void)lineNums;
  #endif

  #ifdef YK_SKIP_SMALLTALK_STD
    if (sourceFile != nullptr && strstr(sourceFile, "Smalltalk/") != nullptr) {
        return;
    }
  #endif
    // Backward-jump targets: hot loop headers.
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
                yk_location_set_debug_str(&yklocs[target], instdebugstrs[target]);
            }
#endif
        }
    }
}

#endif
