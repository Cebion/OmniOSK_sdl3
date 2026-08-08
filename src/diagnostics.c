#include "diagnostics.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

static OmniLogLevel current_level = OMNI_LOG_WARN;
static unsigned char emitted[64];

static const char *level_name(OmniLogLevel level)
{
    switch (level) {
    case OMNI_LOG_ERROR: return "error";
    case OMNI_LOG_WARN: return "warn";
    case OMNI_LOG_INFO: return "info";
    case OMNI_LOG_DEBUG: return "debug";
    }
    return "unknown";
}

void omni_diag_set_level(OmniLogLevel level)
{
    current_level = level;
}

void omni_diag(OmniLogLevel level, const char *format, ...)
{
    va_list args;
    if (level > current_level) {
        return;
    }
    fprintf(stderr, "omni-osk[%s]: ", level_name(level));
    va_start(args, format);
    vfprintf(stderr, format, args);
    va_end(args);
    fputc('\n', stderr);
}

void omni_diag_once(unsigned int id, OmniLogLevel level, const char *format, ...)
{
    va_list args;
    if (id < sizeof(emitted) && emitted[id] != 0) {
        return;
    }
    if (id < sizeof(emitted)) {
        emitted[id] = 1;
    }
    if (level > current_level) {
        return;
    }
    fprintf(stderr, "omni-osk[%s]: ", level_name(level));
    va_start(args, format);
    vfprintf(stderr, format, args);
    va_end(args);
    fputc('\n', stderr);
}
