#pragma once

// All Yk-specific declarations for SOM++.
// Include this (under #ifdef USE_YK) wherever Yk types or dispatch macros
// are needed. It subsumes Yk.h — do not include Yk.h separately.

// yk.h is a C header with two C++-incompatibilities:
//   1. Uses `restrict` (C99, not C++). Clang accepts `__restrict`.
//   2. No `extern "C"` guards — without them C++ mangles the names.
#ifdef __cplusplus
  #define restrict __restrict
extern "C" {
#endif
#include <yk.h>
#ifdef __cplusplus
  #undef restrict
}
#endif

// Yk lifecycle — implemented in YkSOMpp.cpp.
void YkUniverseInit();
void YkUniverseShutdown();
void YkMethodInit(YkLocation*& yklocs, size_t bcCount);
void YkMethodDestroy(YkLocation* yklocs, size_t bcLength);

uint8_t load_bc(uint8_t* bc, size_t big);

// yk_idempotent lookup wrappers (YkSOMpp.cpp): with all args promoted, the
// call folds out of compiled traces to the recorded result. Pass the matching
// promoted epoch (Universe::invokablesEpoch / globalsEpoch) for soundness.
class VMClass;
class VMSymbol;
uintptr_t lookup_invokable_idem(VMClass* cls, VMSymbol* signature,
                                uintptr_t epoch);
uintptr_t get_global_idem(VMSymbol* name, uintptr_t epoch);
uintptr_t get_block_class_idem(uintptr_t numArgs, uintptr_t epoch);

#ifdef YK_DEBUG_STRS
void YkDestroyDebugStrs(char** strs, size_t bcLen);
#endif

// Yk requires exactly one call site for yk_mt_control_point in the binary.
// DISPATCH_NOGC/GC therefore jump to a trampoline label (YK_DISPATCH_START)
// where the single control point call lives. The trampoline is defined in
// Interpreter::Start() via YK_DISPATCH_TRAMPOLINE() below.
//
// Switch-based dispatch: computed gotos (goto*) compile to LLVM indirectbr
// which Yk's tracer cannot trace through. A switch compiles to a regular br
// with multiple successors and is fully traceable.

// NOLINTBEGIN(cppcoreguidelines-macro-usage)
#ifdef YK_DEBUG_STRS
  #define YK_DEBUG_STR_CALL()                                       \
      if (method->instdebugstrs != nullptr) {                       \
          yk_debug_str(method->instdebugstrs[bytecodeIndexGlobal]); \
      }
#else
  #define YK_DEBUG_STR_CALL() (void)0
#endif

// `bc` (the currentBytecodes pointer) is threaded, not re-promoted per bytecode.
// It is loop-invariant between frame changes, so only the dispatches that can
// change currentBytecodes re-promote it:
//   DISPATCH_FULL   — returns and backward jumps. Returns restore the caller's
//                     bytecodes; backward jumps land on a loop header, and
//                     re-promoting there keeps `bc` a per-iteration trace
//                     constant so load_bc still folds inside the loop trace.
//   DISPATCH_GC     — sends and allocating bytecodes. A send enters a callee
//                     (new bytecodes); startGC() reloads currentBytecodes after
//                     a collection; PUSH_GLOBAL can trigger an implicit
//                     unknownGlobal: send. All change the frame, so re-promote.
//   DISPATCH_NOGC   — straight-line bytecodes. currentBytecodes is unchanged, so
//                     reuse the threaded `bc` and skip its reload+guard (~half of
//                     the per-bytecode promotion guards). `pc` is still promoted
//                     and the control point still runs (in YK_DISPATCH_START), so
//                     trace formation is unchanged.
#define YK_PROMOTE_BC() bc = (uint8_t*) yk_promote((void*) currentBytecodes)
#define DISPATCH_NOGC() goto YK_DISPATCH_START
#define DISPATCH_FULL() \
    {                   \
        YK_PROMOTE_BC();\
        goto YK_DISPATCH_START; \
    }
#define DISPATCH_GC()                                       \
    {                                                       \
        if (GetHeap<HEAP_CLS>()->isCollectionTriggered()) { \
            startGC();                                      \
        }                                                   \
        YK_PROMOTE_BC();                                    \
        goto YK_DISPATCH_START;                             \
    }
