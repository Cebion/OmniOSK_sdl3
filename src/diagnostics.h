#ifndef OMNI_DIAGNOSTICS_H
#define OMNI_DIAGNOSTICS_H

#include "config.h"

#ifdef __cplusplus
extern "C" {
#endif

void omni_diag_set_level(OmniLogLevel level);
void omni_diag(OmniLogLevel level, const char *format, ...);
void omni_diag_once(unsigned int id, OmniLogLevel level, const char *format, ...);

#ifdef __cplusplus
}
#endif

#endif
