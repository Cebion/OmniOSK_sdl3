#include "osk_model.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#define CHECK(condition) do { if (!(condition)) { fprintf(stderr, "check failed: %s:%d: %s\n", __FILE__, __LINE__, #condition); return 1; } } while (0)

static const OmniOskItem *find_character(const OmniOskModel *model, char character)
{
    size_t index;
    for (index = 0; index < model->item_count; ++index) {
        if (model->items[index].kind == OMNI_ITEM_CHARACTER && model->items[index].character == character) {
            return &model->items[index];
        }
    }
    return NULL;
}

static const OmniOskItem *find_kind(const OmniOskModel *model, OmniItemKind kind)
{
    size_t index;
    for (index = 0; index < model->item_count; ++index) {
        if (model->items[index].kind == kind) {
            return &model->items[index];
        }
    }
    return NULL;
}

static int focus_kind(OmniOskModel *model, OmniItemKind kind)
{
    const OmniOskItem *item = find_kind(model, kind);
    if (item == NULL) {
        return 0;
    }
    model->focus = (size_t)(item - model->items);
    return 1;
}

int main(void)
{
    OmniConfig config;
    OmniOskModel model;
    OmniModelResult result;
    const OmniOskItem *q;
    const OmniOskItem *a;
    const OmniOskItem *z;
    const OmniOskItem *backspace;
    const OmniOskItem *cancel;
    const OmniOskItem *space;
    const OmniOskItem *submit;

    omni_config_defaults(&config);
    CHECK(omni_osk_model_init(&model, &config) == 0);
    omni_osk_model_activate(&model);
    q = find_character(&model, 'q');
    a = find_character(&model, 'a');
    z = find_character(&model, 'z');
    backspace = find_kind(&model, OMNI_ITEM_BACKSPACE);
    CHECK(q != NULL && a != NULL && z != NULL && backspace != NULL);
    cancel = find_kind(&model, OMNI_ITEM_CANCEL);
    space = find_kind(&model, OMNI_ITEM_SPACE);
    submit = find_kind(&model, OMNI_ITEM_SUBMIT);
    CHECK(cancel != NULL && space != NULL && submit != NULL);
    CHECK(omni_osk_model_focused(&model)->character == 'a');
    CHECK(q->row == 0 && q->column == 0);
    CHECK(a->row == 1 && a->column == 0 && a->x > q->x);
    CHECK(z->row == 2 && z->column == 0 && z->y > a->y);
    CHECK(backspace->row == 2 && backspace->x > find_character(&model, 'm')->x);
    CHECK(backspace->width > 1.5f && backspace->width < 1.7f);
    CHECK(find_kind(&model, OMNI_ITEM_CANCEL)->row == 3);
    CHECK(find_kind(&model, OMNI_ITEM_SPACE)->row == 3);
    CHECK(find_kind(&model, OMNI_ITEM_SUBMIT)->row == 3);
    CHECK(fabsf((cancel->x + submit->x + submit->width) * 0.5f - model.layout_width * 0.5f) < 0.01f);
    CHECK(fabsf(space->x - (cancel->x + cancel->width + 0.1f)) < 0.01f);
    CHECK(fabsf(submit->x - (space->x + space->width + 0.1f)) < 0.01f);
    CHECK(model.layout_rows == 4);
    CHECK(model.layout_width > 10.3f && model.layout_width < 10.5f);

    result = omni_osk_model_apply(&model, OMNI_ACTION_CONFIRM);
    CHECK(result.buffer_changed == 1 && model.buffer_length == 1 && model.buffer[0] == 'a');
    result = omni_osk_model_apply(&model, OMNI_ACTION_BACKSPACE);
    CHECK(result.buffer_changed == 1 && model.buffer_length == 0);
    result = omni_osk_model_apply(&model, OMNI_ACTION_DOWN);
    CHECK(omni_osk_model_focused(&model)->character == 'z');
    result = omni_osk_model_apply(&model, OMNI_ACTION_DOWN);
    CHECK(omni_osk_model_focused(&model)->kind == OMNI_ITEM_CANCEL);
    result = omni_osk_model_apply(&model, OMNI_ACTION_RIGHT);
    CHECK(omni_osk_model_focused(&model)->kind == OMNI_ITEM_SPACE);
    result = omni_osk_model_apply(&model, OMNI_ACTION_RIGHT);
    CHECK(omni_osk_model_focused(&model)->kind == OMNI_ITEM_SUBMIT);
    result = omni_osk_model_apply(&model, OMNI_ACTION_CONFIRM);
    CHECK(result.submitted == 1 && model.active == 0);

    omni_osk_model_activate(&model);
    result = omni_osk_model_apply(&model, OMNI_ACTION_CHARSET);
    CHECK(result.consumed == 1 && strcmp(omni_osk_model_page_name(&model), "upper") == 0);
    CHECK(omni_osk_model_focused(&model)->character == 'A');

    config.mode = OMNI_MODE_INSTANT;
    CHECK(omni_osk_model_init(&model, &config) == 0);
    omni_osk_model_activate(&model);
    result = omni_osk_model_apply(&model, OMNI_ACTION_CONFIRM);
    CHECK(result.emitted_character == 1 && result.character == 'a' && model.buffer_length == 0);
    CHECK(focus_kind(&model, OMNI_ITEM_CLOSE) == 1);
    result = omni_osk_model_apply(&model, OMNI_ACTION_CONFIRM);
    CHECK(result.submitted == 0 && result.canceled == 0 && model.active == 0);

    config.mode = OMNI_MODE_BUFFERED;
    strcpy(config.charset_specs[0], "number");
    config.charset_count = 1;
    CHECK(omni_osk_model_init(&model, &config) == 0);
    omni_osk_model_activate(&model);
    CHECK(model.layout_rows == 4);
    CHECK(model.layout_width > 6.4f && model.layout_width < 6.6f);
    CHECK(omni_osk_model_focused(&model)->character == '0');
    CHECK(find_character(&model, '0')->row == 3);
    CHECK(find_kind(&model, OMNI_ITEM_BACKSPACE)->row == 3);
    CHECK(find_kind(&model, OMNI_ITEM_CANCEL)->row == 3);
    CHECK(find_kind(&model, OMNI_ITEM_SUBMIT)->row == 3);

    strcpy(config.charset_specs[0], "special");
    CHECK(omni_osk_model_init(&model, &config) == 0);
    omni_osk_model_activate(&model);
    CHECK(model.layout_rows == 5);
    CHECK(find_character(&model, '!')->row == 0);
    CHECK(find_character(&model, '~')->row == 3);
    CHECK(find_kind(&model, OMNI_ITEM_BACKSPACE)->row == 4);
    CHECK(find_kind(&model, OMNI_ITEM_SPACE)->row == 4);

    strcpy(config.charset_specs[0], "lower+number");
    CHECK(omni_osk_model_init(&model, &config) == 0);
    omni_osk_model_activate(&model);
    CHECK(model.layout_rows == 6);
    CHECK(find_kind(&model, OMNI_ITEM_CANCEL)->row == 5);
    CHECK(find_kind(&model, OMNI_ITEM_SUBMIT)->row == 5);
    return 0;
}
