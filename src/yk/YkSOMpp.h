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

// Yk requires exactly one call site for yk_mt_control_point in the binary.
// DISPATCH_NOGC/GC therefore jump to a trampoline label (YK_DISPATCH_START)
// where the single control point call lives. The trampoline is defined in
// Interpreter::Start() via YK_DISPATCH_TRAMPOLINE() below.
//
// Switch-based dispatch: computed gotos (goto*) compile to LLVM indirectbr
// which Yk's tracer cannot trace through. A switch compiles to a regular br
// with multiple successors and is fully traceable.

// NOLINTBEGIN(cppcoreguidelines-macro-usage)
#define DISPATCH_NOGC() goto YK_DISPATCH_START
#define DISPATCH_GC()                                       \
    {                                                       \
        if (GetHeap<HEAP_CLS>()->isCollectionTriggered()) { \
            startGC();                                      \
        }                                                   \
        goto YK_DISPATCH_START;                             \
    }
#define YK_DISPATCH_TRAMPOLINE()                               \
    YK_DISPATCH_START:                                         \
    yk_mt_control_point(Universe::yk_mt,                       \
                        &method->yklocs[bytecodeIndexGlobal]); \
    switch (currentBytecodes[bytecodeIndexGlobal]) {           \
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
// NOLINTEND(cppcoreguidelines-macro-usage)
