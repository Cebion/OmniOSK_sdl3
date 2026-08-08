#include "osk_model.h"

#include "diagnostics.h"

#include <ctype.h>
#include <math.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>

static const char qwerty_order[] = "qwertyuiopasdfghjklzxcvbnm";

static void add_item(OmniOskModel *model, OmniItemKind kind, char character,
                     int row, int column, float x, float y, float width)
{
    OmniOskItem *item;
    if (model->item_count >= OMNI_MAX_ITEMS) {
        return;
    }
    item = &model->items[model->item_count++];
    item->kind = kind;
    item->character = character;
    item->row = row;
    item->column = column;
    item->x = x;
    item->y = y;
    item->width = width;
    item->height = 0.88f;
}

static int compare_items(const void *left_pointer, const void *right_pointer)
{
    const OmniOskItem *left = (const OmniOskItem *)left_pointer;
    const OmniOskItem *right = (const OmniOskItem *)right_pointer;
    if (left->row != right->row) {
        return left->row - right->row;
    }
    if (left->column != right->column) {
        return left->column - right->column;
    }
    return (int)left->kind - (int)right->kind;
}

static int qwerty_index(char character)
{
    size_t index;
    char lower = (char)tolower((unsigned char)character);
    for (index = 0; index < sizeof(qwerty_order) - 1; ++index) {
        if (qwerty_order[index] == lower) {
            return (int)index;
        }
    }
    return -1;
}

static int is_qwerty_page(const OmniCharsetPage *page)
{
    int seen[26] = {0};
    size_t index;
    if (page->length != 26) {
        return 0;
    }
    for (index = 0; index < page->length; ++index) {
        int position = qwerty_index(page->chars[index]);
        if (position < 0 || seen[position] != 0) {
            return 0;
        }
        seen[position] = 1;
    }
    return 1;
}

static int is_number_page(const OmniCharsetPage *page)
{
    size_t index;
    if (page->length == 0) {
        return 0;
    }
    for (index = 0; index < page->length; ++index) {
        if (page->chars[index] < '0' || page->chars[index] > '9') {
            return 0;
        }
    }
    return 1;
}

static void add_qwerty_page(OmniOskModel *model, const OmniCharsetPage *page)
{
    size_t index;
    const float row_starts[] = {0.0f, 0.4f, 0.8f};
    const float key_widths[] = {0.95f, 0.98f, 1.04f};
    const float key_pitches[] = {1.05f, 1.08f, 1.14f};
    for (index = 0; index < page->length; ++index) {
        int position = qwerty_index(page->chars[index]);
        int row = position < 10 ? 0 : position < 19 ? 1 : 2;
        int column = position < 10 ? position : position < 19 ? position - 10 : position - 19;
        add_item(model, OMNI_ITEM_CHARACTER, page->chars[index], row, column,
                 row_starts[row] + (float)column * key_pitches[row], (float)row, key_widths[row]);
    }
    add_item(model, OMNI_ITEM_BACKSPACE, 0, 2, 7, 8.8f, 2.0f, 1.6f);
    add_item(model, model->config.mode == OMNI_MODE_BUFFERED ? OMNI_ITEM_CANCEL : OMNI_ITEM_CLOSE,
             0, 3, 0, 0.0f, 3.0f, 1.7f);
    add_item(model, OMNI_ITEM_SPACE, ' ', 3, 1, 1.8f, 3.0f,
             model->config.mode == OMNI_MODE_BUFFERED ? 6.8f : 8.6f);
    if (model->config.mode == OMNI_MODE_BUFFERED) {
        add_item(model, OMNI_ITEM_SUBMIT, 0, 3, 2, 8.7f, 3.0f, 1.7f);
    }
    model->layout_width = 10.4f;
    model->panel_width_scale = 1.0f;
    model->layout_rows = 4;
}

static void add_number_page(OmniOskModel *model, const OmniCharsetPage *page)
{
    size_t index;
    const float grid_start = 0.585f;
    const float key_pitch = 1.81f;
    const float key_width = 1.71f;
    for (index = 0; index < page->length; ++index) {
        int digit = page->chars[index] - '0';
        int row = digit == 0 ? 3 : (digit - 1) / 3;
        int column = digit == 0 ? 1 : (digit - 1) % 3;
        float x = digit == 0 ? 2.015f : grid_start + (float)column * key_pitch;
        float width = digit == 0 ? (model->config.mode == OMNI_MODE_BUFFERED ? 1.185f : 1.90f) : key_width;
        add_item(model, OMNI_ITEM_CHARACTER, page->chars[index], row, column, x, (float)row, width);
    }
    add_item(model, OMNI_ITEM_BACKSPACE, 0, 3, 2,
             model->config.mode == OMNI_MODE_BUFFERED ? 3.3f : 4.015f, 3.0f,
             model->config.mode == OMNI_MODE_BUFFERED ? 1.185f : 1.90f);
    add_item(model, model->config.mode == OMNI_MODE_BUFFERED ? OMNI_ITEM_CANCEL : OMNI_ITEM_CLOSE,
             0, 3, 0, grid_start, 3.0f, 1.33f);
    if (model->config.mode == OMNI_MODE_BUFFERED) {
        add_item(model, OMNI_ITEM_SUBMIT, 0, 3, 3, 4.585f, 3.0f, 1.33f);
    }
    model->layout_width = 6.5f;
    model->panel_width_scale = 0.86f;
    model->layout_rows = 4;
}

