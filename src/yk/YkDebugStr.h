#pragma once
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "../compiler/SourceCoordinate.h"

class VMMethod;
class VMTrivialMethod;
class MethodGenerationContext;

void YkInitMethodLocs(VMMethod* meth,
                      const std::vector<SourceCoordinate>& bcCoords,
                      const std::string& sourceFile);

#ifdef YK_DEBUG_STRS
char** YkBuildDebugStrs(const uint8_t* bytecodes, size_t bcLen,
                        const SourceCoordinate* coords, const char* sourceFile);

void YkDestroyDebugStrs(char** strs, size_t bcLen);
#endif
