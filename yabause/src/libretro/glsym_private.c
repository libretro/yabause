#include <stddef.h>
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

#define SYM(x) { "gl" #x, &(gl##x) }
const struct rglgen_sym_map rglgen_symbol_map_private[] = {
#if defined(_OGLES3_)
    SYM(MemoryBarrier),
    SYM(BindFragDataLocation),
    SYM(PatchParameteri),
    SYM(DispatchCompute),
    SYM(BindImageTexture),
#else
    SYM(TextureBarrierNV),
#endif
    { NULL, NULL },
};
