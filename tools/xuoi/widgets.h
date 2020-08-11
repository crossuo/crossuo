// custom imgui widgets
#pragma once

#include <external/gfx/ui.h>

typedef void (*ListBoxItemCallback)(int index);

void HoverToolTip(const char *desc);
void HelpMarker(const char *desc);
void InputText(
    const char *id,
    const char *label,
    float w,
    char *buf,
    size_t buf_size,
    ImGuiInputTextFlags flags = 0,
    ImGuiInputTextCallback callback = nullptr,
    void *user_data = nullptr);
bool ComboBox(
    const char *id,
    const char *label,
    float w,
    int *current_item,
    const char *const items[],
    int items_count,
    int height_in_items = -1);
bool ListBox(
    const char *label,
    int *current_item,
    bool (*items_getter)(void *, int, const char **),
    void *data,
    int items_count,
    int height_in_items,
    ListBoxItemCallback callback = nullptr);
