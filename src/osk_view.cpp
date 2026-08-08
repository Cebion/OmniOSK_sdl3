#include "osk_view.h"

#include "imgui.h"

#include <algorithm>
#include <cstdio>

namespace {

static ImVec4 as_imgui(const OmniColor &color, float alpha_scale = 1.0f)
{
    return ImVec4(color.r, color.g, color.b, color.a * alpha_scale);
}

static ImVec4 action_color(const OmniConfig &config, OmniItemKind kind)
{
    if (kind == OMNI_ITEM_CANCEL) return as_imgui(config.cancel_color);
    if (kind == OMNI_ITEM_SUBMIT) return as_imgui(config.submit_color);
    if (kind == OMNI_ITEM_CLOSE) return as_imgui(config.close_color);
    return as_imgui(config.key_color);
}

static bool semantic_action(OmniItemKind kind)
{
    return kind == OMNI_ITEM_CANCEL || kind == OMNI_ITEM_SUBMIT || kind == OMNI_ITEM_CLOSE;
}

static ImVec4 focused_button_color(const OmniConfig &config, OmniItemKind kind)
{
    const ImVec4 base = action_color(config, kind);
    if (!semantic_action(kind)) {
        return as_imgui(config.focus_color);
    }
    const ImVec4 focus = as_imgui(config.focus_color);
    return ImVec4(base.x * 0.58f + focus.x * 0.42f,
                  base.y * 0.58f + focus.y * 0.42f,
                  base.z * 0.58f + focus.z * 0.42f,
                  base.w);
}

static void draw_backspace_icon(const ImVec2 &minimum, const ImVec2 &maximum, ImU32 color)
{
    const float width = maximum.x - minimum.x;
    const float height = maximum.y - minimum.y;
    const float size = std::min(width, height) * 0.72f;
    const float icon_left = (minimum.x + maximum.x - size) * 0.5f;
    const float icon_top = (minimum.y + maximum.y - size) * 0.5f;
    const float left = icon_left + size * 0.20f;
    const float point = icon_left;
    const float right = icon_left + size * 0.88f;
    const float top = icon_top + size * 0.20f;
    const float bottom = icon_top + size * 0.80f;
    const float mid = (top + bottom) * 0.5f;
    ImDrawList *draw_list = ImGui::GetWindowDrawList();
    draw_list->AddLine(ImVec2(point, mid), ImVec2(left, top), color, 1.8f);
    draw_list->AddLine(ImVec2(point, mid), ImVec2(left, bottom), color, 1.8f);
    draw_list->AddLine(ImVec2(left, top), ImVec2(right, top), color, 1.8f);
    draw_list->AddLine(ImVec2(left, bottom), ImVec2(right, bottom), color, 1.8f);
    draw_list->AddLine(ImVec2(right, top), ImVec2(right, bottom), color, 1.8f);
    draw_list->AddLine(ImVec2(icon_left + size * 0.48f, icon_top + size * 0.38f),
                       ImVec2(icon_left + size * 0.72f, icon_top + size * 0.62f), color, 1.8f);
    draw_list->AddLine(ImVec2(icon_left + size * 0.72f, icon_top + size * 0.38f),
                       ImVec2(icon_left + size * 0.48f, icon_top + size * 0.62f), color, 1.8f);
}

static void draw_action_icon(OmniItemKind kind, const ImVec2 &minimum, const ImVec2 &maximum, ImU32 color)
{
    const float size = std::min(maximum.x - minimum.x, maximum.y - minimum.y) * 0.72f;
    const float left = (minimum.x + maximum.x - size) * 0.5f;
    const float top = (minimum.y + maximum.y - size) * 0.5f;
    const float right = left + size;
    const float bottom = top + size;
    const ImVec2 center((left + right) * 0.5f, (top + bottom) * 0.5f);
    ImDrawList *draw_list = ImGui::GetWindowDrawList();
    if (kind == OMNI_ITEM_SUBMIT) {
        draw_list->AddLine(ImVec2(left + size * 0.20f, center.y),
                           ImVec2(left + size * 0.42f, center.y + size * 0.20f), color, 2.0f);
        draw_list->AddLine(ImVec2(left + size * 0.42f, center.y + size * 0.20f),
                           ImVec2(left + size * 0.80f, center.y - size * 0.25f), color, 2.0f);
    } else {
        draw_list->AddLine(ImVec2(left + size * 0.25f, top + size * 0.25f),
                           ImVec2(left + size * 0.75f, top + size * 0.75f), color, 2.0f);
        draw_list->AddLine(ImVec2(left + size * 0.75f, top + size * 0.25f),
                           ImVec2(left + size * 0.25f, top + size * 0.75f), color, 2.0f);
    }
}

static void item_label(const OmniOskItem &item, size_t index, char *buffer, size_t buffer_size)
{
    if (item.kind == OMNI_ITEM_BACKSPACE) {
        std::snprintf(buffer, buffer_size, "##osk-backspace-%zu", index);
    } else if (item.kind == OMNI_ITEM_SPACE) {
        std::snprintf(buffer, buffer_size, "Space##osk-%zu", index);
    } else if (item.kind == OMNI_ITEM_SUBMIT) {
        std::snprintf(buffer, buffer_size, "Submit##osk-%zu", index);
    } else if (item.kind == OMNI_ITEM_CANCEL) {
        std::snprintf(buffer, buffer_size, "Cancel##osk-%zu", index);
    } else if (item.kind == OMNI_ITEM_CLOSE) {
        std::snprintf(buffer, buffer_size, "Close##osk-%zu", index);
    } else {
        std::snprintf(buffer, buffer_size, "%c##osk-%zu", item.character, index);
    }
}

static void output_size(OmniRuntime *runtime, int renderer_path, int *width, int *height)
{
    *width = 1280;
    *height = 720;
    if (runtime->window != nullptr) {
        SDL_GetWindowSize(runtime->window, width, height);
    } else if (renderer_path && runtime->renderer != nullptr) {
        SDL_GetRendererOutputSize(runtime->renderer, width, height);
    }
}

static void position_for_anchor(const OmniConfig &config, int output_width, int output_height,
                                float panel_width, float panel_height, int *x, int *y)
{
    const int margin = std::max(8, std::min(output_width, output_height) / 32);
    switch (config.anchor) {
    case OMNI_ANCHOR_TOP_LEFT: *x = margin; *y = margin; break;
    case OMNI_ANCHOR_TOP_CENTER: *x = (output_width - (int)panel_width) / 2; *y = margin; break;
    case OMNI_ANCHOR_TOP_RIGHT: *x = output_width - (int)panel_width - margin; *y = margin; break;
    case OMNI_ANCHOR_CENTER_LEFT: *x = margin; *y = (output_height - (int)panel_height) / 2; break;
    case OMNI_ANCHOR_CENTER: *x = (output_width - (int)panel_width) / 2; *y = (output_height - (int)panel_height) / 2; break;
    case OMNI_ANCHOR_CENTER_RIGHT: *x = output_width - (int)panel_width - margin; *y = (output_height - (int)panel_height) / 2; break;
    case OMNI_ANCHOR_BOTTOM_LEFT: *x = margin; *y = output_height - (int)panel_height - margin; break;
    case OMNI_ANCHOR_BOTTOM_RIGHT: *x = output_width - (int)panel_width - margin; *y = output_height - (int)panel_height - margin; break;
    case OMNI_ANCHOR_BOTTOM_CENTER:
    default: *x = (output_width - (int)panel_width) / 2; *y = output_height - (int)panel_height - margin; break;
    }
    *x += (int)(config.x * (float)output_width);
    *y += (int)(config.y * (float)output_height);
    *x = std::max(margin, std::min(*x, std::max(margin, output_width - (int)panel_width - margin)));
    *y = std::max(margin, std::min(*y, std::max(margin, output_height - (int)panel_height - margin)));
}

} // namespace

