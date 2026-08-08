#include "gl_loader.h"

#include "diagnostics.h"

#include <string.h>

static void *get_proc(const char *name)
{
    return SDL_GL_GetProcAddress(name);
}

int omni_gl_loader_init(OmniGlFunctions *loader)
{
    memset(loader, 0, sizeof(*loader));
    loader->get_proc_address = get_proc;
    loader->ready = get_proc("glGetString") != NULL;
    if (!loader->ready) {
        omni_diag_once(11, OMNI_LOG_WARN, "OpenGL entry points are unavailable; keeping presentation pass-through");
    }
    return loader->ready ? 0 : -1;
}

void *omni_gl_proc(const OmniGlFunctions *loader, const char *name)
{
    return loader->ready && loader->get_proc_address != NULL ? loader->get_proc_address(name) : NULL;
}
