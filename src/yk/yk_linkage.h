#pragma once

// YK_STATIC controls linkage of globals that yk's JIT must resolve via dlsym.
// Dlsym only finds symbols with external linkage, so in yk builds these
// globals cannot be static. In non-yk builds they remain static as originally
// intended.
#ifdef USE_YK
  #define YK_STATIC
#else
  #define YK_STATIC static
#endif
