#pragma once

struct ui_context;
struct ImVec2;

void load_shards();
void ui_shards(ui_context &ui, const ImVec2 &size);