static void add_special_page(OmniOskModel *model, const OmniCharsetPage *page)
{
    size_t index;
    int footer_row = (int)((page->length + 7U) / 8U);
    const float key_pitch = 1.2625f;
    for (index = 0; index < page->length; ++index) {
        int row = (int)(index / 8U);
        int column = (int)(index % 8U);
        add_item(model, OMNI_ITEM_CHARACTER, page->chars[index], row, column,
                 (float)column * key_pitch, (float)row, 1.1625f);
    }
    add_item(model, model->config.mode == OMNI_MODE_BUFFERED ? OMNI_ITEM_CANCEL : OMNI_ITEM_CLOSE,
             0, footer_row, 0, 0.0f, (float)footer_row, 1.7f);
    add_item(model, OMNI_ITEM_SPACE, ' ', footer_row, 1, 1.8f, (float)footer_row,
             model->config.mode == OMNI_MODE_BUFFERED ? 4.6f : 8.2f);
    add_item(model, OMNI_ITEM_BACKSPACE, 0, footer_row, 2, 6.5f, (float)footer_row, 1.7f);
    if (model->config.mode == OMNI_MODE_BUFFERED) {
        add_item(model, OMNI_ITEM_SUBMIT, 0, footer_row, 3, 8.3f, (float)footer_row, 1.7f);
    }
    model->layout_width = 10.0f;
    model->panel_width_scale = 1.0f;
    model->layout_rows = footer_row + 1;
}

static void rebuild_items(OmniOskModel *model, const OmniOskItem *previous_focus)
{
    const OmniCharsetPage *page = &model->pages[model->page_index];
    size_t index;
    int found = -1;
    model->item_count = 0;

    if (is_qwerty_page(page)) {
        add_qwerty_page(model, page);
    } else if (is_number_page(page)) {
        add_number_page(model, page);
    } else {
        add_special_page(model, page);
    }

    qsort(model->items, model->item_count, sizeof(model->items[0]), compare_items);
    if (previous_focus != NULL && previous_focus->kind == OMNI_ITEM_CHARACTER) {
        for (index = 0; index < model->item_count; ++index) {
            if (previous_focus->kind == model->items[index].kind &&
                ((previous_focus->kind == OMNI_ITEM_CHARACTER &&
                  tolower((unsigned char)previous_focus->character) == tolower((unsigned char)model->items[index].character)) ||
                 previous_focus->kind != OMNI_ITEM_CHARACTER)) {
                found = (int)index;
                break;
            }
        }
    }
    if (found < 0) {
        for (index = 0; index < model->item_count; ++index) {
            if (model->items[index].kind == OMNI_ITEM_CHARACTER &&
                model->items[index].character == page->chars[0]) {
                found = (int)index;
                break;
            }
        }
    }
    model->focus = found >= 0 ? (size_t)found : 0;
}

int omni_osk_model_init(OmniOskModel *model, const OmniConfig *config)
{
    memset(model, 0, sizeof(*model));
    model->config = *config;
    if (omni_charset_build(config, model->pages, &model->page_count) != 0) {
        return -1;
    }
    rebuild_items(model, NULL);
    return 0;
}

void omni_osk_model_activate(OmniOskModel *model)
{
    model->active = 1;
    model->buffer_length = 0;
    model->buffer[0] = '\0';
    model->page_index = 0;
    rebuild_items(model, NULL);
}

void omni_osk_model_deactivate(OmniOskModel *model, int clear_buffer)
{
    model->active = 0;
    if (clear_buffer) {
        model->buffer_length = 0;
        model->buffer[0] = '\0';
    }
}

