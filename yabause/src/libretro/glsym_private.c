#include "glsym/glsym.h"

#if defined(_OGLES3_)
RGLSYMGLMEMORYBARRIERPROC __rglgen_glMemoryBarrier;
RGLSYMGLBINDFRAGDATALOCATIONPROC __rglgen_glBindFragDataLocation;
RGLSYMGLPATCHPARAMETERIPROC __rglgen_glPatchParameteri;
RGLSYMGLDISPATCHCOMPUTEPROC __rglgen_glDispatchCompute;
RGLSYMGLBINDIMAGETEXTUREPROC __rglgen_glBindImageTexture;
#else
RGLSYMGLTEXTUREBARRIERNVPROC __rglgen_glTextureBarrierNV;
#endif
