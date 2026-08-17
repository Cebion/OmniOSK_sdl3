#ifndef OMNI_GL_LOADER_H
#define OMNI_GL_LOADER_H

#include <SDL3/SDL.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct OmniGlFunctions {
    void *(*get_proc_address)(const char *name);
    int ready;
} OmniGlFunctions;

int omni_gl_loader_init(OmniGlFunctions *loader);
void *omni_gl_proc(const OmniGlFunctions *loader, const char *name);

#ifdef __cplusplus
}
#endif

#endif