#define DISPATCH_FULL_GC() DISPATCH_GC()
#define YK_BC_SWITCH()                                         \
    YK_DEBUG_STR_CALL();                                       \
    switch (load_bc(bc, big)) {                                \
        case BC_HALT:                                          \
            goto LABEL_BC_HALT;                                \
        case BC_DUP:                                           \
            goto LABEL_BC_DUP;                                 \
        case BC_DUP_SECOND:                                    \
            goto LABEL_BC_DUP_SECOND;                          \
        case BC_PUSH_LOCAL:                                    \
            goto LABEL_BC_PUSH_LOCAL;                          \
        case BC_PUSH_LOCAL_0:                                  \
            goto LABEL_BC_PUSH_LOCAL_0;                        \
        case BC_PUSH_LOCAL_1:                                  \
            goto LABEL_BC_PUSH_LOCAL_1;                        \
        case BC_PUSH_LOCAL_2:                                  \
            goto LABEL_BC_PUSH_LOCAL_2;                        \
        case BC_PUSH_ARGUMENT:                                 \
            goto LABEL_BC_PUSH_ARGUMENT;                       \
        case BC_PUSH_SELF:                                     \
            goto LABEL_BC_PUSH_SELF;                           \
        case BC_PUSH_ARG_1:                                    \
            goto LABEL_BC_PUSH_ARG_1;                          \
        case BC_PUSH_ARG_2:                                    \
            goto LABEL_BC_PUSH_ARG_2;                          \
        case BC_PUSH_FIELD:                                    \
            goto LABEL_BC_PUSH_FIELD;                          \
        case BC_PUSH_FIELD_0:                                  \
            goto LABEL_BC_PUSH_FIELD_0;                        \
        case BC_PUSH_FIELD_1:                                  \
            goto LABEL_BC_PUSH_FIELD_1;                        \
        case BC_PUSH_BLOCK:                                    \
            goto LABEL_BC_PUSH_BLOCK;                          \
        case BC_PUSH_CONSTANT:                                 \
            goto LABEL_BC_PUSH_CONSTANT;                       \
        case BC_PUSH_CONSTANT_0:                               \
            goto LABEL_BC_PUSH_CONSTANT_0;                     \
        case BC_PUSH_CONSTANT_1:                               \
            goto LABEL_BC_PUSH_CONSTANT_1;                     \
        case BC_PUSH_CONSTANT_2:                               \
            goto LABEL_BC_PUSH_CONSTANT_2;                     \
        case BC_PUSH_0:                                        \
            goto LABEL_BC_PUSH_0;                              \
        case BC_PUSH_1:                                        \
            goto LABEL_BC_PUSH_1;                              \
        case BC_PUSH_NIL:                                      \
            goto LABEL_BC_PUSH_NIL;                            \
        case BC_PUSH_GLOBAL:                                   \
            goto LABEL_BC_PUSH_GLOBAL;                         \
        case BC_POP:                                           \
            goto LABEL_BC_POP;                                 \
        case BC_POP_LOCAL:                                     \
            goto LABEL_BC_POP_LOCAL;                           \
        case BC_POP_LOCAL_0:                                   \
            goto LABEL_BC_POP_LOCAL_0;                         \
        case BC_POP_LOCAL_1:                                   \
            goto LABEL_BC_POP_LOCAL_1;                         \
        case BC_POP_LOCAL_2:                                   \
            goto LABEL_BC_POP_LOCAL_2;                         \
        case BC_POP_ARGUMENT:                                  \
            goto LABEL_BC_POP_ARGUMENT;                        \
        case BC_POP_FIELD:                                     \
            goto LABEL_BC_POP_FIELD;                           \
        case BC_POP_FIELD_0:                                   \
            goto LABEL_BC_POP_FIELD_0;                         \
        case BC_POP_FIELD_1:                                   \
            goto LABEL_BC_POP_FIELD_1;                         \
        case BC_SEND:                                          \
            goto LABEL_BC_SEND;                                \
        case BC_SEND_1:                                        \
            goto LABEL_BC_SEND_1;                              \
        case BC_SUPER_SEND:                                    \
            goto LABEL_BC_SUPER_SEND;                          \
        case BC_RETURN_LOCAL:                                  \
            goto LABEL_BC_RETURN_LOCAL;                        \
        case BC_RETURN_NON_LOCAL:                              \
            goto LABEL_BC_RETURN_NON_LOCAL;                    \
        case BC_RETURN_SELF:                                   \
            goto LABEL_BC_RETURN_SELF;                         \
        case BC_RETURN_FIELD_0:                                \
            goto LABEL_BC_RETURN_FIELD_0;                      \
        case BC_RETURN_FIELD_1:                                \
            goto LABEL_BC_RETURN_FIELD_1;                      \
        case BC_RETURN_FIELD_2:                                \
            goto LABEL_BC_RETURN_FIELD_2;                      \
        case BC_INC:                                           \
            goto LABEL_BC_INC;                                 \
        case BC_DEC:                                           \
            goto LABEL_BC_DEC;                                 \
        case BC_INC_FIELD:                                     \
            goto LABEL_BC_INC_FIELD;                           \
        case BC_INC_FIELD_PUSH:                                \
            goto LABEL_BC_INC_FIELD_PUSH;                      \
        case BC_JUMP:                                          \
            goto LABEL_BC_JUMP;                                \
        case BC_JUMP_ON_FALSE_POP:                             \
            goto LABEL_BC_JUMP_ON_FALSE_POP;                   \
        case BC_JUMP_ON_TRUE_POP:                              \
            goto LABEL_BC_JUMP_ON_TRUE_POP;                    \
        case BC_JUMP_ON_FALSE_TOP_NIL:                         \
            goto LABEL_BC_JUMP_ON_FALSE_TOP_NIL;               \
        case BC_JUMP_ON_TRUE_TOP_NIL:                          \
            goto LABEL_BC_JUMP_ON_TRUE_TOP_NIL;                \
        case BC_JUMP_ON_NOT_NIL_POP:                           \
            goto LABEL_BC_JUMP_ON_NOT_NIL_POP;                 \
        case BC_JUMP_ON_NIL_POP:                               \
            goto LABEL_BC_JUMP_ON_NIL_POP;                     \
        case BC_JUMP_ON_NOT_NIL_TOP_TOP:                       \
            goto LABEL_BC_JUMP_ON_NOT_NIL_TOP_TOP;             \
        case BC_JUMP_ON_NIL_TOP_TOP:                           \
            goto LABEL_BC_JUMP_ON_NIL_TOP_TOP;                 \
        case BC_JUMP_IF_GREATER:                               \
            goto LABEL_BC_JUMP_IF_GREATER;                     \
        case BC_JUMP_BACKWARD:                                 \
            goto LABEL_BC_JUMP_BACKWARD;                       \
        case BC_JUMP2:                                         \
            goto LABEL_BC_JUMP2;                               \
        case BC_JUMP2_ON_FALSE_POP:                            \
            goto LABEL_BC_JUMP2_ON_FALSE_POP;                  \
        case BC_JUMP2_ON_TRUE_POP:                             \
            goto LABEL_BC_JUMP2_ON_TRUE_POP;                   \
        case BC_JUMP2_ON_FALSE_TOP_NIL:                        \
            goto LABEL_BC_JUMP2_ON_FALSE_TOP_NIL;              \
        case BC_JUMP2_ON_TRUE_TOP_NIL:                         \
            goto LABEL_BC_JUMP2_ON_TRUE_TOP_NIL;               \
        case BC_JUMP2_ON_NOT_NIL_POP:                          \
            goto LABEL_BC_JUMP2_ON_NOT_NIL_POP;                \
        case BC_JUMP2_ON_NIL_POP:                              \
            goto LABEL_BC_JUMP2_ON_NIL_POP;                    \
        case BC_JUMP2_ON_NOT_NIL_TOP_TOP:                      \
            goto LABEL_BC_JUMP2_ON_NOT_NIL_TOP_TOP;            \
        case BC_JUMP2_ON_NIL_TOP_TOP:                          \
            goto LABEL_BC_JUMP2_ON_NIL_TOP_TOP;                \
        case BC_JUMP2_IF_GREATER:                              \
            goto LABEL_BC_JUMP2_IF_GREATER;                    \
        case BC_JUMP2_BACKWARD:                                \
            goto LABEL_BC_JUMP2_BACKWARD;                      \
        default:                                               \
            __builtin_unreachable();                           \
    }
#define YK_DISPATCH_TRAMPOLINE()                                \
    YK_DISPATCH_START:                                          \
    big = (size_t) yk_promote((uintptr_t) bytecodeIndexGlobal); \
    yk_mt_control_point(Universe::yk_mt,                        \
                        &method->yklocs[bytecodeIndexGlobal]);  \
    YK_BC_SWITCH();
// NOLINTEND(cppcoreguidelines-macro-usage)
