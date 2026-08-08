#ifndef OMNI_CHARSET_H
#define OMNI_CHARSET_H

#include "config.h"

#ifdef __cplusplus
extern "C" {
#endif

#define OMNI_MAX_PAGE_CHARS 128

typedef struct OmniCharsetPage {
    char name[OMNI_MAX_PAGE_SPEC];
    char chars[OMNI_MAX_PAGE_CHARS];
    size_t length;
} OmniCharsetPage;

int omni_charset_build(const OmniConfig *config, OmniCharsetPage *pages, size_t *page_count);

#ifdef __cplusplus
}
#endif

#endif
