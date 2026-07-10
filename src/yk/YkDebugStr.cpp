#include "YkDebugStr.h"

#include <string>
#include <vector>

#include "../compiler/SourceCoordinate.h"
#include "../vmobjects/VMMethod.h"

#ifdef YK_DEBUG_STRS
  #include <cstddef>
  #include <cstdint>
  #include <cstdio>
  #include <cstdlib>
  #include <cstring>

  #include "../interpreter/bytecodes.h"
#endif

void YkInitMethodLocs(
    VMMethod* meth,
    [[maybe_unused]] const std::vector<SourceCoordinate>& bcCoords,
    [[maybe_unused]] const std::string& sourceFile) {
#ifdef YK_DEBUG_STRS
    const size_t n = meth->GetNumberOfBytecodes();
    std::vector<SourceCoordinate> coords(n);
    for (size_t i = 0; i < n && i < bcCoords.size(); i++) {
        coords[i] = bcCoords[i];
    }
    meth->InitYkLocs(coords.data(), sourceFile.c_str());
#else
    meth->InitYkLocs();
#endif
}

#ifdef YK_DEBUG_STRS

char** YkBuildDebugStrs(const uint8_t* bytecodes, size_t bcLen,
                        const SourceCoordinate* coords,
                        const char* sourceFile) {
    char** strs = static_cast<char**>(calloc(bcLen, sizeof(char*)));
    for (size_t i = 0; i < bcLen;
         i += Bytecode::GetBytecodeLength(bytecodes[i])) {
        const char* name = Bytecode::GetBytecodeName(bytecodes[i]);
        char tmp[256];
        if (coords[i].GetLine() != 0) {
            (void)snprintf(tmp, sizeof(tmp), "%s:%zu:%zu:%s", sourceFile,
                           coords[i].GetLine(), coords[i].GetColumn(), name);
        } else {
            (void)snprintf(tmp, sizeof(tmp), "%s:<unknown>:%s", sourceFile,
                           name);
        }
        size_t const len = strlen(tmp) + 1;
        strs[i] = static_cast<char*>(malloc(len));
        memcpy(strs[i], tmp, len);
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
    free(static_cast<void*>(strs));
}

#endif  // YK_DEBUG_STRS