static size_t nearest_item(const OmniOskModel *model, int direction)
{
    const OmniOskItem *current = &model->items[model->focus];
    size_t index;
    size_t best = model->focus;
    float best_score = 0.0f;
    int best_row_distance = INT_MAX;
    int found = 0;
    for (index = 0; index < model->item_count; ++index) {
        const OmniOskItem *candidate = &model->items[index];
        float current_center = current->x + current->width * 0.5f;
        float candidate_center = candidate->x + candidate->width * 0.5f;
        float dx = candidate_center - current_center;
        float dy = candidate->y - current->y;
        int row_distance = abs(candidate->row - current->row);
        float score;
        if (index == model->focus) {
            continue;
        }
        if ((direction == OMNI_ACTION_LEFT && dx >= -0.001f) ||
            (direction == OMNI_ACTION_RIGHT && dx <= 0.001f) ||
            (direction == OMNI_ACTION_UP && dy >= -0.001f) ||
            (direction == OMNI_ACTION_DOWN && dy <= 0.001f)) {
            continue;
        }
        if ((direction == OMNI_ACTION_LEFT || direction == OMNI_ACTION_RIGHT) && row_distance != 0) {
            continue;
        }
        score = dx * dx + dy * dy;
        if ((direction == OMNI_ACTION_UP || direction == OMNI_ACTION_DOWN) && row_distance < best_row_distance) {
            best = index;
            best_score = score;
            best_row_distance = row_distance;
            found = 1;
        } else if (!found ||
                   ((direction == OMNI_ACTION_LEFT || direction == OMNI_ACTION_RIGHT || row_distance == best_row_distance) &&
                    (score < best_score || (fabsf(score - best_score) < 0.0001f && candidate->row < model->items[best].row)))) {
            best = index;
            best_score = score;
            best_row_distance = row_distance;
            found = 1;
        }
    }
    return best;
}

static OmniModelResult result(int consumed)
{
    OmniModelResult output;
    memset(&output, 0, sizeof(output));
    output.consumed = consumed;
    return output;
}

OmniModelResult omni_osk_model_apply(OmniOskModel *model, OmniModelAction action)
{
    OmniModelResult output = result(1);
    const OmniOskItem *item;
    if (!model->active || model->item_count == 0) {
        output.consumed = 0;
        return output;
    }
    if (action == OMNI_ACTION_UP || action == OMNI_ACTION_DOWN ||
        action == OMNI_ACTION_LEFT || action == OMNI_ACTION_RIGHT) {
        model->focus = nearest_item(model, action);
        return output;
    }
    if (action == OMNI_ACTION_CHARSET) {
        OmniOskItem previous_item = model->items[model->focus];
        model->page_index = (model->page_index + 1) % model->page_count;
        rebuild_items(model, &previous_item);
        return output;
    }
    if (action == OMNI_ACTION_BACKSPACE) {
        if (model->config.mode == OMNI_MODE_BUFFERED && model->buffer_length > 0) {
            --model->buffer_length;
            model->buffer[model->buffer_length] = '\0';
            output.buffer_changed = 1;
        } else if (model->config.mode == OMNI_MODE_INSTANT) {
            output.backspace = 1;
        }
        return output;
    }
    item = &model->items[model->focus];
    if (item->kind == OMNI_ITEM_BACKSPACE) {
        return omni_osk_model_apply(model, OMNI_ACTION_BACKSPACE);
    }
    if (item->kind == OMNI_ITEM_CHARACTER || item->kind == OMNI_ITEM_SPACE) {
        if (model->config.mode == OMNI_MODE_BUFFERED) {
            if (model->buffer_length < model->config.buffer_limit && model->buffer_length < OMNI_MAX_BUFFER_LIMIT) {
                model->buffer[model->buffer_length++] = item->character;
                model->buffer[model->buffer_length] = '\0';
                output.buffer_changed = 1;
            } else {
                omni_diag_once(7, OMNI_LOG_INFO, "buffer limit reached; character ignored");
            }
        } else {
            output.emitted_character = 1;
            output.character = item->character;
        }
        return output;
    }
    if (item->kind == OMNI_ITEM_SUBMIT) {
        output.submitted = 1;
        model->active = 0;
        return output;
    }
    if (item->kind == OMNI_ITEM_CANCEL) {
        output.canceled = 1;
        model->active = 0;
        model->buffer_length = 0;
        model->buffer[0] = '\0';
        return output;
    }
    if (item->kind == OMNI_ITEM_CLOSE) {
        output.closed = 1;
        model->active = 0;
        return output;
    }
    return output;
}

OmniModelResult omni_osk_model_dismiss(OmniOskModel *model, OmniDismissMode mode)
{
    OmniModelResult output = result(1);
    if (mode == OMNI_DISMISS_SUBMIT) {
        output.submitted = 1;
        model->active = 0;
    } else if (mode == OMNI_DISMISS_CANCEL) {
        output.canceled = 1;
        model->active = 0;
        model->buffer_length = 0;
        model->buffer[0] = '\0';
    }
    return output;
}

const OmniOskItem *omni_osk_model_focused(const OmniOskModel *model)
{
    return model->item_count == 0 ? NULL : &model->items[model->focus];
}

const char *omni_osk_model_page_name(const OmniOskModel *model)
{
    return model->page_count == 0 ? "" : model->pages[model->page_index].name;
}