extern "C" void omni_osk_view_draw(OmniRuntime *runtime, int renderer_path)
{
    int output_width;
    int output_height;
    int window_x;
    int window_y;
    const OmniConfig &config = runtime->config;
    const OmniOskModel &model = runtime->model;
    float unit;
    float panel_width;
    float panel_height;
    float key_top;
    float vertical_offset;
    float content_width;
    float header_height = config.mode == OMNI_MODE_BUFFERED ? 42.0f : 0.0f;
    float padding = config.window_padding;
    int style_vars = 0;
    int style_colors = 0;

    output_size(runtime, renderer_path, &output_width, &output_height);
    content_width = std::min((float)output_width * config.width * model.panel_width_scale, (float)output_width);
    const float content_height = std::min((float)output_height * config.height, (float)output_height);
    const float available_width = std::max(1.0f, content_width - padding * 2.0f);
    const float available_height = std::max(1.0f, content_height - padding * 2.0f - header_height);
    unit = std::min(available_width / model.layout_width, available_height / (float)model.layout_rows);
    unit = std::max(1.0f, unit);
    panel_width = model.layout_width * unit + padding * 2.0f;
    panel_height = content_height;
    vertical_offset = std::max(0.0f, (available_height - (float)model.layout_rows * unit) * 0.5f);
    position_for_anchor(config, output_width, output_height, panel_width, panel_height, &window_x, &window_y);

    ImGui::SetNextWindowPos(ImVec2((float)window_x, (float)window_y), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(panel_width, panel_height), ImGuiCond_Always);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(padding, padding));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, config.window_rounding);
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0.0f, 0.0f));
    style_vars = 3;
    ImGui::PushStyleColor(ImGuiCol_WindowBg, as_imgui(config.panel_color, config.opacity));
    ImGui::PushStyleColor(ImGuiCol_Text, as_imgui(config.text_color, config.opacity));
    ImGui::PushStyleColor(ImGuiCol_Border, as_imgui(config.border_color, config.opacity));
    style_colors = 3;

    const ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoSavedSettings |
                                   ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize |
                                   ImGuiWindowFlags_NoNavInputs;
    if (!ImGui::Begin("##omni-osk", nullptr, flags)) {
        ImGui::End();
        ImGui::PopStyleColor(style_colors);
        ImGui::PopStyleVar(style_vars);
        return;
    }

    const ImVec2 available = ImGui::GetContentRegionAvail();
    if (config.mode == OMNI_MODE_BUFFERED) {
        ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.04f, 0.06f, 0.07f, 0.62f * config.opacity));
        ImGui::BeginChild("##omni-entry", ImVec2(available.x, header_height - 6.0f), true,
                          ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
        if (model.buffer_length > 0) {
            ImGui::TextUnformatted(model.buffer);
        } else {
            ImGui::TextUnformatted("|");
        }
        ImGui::SameLine(available.x - ImGui::CalcTextSize(omni_osk_model_page_name(&model)).x - 8.0f);
        ImGui::TextColored(ImVec4(0.64f, 0.76f, 0.84f, config.opacity), "%s", omni_osk_model_page_name(&model));
        ImGui::EndChild();
        ImGui::PopStyleColor();
    }

    key_top = ImGui::GetCursorPosY() + vertical_offset + (header_height > 0.0f ? 5.0f : 0.0f);
    const float key_gap = std::min(config.key_gap, unit * 0.2f);
    const float row_gap = std::min(config.row_gap, std::max(0.0f, unit * 0.88f - 1.0f));
    const float key_left = padding + (available.x - model.layout_width * unit) * 0.5f;
    for (size_t index = 0; index < model.item_count; ++index) {
        const OmniOskItem &item = model.items[index];
        const bool focused = index == model.focus;
        ImVec4 button_color = focused ? focused_button_color(config, item.kind) : action_color(config, item.kind);
        ImVec4 hovered_color = focused ? focused_button_color(config, item.kind) : button_color;
        char label[64];
        item_label(item, index, label, sizeof(label));
        const char *action_text = item.kind == OMNI_ITEM_CANCEL ? "Cancel" :
                                  item.kind == OMNI_ITEM_SUBMIT ? "Submit" : "Close";
        const float item_x = key_left + item.x * unit + key_gap * 0.5f;
        const float item_y = key_top + item.y * unit + row_gap * 0.5f;
        const float item_width = std::max(1.0f, item.width * unit - key_gap);
        const float item_height = std::max(1.0f, item.height * unit - row_gap);
        const bool icon_action = semantic_action(item.kind) &&
                                 item_width < ImGui::CalcTextSize(action_text).x + 10.0f;
        if (icon_action) {
            std::snprintf(label, sizeof(label), "##osk-action-%zu", index);
        }
        ImGui::SetCursorPos(ImVec2(item_x, item_y));
        button_color.w *= config.opacity;
        hovered_color.w *= config.opacity;
        ImGui::PushStyleColor(ImGuiCol_Button, button_color);
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, hovered_color);
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, as_imgui(config.focus_color, config.opacity));
        ImGui::PushStyleColor(ImGuiCol_Border, as_imgui(focused ? config.focus_color : config.border_color, config.opacity));
        if (semantic_action(item.kind) && !icon_action) {
            ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(2.0f, 0.0f));
        }
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, config.key_rounding);
        ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, focused ? 2.0f : 1.0f);
        ImGui::Button(label, ImVec2(item_width, item_height));
        const ImVec2 rect_min = ImGui::GetItemRectMin();
        const ImVec2 rect_max = ImGui::GetItemRectMax();
        ImGui::PopStyleVar();
        ImGui::PopStyleVar();
        if (semantic_action(item.kind) && !icon_action) {
            ImGui::PopStyleVar();
        }
        ImGui::PopStyleColor(4);
        if (item.kind == OMNI_ITEM_BACKSPACE) {
            draw_backspace_icon(rect_min, rect_max, ImGui::GetColorU32(as_imgui(config.text_color, config.opacity)));
        } else if (icon_action) {
            draw_action_icon(item.kind, rect_min, rect_max, ImGui::GetColorU32(as_imgui(config.text_color, config.opacity)));
        }
    }

    ImGui::End();
    ImGui::PopStyleColor(style_colors);
    ImGui::PopStyleVar(style_vars);
}
