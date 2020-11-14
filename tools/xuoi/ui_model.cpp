// SPDX-License-Identifier: AGPL-3.0-or-later
// SPDX-FileCopyrightText: 2020 Danny Angelo Carminati Grein

#include "ui_model.h"
#include <stack>

static std::stack<ui_view> s_view_stack;

void ui_push(ui_model &m, ui_view v)
{
    s_view_stack.push(m.view);
    m.view = v;
}

void ui_pop(ui_model &m)
{
    m.view = s_view_stack.top();
    s_view_stack.pop();
}
