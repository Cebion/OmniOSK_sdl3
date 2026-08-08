#ifndef OMNI_OSK_MODEL_H
#define OMNI_OSK_MODEL_H

#include "charset.h"

#ifdef __cplusplus
extern "C" {
#endif

#define OMNI_MAX_ITEMS 160

typedef enum OmniItemKind {
    OMNI_ITEM_CHARACTER,
    OMNI_ITEM_SPACE,
    OMNI_ITEM_BACKSPACE,
    OMNI_ITEM_SUBMIT,
    OMNI_ITEM_CANCEL,
    OMNI_ITEM_CLOSE
} OmniItemKind;

typedef struct OmniOskItem {
    OmniItemKind kind;
    char character;
    int row;
    int column;
    float x;
    float y;
    float width;
    float height;
} OmniOskItem;

typedef enum OmniModelAction {
    OMNI_ACTION_UP,
    OMNI_ACTION_DOWN,
    OMNI_ACTION_LEFT,
    OMNI_ACTION_RIGHT,
    OMNI_ACTION_CONFIRM,
    OMNI_ACTION_BACKSPACE,
    OMNI_ACTION_CHARSET
} OmniModelAction;

typedef struct OmniModelResult {
    int consumed;
    int emitted_character;
    char character;
    int submitted;
    int canceled;
    int closed;
    int backspace;
    int buffer_changed;
} OmniModelResult;

typedef struct OmniOskModel {
    OmniConfig config;
    OmniCharsetPage pages[OMNI_MAX_PAGES];
    size_t page_count;
    OmniOskItem items[OMNI_MAX_ITEMS];
    size_t item_count;
    int layout_rows;
    float layout_width;
    float panel_width_scale;
    size_t focus;
    size_t page_index;
    char buffer[OMNI_MAX_BUFFER_LIMIT + 1];
    size_t buffer_length;
    int active;
} OmniOskModel;

int omni_osk_model_init(OmniOskModel *model, const OmniConfig *config);
void omni_osk_model_activate(OmniOskModel *model);
void omni_osk_model_deactivate(OmniOskModel *model, int clear_buffer);
OmniModelResult omni_osk_model_apply(OmniOskModel *model, OmniModelAction action);
OmniModelResult omni_osk_model_dismiss(OmniOskModel *model, OmniDismissMode mode);
const OmniOskItem *omni_osk_model_focused(const OmniOskModel *model);
const char *omni_osk_model_page_name(const OmniOskModel *model);

#ifdef __cplusplus
}
#endif

#endif
