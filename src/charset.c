#include "charset.h"

#include "diagnostics.h"

#include <string.h>

static const char *category(const char *name)
{
    if (strcmp(name, "lower") == 0) return "abcdefghijklmnopqrstuvwxyz";
    if (strcmp(name, "upper") == 0) return "ABCDEFGHIJKLMNOPQRSTUVWXYZ";
    if (strcmp(name, "number") == 0) return "0123456789";
    if (strcmp(name, "special") == 0) return "!@#$%^&*()-_=+[]{};:'\",.<>/?\\|`~";
    return NULL;
}

int omni_charset_build(const OmniConfig *config, OmniCharsetPage *pages, size_t *page_count)
{
    size_t page_index;
    size_t output_index = 0;
    *page_count = 0;
    for (page_index = 0; page_index < config->charset_count; ++page_index) {
        char spec[OMNI_MAX_PAGE_SPEC];
        char *cursor;
        char *part;
        size_t length = 0;
        strncpy(spec, config->charset_specs[page_index], sizeof(spec) - 1);
        spec[sizeof(spec) - 1] = '\0';
        cursor = spec;
        while ((part = strsep(&cursor, "+")) != NULL) {
            const char *chars = category(part);
            size_t i;
            if (chars == NULL) {
                omni_diag(OMNI_LOG_WARN, "OMNI_CHARSETS page '%s' has unknown category '%s'", spec, part);
                continue;
            }
            for (i = 0; chars[i] != '\0' && length < OMNI_MAX_PAGE_CHARS - 1; ++i) {
                size_t existing;
                int duplicate = 0;
                for (existing = 0; existing < length; ++existing) {
                    if (pages[output_index].chars[existing] == chars[i]) {
                        duplicate = 1;
                        break;
                    }
                }
                if (!duplicate) {
                    pages[output_index].chars[length++] = chars[i];
                }
            }
        }
        pages[output_index].chars[length] = '\0';
        pages[output_index].length = length;
        strncpy(pages[output_index].name, config->charset_specs[page_index], sizeof(pages[output_index].name) - 1);
        pages[output_index].name[sizeof(pages[output_index].name) - 1] = '\0';
        if (length == 0) {
            omni_diag(OMNI_LOG_WARN, "OMNI_CHARSETS page '%s' is empty", config->charset_specs[page_index]);
            continue;
        }
        ++output_index;
        ++(*page_count);
    }
    if (*page_count == 0) {
        omni_diag(OMNI_LOG_ERROR, "no valid character pages are configured");
        return -1;
    }
    return 0;
}
